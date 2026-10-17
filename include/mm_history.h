#ifndef TT_MM_HISTORY_H
#define TT_MM_HISTORY_H
#include "mm_core.h"
#include <stdlib.h>
#include <string.h>
#define MM_HISTORY_LENGTH 8
#define MM_HISTORY_CONTROLS 12
typedef struct {mm_phrase phrase;float controls[MM_HISTORY_CONTROLS];} mm_history_entry;
typedef struct {mm_history_entry *entries;unsigned count,cursor;} mm_history;
/* Worker/control thread only, lazily bounded to eight creations per used pad. */
static inline int mm_history_record(mm_history *h,const mm_phrase *phrase,const float *controls){
 if(!h->entries){h->entries=calloc(MM_HISTORY_LENGTH,sizeof *h->entries);if(!h->entries)return 0;}
 if(h->count)h->count=h->cursor+1;
 if(h->count==MM_HISTORY_LENGTH){memmove(h->entries,h->entries+1,(MM_HISTORY_LENGTH-1)*sizeof *h->entries);h->count--;}
 h->cursor=h->count++;h->entries[h->cursor].phrase=*phrase;memcpy(h->entries[h->cursor].controls,controls,MM_HISTORY_CONTROLS*sizeof(float));return 1;
}
static inline const mm_history_entry *mm_history_move(mm_history *h,int direction){
 if(!h->count||(direction<0&&!h->cursor)||(direction>0&&h->cursor+1>=h->count))return NULL;
 h->cursor=(unsigned)((int)h->cursor+direction);return &h->entries[h->cursor];
}
static inline void mm_history_clear(mm_history *h){h->count=0;h->cursor=0;}
static inline void mm_history_close(mm_history *h){free(h->entries);memset(h,0,sizeof *h);}
#endif
