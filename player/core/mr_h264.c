/*
 * MintRIVA - H.264/AVC decoder adapter.
 *
 * Ittiam libavc supplies the actual Baseline/Main/High Profile decoder,
 * including CABAC, B slices, multiple references, deblocking and DPB/display
 * reordering.  This file adapts MintRIVA's avc1/AVCC packets to libavc's
 * Annex-B API and converts its planar YUV420 output to the RGB24 frame used by
 * the current display backends.
 */
#include "mr_h264.h"

#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264d.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef MR_H264_DEBUG
#include <stdio.h>
#endif

typedef struct {
    iv_obj_t *handle;
    uint8_t  *packet;
    uint32_t  packet_cap;
    uint8_t  *out[3];
    uint32_t  out_size[3];
    uint32_t  out_count;
    uint8_t  *rgb;
    uint8_t   nal_length_size;
    uint32_t  timestamp;
} h264_state;

static void *h264_aligned_alloc(void *context, WORD32 alignment, WORD32 size)
{
    uintptr_t p, aligned;
    void *raw;
    (void)context;
    if (alignment < (WORD32)sizeof(void *))
        alignment = (WORD32)sizeof(void *);
    raw = malloc((size_t)size + (size_t)alignment - 1 + sizeof(void *));
    if (!raw) return NULL;
    p = (uintptr_t)raw + sizeof(void *);
    aligned = (p + (uintptr_t)alignment - 1) &
              ~((uintptr_t)alignment - 1);
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}

static void h264_aligned_free(void *context, void *ptr)
{
    (void)context;
    if (ptr) free(((void **)ptr)[-1]);
}

static mr_status reserve_packet(h264_state *s, uint32_t need)
{
    uint8_t *p;
    uint32_t cap;
    if (need <= s->packet_cap) return MR_OK;
    cap = s->packet_cap ? s->packet_cap : 4096;
    while (cap < need) {
        uint32_t next = cap < 0x40000000u ? cap * 2u : need;
        if (next < cap || next < need) next = need;
        cap = next;
    }
    p = (uint8_t *)realloc(s->packet, cap);
    if (!p) return MR_ENOMEM;
    s->packet = p;
    s->packet_cap = cap;
    return MR_OK;
}

static mr_status append_annexb_nal(h264_state *s, uint32_t *used,
                                   const uint8_t *nal, uint32_t len)
{
    mr_status st;
    if (!len || *used > UINT32_MAX - len - 4u) return MR_EFORMAT;
    st = reserve_packet(s, *used + len + 4u);
    if (st != MR_OK) return st;
    s->packet[*used + 0] = 0;
    s->packet[*used + 1] = 0;
    s->packet[*used + 2] = 0;
    s->packet[*used + 3] = 1;
    memcpy(s->packet + *used + 4u, nal, len);
    *used += len + 4u;
    return MR_OK;
}

/* Convert AVCDecoderConfigurationRecord (avcC) SPS/PPS arrays to Annex B. */
static mr_status avcc_config_to_annexb(h264_state *s,
                                       const uint8_t *cfg, uint32_t cfg_len,
                                       uint32_t *out_len)
{
    uint32_t p = 6, used = 0;
    unsigned count, i;
    if (!cfg || cfg_len < 7 || cfg[0] != 1) return MR_EFORMAT;
    s->nal_length_size = (uint8_t)((cfg[4] & 3u) + 1u);

    count = cfg[5] & 0x1fu;
    for (i = 0; i < count; i++) {
        uint32_t n;
        mr_status st;
        if (p + 2u > cfg_len) return MR_EFORMAT;
        n = mr_rb16(cfg + p); p += 2;
        if (n > cfg_len - p) return MR_EFORMAT;
        st = append_annexb_nal(s, &used, cfg + p, n);
        if (st != MR_OK) return st;
        p += n;
    }
    if (p >= cfg_len) return MR_EFORMAT;
    count = cfg[p++];
    for (i = 0; i < count; i++) {
        uint32_t n;
        mr_status st;
        if (p + 2u > cfg_len) return MR_EFORMAT;
        n = mr_rb16(cfg + p); p += 2;
        if (n > cfg_len - p) return MR_EFORMAT;
        st = append_annexb_nal(s, &used, cfg + p, n);
        if (st != MR_OK) return st;
        p += n;
    }
    if (!used) return MR_EFORMAT;
    *out_len = used;
    return MR_OK;
}

static uint32_t read_nal_size(const uint8_t *p, unsigned bytes)
{
    uint32_t n = 0;
    unsigned i;
    for (i = 0; i < bytes; i++) n = (n << 8) | p[i];
    return n;
}

