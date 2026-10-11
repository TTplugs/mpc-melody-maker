/* Experimental SPSC admission/worker/result bridge. Not wired to installed VST.
   One control producer copies immutable current creations; one worker consumes;
   audio only consumes completed results. No per-job allocation. */
#ifndef TT_MM_VARIATION_QUEUE_H
#define TT_MM_VARIATION_QUEUE_H
#include "mm_jobs.h"
#include "mm_results.h"
#include "mm_variation_frame.h"
#define MMVS_SNAPSHOTS (MMJ_CAPACITY+1u)
#define MMVS_MARKER 0x56524d4du
typedef struct {int32_t operation,intensity,dark,candidates,passes;uint32_t strict_scale;} mmvs_controls;
typedef struct {uint32_t index,generation,source_revision;mmvs_controls controls;} mmvs_request;
typedef struct {uint32_t state,generation,job_id;mmv_frame creation;} mmvs_snapshot;
typedef struct {uint32_t cursor;mmvs_snapshot slots[MMVS_SNAPSHOTS];} mmvs_arena;
typedef struct {uint32_t marker,status,job_id,epoch,revision,seed,pad,touch,source_revision;} mmvs_header;
typedef struct {mmvs_header header;mmv_frame frame;} mmvs_packet;
typedef struct {
 m3_context *context;const m3_pattern *patterns;uint32_t pattern_count;
 mmv_scratch *scratch;mmvs_packet *packet;unsigned char allowed[128][128];
} mmvs_backend;
enum {MMVS_IDLE=0,MMVS_PUBLISHED=1,MMVS_BACKPRESSURE=2,MMVS_BAD_OWNER=3,MMVS_OTHER_ACTION=4};
enum {MMVS_NO_EVENT=0,MMVS_NEW_FRAME=1,MMVS_FAILED=2,MMVS_OTHER_EVENT=3};
static inline void mmvs_init(mmvs_arena *a){memset(a,0,sizeof *a);}
static inline int mmvs_controls_valid(const mmvs_controls *c){
 return c&&c->operation>=0&&c->operation<=2&&c->intensity>=0&&c->intensity<=1000&&c->dark>=0&&c->dark<=1000&&c->candidates>=8&&c->candidates<=32&&c->passes>=1&&c->passes<=8&&c->strict_scale<=1;
}
/* CONTROL thread only: copying a frame is deliberately not audio work. Caller
   keeps source immutable through this call. A failed admission consumes no
   touch and owns no snapshot. One extra slot covers a worker-held request even
   after mmj_take has already released its queue entry. */
