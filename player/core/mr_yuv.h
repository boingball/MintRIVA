/* MintRIVA - shared integer YUV conversion helpers. */
#ifndef MR_YUV_H
#define MR_YUV_H

#include <stdint.h>

typedef void (*mr_yuv_service_fn)(void *opaque);

/* Convert planar limited-range YUV420 to packed RGB24. U and V are sampled
 * once for each horizontal pair, matching 4:2:0 chroma geometry. Strides may
 * include padding. The optional service hook runs after every 16 output rows. */
void mr_yuv420_to_rgb24(uint8_t *dst, int dst_stride,
                        const uint8_t *y_plane, int y_stride,
                        const uint8_t *u_plane, int u_stride,
                        const uint8_t *v_plane, int v_stride,
                        int width, int height,
                        mr_yuv_service_fn service, void *service_opaque);

#endif /* MR_YUV_H */
