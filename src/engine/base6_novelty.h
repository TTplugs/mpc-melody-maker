#ifndef TT_BASE6_NOVELTY_H
#define TT_BASE6_NOVELTY_H
#include "base6_sampler.h"
#include "mutation3.h"
typedef struct {unsigned char *data;size_t size;uint32_t count;const unsigned char **rows;} b6_library;
typedef struct {int32_t degree,duration;} b6_mel_token;
typedef struct {double distance;uint32_t nearest,longest,run_clip,run_query_start,run_clip_start;} b6_novelty;
static inline void b6_library_close(b6_library *l){free(l->rows);free(l->data);memset(l,0,sizeof *l);}
static inline int b6_library_load(b6_library *l,const char *path){
 FILE *fp;long bytes;size_t pos;uint32_t i,j;memset(l,0,sizeof *l);
 fp=fopen(path,"rb");if(!fp)return 0;
 if(fseek(fp,0,SEEK_END)||(bytes=ftell(fp))<12||bytes>16*1024*1024||fseek(fp,0,SEEK_SET)){fclose(fp);return 0;}
 l->size=(size_t)bytes;l->data=malloc(l->size);
 if(!l->data){fclose(fp);return 0;}
 if(fread(l->data,1,l->size,fp)!=l->size){fclose(fp);goto bad;}fclose(fp);
 if(memcmp(l->data,"TTMM6NV1",8))goto bad;
 l->count=b6_le32(l->data+8);if(!l->count||l->count>20000)goto bad;
 l->rows=calloc(l->count,sizeof *l->rows);if(!l->rows)goto bad;
 pos=12;
 for(i=0;i<l->count;i++){
  uint32_t name,n;
  if(l->size-pos<12)goto bad;
  l->rows[i]=l->data+pos;name=b6_le32(l->data+pos+4);n=b6_le32(l->data+pos+8);pos+=12;
  if(name>128||!n||n>128||(size_t)name+8*n>l->size-pos)goto bad;
  pos+=name;
  for(j=0;j<n;j++)if(b6_i32(l->data+pos+8*j+4)<1)goto bad;
  pos+=8*n;
 }
 if(pos!=l->size)goto bad;
 return 1;
bad:b6_library_close(l);return 0;
}
static inline int b6_mel_tokens(const m3_phrase *p,b6_mel_token tokens[128]){
 int i,previous=0;
 if(!p||p->count<1||p->count>128||p->key<0||p->key>11||p->mode<0||p->mode>4)return 0;
 for(i=0;i<p->count;i++){
  int g;if(p->notes[i].pitch<0||p->notes[i].pitch>127||p->notes[i].duration<1)return 0;
  g=b6_degree(p->key,p->mode,p->notes[i].pitch);
  tokens[i].degree=i?g-previous:0;tokens[i].duration=p->notes[i].duration;previous=g;
 }
 return 1;
}
/* Exact edit distance and longest contiguous equal-token run, single library
 * scan. No family exclusion for new outputs; exclusion only for real controls.
 * Witness ties may differ from the Python batch index; lengths are identical.
 */
static inline int b6_check_novelty(const b6_library *l,const b6_mel_token *q,uint32_t n,
                                  uint32_t exclude_family,b6_novelty *out){
 uint32_t k,i,j;uint16_t prev[129],curr[129],runs[129];double best=2.;int found=0;
 if(!l||!q||!out||n<1||n>128)return 0;
 memset(out,0,sizeof *out);out->nearest=out->run_clip=UINT32_MAX;
 for(k=0;k<l->count;k++){
  const unsigned char *r=l->rows[k],*tokens;uint32_t m=b6_le32(r+8);
  double distance;
  if(b6_le32(r)==exclude_family)continue;
  tokens=r+12+b6_le32(r+4);
  for(j=0;j<=m;j++){prev[j]=(uint16_t)j;runs[j]=0;}
  for(i=1;i<=n;i++){
   uint16_t diagonal=0;curr[0]=(uint16_t)i;
   for(j=1;j<=m;j++){
    int equal=q[i-1].degree==b6_i32(tokens+8*(j-1))&&q[i-1].duration==b6_i32(tokens+8*(j-1)+4);
    unsigned a=prev[j]+1,b=curr[j-1]+1,c=prev[j-1]+!equal;
    uint16_t old=runs[j];curr[j]=(uint16_t)(a<b?(a<c?a:c):(b<c?b:c));
    runs[j]=equal?(uint16_t)(diagonal+1):0;diagonal=old;
    if(runs[j]>out->longest){out->longest=runs[j];out->run_clip=k;out->run_query_start=i-runs[j];out->run_clip_start=j-runs[j];}
   }
   memcpy(prev,curr,(m+1)*sizeof *prev);
  }
  distance=(double)prev[m]/(n>m?n:m);
  if(distance<best){best=distance;out->nearest=k;found=1;}
 }
 out->distance=best;return found;
}
#endif