static inline int mmvs_submit(mmj_queue *q,mmvs_arena *a,uint32_t pad,uint32_t epoch,
 uint32_t revision,uint32_t source_revision,const mmv_frame *source,
 const mmvs_controls *controls,mmj_job *receipt){
 unsigned n;mmvs_snapshot *s=NULL;mmvs_request request;int status;
 if(!q||!a||!source||!mmvs_controls_valid(controls)||pad>=16)return MMJ_INVALID;
 if(q->next_id==UINT32_MAX)return MMJ_EXHAUSTED;
 for(n=0;n<MMVS_SNAPSHOTS;n++){
  unsigned i=(a->cursor+n)%MMVS_SNAPSHOTS;
  if(!__atomic_load_n(&a->slots[i].state,__ATOMIC_ACQUIRE)&&a->slots[i].generation!=UINT32_MAX){s=&a->slots[i];request.index=i;break;}
 }
 if(!s)return MMJ_FULL;
 request.generation=s->generation+1;request.source_revision=source_revision;request.controls=*controls;
 s->creation=*source;s->generation=request.generation;s->job_id=q->next_id+1;
 __atomic_store_n(&s->state,1,__ATOMIC_RELEASE);
 status=mmj_submit(q,pad,MMJ_VARIATION,epoch,revision,&request,sizeof request,receipt);
 if(status!=MMJ_OK){__atomic_store_n(&s->state,0,__ATOMIC_RELEASE);return status;}
 a->cursor=(request.index+1)%MMVS_SNAPSHOTS;return MMJ_OK;
}
static inline int mmvs_step(mmvs_backend *b,mmj_queue *q,mmvs_arena *a,mmr_pool *pool){
 mmr_slot *slot=NULL;mmj_job job;mmvs_request request;mmvs_snapshot *source=NULL;unsigned n;
 uint32_t head,tail;
 if(!b||!q||!a||!pool||!b->context||!b->scratch||!b->packet||sizeof(mmvs_packet)>MMR_BYTES)return MMVS_BAD_OWNER;
 tail=__atomic_load_n(&q->tail,__ATOMIC_RELAXED);head=__atomic_load_n(&q->head,__ATOMIC_ACQUIRE);
 if(tail==head)return MMVS_IDLE;
 if(q->slots[tail%MMJ_CAPACITY].action!=MMJ_VARIATION)return MMVS_OTHER_ACTION;
 for(n=0;n<MMR_SLOTS;n++){
  if(__atomic_load_n(&pool->slots[pool->write_cursor].state,__ATOMIC_ACQUIRE)==MMR_FREE){slot=mmr_begin(pool);break;}
  pool->write_cursor=(pool->write_cursor+1)%MMR_SLOTS;
 }
 if(!slot)return MMVS_BACKPRESSURE;
 if(mmj_take(q,&job)!=MMJ_OK){mmr_abort(pool);return MMVS_IDLE;}
 memset(b->packet,0,sizeof *b->packet);
 b->packet->header=(mmvs_header){MMVS_MARKER,MMV_INVALID,job.id,job.epoch,job.revision,job.seed,job.pad,job.touch,0};
 if(job.snapshot_bytes==sizeof request){
  memcpy(&request,job.snapshot,sizeof request);b->packet->header.source_revision=request.source_revision;
  if(request.index<MMVS_SNAPSHOTS){
   mmvs_snapshot *candidate=&a->slots[request.index];
   if(__atomic_load_n(&candidate->state,__ATOMIC_ACQUIRE)&&candidate->generation==request.generation&&candidate->job_id==job.id)source=candidate;
  }
  if(source&&mmvs_controls_valid(&request.controls)){
   const mmvs_controls *c=&request.controls;m3_params params={job.seed,c->operation,c->intensity,c->dark,c->candidates,c->passes};
   const unsigned char (*allowed)[128]=NULL;int valid=1;
   if(c->strict_scale){
    static const unsigned char modes[5][7]={{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,3,5,7,8,11},{0,2,4,5,7,9,11}};
    const m3_phrase *p=&source->creation.melody;uint32_t mask=0;unsigned i,j;
    if(p->mode<0||p->mode>4||p->key<0||p->key>11)valid=0;
    else{
     for(i=0;i<7;i++)mask|=1u<<((p->key+modes[p->mode][i])%12);
     for(i=0;i<128;i++)for(j=0;j<128;j++)b->allowed[i][j]=(unsigned char)((mask>>(j%12))&1u);
     allowed=b->allowed;
    }
   }
   if(valid)b->packet->header.status=(uint32_t)mmv_transform(b->context,b->patterns,b->pattern_count,&params,allowed,&source->creation.melody,&source->creation.playback,b->scratch,&b->packet->frame);
  }
 }
 /* Worker is finished reading this immutable snapshot, including failure. */
 if(source)__atomic_store_n(&source->state,0,__ATOMIC_RELEASE);
 memcpy(slot->payload,b->packet,sizeof *b->packet);
 if(!mmr_publish(pool,job.id,job.epoch,sizeof *b->packet,b->packet->header.status)){mmr_abort(pool);return MMVS_BAD_OWNER;}
 return MMVS_PUBLISHED;
}
/* AUDIO-side bounded acknowledgement. Host calls only at a musical boundary,
   finishes previous reads and sends note-offs before adopting a new frame.
   A failed job is freed without releasing the last valid active result. */
static inline int mmvs_poll(mmr_pool *pool,uint32_t epoch,mmvs_header *event,const mmv_frame **frame){
 unsigned i,best=MMR_SLOTS;uint32_t oldest=UINT32_MAX;
 if(!pool||!event||!frame)return MMVS_NO_EVENT;*frame=NULL;
 if(pool->active<MMR_SLOTS&&pool->slots[pool->active].epoch!=epoch)mmr_drop(pool);
 for(i=0;i<MMR_SLOTS;i++){
  mmr_slot *s=&pool->slots[i];
  if(__atomic_load_n(&s->state,__ATOMIC_ACQUIRE)!=MMR_READY)continue;
  if(s->epoch!=epoch){__atomic_store_n(&s->state,MMR_FREE,__ATOMIC_RELEASE);continue;}
  if(best==MMR_SLOTS||s->job_id<oldest){best=i;oldest=s->job_id;}
 }
 if(best<MMR_SLOTS){
  mmr_slot *s=&pool->slots[best];mmvs_header header;uint32_t marker=0;
  if(s->bytes>=sizeof marker)memcpy(&marker,s->payload,sizeof marker);
  if(marker!=MMVS_MARKER)return MMVS_OTHER_EVENT;
  memset(&header,0,sizeof header);header.status=MMV_INVALID;
  if(s->bytes==sizeof(mmvs_packet))memcpy(&header,s->payload,sizeof header);*event=header;
  if(header.status!=MMV_OK){__atomic_store_n(&s->state,MMR_FREE,__ATOMIC_RELEASE);return MMVS_FAILED;}
  pool->read_cursor=best;s=(mmr_slot *)mmr_advance(pool,epoch);
  if(s){*frame=&((const mmvs_packet *)(const void *)s->payload)->frame;return MMVS_NEW_FRAME;}
 }
 return MMVS_NO_EVENT;
}
#endif
