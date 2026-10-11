/* Bounded multi-producer/single-consumer trigger admission. UI and MIDI may
   call push concurrently. No locks, allocation or unbounded retry on entry.
   A reserved slot is published only after its entire snapshot is copied.
   Consumer never overtakes a reserved but unpublished earlier event. */
#ifndef TT_MM_INPUT_QUEUE_H
#define TT_MM_INPUT_QUEUE_H
#include <stdint.h>
#include <string.h>
#define MMI_CAPACITY 64u
#define MMI_RETRIES 8u
typedef struct {uint32_t pad,action,epoch;float parameters[11];} mmi_event;
typedef struct {uint32_t sequence;mmi_event event;} mmi_slot;
typedef struct {uint32_t head,tail;mmi_slot slots[MMI_CAPACITY];} mmi_queue;
enum {MMI_OK=0,MMI_EMPTY=1,MMI_FULL=2,MMI_CONTENDED=3,MMI_INVALID=4};
static inline void mmi_init(mmi_queue *q){unsigned i;memset(q,0,sizeof *q);for(i=0;i<MMI_CAPACITY;i++)q->slots[i].sequence=i;}
static inline int mmi_push(mmi_queue *q,const mmi_event *e){
 uint32_t pos;unsigned tries;if(!q||!e||e->pad>=16||e->action>1)return MMI_INVALID;
 pos=__atomic_load_n(&q->head,__ATOMIC_RELAXED);
 for(tries=0;tries<MMI_RETRIES;tries++){
  mmi_slot *s=&q->slots[pos%MMI_CAPACITY];uint32_t seq=__atomic_load_n(&s->sequence,__ATOMIC_ACQUIRE),difference=seq-pos;
  if(!difference){
   if(__atomic_compare_exchange_n(&q->head,&pos,pos+1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){
    s->event=*e;__atomic_store_n(&s->sequence,pos+1,__ATOMIC_RELEASE);return MMI_OK;
   }
  }else if(difference>UINT32_MAX/2u)return MMI_FULL;
  else pos=__atomic_load_n(&q->head,__ATOMIC_RELAXED);
 }
 return MMI_CONTENDED;
}
static inline int mmi_take(mmi_queue *q,mmi_event *e){
 uint32_t pos;mmi_slot *s;if(!q||!e)return MMI_INVALID;
 pos=q->tail;s=&q->slots[pos%MMI_CAPACITY];
 if(__atomic_load_n(&s->sequence,__ATOMIC_ACQUIRE)!=pos+1)return MMI_EMPTY;
 *e=s->event;q->tail=pos+1;__atomic_store_n(&s->sequence,pos+MMI_CAPACITY,__ATOMIC_RELEASE);return MMI_OK;
}
#endif
