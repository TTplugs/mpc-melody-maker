/* SPSC result ownership, independent of the musical backend.
 * Producer: worker only. Consumer: audio only. No UI access to mutable slots.
 * Call advance only after finishing all reads of the previous active result,
 * at the host's chosen musical boundary. Returned pointer remains immutable
 * until the NEXT advance/drop. Initialization requires quiescent endpoints.
 */
#ifndef TT_MM_RESULTS_H
#define TT_MM_RESULTS_H
#include <stdint.h>
#include <string.h>
#ifndef MMR_BYTES
#define MMR_BYTES 65536u
#endif
#define MMR_SLOTS 3u
enum { MMR_FREE=0, MMR_WRITING=1, MMR_READY=2, MMR_ACTIVE=3 };
typedef struct {
    uint32_t state, job_id, epoch, bytes, warning;
    unsigned char payload[MMR_BYTES];
} mmr_slot;
typedef struct {
    uint32_t write_cursor; /* worker owned */
    uint32_t read_cursor, active; /* audio owned; active=MMR_SLOTS if none */
    mmr_slot slots[MMR_SLOTS];
} mmr_pool;
static inline void mmr_init(mmr_pool *p) {
    memset(p,0,sizeof *p);p->active=MMR_SLOTS;
}
/* NULL means backpressure, not permission to overwrite any active result. */
static inline mmr_slot *mmr_begin(mmr_pool *p) {
    mmr_slot *s=&p->slots[p->write_cursor];
    if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=MMR_FREE)return 0;
    __atomic_store_n(&s->state,MMR_WRITING,__ATOMIC_RELAXED);
    return s;
}
/* Producer calls after writing payload. Invalid size leaves slot WRITING;
 * caller must abort. Host must handle epoch invalidation independently.
 */
static inline int mmr_publish(mmr_pool *p,uint32_t id,uint32_t epoch,
                       uint32_t bytes,uint32_t warning) {
    mmr_slot *s=&p->slots[p->write_cursor];
    if(bytes>MMR_BYTES || __atomic_load_n(&s->state,__ATOMIC_RELAXED)!=MMR_WRITING)return 0;
    s->job_id=id;s->epoch=epoch;s->bytes=bytes;s->warning=warning;
    __atomic_store_n(&s->state,MMR_READY,__ATOMIC_RELEASE);
    p->write_cursor=(p->write_cursor+1)%MMR_SLOTS;
    return 1;
}
static inline void mmr_abort(mmr_pool *p) {
    mmr_slot *s=&p->slots[p->write_cursor];
    if(__atomic_load_n(&s->state,__ATOMIC_RELAXED)==MMR_WRITING)
        __atomic_store_n(&s->state,MMR_FREE,__ATOMIC_RELEASE);
}
/* Release current result after its LAST read (explicit consumer ACK). */
static inline void mmr_drop(mmr_pool *p) {
    if(p->active<MMR_SLOTS) {
        __atomic_store_n(&p->slots[p->active].state,MMR_FREE,__ATOMIC_RELEASE);
        p->active=MMR_SLOTS;
    }
}
/* Nonblocking, bounded inspection. Does not free a current valid result when
 * there is no ready replacement. Stale epochs are discarded, never returned.
 * This does not emit note-offs; host must do that when changing results.
 */
static inline const mmr_slot *mmr_advance(mmr_pool *p,uint32_t epoch) {
    unsigned n;
    if(p->active<MMR_SLOTS && p->slots[p->active].epoch!=epoch)mmr_drop(p);
    for(n=0;n<MMR_SLOTS;n++) {
        uint32_t index=p->read_cursor;
        mmr_slot *s=&p->slots[index];
        if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=MMR_READY)return 0;
        p->read_cursor=(index+1)%MMR_SLOTS;
        if(s->epoch!=epoch) {
            __atomic_store_n(&s->state,MMR_FREE,__ATOMIC_RELEASE);
            continue;
        }
        __atomic_store_n(&s->state,MMR_ACTIVE,__ATOMIC_RELAXED);
        mmr_drop(p);p->active=index;
        return s;
    }
    return 0;
}
#endif

