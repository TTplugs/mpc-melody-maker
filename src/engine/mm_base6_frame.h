/* Experimental worker-owned complete creation. Not wired into installed VST.
   Publish only a fully built frame, never melody and accompaniment separately.
   One source asset supplies its base, resolved chords and resolved bass. */
#ifndef TT_MM_BASE6_FRAME_H
#define TT_MM_BASE6_FRAME_H
#include "mm_base6_backend.h"
#include "mm_base6_playback.h"

typedef struct {
 const char *source_id;
 const mm_part *chords,*bass;
} mm6_accompaniment;

typedef struct {
 mm6_packet generation;
 mm_phrase playback;
 int32_t bass_shift;
 uint32_t playback_status;
} mm6_frame;

enum {MM6_FRAME_OK=0,MM6_FRAME_INVALID=1,MM6_FRAME_GENERATION_FAILED=2,
      MM6_FRAME_PLAYBACK_FAILED=3};

/* Worker-only. Assets are indexed by the exact generation source index and
   pinned through completion. scratch must be caller-owned, not an active audio
   frame. Failure leaves out unchanged, allowing the old creation to continue.
   Caller must allocate a result pool large enough for sizeof(mm6_frame).
   No allocations, host calls or publication take place in this helper. */
static inline int mm6_build_frame(const mm6_packet *packet,
 const mm6_accompaniment *assets,uint32_t asset_count,
 int bass_mode,int low,int high,mm6_frame *scratch,mm6_frame *out){
 const mm6_accompaniment *asset;int shift=0,status;
 if(!packet||!assets||!scratch||!out||scratch==out||
    packet==&scratch->generation||packet==&out->generation)
  return MM6_FRAME_INVALID;
 if(packet->header.status!=B6_CHECKED_ACCEPTED)return MM6_FRAME_GENERATION_FAILED;
 if(packet->header.source>=asset_count)return MM6_FRAME_INVALID;
 asset=&assets[packet->header.source];
 if(asset->chords==&out->playback.parts[0]||asset->chords==&out->playback.parts[1]||
    asset->chords==&out->playback.parts[2]||asset->bass==&out->playback.parts[0]||
    asset->bass==&out->playback.parts[1]||asset->bass==&out->playback.parts[2])
  return MM6_FRAME_INVALID;
 /* Converter also rejects accompaniment pointers aliasing scratch playback. */
 status=mm6_build_playback(&packet->phrase,asset->source_id,asset->chords,
   asset->bass,bass_mode,low,high,&scratch->playback,&shift);
 if(status!=MM6_PLAYBACK_OK)return MM6_FRAME_PLAYBACK_FAILED;
 scratch->generation=*packet;scratch->bass_shift=shift;
 scratch->playback_status=(uint32_t)status;
 *out=*scratch;return MM6_FRAME_OK;
}
#endif
