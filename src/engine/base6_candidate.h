#ifndef TT_BASE6_CANDIDATE_H
#define TT_BASE6_CANDIDATE_H
#include "base6_sampler.h"
#include "mutation3.h"
/* Original Part6 candidate proposal. Does NOT select a profile, score,
 * rank, test novelty, modify harmony, or reset the stream between candidates.
 * out must not alias base. Caller initializes RNG before the first candidate.
 */
static inline int b6_candidate(const b6_tables *m,uint32_t smooth,
                               const int32_t condition[3],const m3_phrase *base,
                               uint32_t *seed,m3_phrase *out,
                               const unsigned char (*allowed)[128]) {
    int32_t history[6]={-999,-999,-999,-999,-999,-999};
    int i,prev=-1,center,sum=0;
    uint64_t weights[128];
    if(!base||!out||base==out||base->count<1||base->count>M3_NOTES||
       base->key<0||base->key>11||base->mode<0||base->mode>4)return 0;
    for(i=0;i<base->count;i++) {
        if(base->notes[i].pitch<0||base->notes[i].pitch>127)return 0;
        sum+=base->notes[i].pitch;
    }
    center=sum/base->count;memcpy(out,base,sizeof *out);
    for(i=0;i<base->count;i++) {
        m3_note *note=&out->notes[i];int pitch;int32_t token[3];
        if(!b6_draw(m,smooth,condition,base->key,base->mode,prev,note->root,note->bass,
                    center,history,allowed?allowed[i]:NULL,seed,weights,&pitch))return 0;
        token[0]=prev<0?0:b6_degree(base->key,base->mode,pitch)-b6_degree(base->key,base->mode,prev);
        token[1]=b6_mod12(pitch-(note->root<0?base->key:note->root));
        token[2]=note->bass<0?12:b6_mod12(pitch-note->bass);
        memmove(history,history+3,3*sizeof *history);memcpy(history+3,token,sizeof token);
        note->pitch=pitch;prev=pitch;
    }
    return 1;
}
#endif
