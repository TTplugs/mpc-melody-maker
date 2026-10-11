#ifndef TT_MM_CANDIDATE_ENGINE_H
#define TT_MM_CANDIDATE_ENGINE_H
#include "mm_core.h"
typedef struct mmce_engine mmce_engine;
mmce_engine *mmce_open(const char *directory);
void mmce_close(mmce_engine *e);
/* Worker-only; explicit resolved source labels, never guessed UI semantics. */
int mmce_create(mmce_engine *e,const mm_request *r,int source_light_dark,int source_complexity,int candidates,mm_phrase *out);
int mmce_vary(mmce_engine *e,const mm_request *r,int operation,int dark,int candidates,const mm_phrase *current,mm_phrase *out);
int mmce_generation_diagnostic(const mmce_engine *e);
const char *mmce_error(int status);
#endif
