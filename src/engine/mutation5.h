#ifndef TT_MUTATION5_H
#define TT_MUTATION5_H
#include "proposal3.h"
#define M5_FEATURES 49
#define M5_ROWS 4296
#define M5_GROUPS 48
#define M5_WEIGHTS 12
typedef struct {int32_t kind,speed,fit,count,features[M5_FEATURES];} m5_row;
typedef struct {int32_t count;int32_t spread[M5_GROUPS][M5_FEATURES],minimum[M5_GROUPS][2],maximum[M5_GROUPS][2];int32_t velocity_mean[3][16],velocity_sd[3][16];m5_row rows[M5_ROWS];} m5_model;
typedef struct {int32_t operation,intensity,dark,candidates,passes,fit,bar_ticks,harmony;uint32_t seed;} m5_params;
typedef struct {int32_t changed,start,end,old_mask,new_mask,root_preserved,ct_before,pcs_before,ct_after,pcs_after,penalty_before,penalty_after,gate_ct_out,gate_pcs_out;} m5_notice;
typedef struct m5_context {m3_context proposal;m5_model model;int32_t weights[M5_WEIGHTS],target[M5_FEATURES],group,kind,fit,bar,intensity,dark;uint32_t seed;m3_phrase trial,best;m5_notice notice;} m5_context;
uint32_t m5_context_size(void);
uint32_t m5_model_size(void);
int m5_load(m5_context *,const char *,const char *);
int m5_load_weights(m5_context *,const char *);
void m5_features(m5_context *,const m3_phrase *,const m3_phrase *,int32_t *);
void m5_refresh(m3_phrase *);
int64_t m5_score(m5_context *,const m3_phrase *,const m3_phrase *);
int m5_generate(m5_context *,const m3_phrase *,const m5_params *,m3_phrase *,m5_notice *);
int m5_propose(m3_context *,const m3_phrase *,const m3_params *,m3_phrase *,m5_context *);
#endif
