/*
 * MintRIVA - portable core types
 *
 * Deliberately dependency-free and fixed-width so the same core compiles on a
 * modern host (for testing) and under vbcc/bebbo-gcc for m68k AmigaOS.
 */
#ifndef MR_TYPES_H
#define MR_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t mr_status;
#define MR_OK            0
#define MR_ERR          (-1)
#define MR_ENOMEM       (-2)
#define MR_EFORMAT      (-3)   /* container/stream malformed                */
#define MR_EUNSUPPORTED (-4)   /* recognised but not (yet) handled          */
#define MR_EAGAIN       (-5)   /* need more data / no output this call       */

/* Big-endian helpers are provided so the core never depends on host byte
 * order (m68k is big-endian; AVI/RIFF payloads are little-endian). */
static inline uint16_t mr_rl16(const uint8_t *p)
{ return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t mr_rl32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline uint16_t mr_rb16(const uint8_t *p)
{ return (uint16_t)((p[0] << 8) | p[1]); }
static inline uint32_t mr_rb24(const uint8_t *p)
{ return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
static inline uint32_t mr_rb32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8)  | p[3]; }

#define MR_FOURCC(a,b,c,d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
     ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* Surface pixel formats the decoders can emit. RGB24 is the lowest common
 * denominator for host validation; the Amiga renderer tier will add packed
 * chunky and planar targets that a decoder can write to directly. */
typedef enum {
    MR_PIX_RGB24 = 0,   /* r,g,b per pixel, top-down                        */
    MR_PIX_YUV420P      /* planar Y, Cb, Cr (2x2 subsampled)                */
} mr_pixfmt;

typedef struct {
    int       width;
    int       height;
    mr_pixfmt fmt;
    int       stride;   /* bytes per row of the primary plane              */
    uint8_t  *data;     /* owned by the decoder; valid until next decode    */
    /* Rows [dirty_y0, dirty_y1) changed this frame (the rest are identical to
     * the previous frame, since decoders patch a persistent buffer). A decoder
     * may report the full frame; dirty_y1 <= dirty_y0 means nothing changed. */
    int       dirty_y0;
    int       dirty_y1;
} mr_frame;

#endif /* MR_TYPES_H */
