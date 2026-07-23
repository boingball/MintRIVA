/*
 * Single-core thread shim for MintRIVA's libavc build.
 *
 * MintRIVA always configures the decoder for one core.  libavc still links
 * against its small thread abstraction, so provide inert locks/conditions
 * without requiring pthreads (which are not available in the noixemul
 * AmigaOS build).  Thread creation deliberately fails if a future change
 * accidentally requests a multi-core decoder.
 */
#include "ih264_typedefs.h"
#include "ithread.h"

#define UNUSED(x) ((void)(x))

UWORD32 ithread_get_handle_size(void)       { return sizeof(void *); }
UWORD32 ithread_get_mutex_lock_size(void)   { return sizeof(void *); }
WORD32  ithread_get_mutex_struct_size(void) { return (WORD32)sizeof(void *); }
WORD32  ithread_get_cond_struct_size(void)  { return (WORD32)sizeof(void *); }
UWORD32 ithread_get_cond_size(void)         { return sizeof(void *); }
UWORD32 ithread_get_sem_struct_size(void)   { return sizeof(void *); }

WORD32 ithread_create(void *h, void *a, void *start, void *arg)
{
    UNUSED(h); UNUSED(a); UNUSED(start); UNUSED(arg);
    return -1;
}
void ithread_exit(void *value) { UNUSED(value); }
WORD32 ithread_join(void *h, void **value)
{ UNUSED(h); UNUSED(value); return -1; }

WORD32 ithread_mutex_init(void *p)    { UNUSED(p); return 0; }
WORD32 ithread_mutex_destroy(void *p) { UNUSED(p); return 0; }
WORD32 ithread_mutex_lock(void *p)    { UNUSED(p); return 0; }
WORD32 ithread_mutex_unlock(void *p)  { UNUSED(p); return 0; }

WORD32 ithread_cond_init(void *p)            { UNUSED(p); return 0; }
WORD32 ithread_cond_destroy(void *p)         { UNUSED(p); return 0; }
WORD32 ithread_cond_wait(void *c, void *m)
{ UNUSED(c); UNUSED(m); return -1; }
WORD32 ithread_cond_signal(void *p)          { UNUSED(p); return 0; }
WORD32 ithread_cond_broadcast(void *p)       { UNUSED(p); return 0; }

WORD32 ithread_sem_init(void *p, WORD32 shared, UWORD32 value)
{ UNUSED(p); UNUSED(shared); UNUSED(value); return 0; }
WORD32 ithread_sem_post(void *p)    { UNUSED(p); return 0; }
WORD32 ithread_sem_wait(void *p)    { UNUSED(p); return -1; }
WORD32 ithread_sem_destroy(void *p) { UNUSED(p); return 0; }

void ithread_yield(void) {}
void ithread_sleep(UWORD32 seconds)       { UNUSED(seconds); }
void ithread_msleep(UWORD32 milliseconds) { UNUSED(milliseconds); }
void ithread_usleep(UWORD32 microseconds) { UNUSED(microseconds); }
WORD32 ithread_set_affinity(WORD32 core)  { UNUSED(core); return 0; }
void ithread_set_name(CHAR *name)         { UNUSED(name); }
