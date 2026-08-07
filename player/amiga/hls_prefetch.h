/*
 * MintRIVA - HLS segment prefetch worker (Amiga).
 *
 * Starts a background process that becomes the sole HTTP user and downloads
 * upcoming HLS segments into RAM ahead of the reader, then installs itself as
 * the mr_hls backend. The main player task never touches the network, so a
 * blocking segment fetch no longer freezes video presentation.
 *
 * hls_prefetch_start() returns 1 if the worker came up (prefetch active) or 0
 * otherwise, in which case the player keeps its default synchronous behaviour.
 * hls_prefetch_stop() must be called (from the main task) before the process
 * exits and before any other HTTP teardown, so the worker releases the
 * socket/TLS state it opened from its own task.
 */
#ifndef HLS_PREFETCH_H
#define HLS_PREFETCH_H

int  hls_prefetch_start(void);
void hls_prefetch_stop(void);

#endif /* HLS_PREFETCH_H */