/* Convert one MP4 sample (one AVCC access unit) to Annex B. */
static mr_status avcc_sample_to_annexb(h264_state *s,
                                       const uint8_t *data, uint32_t len,
                                       uint32_t *out_len)
{
    uint32_t p = 0, used = 0;
    unsigned nls = s->nal_length_size;
    if (nls < 1 || nls > 4) return MR_EFORMAT;
    while (p < len) {
        uint32_t n;
        mr_status st;
        if (len - p < nls) return MR_EFORMAT;
        n = read_nal_size(data + p, nls);
        p += nls;
        if (!n || n > len - p) return MR_EFORMAT;
#ifdef MR_H264_DEBUG
        fprintf(stderr, " nal=%u type=%u", (unsigned)n,
                (unsigned)(data[p] & 0x1f));
#endif
        st = append_annexb_nal(s, &used, data + p, n);
        if (st != MR_OK) return st;
        p += n;
    }
    if (!used) return MR_EFORMAT;
#ifdef MR_H264_DEBUG
    fputc('\n', stderr);
#endif
    *out_len = used;
    return MR_OK;
}

static IV_API_CALL_STATUS_T set_decode_mode(h264_state *s,
                                            IVD_VIDEO_DECODE_MODE_T mode)
{
    ih264d_ctl_set_config_ip_t in;
    ih264d_ctl_set_config_op_t out;
    memset(&in, 0, sizeof in);
    memset(&out, 0, sizeof out);
    in.s_ivd_ctl_set_config_ip_t.u4_size = sizeof in;
    in.s_ivd_ctl_set_config_ip_t.e_cmd = IVD_CMD_VIDEO_CTL;
    in.s_ivd_ctl_set_config_ip_t.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
    in.s_ivd_ctl_set_config_ip_t.e_vid_dec_mode = mode;
    in.s_ivd_ctl_set_config_ip_t.u4_disp_wd = 0;
    in.s_ivd_ctl_set_config_ip_t.e_frm_skip_mode = IVD_SKIP_NONE;
    in.s_ivd_ctl_set_config_ip_t.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
    out.s_ivd_ctl_set_config_op_t.u4_size = sizeof out;
    return ih264d_api_function(s->handle, &in, &out);
}

static void fill_output_desc(const h264_state *s, ivd_out_bufdesc_t *out)
{
    uint32_t i;
    memset(out, 0, sizeof *out);
    out->u4_num_bufs = s->out_count;
    for (i = 0; i < s->out_count && i < 3; i++) {
        out->pu1_bufs[i] = s->out[i];
        out->u4_min_out_buf_size[i] = s->out_size[i];
    }
}

static IV_API_CALL_STATUS_T decode_annexb(h264_state *s,
                                          const uint8_t *data, uint32_t len,
                                          ih264d_video_decode_op_t *out)
{
    ih264d_video_decode_ip_t in;
    memset(&in, 0, sizeof in);
    memset(out, 0, sizeof *out);
    in.s_ivd_video_decode_ip_t.u4_size = sizeof in;
    in.s_ivd_video_decode_ip_t.e_cmd = IVD_CMD_VIDEO_DECODE;
    in.s_ivd_video_decode_ip_t.u4_ts = s->timestamp++;
    in.s_ivd_video_decode_ip_t.pv_stream_buffer = (void *)data;
    in.s_ivd_video_decode_ip_t.u4_num_Bytes = len;
    fill_output_desc(s, &in.s_ivd_video_decode_ip_t.s_out_buffer);
    out->s_ivd_video_decode_op_t.u4_size = sizeof *out;
    return ih264d_api_function(s->handle, &in, out);
}

static int clip8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static mr_status emit_rgb(mr_decoder *dec,
                          const ivd_video_decode_op_t *base)
{
    h264_state *s = (h264_state *)dec->priv;
    const iv_yuv_buf_t *f = &base->s_disp_frm_buf;
    const uint8_t *yp = (const uint8_t *)f->pv_y_buf;
    const uint8_t *up = (const uint8_t *)f->pv_u_buf;
    const uint8_t *vp = (const uint8_t *)f->pv_v_buf;
    int width = dec->width, height = dec->height;
    int y;
    if (!yp || !up || !vp || !s->rgb) return MR_ERR;
    if ((int)f->u4_y_wd < width) width = (int)f->u4_y_wd;
    if ((int)f->u4_y_ht < height) height = (int)f->u4_y_ht;

    for (y = 0; y < height; y++) {
        const uint8_t *yr = yp + (size_t)y * f->u4_y_strd;
        const uint8_t *ur = up + (size_t)(y >> 1) * f->u4_u_strd;
        const uint8_t *vr = vp + (size_t)(y >> 1) * f->u4_v_strd;
        uint8_t *dst = s->rgb + (size_t)y * dec->width * 3u;
        int x;
        for (x = 0; x < width; x++) {
            int c = (int)yr[x] - 16;
            int d = (int)ur[x >> 1] - 128;
            int e = (int)vr[x >> 1] - 128;
            if (c < 0) c = 0;
            dst[x * 3 + 0] = (uint8_t)clip8((298 * c + 409 * e + 128) >> 8);
            dst[x * 3 + 1] = (uint8_t)clip8((298 * c - 100 * d -
                                            208 * e + 128) >> 8);
            dst[x * 3 + 2] = (uint8_t)clip8((298 * c + 516 * d + 128) >> 8);
        }
    }
    dec->frame.width = dec->width;
    dec->frame.height = dec->height;
    dec->frame.fmt = MR_PIX_RGB24;
    dec->frame.stride = dec->width * 3;
    dec->frame.data = s->rgb;
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = dec->height;
    return MR_OK;
}

