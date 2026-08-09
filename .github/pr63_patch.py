from pathlib import Path


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


h264_path = Path("player/core/mr_h264.c")
h264 = h264_path.read_text()

old = '''    Close(fh);
}
#endif

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
'''
new = '''    Close(fh);
}

/* Leave a second, allocation-specific breadcrumb. Total free Fast RAM can be
 * misleading on a fragmented Amiga heap, so record the largest contiguous Fast
 * block as well as the exact allocation libavc asked us to make. */
static void h264_diag_allocfail(h264_state *s, WORD32 alignment, WORD32 size)
{
    BPTR fh;
    char buf[320];
    int n;
    ULONG fast_total, fast_largest, any_total, any_largest;

    if (!s || !s->diag_path) return;
    fh = Open((CONST_STRPTR)"RAM:MintRIVA-H264.allocfail", MODE_NEWFILE);
    if (!fh) return;

    fast_total = AvailMem(MEMF_FAST);
    fast_largest = AvailMem(MEMF_FAST | MEMF_LARGEST);
    any_total = AvailMem(MEMF_ANY);
    any_largest = AvailMem(MEMF_ANY | MEMF_LARGEST);
    n = snprintf(buf, sizeof buf,
                 "res=%dx%d requested=%ld alignment=%ld fast_total=%lu "
                 "fast_largest=%lu any_total=%lu any_largest=%lu\\n",
                 s->diag_width, s->diag_height, (long)size, (long)alignment,
                 (unsigned long)fast_total, (unsigned long)fast_largest,
                 (unsigned long)any_total, (unsigned long)any_largest);
    if (n > 0) {
        LONG bytes = n < (int)sizeof buf ? (LONG)n : (LONG)(sizeof buf - 1);
        Write(fh, (APTR)buf, bytes);
    }
    Close(fh);
}
#endif

static void *h264_aligned_alloc(void *context, WORD32 alignment, WORD32 size)
{
    h264_state *s = (h264_state *)context;
    uintptr_t p, aligned;
    size_t total;
    void *raw;

    if (size <= 0) return NULL;
    if (alignment < (WORD32)sizeof(void *))
        alignment = (WORD32)sizeof(void *);
    total = (size_t)size + (size_t)alignment - 1 + sizeof(void *);
    if (total < (size_t)size) return NULL;
    raw = malloc(total);
    if (!raw) {
#ifdef AMIGA_M68K
        h264_diag_allocfail(s, alignment, size);
#else
        (void)s;
#endif
        return NULL;
    }
    p = (uintptr_t)raw + sizeof(void *);
    aligned = (p + (uintptr_t)alignment - 1) &
              ~((uintptr_t)alignment - 1);
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}
'''
h264 = replace_once(h264, old, new, "allocator instrumentation")

old = '''    int width = dec->width, height = dec->height;
    int y;
    if (!yp || !up || !vp || !s->rgb) return MR_ERR;
    if ((int)f->u4_y_wd < width) width = (int)f->u4_y_wd;
'''
new = '''    int width = dec->width, height = dec->height;
    int y;
    if (!yp || !up || !vp) return MR_ERR;

    /* Do not reserve a full RGB24 frame during h264_open(). At 1080p that is
     * 6,220,800 bytes held idle while libavc performs its much larger first-
     * picture / DPB allocations. Allocate RGB only after libavc has actually
     * produced a picture, giving its decoder state first claim on contiguous
     * Fast RAM. */
    if (!s->rgb) {
        size_t rgb_bytes;
        if ((size_t)dec->width * (size_t)dec->height > SIZE_MAX / 3u)
            return MR_ENOMEM;
        rgb_bytes = (size_t)dec->width * (size_t)dec->height * 3u;
        s->rgb = (uint8_t *)malloc(rgb_bytes);
        if (!s->rgb) {
#ifdef AMIGA_M68K
            h264_diag_allocfail(s, (WORD32)sizeof(void *), (WORD32)rgb_bytes);
#endif
            return MR_ENOMEM;
        }
        dec->frame.data = s->rgb;
    }
    if ((int)f->u4_y_wd < width) width = (int)f->u4_y_wd;
'''
h264 = replace_once(h264, old, new, "lazy RGB allocation")

