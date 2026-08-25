/*
 * MintVID - native YouTube n-challenge solver.
 *
 * The implementation embeds the QuickJS interpreter but deliberately omits
 * quickjs-libc: evaluated player code has no file, socket, process or AmigaOS
 * bindings. The pinned solver assets are embedded as architecture-matched
 * QuickJS bytecode, so no sidecar program or JavaScript files are required.
 */
#ifndef MR_YOUTUBE_NSIG_H
#define MR_YOUTUBE_NSIG_H

#include <stddef.h>

/* Solve one raw n challenge with an already-downloaded YouTube player. */
int mr_youtube_nsig_solve(const char *player_js, size_t player_js_len,
                          const char *challenge,
                          char *out, size_t out_size);

/* Replace either /n/<challenge> or the n= query parameter in a media URL. */
int mr_youtube_nsig_transform_url(const char *player_js,
                                  size_t player_js_len,
                                  const char *url,
                                  char *out, size_t out_size,
                                  void *opaque);

const char *mr_youtube_nsig_last_error(void);

#endif /* MR_YOUTUBE_NSIG_H */