static void h264_close(mr_decoder *dec);

static mr_status h264_open(mr_decoder *dec)
{
    h264_state *s;
    ih264d_create_ip_t create_in;
    ih264d_create_op_t create_out;
    ih264d_video_decode_op_t decode_out;
    ivd_ctl_getbufinfo_ip_t info_in;
    ivd_ctl_getbufinfo_op_t info_out;
    ih264d_ctl_set_num_cores_ip_t cores_in;
    ih264d_ctl_set_num_cores_op_t cores_out;
    uint32_t cfg_len, off;
    uint32_t i;

    if (!dec->config || dec->config_len < 7) return MR_EFORMAT;
    s = (h264_state *)calloc(1, sizeof *s);
    if (!s) return MR_ENOMEM;
    dec->priv = s;

    memset(&create_in, 0, sizeof create_in);
    memset(&create_out, 0, sizeof create_out);
    create_in.s_ivd_create_ip_t.u4_size = sizeof create_in;
    create_in.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
    create_in.s_ivd_create_ip_t.e_output_format = IV_YUV_420P;
    create_in.s_ivd_create_ip_t.u4_share_disp_buf = 0;
    create_in.s_ivd_create_ip_t.pf_aligned_alloc = h264_aligned_alloc;
    create_in.s_ivd_create_ip_t.pf_aligned_free = h264_aligned_free;
    create_out.s_ivd_create_op_t.u4_size = sizeof create_out;
    if (ih264d_api_function(NULL, &create_in, &create_out) != IV_SUCCESS)
        goto bad_format;
    s->handle = (iv_obj_t *)create_out.s_ivd_create_op_t.pv_handle;
    s->handle->pv_fxns = (void *)&ih264d_api_function;
    s->handle->u4_size = sizeof *s->handle;

    memset(&cores_in, 0, sizeof cores_in);
    memset(&cores_out, 0, sizeof cores_out);
    cores_in.u4_size = sizeof cores_in;
    cores_in.e_cmd = IVD_CMD_VIDEO_CTL;
    cores_in.e_sub_cmd =
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_SET_NUM_CORES;
    cores_in.u4_num_cores = 1;
    cores_out.u4_size = sizeof cores_out;
    if (ih264d_api_function(s->handle, &cores_in, &cores_out) != IV_SUCCESS)
        goto bad_format;

    if (set_decode_mode(s, IVD_DECODE_HEADER) != IV_SUCCESS)
        goto bad_format;
    if (avcc_config_to_annexb(s, dec->config, dec->config_len,
                              &cfg_len) != MR_OK)
        goto bad_format;

    /* Header mode can consume one NAL at a time.  Continue until every SPS
     * and PPS from avcC has been offered, requiring forward progress. */
    off = 0;
    while (off < cfg_len) {
        IV_API_CALL_STATUS_T ret =
            decode_annexb(s, s->packet + off, cfg_len - off, &decode_out);
        uint32_t used =
            decode_out.s_ivd_video_decode_op_t.u4_num_bytes_consumed;
        if (!used || used > cfg_len - off) {
            if (ret == IV_SUCCESS) break;
            goto bad_format;
        }
        off += used;
    }

    memset(&info_in, 0, sizeof info_in);
    memset(&info_out, 0, sizeof info_out);
    info_in.u4_size = sizeof info_in;
    info_in.e_cmd = IVD_CMD_VIDEO_CTL;
    info_in.e_sub_cmd = IVD_CMD_CTL_GETBUFINFO;
    info_out.u4_size = sizeof info_out;
    if (ih264d_api_function(s->handle, &info_in, &info_out) != IV_SUCCESS)
        goto bad_format;
    if (info_out.u4_min_num_out_bufs < 3) goto bad_format;
    s->out_count = 3;
    for (i = 0; i < 3; i++) {
        s->out_size[i] = info_out.u4_min_out_buf_size[i];
        if (!s->out_size[i]) goto bad_format;
        s->out[i] = (uint8_t *)malloc(s->out_size[i]);
        if (!s->out[i]) goto no_memory;
    }
    if ((size_t)dec->width * (size_t)dec->height >
        (SIZE_MAX / 3u)) goto no_memory;
    s->rgb = (uint8_t *)malloc((size_t)dec->width * dec->height * 3u);
    if (!s->rgb) goto no_memory;
    if (set_decode_mode(s, IVD_DECODE_FRAME) != IV_SUCCESS)
        goto bad_format;

    dec->frame.width = dec->width;
    dec->frame.height = dec->height;
    dec->frame.fmt = MR_PIX_RGB24;
    dec->frame.stride = dec->width * 3;
    dec->frame.data = s->rgb;
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = 0;
    return MR_OK;

no_memory:
    h264_close(dec);
    return MR_ENOMEM;
bad_format:
    h264_close(dec);
    return MR_EFORMAT;
}

