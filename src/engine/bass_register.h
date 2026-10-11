#ifndef TT_BASS_REGISTER_H
#define TT_BASS_REGISTER_H
#include <stdint.h>
#include <limits.h>
/* Optional output policy. Range applies to the arithmetic mean register.
   Returns 0 on success, -1 on invalid input. Never folds individual notes.
   Caller applies *shift equally to every bass event, including annotations. */
static int tt_bass_octave_shift(const int32_t *pitches,int count,int low,int high,int *shift){
 int i,s,best=0,minp=127,maxp=0;int64_t total=0,bestd=INT64_MAX;
 if(!shift||count<0||count>2048||(count&&!pitches)||low<0||high>127||low>high)return -1;
 for(i=0;i<count;i++){int p=pitches[i];if(p<0||p>127)return -1;total+=p;if(p<minp)minp=p;if(p>maxp)maxp=p;}
 if(!count||(total>=(int64_t)low*count&&total<=(int64_t)high*count)){*shift=0;return 0;}
 for(s=-120;s<=120;s+=12){int64_t d;int ab=s<0?-s:s,oldab=best<0?-best:best;
  if(minp+s<0||maxp+s>127)continue;
  d=2*(total+(int64_t)s*count)-(int64_t)(low+high)*count;if(d<0)d=-d;
  if(d<bestd||(d==bestd&&(ab<oldab||(ab==oldab&&s<best)))){best=s;bestd=d;}
 }
 *shift=best;return 0;
}
#endif
