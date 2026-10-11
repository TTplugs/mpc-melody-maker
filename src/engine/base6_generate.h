#ifndef TT_BASE6_GENERATE_H
#define TT_BASE6_GENERATE_H
#include "mutation5.h"
#include "base6_candidate.h"
#include "base6_profiles.h"
#include <math.h>
typedef struct {double score;uint32_t profile,desired,final_seed;double pout;} b6_result;
typedef void (*b6_candidate_observer)(void *user,uint32_t candidate,double score,const m3_phrase *phrase);
static inline double b6_double(const unsigned char *data){uint64_t bits=b6_le64(data);double value;memcpy(&value,&bits,8);return value;}
/* Same original Part6 sampling and soft ranking; no novelty retries here.
 * Theory context and model loaders are called outside audio by the owner.
 */
static inline int b6_generate_observed(const b6_tables *model,const b6_profiles *profiles,
    uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
    int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
    uint32_t candidates,m3_phrase *out,b6_result *result,
    const unsigned char (*allowed)[128],b6_candidate_observer observer,void *observer_user){
 b6_profile_choice choice;m3_phrase trial;uint32_t seed=initial_seed?initial_seed:0x9e3779b9u,k,j;
 double guide_sd,contour_sd,best=INFINITY;
 if(!model||!model->dispersion||!profiles||!co||!ctx||!base||!out||!result||out==base)return 0;
 guide_sd=b6_double(model->dispersion);contour_sd=b6_double(model->dispersion+8);
 if(!ctx||!base||!out||!result||base->count<2||base->count>128||bar<2||co[0]<0||co[0]>3||co[2]<0||co[2]>3||candidates<1||candidates>32||!isfinite(guide_sd)||guide_sd<=0||!isfinite(contour_sd)||contour_sd<=0)return 0;
 if(guide_count>128||(!guides&&guide_count))return 0;
 for(j=0;j<guide_count;j++)if(guides[j]>=(uint32_t)base->count)return 0;
 if(!b6_choose_profile(profiles,co,(uint32_t)base->count,&seed,&choice))return 0;
 ctx->bar=bar;ctx->fit=co[2];ctx->group=(8+co[0])*4+co[2];ctx->kind=2;ctx->dark=500;ctx->intensity=1000;ctx->weights[9]=0;
 memcpy(ctx->target,choice.target,sizeof choice.target);memcpy(ctx->model.spread[ctx->group],choice.spread,sizeof choice.spread);
 for(k=0;k<candidates;k++){
  double score,mismatch;int turns=0;int64_t distance=0;
  if(!b6_candidate(model,smooth,co,base,&seed,&trial,allowed))return 0;
  score=(double)m5_score(ctx,base,&trial)/1000.0;
  for(j=0;j<guide_count;j++){int d=trial.notes[guides[j]].pitch-base->notes[guides[j]].pitch;distance+=d<0?-d:d;}
  if(guide_count)score+=((double)distance/guide_count)/guide_sd;
  for(j=1;j<(uint32_t)base->count;j++){
   int a=trial.notes[j].pitch-trial.notes[j-1].pitch,b=base->notes[j].pitch-base->notes[j-1].pitch;
   turns+=((a>0)-(a<0))!=((b>0)-(b<0));
  }
  mismatch=(double)turns/(base->count-1);score+=mismatch/contour_sd;
  if(trial.notes[trial.count-1].chord){
   const m3_note *n=&trial.notes[trial.count-1];int outside=!(n->chord&(1u<<(n->pitch%12)));
   double sd=sqrt(choice.pout*(1.0-choice.pout));if(sd<.001)sd=.001;
   score+=(double)(outside!=(int)choice.desired)/sd;
  }
  /* Observational callback: never changes the RNG or selection. The phrase
   * pointer is valid only during this call; observers must copy to retain it. */
  if(observer)observer(observer_user,k,score,&trial);
  if(score<best){best=score;*out=trial;}
 }
 if(!isfinite(best))return 0;
 result->score=best;result->profile=choice.profile;result->desired=choice.desired;result->pout=choice.pout;result->final_seed=seed;return 1;
}
static inline int b6_generate_supported(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,m3_phrase *out,b6_result *result,const unsigned char (*allowed)[128]){
 return b6_generate_observed(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,
 initial_seed,candidates,out,result,allowed,NULL,NULL);
}
static inline int b6_generate(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,m3_phrase *out,b6_result *result){
 return b6_generate_supported(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,
                              initial_seed,candidates,out,result,NULL);
}
#endif
