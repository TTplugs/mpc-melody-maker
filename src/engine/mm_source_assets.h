/* Private source records, decoded only on worker/control threads. Never audio.
   Library bytes stay pinned while records are read. Zero-init before load;
   close only after all worker users have stopped. No corpus data in this header. */
#ifndef TT_MM_SOURCE_ASSETS_H
#define TT_MM_SOURCE_ASSETS_H
#include "base6_tables.h"
#include "mm_base6_playback.h"
#include <math.h>
typedef struct {unsigned char *data;size_t size;uint32_t count,*offsets;} mmsa_library;
typedef struct {
 char source_id[48];int32_t co[3];uint32_t bar,guide_count,guides[128];
 m3_phrase base;unsigned char allowed[128][128];mm_part chords,bass;
 double minimum_distance,maximum_run;
} mmsa_source;
typedef struct {const unsigned char *p,*end;} mmsa_reader;
static inline int mmsa_read(mmsa_reader *r,void *out,size_t n){if(n>(size_t)(r->end-r->p))return 0;memcpy(out,r->p,n);r->p+=n;return 1;}
static inline int mmsa_u32(mmsa_reader *r,uint32_t *v){unsigned char b[4];if(!mmsa_read(r,b,4))return 0;*v=b6_le32(b);return 1;}
static inline int mmsa_i32(mmsa_reader *r,int32_t *v){uint32_t x;if(!mmsa_u32(r,&x))return 0;*v=(int32_t)(x<=INT32_MAX?(int64_t)x:(int64_t)x-4294967296LL);return 1;}
static inline int mmsa_double(mmsa_reader *r,double *v){unsigned char b[8];uint64_t bits;if(sizeof(double)!=8||!mmsa_read(r,b,8))return 0;bits=b6_le64(b);memcpy(v,&bits,8);return isfinite(*v);}
static inline int mmsa_decode_record(const unsigned char *bytes,size_t size,mmsa_source *s){
 mmsa_reader r={bytes,bytes+size};unsigned i,j,part;uint32_t mask;int32_t *fields[7]={&s->base.count,&s->base.length,&s->base.speed,&s->base.movement,&s->base.groove,&s->base.key,&s->base.mode};
 memset(s,0,sizeof *s);
 if(!mmsa_read(&r,s->source_id,48)||!s->source_id[0]||!memchr(s->source_id,0,48))return 0;
 for(i=0;i<3;i++)if(!mmsa_i32(&r,&s->co[i]))return 0;
 if(!mmsa_u32(&r,&s->bar)||s->bar!=3840||!mmsa_u32(&r,&s->guide_count)||s->guide_count>128)return 0;
 for(i=0;i<s->guide_count;i++)if(!mmsa_u32(&r,&s->guides[i])||(i&&s->guides[i]<=s->guides[i-1]))return 0;
 for(i=0;i<7;i++)if(!mmsa_i32(&r,fields[i]))return 0;
 if(s->base.count<1||s->base.count>128||s->base.length!=15360||s->base.speed<0||s->base.speed>2||s->base.movement<0||s->base.movement>3||s->base.groove<0||s->base.groove>3||s->base.key<0||s->base.key>11||s->base.mode<0||s->base.mode>4||s->guide_count>(unsigned)s->base.count/2)return 0;
 for(i=0;i<s->guide_count;i++)if(s->guides[i]>=(unsigned)s->base.count)return 0;
 for(i=0;i<(unsigned)s->base.count;i++){
  m3_note *n=&s->base.notes[i];if(!mmsa_i32(&r,&n->start)||!mmsa_i32(&r,&n->duration)||!mmsa_i32(&r,&n->pitch)||!mmsa_i32(&r,&n->velocity)||!mmsa_u32(&r,&n->chord)||!mmsa_i32(&r,&n->root)||!mmsa_i32(&r,&n->bass))return 0;
  if(n->start<0||n->start>=15360||n->duration<1||n->duration>15360-n->start||n->pitch<0||n->pitch>127||n->velocity<1||n->velocity>127||n->chord>4095||n->root< -1||n->root>11||n->bass< -1||n->bass>127)return 0;
 }
 if(!mmsa_i32(&r,&s->base.chord_count)||s->base.chord_count<0||s->base.chord_count>512)return 0;
 for(i=0;i<(unsigned)s->base.chord_count;i++){
  m3_chord *c=&s->base.chords[i];if(!mmsa_i32(&r,&c->start)||!mmsa_i32(&r,&c->end)||!mmsa_i32(&r,&c->count))return 0;
  for(j=0;j<16;j++)if(!mmsa_i32(&r,&c->pitches[j]))return 0;
  if(!mmsa_u32(&r,&c->mask)||c->start<0||c->end<=c->start||c->end>15360||c->count<1||c->count>16||c->mask>4095)return 0;
  mask=0;for(j=0;j<(unsigned)c->count;j++){if(c->pitches[j]<0||c->pitches[j]>127)return 0;mask|=1u<<(c->pitches[j]%12);}if(mask!=c->mask)return 0;
 }
 for(i=0;i<(unsigned)s->base.count;i++){
  unsigned char b[2];if(!mmsa_read(&r,b,2))return 0;mask=b[0]|((uint32_t)b[1]<<8);if(!mask||mask>4095)return 0;
  for(j=0;j<128;j++)s->allowed[i][j]=(unsigned char)((mask>>(j%12))&1u);
 }
 for(part=0;part<2;part++){
  mm_part *p=part?&s->bass:&s->chords;if(!mmsa_u32(&r,&p->count)||!p->count||p->count>MM_MAX_NOTES)return 0;
  for(i=0;i<p->count;i++){mm_note *n=&p->notes[i];if(!mmsa_u32(&r,&n->start)||!mmsa_u32(&r,&n->duration)||!mmsa_read(&r,&n->pitch,1)||!mmsa_read(&r,&n->velocity,1))return 0;}
  if(!mm6_part_valid(p,15360))return 0;
 }
 return mmsa_double(&r,&s->minimum_distance)&&s->minimum_distance>=0&&s->minimum_distance<=1&&mmsa_double(&r,&s->maximum_run)&&s->maximum_run>=0&&s->maximum_run<=128&&r.p==r.end;
}
static inline void mmsa_close(mmsa_library *l){free(l->offsets);free(l->data);memset(l,0,sizeof *l);}
static inline int mmsa_load(mmsa_library *l,const char *path){
 FILE *fp;long n;size_t pos;unsigned i;mmsa_source *scratch=NULL;
 memset(l,0,sizeof *l);fp=fopen(path,"rb");if(!fp)return 0;
 if(fseek(fp,0,SEEK_END)||(n=ftell(fp))<12||n>32*1024*1024||fseek(fp,0,SEEK_SET)){fclose(fp);return 0;}
 l->size=(size_t)n;l->data=malloc(l->size);if(!l->data){fclose(fp);return 0;}
 if(fread(l->data,1,l->size,fp)!=l->size){fclose(fp);goto bad;}fclose(fp);
 if(memcmp(l->data,"TTMMSR01",8))goto bad;l->count=b6_le32(l->data+8);if(!l->count||l->count>3000)goto bad;
 l->offsets=calloc(l->count,sizeof *l->offsets);scratch=malloc(sizeof *scratch);if(!l->offsets||!scratch)goto bad;
 pos=12;for(i=0;i<l->count;i++){uint32_t bytes;if(l->size-pos<4)goto bad;bytes=b6_le32(l->data+pos);l->offsets[i]=(uint32_t)pos;pos+=4;if(bytes>l->size-pos||!mmsa_decode_record(l->data+pos,bytes,scratch))goto bad;pos+=bytes;}
 if(pos!=l->size)goto bad;free(scratch);return 1;
 bad:free(scratch);mmsa_close(l);return 0;
}
/* Decode into worker scratch, commit out only after complete validation. */
static inline int mmsa_get(const mmsa_library *l,uint32_t index,mmsa_source *scratch,mmsa_source *out){
 size_t pos;uint32_t bytes;if(!l||!l->data||!l->offsets||index>=l->count||!scratch||!out||scratch==out)return 0;
 pos=l->offsets[index];if(pos>l->size||l->size-pos<4)return 0;bytes=b6_le32(l->data+pos);pos+=4;if(bytes>l->size-pos||!mmsa_decode_record(l->data+pos,bytes,scratch))return 0;*out=*scratch;return 1;
}
#endif
