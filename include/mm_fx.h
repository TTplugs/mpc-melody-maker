#ifndef MM_FX_H
#define MM_FX_H
#include "mm_core.h"
#define MM_FX_SLOTS 12
typedef struct {int enabled,type,target;float amount;} mm_fx;
void mm_fx_apply(const mm_phrase *dry,mm_phrase *wet,const mm_fx chain[MM_FX_SLOTS]);
#endif
