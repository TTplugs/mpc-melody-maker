#ifndef TT_BASE6_ADMISSIBLE_H
#define TT_BASE6_ADMISSIBLE_H
#include "base6_checked.h"

/* Owner allocates once outside audio; never allocate this on the audio stack. */
typedef struct {m3_phrase phrases[32];double scores[32];uint32_t count;int error;} b6_candidate_workspace;
typedef struct {b6_result generation;double original_score;uint32_t selected_candidate,novelty_queries;
                b6_novelty novelty;} b6_admissible_result;
static inline void b6_collect_candidate(void *user,uint32_t candidate,double score,const m3_phrase *phrase){
 b6_candidate_workspace *w=user;
 if(candidate>=32||candidate!=w->count||!isfinite(score)){w->error=1;return;}
 w->phrases[candidate]=*phrase;w->scores[candidate]=score;w->count++;
}

/* Original proposals, RNG stream and musical score unchanged. Inspect candidates
 * in ascending original score, with first-index tie breaking. Stop at the first
 * novelty-admissible candidate. This is one batch, not a retry implementation.
 */
static inline int b6_generate_admissible_batch(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,const b6_library *library,const b6_novelty_limits *limits,
 m3_phrase *out,b6_admissible_result *result,const unsigned char (*allowed)[128],
 b6_candidate_workspace *workspace){
 m3_phrase original;b6_result generation;unsigned char tried[32]={0};uint32_t k,j;
 if(!workspace||!result||!base||!out||out==base||!library||!library->rows||!library->count||
    !limits||!isfinite(limits->minimum_distance)||limits->minimum_distance<0||limits->minimum_distance>1||
    !isfinite(limits->maximum_run)||limits->maximum_run<0||candidates<1||candidates>32)return B6_CHECKED_ERROR;
 workspace->count=0;workspace->error=0;memset(result,0,sizeof *result);result->selected_candidate=UINT32_MAX;
 if(!b6_generate_observed(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,initial_seed,
     candidates,&original,&generation,allowed,b6_collect_candidate,workspace)||workspace->error||
     workspace->count!=candidates)return B6_CHECKED_ERROR;
 result->generation=generation;result->original_score=generation.score;
 for(k=0;k<candidates;k++){
  uint32_t best=UINT32_MAX;b6_mel_token tokens[128];
  for(j=0;j<candidates;j++)if(!tried[j]&&(best==UINT32_MAX||workspace->scores[j]<workspace->scores[best]))best=j;
  if(best==UINT32_MAX)return B6_CHECKED_ERROR;
  tried[best]=1;result->novelty_queries++;
  if(!b6_mel_tokens(&workspace->phrases[best],tokens)||!b6_check_novelty(library,tokens,
      (uint32_t)workspace->phrases[best].count,UINT32_MAX,&result->novelty))return B6_CHECKED_ERROR;
  if(result->novelty.distance>=limits->minimum_distance&&(double)result->novelty.longest<=limits->maximum_run){
   *out=workspace->phrases[best];result->selected_candidate=best;
   result->generation.score=workspace->scores[best];return B6_CHECKED_ACCEPTED;
  }
 }
 return B6_CHECKED_EXHAUSTED;
}
typedef struct {uint32_t seed;b6_admissible_result batch;} b6_admissible_attempt;
typedef struct {uint32_t count;b6_admissible_attempt attempts[32];} b6_admissible_checked_result;
static inline int b6_generate_admissible_checked(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,const b6_library *library,const b6_novelty_limits *limits,
 uint32_t max_attempts,m3_phrase *out,b6_admissible_checked_result *result,
 const unsigned char (*allowed)[128],b6_candidate_workspace *workspace){
 uint32_t k,j;m3_phrase trial;
 if(!result||!base||!out||out==base||max_attempts<1||max_attempts>32)return B6_CHECKED_ERROR;
 memset(result,0,sizeof *result);
 for(k=0;k<max_attempts;k++){
  b6_admissible_attempt *a=&result->attempts[k];int status;
  a->seed=k?m3_pad_seed(initial_seed,k):initial_seed;
  for(j=0;j<k;j++)if(result->attempts[j].seed==a->seed)return B6_CHECKED_ERROR;
  status=b6_generate_admissible_batch(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,
     a->seed,candidates,library,limits,&trial,&a->batch,allowed,workspace);
  if(status==B6_CHECKED_ERROR)return status;
  result->count=k+1;
  if(status==B6_CHECKED_ACCEPTED){*out=trial;return status;}
 }
 return B6_CHECKED_EXHAUSTED;
}
#endif