static mr_status h264_decode(mr_decoder *dec,
                             const uint8_t *data, uint32_t len)
{
    h264_state *s = (h264_state *)dec->priv;
    ih264d_video_decode_op_t out;
    IV_API_CALL_STATUS_T ret;
    uint32_t annexb_len;
    mr_status st;
    if (!s || !data || !len) return MR_EFORMAT;
    st = avcc_sample_to_annexb(s, data, len, &annexb_len);
    if (st != MR_OK) return st;
    ret = decode_annexb(s, s->packet, annexb_len, &out);
#ifdef MR_H264_DEBUG
    fprintf(stderr, "h264 ts=%lu in=%lu annexb=%lu ret=%d consumed=%lu "
            "decoded=%lu output=%lu error=%08lx type=%d\n",
            (unsigned long)(s->timestamp - 1), (unsigned long)len,
            (unsigned long)annexb_len, (int)ret,
            (unsigned long)out.s_ivd_video_decode_op_t.u4_num_bytes_consumed,
            (unsigned long)out.s_ivd_video_decode_op_t.u4_frame_decoded_flag,
            (unsigned long)out.s_ivd_video_decode_op_t.u4_output_present,
            (unsigned long)out.s_ivd_video_decode_op_t.u4_error_code,
            (int)out.s_ivd_video_decode_op_t.e_pic_type);
#endif
    if (out.s_ivd_video_decode_op_t.u4_output_present)
        return emit_rgb(dec, &out.s_ivd_video_decode_op_t);
    return ret == IV_SUCCESS ? MR_EAGAIN : MR_EFORMAT;
}

static mr_status h264_flush(mr_decoder *dec)
{
    h264_state *s = (h264_state *)dec->priv;
    ivd_ctl_flush_ip_t flush_in;
    ivd_ctl_flush_op_t flush_out;
    ih264d_video_decode_op_t out;
    IV_API_CALL_STATUS_T ret;
    if (!s) return MR_EAGAIN;
    memset(&flush_in, 0, sizeof flush_in);
    memset(&flush_out, 0, sizeof flush_out);
    flush_in.u4_size = sizeof flush_in;
    flush_in.e_cmd = IVD_CMD_VIDEO_CTL;
    flush_in.e_sub_cmd = IVD_CMD_CTL_FLUSH;
    flush_out.u4_size = sizeof flush_out;
    ret = ih264d_api_function(s->handle, &flush_in, &flush_out);
    if (ret != IV_SUCCESS) return MR_EAGAIN;
    ret = decode_annexb(s, s->packet, 0, &out);
    if (out.s_ivd_video_decode_op_t.u4_output_present)
        return emit_rgb(dec, &out.s_ivd_video_decode_op_t);
    return MR_EAGAIN;
}

static void h264_close(mr_decoder *dec)
{
    h264_state *s = dec ? (h264_state *)dec->priv : NULL;
    uint32_t i;
    if (!s) return;
    if (s->handle) {
        ih264d_delete_ip_t in;
        ih264d_delete_op_t out;
        memset(&in, 0, sizeof in);
        memset(&out, 0, sizeof out);
        in.s_ivd_delete_ip_t.u4_size = sizeof in;
        in.s_ivd_delete_ip_t.e_cmd = IVD_CMD_DELETE;
        out.s_ivd_delete_op_t.u4_size = sizeof out;
        ih264d_api_function(s->handle, &in, &out);
    }
    for (i = 0; i < 3; i++) free(s->out[i]);
    free(s->packet);
    free(s->rgb);
    free(s);
    dec->priv = NULL;
    dec->frame.data = NULL;
}

const mr_codec mr_codec_h264 = {
    "H.264/AVC (libavc)",
    {
        MR_FOURCC('a','v','c','1'),
        0, 0, 0, 0, 0, 0, 0
    },
    h264_open,
    h264_decode,
    h264_close,
    h264_flush
};
