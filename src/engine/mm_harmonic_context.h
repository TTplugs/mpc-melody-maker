/* Worker-only reconstruction from the creation's actual accompaniment.
   Same root-inference templates/tie priorities as original Part2 measure.infer.
   This annotates the melody; it does not compose or replace accompaniment. */
#ifndef TT_MM_HARMONIC_CONTEXT_H
#define TT_MM_HARMONIC_CONTEXT_H
#include "mm_base6_playback.h"
#include "ttmm_phrase_rules.h"
#include <limits.h>
typedef struct {m3_phrase phrase;uint32_t boundaries[2*MM_MAX_NOTES+2];} mmh_scratch;
enum {MMH_OK=0,MMH_INVALID=1,MMH_CAPACITY=2};
static inline unsigned mmh_pop(uint32_t v){unsigned n=0;while(v){v&=v-1;n++;}return n;}
static inline int mmh_root(uint32_t mask,int bass){
 static const unsigned char sizes[16]={3,3,3,3,4,4,4,4,4,5,5,5,3,3,4,4};
 static const unsigned char intervals[16][5]={{0,4,7},{0,3,7},{0,3,6},{0,4,8},
  {0,4,7,10},{0,4,7,11},{0,3,7,10},{0,3,6,10},{0,3,6,9},
  {0,2,4,7,10},{0,2,4,7,11},{0,2,3,7,10},{0,2,7},{0,5,7},{0,4,7,9},{0,3,7,9}};
 unsigned root,type,best=UINT_MAX;int selected=-1;if(!mask)return -1;
 for(root=0;root<12;root++)for(type=0;type<16;type++){
  uint32_t target=0;unsigned i,score;
  for(i=0;i<sizes[type];i++)target|=1u<<((root+intervals[type][i])%12);
  score=((((mmh_pop(mask^target)*2+(bass<0||bass%12!=(int)root))*2+!(mask&(1u<<root)))*6+sizes[type])*12+root);
  if(score<best){best=score;selected=(int)root;}
 }return selected;
}
static inline int mmh_active(const mm_part *part,uint64_t twice_tick,int *lowest,uint32_t *mask,int32_t *pitches){
 unsigned i;int count=0;*lowest=-1;*mask=0;
 for(i=0;i<part->count;i++){
  const mm_note *n=&part->notes[i];
  if(2*(uint64_t)n->start<=twice_tick&&twice_tick<2*((uint64_t)n->start+n->duration)){
   if(pitches){if(count>=16)return -1;pitches[count]=n->pitch;}
   count++;*mask|=1u<<(n->pitch%12);if(*lowest<0||n->pitch<*lowest)*lowest=n->pitch;
  }
 }return count;
}
/* Input/output may be the same creation; scratch must be separate. Any error
   leaves out unchanged. No allocation, publication, locks or host callbacks. */
static inline int mmh_refresh(const m3_phrase *input,const mm_part *chords,const mm_part *bass,mmh_scratch *scratch,m3_phrase *out){
 unsigned i,j,count=0;uint32_t length;
 if(!input||!chords||!bass||!scratch||!out||input==&scratch->phrase||out==&scratch->phrase||input->count<1||input->count>128||input->length<1)return MMH_INVALID;
 length=(uint32_t)input->length;
 if(!mm6_part_valid(chords,length)||!mm6_part_valid(bass,length))return MMH_INVALID;
 for(i=0;i<(unsigned)input->count;i++){
  const m3_note *n=&input->notes[i];
  if(n->start<0||n->start>=input->length||n->duration<1||n->duration>input->length-n->start||n->pitch<0||n->pitch>127||n->velocity<1||n->velocity>127)return MMH_INVALID;
 }
 scratch->phrase=*input;scratch->phrase.chord_count=0;memset(scratch->phrase.chords,0,sizeof scratch->phrase.chords);
 scratch->boundaries[count++]=0;scratch->boundaries[count++]=length;
 for(i=0;i<chords->count;i++){
  uint64_t end=(uint64_t)chords->notes[i].start+chords->notes[i].duration;
  scratch->boundaries[count++]=chords->notes[i].start;
  scratch->boundaries[count++]=(uint32_t)(end>length?length:end);
 }
 /* Bounded insertion sort avoids allocator-dependent qsort implementations. */
 for(i=1;i<count;i++){uint32_t v=scratch->boundaries[i];j=i;while(j&&scratch->boundaries[j-1]>v){scratch->boundaries[j]=scratch->boundaries[j-1];j--;}scratch->boundaries[j]=v;}
 for(i=0;i+1<count;i++){
  uint32_t a=scratch->boundaries[i],b=scratch->boundaries[i+1],mask;int low,n;m3_chord segment;
  if(a==b)continue;
  memset(&segment,0,sizeof segment);
  n=mmh_active(chords,(uint64_t)a+b,&low,&mask,segment.pitches);if(n<0)return MMH_CAPACITY;if(!n)continue;
  if(scratch->phrase.chord_count>=512)return MMH_CAPACITY;
  segment.start=(int32_t)a;segment.end=(int32_t)b;segment.count=n;segment.mask=mask;
  scratch->phrase.chords[scratch->phrase.chord_count++]=segment;
 }
 for(i=0;i<(unsigned)input->count;i++){
  m3_note *n=&scratch->phrase.notes[i];int low,lowest_bass;uint32_t mask,ignored;
  uint32_t next_start=(i+1<(unsigned)input->count)?(uint32_t)input->notes[i+1].start:length;
  uint32_t trig_tick=ttmm_resolve_trigger_tick((uint32_t)n->start,(uint32_t)n->duration,next_start,length,input->speed);
  mmh_active(chords,2*(uint64_t)n->start,&low,&mask,NULL);
  if(!mask&&trig_tick!=(uint32_t)n->start)mmh_active(chords,2*(uint64_t)trig_tick,&low,&mask,NULL);
  mmh_active(bass,2*(uint64_t)n->start,&lowest_bass,&ignored,NULL);
  if(lowest_bass<0&&trig_tick!=(uint32_t)n->start)mmh_active(bass,2*(uint64_t)trig_tick,&lowest_bass,&ignored,NULL);
  n->chord=mask;n->bass=lowest_bass;n->root=mmh_root(mask,lowest_bass);
 }
 *out=scratch->phrase;return MMH_OK;
}
#endif
