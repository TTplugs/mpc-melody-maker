#ifndef TT_BASE6_CHECKED_H
#define TT_BASE6_CHECKED_H
#include "base6_generate.h"
#include "base6_novelty.h"

typedef struct {double minimum_distance,maximum_run;} b6_novelty_limits;
typedef struct {uint32_t seed; b6_result generation; b6_novelty novelty; int distance_flag,run_flag;} b6_attempt;
typedef struct {uint32_t count; b6_attempt attempts[32];} b6_checked_result;
enum {B6_CHECKED_ERROR=0,B6_CHECKED_ACCEPTED=1,B6_CHECKED_EXHAUSTED=2};

/* Worker-only: full private-library scans; never call from the audio callback.
 * New outputs include their own source family in both novelty checks.
 * On error or exhaustion caller's output remains unchanged.
 */
static inline int b6_generate_checked_supported(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,const b6_library *library,const b6_novelty_limits *limits,
 uint32_t max_attempts,m3_phrase *out,b6_checked_result *result,
 const unsigned char (*allowed)[128]){
 uint32_t k,j;b6_mel_token tokens[128];m3_phrase trial;
 if(!library||!library->rows||!library->count||!limits||!out||!base||out==base||!result||
    max_attempts<1||max_attempts>32||!isfinite(limits->minimum_distance)||
    limits->minimum_distance<0||limits->minimum_distance>1||
    !isfinite(limits->maximum_run)||limits->maximum_run<0)return B6_CHECKED_ERROR;
 memset(result,0,sizeof *result);
 for(k=0;k<max_attempts;k++){
  b6_attempt *a=&result->attempts[k];
  a->seed=k?m3_pad_seed(initial_seed,k):initial_seed;
  for(j=0;j<k;j++)if(result->attempts[j].seed==a->seed)return B6_CHECKED_ERROR;
  if(!b6_generate_supported(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,a->seed,
                   candidates,&trial,&a->generation,allowed)||!b6_mel_tokens(&trial,tokens)||
     !b6_check_novelty(library,tokens,(uint32_t)trial.count,UINT32_MAX,&a->novelty))return B6_CHECKED_ERROR;
  a->distance_flag=a->novelty.distance<limits->minimum_distance;
  a->run_flag=(double)a->novelty.longest>limits->maximum_run;
  result->count=k+1;
  if(!a->distance_flag&&!a->run_flag){*out=trial;return B6_CHECKED_ACCEPTED;}
 }
 return B6_CHECKED_EXHAUSTED;
}
static inline int b6_generate_checked(const b6_tables *model,const b6_profiles *profiles,
 uint32_t smooth,m5_context *ctx,const int32_t co[3],const m3_phrase *base,
 int bar,const uint32_t *guides,uint32_t guide_count,uint32_t initial_seed,
 uint32_t candidates,const b6_library *library,const b6_novelty_limits *limits,
 uint32_t max_attempts,m3_phrase *out,b6_checked_result *result){
 return b6_generate_checked_supported(model,profiles,smooth,ctx,co,base,bar,guides,guide_count,
 initial_seed,candidates,library,limits,max_attempts,out,result,NULL);
}

static inline int b6_novelty_batch_stop(uint32_t generated,uint32_t exhausted){
 /* Strictly greater than twice the requested 5 percent expected rate. */
 if(!generated||exhausted>generated)return -1;
 return (uint64_t)exhausted*10u>generated;
}
#endif
