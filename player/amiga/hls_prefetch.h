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
 * The caller supplies signal bits and a quit probe used to interrupt waits on
 * the worker reply port. hls_prefetch_set_interrupt() updates that wait context
 * (e.g. after opening/closing a display window). hls_prefetch_stop() must be
 * called (from the main task) before the process exits and before any other
 * HTTP teardown, so the worker releases the socket/TLS state it opened from
 * its own task.
 */
#ifndef HLS_PREFETCH_H
#define HLS_PREFETCH_H

typedef int (*hls_prefetch_quit_probe_fn)(void *opaque);

int  hls_prefetch_start(int verbose, unsigned long interrupt_mask,
                        hls_prefetch_quit_probe_fn quit_probe, void *opaque);
void hls_prefetch_set_interrupt(unsigned long interrupt_mask,
                                hls_prefetch_quit_probe_fn quit_probe,
                                void *opaque);
void hls_prefetch_stop(unsigned long interrupt_mask,
                       hls_prefetch_quit_probe_fn quit_probe, void *opaque);

#endif /* HLS_PREFETCH_H */
