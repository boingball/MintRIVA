/*
 * MintRIVA - task-safe allocation for code reachable from a background task.
 *
 * The HLS prefetch worker (player/amiga/hls_prefetch.c) runs the HTTP/source
 * download path on a *separate* Amiga task, concurrently with the main player
 * task. libnix's malloc arena is not safe to call from two tasks at once, so any
 * allocation on the worker's call path must instead use exec's AllocVec/FreeVec,
 * which draw from the system pool and are task-safe. The main task is free to
 * keep using malloc for everything else - the two allocators are independent
 * heaps, so they never collide.
 *
 * Only code the worker can reach (mr_http.c, and mr_source_create/close) needs
 * these. Everywhere else keeps using the C allocator directly. On the host, and
 * any non-Amiga build, these map straight back to malloc/calloc/free so the
 * portable core still builds and behaves identically.
 */
#ifndef MR_ALLOC_H
#define MR_ALLOC_H

#include <stddef.h>

#if defined(AMIGA_M68K) || defined(__amigaos__) || defined(__AMIGA__)

#include <exec/memory.h>
#include <proto/exec.h>

static inline void *mr_alloc(size_t n)  { return AllocVec(n, MEMF_ANY); }
static inline void *mr_allocz(size_t n) { return AllocVec(n, MEMF_ANY | MEMF_CLEAR); }
static inline void  mr_free(void *p)    { if (p) FreeVec(p); }

#else

#include <stdlib.h>

static inline void *mr_alloc(size_t n)  { return malloc(n); }
static inline void *mr_allocz(size_t n) { return calloc(1, n); }
static inline void  mr_free(void *p)    { free(p); }

#endif

#endif /* MR_ALLOC_H */
