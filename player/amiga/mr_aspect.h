/* MintRIVA - integer aspect-fit geometry shared by RTG display/tests. */
#ifndef MR_ASPECT_H
#define MR_ASPECT_H

#include <stdint.h>

typedef struct mr_aspect_rect {
    int x, y, w, h;
} mr_aspect_rect;

/* Fit source pixels inside the destination without cropping or distortion.
 * The result is centred; unused space is intended for letter/pillar boxing. */
static inline mr_aspect_rect mr_aspect_fit(int src_w, int src_h,
                                           int box_w, int box_h)
{
    mr_aspect_rect r;
    r.x = r.y = 0;
    r.w = box_w > 0 ? box_w : 1;
    r.h = box_h > 0 ? box_h : 1;
    if (src_w <= 0 || src_h <= 0 || box_w <= 0 || box_h <= 0)
        return r;

    if ((uint64_t)(unsigned)src_w * (unsigned)box_h >
        (uint64_t)(unsigned)src_h * (unsigned)box_w) {
        r.w = box_w;
        r.h = (int)(((uint64_t)(unsigned)src_h * (unsigned)box_w +
                       (unsigned)src_w / 2U) / (unsigned)src_w);
    } else {
        r.h = box_h;
        r.w = (int)(((uint64_t)(unsigned)src_w * (unsigned)box_h +
                       (unsigned)src_h / 2U) / (unsigned)src_h);
    }
    if (r.w < 1) r.w = 1;
    if (r.h < 1) r.h = 1;
    if (r.w > box_w) r.w = box_w;
    if (r.h > box_h) r.h = box_h;
    r.x = (box_w - r.w) / 2;
    r.y = (box_h - r.h) / 2;
    return r;
}

#endif /* MR_ASPECT_H */
