/* Experimental full Part6 worker bridge. Never call step on the audio thread.
   Owner pins all model/source assets until jobs drain; context is worker-only.
   Original score-first generator with novelty check, not experimental selectors.
   Source index accompanies melody so host can retrieve its own accompaniment.
   Installed plugin is not wired to this adapter yet. */
#ifndef TT_MM_BASE6_BACKEND_H
#define TT_MM_BASE6_BACKEND_H
#include "mm_jobs.h"
#include "mm_results.h"
#include "base6_checked.h"
typedef struct {uint32_t source,candidates,attempts,tonal;} mm6_request;
typedef struct {
 const m3_phrase *base;int32_t co[3];int bar;
 const uint32_t *guides;uint32_t guide_count;
 const unsigned char (*allowed)[128];b6_novelty_limits limits;
} mm6_source;
typedef struct {
 const b6_tables *tables;const b6_profiles *profiles;const b6_library *library;
 m5_context *context;const mm6_source *sources;uint32_t source_count,smooth;
} mm6_backend;
typedef struct {uint32_t status,job_id,epoch,revision,source,seed,pad,touch;} mm6_header;
typedef struct {mm6_header header;m3_phrase phrase;b6_checked_result audit;} mm6_packet;
enum {MM6_IDLE=0,MM6_PUBLISHED=1,MM6_BACKPRESSURE=2,MM6_BAD_OWNER=3};
enum {MM6_NO_EVENT=0,MM6_NEW_PHRASE=1,MM6_JOB_FAILED=2};
static inline int mm6_step(mm6_backend *b,mmj_queue *q,mmr_pool *pool){
 mmr_slot *slot;mmj_job job;mm6_request request;mm6_packet packet;
 if(!b||!q||!pool||!b->context||!b->sources||sizeof packet>MMR_BYTES)return MM6_BAD_OWNER;
 /* Failures retain the active slot. Search the remaining free slots rather
    than waiting for a ring cursor that may point at that retained phrase. */
 {unsigned n;slot=NULL;for(n=0;n<MMR_SLOTS;n++){
   if(__atomic_load_n(&pool->slots[pool->write_cursor].state,__ATOMIC_ACQUIRE)==MMR_FREE){slot=mmr_begin(pool);break;}
   pool->write_cursor=(pool->write_cursor+1)%MMR_SLOTS;
  }}
 if(!slot)return MM6_BACKPRESSURE;
 if(mmj_take(q,&job)!=MMJ_OK){mmr_abort(pool);return MM6_IDLE;}
 memset(&packet,0,sizeof packet);packet.header.job_id=job.id;packet.header.epoch=job.epoch;
 packet.header.revision=job.revision;packet.header.seed=job.seed;packet.header.pad=job.pad;packet.header.touch=job.touch;
 packet.header.source=UINT32_MAX;
 if(job.action==MMJ_CREATE&&job.snapshot_bytes==sizeof request){
  memcpy(&request,job.snapshot,sizeof request);packet.header.source=request.source;
  if(request.source<b->source_count&&request.candidates>=8&&request.candidates<=32&&
     request.attempts>=1&&request.attempts<=32&&request.tonal<=1){
   const mm6_source *s=&b->sources[request.source];
   if(!request.tonal||s->allowed)
    packet.header.status=(uint32_t)b6_generate_checked_supported(b->tables,b->profiles,b->smooth,
      b->context,s->co,s->base,s->bar,s->guides,s->guide_count,job.seed,request.candidates,
      b->library,&s->limits,request.attempts,&packet.phrase,&packet.audit,request.tonal?s->allowed:NULL);
  }
 }
 memcpy(slot->payload,&packet,sizeof packet);
 if(!mmr_publish(pool,job.id,job.epoch,sizeof packet,packet.header.status)){mmr_abort(pool);return MM6_BAD_OWNER;}
 return MM6_PUBLISHED;
}
/* Audio-side bounded poll. Failure ACK does not replace/release the last valid
   active phrase. Header can be copied to a separate host notification queue.
   Phrase pointer valid only until next successful poll/drop; no UI dereference.
   Host still sends note-offs when replacing a phrase or changing epoch. */
static inline int mm6_poll(mmr_pool *pool,uint32_t epoch,mm6_header *event,const unsigned char **phrase){
 unsigned n;
 if(!pool||!event||!phrase)return MM6_NO_EVENT;
 *phrase=NULL;
 if(pool->active<MMR_SLOTS&&pool->slots[pool->active].epoch!=epoch)mmr_drop(pool);
 {unsigned best=MMR_SLOTS;uint32_t oldest=UINT32_MAX;
 for(n=0;n<MMR_SLOTS;n++){
  mmr_slot *s=&pool->slots[n];
  if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=MMR_READY)continue;
  if(s->epoch!=epoch){__atomic_store_n(&s->state,MMR_FREE,__ATOMIC_RELEASE);continue;}
  if(best==MMR_SLOTS||s->job_id<oldest){best=n;oldest=s->job_id;}
 }
 if(best<MMR_SLOTS){
  mmr_slot *slot=&pool->slots[best];mm6_header header;pool->read_cursor=best;
  memset(&header,0,sizeof header);
  if(slot->bytes==sizeof(mm6_packet))memcpy(&header,slot->payload,sizeof header);
  *event=header;
  if(header.status!=B6_CHECKED_ACCEPTED){
   pool->read_cursor=(pool->read_cursor+1)%MMR_SLOTS;__atomic_store_n(&slot->state,MMR_FREE,__ATOMIC_RELEASE);return MM6_JOB_FAILED;
  }
  slot=(mmr_slot *)mmr_advance(pool,epoch);
  if(!slot)return MM6_NO_EVENT;
  *phrase=slot->payload+sizeof(mm6_header);return MM6_NEW_PHRASE;
 }}
 return MM6_NO_EVENT;
}
#endif
