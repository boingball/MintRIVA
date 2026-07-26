#ifndef MR_RAWVIDEO_H
#define MR_RAWVIDEO_H

#include "mr_codec.h"

extern const mr_codec mr_codec_rawvideo;
int mr_rawvideo_is_uyvy422(uint32_t fourcc);
uint32_t mr_rawvideo_uyvy422_stride(int width, int height,
                                    uint32_t packet_size);

#endif /* MR_RAWVIDEO_H */
