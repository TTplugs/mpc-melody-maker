/* Experimental worker-owned current-creation variation, not installed.
   Prepare a complete result, then publish through the host result pool.
   This helper itself performs no cross-thread publication. */
#ifndef TT_MM_VARIATION_FRAME_H
#define TT_MM_VARIATION_FRAME_H
#include "mm_harmonic_context.h"
typedef struct {m3_phrase melody;mm_phrase playback;uint32_t seed;int32_t warning;} mmv_frame;
typedef struct {mmh_scratch harmony;m3_phrase input,generated,refreshed;mmv_frame frame;} mmv_scratch;
enum {MMV_OK=0,MMV_INVALID=1,MMV_CONTEXT=2,MMV_CAPACITY=3,
      MMV_GENERATION=4,MMV_NO_COMPATIBLE_RHYTHM=5,MMV_PLAYBACK=6};
static inline int mmv_overlap(const void *a,size_t na,const void *b,size_t nb){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<na:x-y<nb;
}
static inline int mmv_pattern_valid(const m3_pattern *p){
 int i;if(p->count<1||p->count>128||p->length<1)return 0;
 for(i=0;i<p->count;i++)if(p->starts[i]<0||p->starts[i]>=p->length||p->durations[i]<1||p->durations[i]>p->length-p->starts[i]||(i&&p->starts[i]<p->starts[i-1]))return 0;
 return 1;
}
/* Caller pins current and assets until completion; ctx and scratch belong to
   exactly one worker. Never call on the audio thread or write an active frame.
   All failures, including NO_COMPATIBLE_RHYTHM, leave out unchanged.
   The original strict bank filter retains order and repeated observations. */
static inline int mmv_transform(m3_context *ctx,const m3_pattern *bank,uint32_t bank_count,
 const m3_params *params,const unsigned char allowed[M3_NOTES][128],
 const m3_phrase *current,const mm_phrase *playback,mmv_scratch *scratch,mmv_frame *out){
 unsigned i;int status,shift=0;
 if(!ctx||!params||!current||!playback||!scratch||!out||
    (bank_count&&!bank)||current->count<1||current->count>128||current->length<1||
    playback->length!=(uint32_t)current->length||playback->parts[0].count!=(uint32_t)current->count||
    mmv_overlap(current,sizeof *current,scratch,sizeof *scratch)||
    mmv_overlap(playback,sizeof *playback,scratch,sizeof *scratch)||
    mmv_overlap(out,sizeof *out,scratch,sizeof *scratch)||
    mmv_overlap(out,sizeof *out,current,sizeof *current)||
    mmv_overlap(out,sizeof *out,playback,sizeof *playback))return MMV_INVALID;
 /* Refuse stale/cached melody plus unrelated current playback. */
 for(i=0;i<(unsigned)current->count;i++){
  const m3_note *a=&current->notes[i];const mm_note *b=&playback->parts[0].notes[i];
  if(a->start<0||(uint32_t)a->start!=b->start||a->duration<1||(uint32_t)a->duration!=b->duration||a->pitch!=b->pitch||a->velocity!=b->velocity)return MMV_INVALID;
 }
 status=mmh_refresh(current,&playback->parts[1],&playback->parts[2],&scratch->harmony,&scratch->input);
 if(status!=MMH_OK)return status==MMH_CAPACITY?MMV_CAPACITY:MMV_CONTEXT;
 if(params->operation==1){
  ctx->pattern_count=0;
  for(i=0;i<bank_count;i++)if(bank[i].speed==current->speed&&bank[i].length==current->length&&bank[i].groove==current->groove){
   if(!mmv_pattern_valid(&bank[i]))return MMV_INVALID;
   if(ctx->pattern_count>=M3_PATTERNS)return MMV_CAPACITY;
   ctx->patterns[ctx->pattern_count++]=bank[i];
  }
 }
 status=m3_generate_supported(ctx,&scratch->input,params,&scratch->generated,allowed);
 if(status<0)return MMV_GENERATION;
 if(params->operation==1&&(status&M3_NO_COMPATIBLE_RHYTHM))return MMV_NO_COMPATIBLE_RHYTHM;
 scratch->frame.warning=status;scratch->frame.seed=params->seed;
 /* Rebuild again after changed attacks so a following variation sees the
    chord, root and bass at the new times, never the original note positions. */
 status=mmh_refresh(&scratch->generated,&playback->parts[1],&playback->parts[2],&scratch->harmony,&scratch->refreshed);
 if(status!=MMH_OK)return status==MMH_CAPACITY?MMV_CAPACITY:MMV_CONTEXT;
 status=mm6_build_playback(&scratch->refreshed,playback->source_id,&playback->parts[1],&playback->parts[2],0,28,47,&scratch->frame.playback,&shift);
 if(status!=MM6_PLAYBACK_OK)return MMV_PLAYBACK;
 scratch->frame.melody=scratch->refreshed;
 *out=scratch->frame;return MMV_OK;
}
#endif