old = '''    create_in.s_ivd_create_ip_t.u4_share_disp_buf = 0;
    create_in.s_ivd_create_ip_t.pf_aligned_alloc = h264_aligned_alloc;
    create_in.s_ivd_create_ip_t.pf_aligned_free = h264_aligned_free;
    create_out.s_ivd_create_op_t.u4_size = sizeof create_out;
'''
new = '''    create_in.s_ivd_create_ip_t.u4_share_disp_buf = 0;
    create_in.s_ivd_create_ip_t.pf_aligned_alloc = h264_aligned_alloc;
    create_in.s_ivd_create_ip_t.pf_aligned_free = h264_aligned_free;
    create_in.s_ivd_create_ip_t.pv_mem_ctxt = s;
    create_out.s_ivd_create_op_t.u4_size = sizeof create_out;
'''
h264 = replace_once(h264, old, new, "libavc allocator context")

old = '''    if ((size_t)dec->width * (size_t)dec->height >
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
'''
new = '''    if (set_decode_mode(s, IVD_DECODE_FRAME) != IV_SUCCESS)
        goto bad_format;

    dec->frame.width = dec->width;
    dec->frame.height = dec->height;
    dec->frame.fmt = MR_PIX_RGB24;
    dec->frame.stride = dec->width * 3;
    dec->frame.data = NULL; /* allocated lazily by emit_rgb() */
'''
h264 = replace_once(h264, old, new, "remove eager RGB allocation")

old = '''    mr_status captured_status = MR_EAGAIN;
    int output_captured = 0;
    clock_t input_mark;
'''
new = '''    mr_status captured_status = MR_EAGAIN;
    mr_status decode_failure = MR_OK;
    int output_captured = 0;
    clock_t input_mark;
'''
h264 = replace_once(h264, old, new, "decode failure state")

old = '''            if (r != IV_SUCCESS) {
                ret = r;
                break;
            }
'''
new = '''            if (r != IV_SUCCESS) {
                uint32_t error = sub_out.s_ivd_video_decode_op_t.u4_error_code;
                ret = r;
                /* Hardware caught 0x0000402b at 1080p: fatal plus
                 * IVD_MEM_ALLOC_FAILED. Preserve that distinction so mrplay can
                 * tear down cleanly instead of feeding more AUs to a dead codec. */
                if ((error & IVD_ERROR_MASK) == IVD_MEM_ALLOC_FAILED)
                    decode_failure = MR_ENOMEM;
                else
                    decode_failure = MR_EFORMAT;
                break;
            }
'''
h264 = replace_once(h264, old, new, "map libavc allocation failure")

old = '''    if (s->service) s->service(s->service_opaque);
    if (output_captured)
        return captured_status;
    return ret == IV_SUCCESS ? MR_EAGAIN : MR_EFORMAT;
'''
new = '''    if (s->service) s->service(s->service_opaque);
    if (decode_failure != MR_OK)
        return decode_failure;
    if (output_captured)
        return captured_status;
    return ret == IV_SUCCESS ? MR_EAGAIN : MR_EFORMAT;
'''
h264 = replace_once(h264, old, new, "propagate decode failure")

h264_path.write_text(h264)

mrplay_path = Path("player/amiga/mrplay.c")
mrplay = mrplay_path.read_text()
old = '''                    if (decode_status == MR_EFORMAT) {
                        printf("h264-decode-error: packet %lu len=%lu\\n",
                               (unsigned long)decoded_index,
                               (unsigned long)pkt.len);
                    }
                    if (decode_status == MR_OK) {
'''
new = '''                    if (decode_status == MR_ENOMEM) {
                        printf("h264-decode-oom: packet %lu len=%lu - "
                               "stopping playback\\n",
                               (unsigned long)decoded_index,
                               (unsigned long)pkt.len);
                        display_set_status(disp, "H.264 decoder out of memory");
                        /* libavc marks IVD_MEM_ALLOC_FAILED fatal. Do not keep
                         * demuxing into a dead decoder: take mrplay's existing
                         * quit/cleanup path so RTG, Paula and decoder buffers are
                         * released immediately. */
                        quit = 1;
                        continue;
                    }
                    if (decode_status == MR_EFORMAT) {
                        printf("h264-decode-error: packet %lu len=%lu\\n",
                               (unsigned long)decoded_index,
                               (unsigned long)pkt.len);
                    }
                    if (decode_status == MR_OK) {
'''
mrplay = replace_once(mrplay, old, new, "mrplay fatal OOM teardown")
mrplay_path.write_text(mrplay)

# All patch helpers are temporary and must disappear in the same commit.
for helper in (
    Path(".github/pr63_patch.py"),
    Path(".github/workflows/pr63-run.yml"),
    Path(".github/workflows/pr63-one-shot.yml"),
):
    if helper.exists():
        helper.unlink()
