/* Worker-only conversion into the existing plugin's three-part phrase format.
   Chords/bass must already be resolved from the own clip/family by Part8.
   Never infer a bass line from per-melody-note bass annotations. */
#ifndef TT_MM_BASE6_PLAYBACK_H
#define TT_MM_BASE6_PLAYBACK_H
#include "mm_core.h"
#include "mutation3.h"
#include "bass_register.h"
#include <string.h>
enum {MM6_PLAYBACK_OK=0,MM6_PLAYBACK_INVALID=1,MM6_PLAYBACK_MISSING=2};
static inline int mm6_part_valid(const mm_part *p,uint32_t length){
 unsigned i;if(!p||p->count>MM_MAX_NOTES)return 0;
 for(i=0;i<p->count;i++){
  const mm_note *n=&p->notes[i];
  /* Preserve source articulation crossing the loop boundary. The existing
     scheduler wraps note-offs modulo length. Durations > one loop rejected. */
  if(!n->duration||n->start>=length||n->duration>length||n->pitch>127||!n->velocity||n->velocity>127)return 0;
 }
 return 1;
}
/* Input ownership must be distinct from out. Validate before touching out.
   On any error, caller retains its previous output unchanged.
   bass_mode=0 preserves source register; 1 applies the explicit mean range.
   Returns the common octave shift through bass_shift on success. */
static inline int mm6_build_playback(const m3_phrase *melody,const char *source_id,
 const mm_part *chords,const mm_part *bass,int bass_mode,int low,int high,
 mm_phrase *out,int *bass_shift){
 int i,shift=0;size_t id_length;int32_t pitches[MM_MAX_NOTES];
 if(!melody||!source_id||!out||!bass_shift||bass_mode<0||bass_mode>1||melody->count<1||melody->count>128||melody->length<1)return MM6_PLAYBACK_INVALID;
 for(id_length=0;id_length<sizeof out->source_id&&source_id[id_length];id_length++){}
 if(id_length>=sizeof out->source_id)return MM6_PLAYBACK_INVALID;
 if(!chords||!bass||!chords->count||!bass->count)return MM6_PLAYBACK_MISSING;
 if(chords==&out->parts[0]||chords==&out->parts[1]||chords==&out->parts[2]||bass==&out->parts[0]||bass==&out->parts[1]||bass==&out->parts[2])return MM6_PLAYBACK_INVALID;
 if(!mm6_part_valid(chords,(uint32_t)melody->length)||!mm6_part_valid(bass,(uint32_t)melody->length))return MM6_PLAYBACK_INVALID;
 for(i=0;i<melody->count;i++){
  const m3_note *n=&melody->notes[i];
  if(n->start<0||n->start>=melody->length||n->duration<1||n->duration>melody->length-n->start||n->pitch<0||n->pitch>127||n->velocity<1||n->velocity>127)return MM6_PLAYBACK_INVALID;
 }
 for(i=0;i<(int)bass->count;i++)pitches[i]=bass->notes[i].pitch;
 if(bass_mode&&tt_bass_octave_shift(pitches,(int)bass->count,low,high,&shift))return MM6_PLAYBACK_INVALID;
 memset(out,0,sizeof *out);memcpy(out->source_id,source_id,id_length);out->length=(uint32_t)melody->length;
 out->parts[0].count=(uint32_t)melody->count;
 for(i=0;i<melody->count;i++){
  mm_note *dst=&out->parts[0].notes[i];const m3_note *src=&melody->notes[i];
  dst->start=(uint32_t)src->start;dst->duration=(uint32_t)src->duration;dst->pitch=(uint8_t)src->pitch;dst->velocity=(uint8_t)src->velocity;
 }
 out->parts[1]=*chords;out->parts[2]=*bass;
 for(i=0;i<(int)bass->count;i++)out->parts[2].notes[i].pitch=(uint8_t)(pitches[i]+shift);
 *bass_shift=shift;return MM6_PLAYBACK_OK;
}
#endif
