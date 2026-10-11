/* Experimental host adapter, C99 + GCC/Clang atomics. No musical backend.
 * Exactly ONE producer and ONE consumer. Never call from UI and audio
 * concurrently: the host must route those sources to one producer first.
 * Initialize/restore only when both endpoints are quiescent.
 */
#ifndef TT_MM_JOBS_H
#define TT_MM_JOBS_H
#include <stdint.h>
#include <string.h>
#define MMJ_CAPACITY 32u
#define MMJ_SNAPSHOT_BYTES 256u
enum { MMJ_OK=0, MMJ_EMPTY=1, MMJ_FULL=2, MMJ_EXHAUSTED=3,
       MMJ_INVALID=4 };
enum { MMJ_CREATE=0, MMJ_VARIATION=1 };
typedef struct {
    uint32_t id, epoch, revision, pad, touch, seed, action, snapshot_bytes;
    /* Host-defined canonical parameter snapshot, never pointers. */
    unsigned char snapshot[MMJ_SNAPSHOT_BYTES];
} mmj_job;
typedef struct {
    uint32_t head, tail;
    /* Producer owned; persist these at a quiescent checkpoint. */
    uint32_t next_id, pad_seed[16], touches[16];
    mmj_job slots[MMJ_CAPACITY];
} mmj_queue;
/* Identical unsigned arithmetic to original m3_pad_seed, version 1. */
static inline uint32_t mmj_seed(uint32_t s, uint32_t n) {
    s ^= n + 0x9e3779b9u + (s<<6) + (s>>2);
    s ^= s>>16; s *= 0x7feb352du;
    s ^= s>>15; s *= 0x846ca68bu;
    return s ^ (s>>16);
}
static inline void mmj_init(mmj_queue *q, uint32_t session_seed) {
    unsigned i;
    memset(q,0,sizeof *q);
    for(i=0;i<16;i++)
        q->pad_seed[i]=mmj_seed(session_seed ^ 0x50414431u,i);
}
/* Nonblocking admission. A successful admission consumes exactly one touch.
 * No malloc, mutex, system clock, musical generation or host callbacks.
 * FIFO: newer jobs DO NOT cancel older accepted jobs.
 */
static inline int mmj_submit(mmj_queue *q, uint32_t pad, uint32_t action,
                      uint32_t epoch, uint32_t revision,
                      const void *snapshot, uint32_t size, mmj_job *receipt) {
    uint32_t h,t;
    mmj_job j;
    if(!q || pad>=16 || action>MMJ_VARIATION || size>MMJ_SNAPSHOT_BYTES ||
       (size && !snapshot)) return MMJ_INVALID;
    h=__atomic_load_n(&q->head,__ATOMIC_RELAXED);
    t=__atomic_load_n(&q->tail,__ATOMIC_ACQUIRE);
    if(h-t>=MMJ_CAPACITY) return MMJ_FULL;
    if(q->touches[pad]==UINT32_MAX || q->next_id==UINT32_MAX)
        return MMJ_EXHAUSTED;
    memset(&j,0,sizeof j);
    j.id=++q->next_id; j.epoch=epoch; j.revision=revision;
    j.pad=pad; j.action=action; j.touch=++q->touches[pad];
    j.seed=mmj_seed(q->pad_seed[pad],j.touch);
    j.snapshot_bytes=size;
    if(size) memcpy(j.snapshot,snapshot,size);
    q->slots[h%MMJ_CAPACITY]=j;
    if(receipt) *receipt=j;
    __atomic_store_n(&q->head,h+1,__ATOMIC_RELEASE);
    return MMJ_OK;
}
static inline int mmj_take(mmj_queue *q, mmj_job *out) {
    uint32_t h,t;
    if(!q || !out) return MMJ_INVALID;
    t=__atomic_load_n(&q->tail,__ATOMIC_RELAXED);
    h=__atomic_load_n(&q->head,__ATOMIC_ACQUIRE);
    if(t==h) return MMJ_EMPTY;
    *out=q->slots[t%MMJ_CAPACITY];
    __atomic_store_n(&q->tail,t+1,__ATOMIC_RELEASE);
    return MMJ_OK;
}
#endif

