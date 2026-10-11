/* One worker dispatches CREATE and VARIATION to the existing musical engines.
   Both publish the same complete immutable mmv_frame. Experimental VST wiring
   is behind MM_CANDIDATE_ENGINE; installed host remains unchanged. */
#ifndef TT_MM_CREATION_WORKER_H
#define TT_MM_CREATION_WORKER_H
#include "mm_source_assets.h"
#include "mm_variation_queue.h"
#include "base6_checked.h"
typedef struct {uint32_t source,candidates,attempts,tonal,place_register;} mmcw_request;
typedef struct {
 const b6_tables *tables;const b6_profiles *profiles;const b6_library *novelty;
 const mmsa_library *sources;m5_context *context;
 mmsa_source *source_scratch,*source;mmvs_backend *variation;
 b6_checked_result audit;uint32_t audit_job_id;int generation_status;
} mmcw_backend;
static inline int mmcw_submit(mmj_queue *q,uint32_t pad,uint32_t epoch,uint32_t revision,const mmcw_request *r,mmj_job *receipt){
 if(!r||r->candidates<8||r->candidates>32||r->attempts<1||r->attempts>32||r->tonal>1||r->place_register>1)return MMJ_INVALID;
 return mmj_submit(q,pad,MMJ_CREATE,epoch,revision,r,sizeof *r,receipt);
}
static inline int mmcw_step(mmcw_backend *b,mmj_queue *q,mmvs_arena *arena,mmr_pool *pool){
 uint32_t head,tail;mmj_job job;mmcw_request request;mmr_slot *slot=NULL;mmvs_packet *p;unsigned i;int shift=0;
 if(!b||!q||!arena||!pool||!b->variation||!b->variation->packet||!b->variation->scratch||!b->tables||!b->profiles||!b->novelty||!b->sources||!b->context||!b->source_scratch||!b->source||sizeof(mmvs_packet)>MMR_BYTES)return MMVS_BAD_OWNER;
 tail=__atomic_load_n(&q->tail,__ATOMIC_RELAXED);head=__atomic_load_n(&q->head,__ATOMIC_ACQUIRE);if(tail==head)return MMVS_IDLE;
 if(q->slots[tail%MMJ_CAPACITY].action==MMJ_VARIATION)return mmvs_step(b->variation,q,arena,pool);
 for(i=0;i<MMR_SLOTS;i++){if(__atomic_load_n(&pool->slots[pool->write_cursor].state,__ATOMIC_ACQUIRE)==MMR_FREE){slot=mmr_begin(pool);break;}pool->write_cursor=(pool->write_cursor+1)%MMR_SLOTS;}
 if(!slot)return MMVS_BACKPRESSURE;
 if(mmj_take(q,&job)!=MMJ_OK){mmr_abort(pool);return MMVS_IDLE;}
 p=b->variation->packet;memset(p,0,sizeof *p);p->header=(mmvs_header){MMVS_MARKER,MMV_INVALID,job.id,job.epoch,job.revision,job.seed,job.pad,job.touch,0};
 b->audit_job_id=job.id;memset(&b->audit,0,sizeof b->audit);b->generation_status=B6_CHECKED_ERROR;
 if(job.action==MMJ_CREATE&&job.snapshot_bytes==sizeof request){
  memcpy(&request,job.snapshot,sizeof request);
  if(request.candidates>=8&&request.candidates<=32&&request.attempts>=1&&request.attempts<=32&&request.tonal<=1&&request.place_register<=1&&mmsa_get(b->sources,request.source,b->source_scratch,b->source)){
   const mmsa_source *s=b->source;b6_novelty_limits limits={s->minimum_distance,s->maximum_run};
   p->header.status=MMV_GENERATION;
   b->generation_status=b6_generate_checked_supported(b->tables,b->profiles,100,b->context,s->co,&s->base,s->bar,s->guides,s->guide_count,job.seed,request.candidates,b->novelty,&limits,request.attempts,&p->frame.melody,&b->audit,request.tonal?s->allowed:NULL);
   if(b->generation_status==B6_CHECKED_ACCEPTED){
    int valid=1;int32_t pitches[128];
    if(request.place_register){
     for(i=0;i<(unsigned)p->frame.melody.count;i++)pitches[i]=p->frame.melody.notes[i].pitch;
     if(tt_bass_octave_shift(pitches,p->frame.melody.count,60,83,&shift))valid=0;
     else for(i=0;i<(unsigned)p->frame.melody.count;i++)p->frame.melody.notes[i].pitch+=shift;
    }
    if(valid&&mm6_build_playback(&p->frame.melody,s->source_id,&s->chords,&s->bass,request.place_register?1:0,28,47,&p->frame.playback,&shift)==MM6_PLAYBACK_OK){
     /* Refresh bass annotations after whole-octave placement, using actual parts. */
     mmv_scratch *scratch=b->variation->scratch;
     if(mmh_refresh(&p->frame.melody,&p->frame.playback.parts[1],&p->frame.playback.parts[2],&scratch->harmony,&scratch->refreshed)==MMH_OK){
      p->frame.melody=scratch->refreshed;p->frame.seed=job.seed;p->frame.warning=0;p->header.status=MMV_OK;
     }else p->header.status=MMV_CONTEXT;
    }else p->header.status=MMV_PLAYBACK;
   }
  }
 }
 memcpy(slot->payload,p,sizeof *p);if(!mmr_publish(pool,job.id,job.epoch,sizeof *p,p->header.status)){mmr_abort(pool);return MMVS_BAD_OWNER;}return MMVS_PUBLISHED;
}
#endif
