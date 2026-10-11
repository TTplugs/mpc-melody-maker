#ifndef TT_MM_SOURCE_CATALOG_H
#define TT_MM_SOURCE_CATALOG_H
#include "base6_sampler.h"
typedef struct {unsigned char *data;size_t size;uint32_t count;} mmsc_catalog;
typedef struct {uint32_t speed,light_dark,simple_complex;const char *style;} mmsc_request;
static inline void mmsc_close(mmsc_catalog *c){free(c->data);memset(c,0,sizeof *c);}
static inline int mmsc_load(mmsc_catalog *c,const char *path,uint32_t source_count){
 FILE *fp;size_t wanted;uint32_t i;unsigned char seen[3000]={0};memset(c,0,sizeof *c);if(!source_count||source_count>3000)return 0;
 wanted=12+(size_t)source_count*96;fp=fopen(path,"rb");if(!fp)return 0;c->data=malloc(wanted);if(!c->data){fclose(fp);return 0;}
 if(fread(c->data,1,wanted,fp)!=wanted||fgetc(fp)!=EOF){fclose(fp);goto bad;}fclose(fp);
 if(memcmp(c->data,"TTMMCT01",8)||b6_le32(c->data+8)!=source_count)goto bad;c->count=source_count;c->size=wanted;
 for(i=0;i<c->count;i++){const unsigned char *r=c->data+12+96*i;if(b6_le32(r)>=source_count||seen[b6_le32(r)]||b6_le32(r+4)>2||b6_le32(r+8)>3||b6_le32(r+12)>4||!b6_le32(r+16)||b6_le32(r+16)>1000000||b6_le32(r+20)>11||b6_le32(r+24)>4||!memchr(r+32,0,64))goto bad;seen[b6_le32(r)]=1;}
 return 1;bad:mmsc_close(c);return 0;
}
static inline int mmsc_match(const unsigned char *r,const mmsc_request *q){return b6_le32(r+4)==q->speed&&b6_le32(r+8)==q->light_dark&&b6_le32(r+12)==q->simple_complex&&(!q->style||!q->style[0]||!strcmp(q->style,(const char *)r+32));}
/* Return 0 on no compatible source; output remains unchanged. Worker only. */
static inline int mmsc_select_mode(const mmsc_catalog *c,const mmsc_request *q,int mode,uint32_t seed,uint32_t *out,uint32_t *eligible){
 uint32_t i,count=0,first=0;int uniform=1;uint64_t total=0,choice;if(!c||!c->data||!q||!out||!eligible||q->speed>2||q->light_dark>3||q->simple_complex>4)return 0;
 for(i=0;i<c->count;i++){const unsigned char *r=c->data+12+96*i;if(mmsc_match(r,q)&&(mode<0||b6_le32(r+24)==(uint32_t)mode)){uint32_t weight=b6_le32(r+16);if(!count)first=weight;else if(weight!=first)uniform=0;count++;total+=weight;}}
 *eligible=count;if(!count)return 0;seed=seed?seed:0x9e3779b9u;choice=b6_below(&seed,uniform?count:total);
 for(i=0;i<c->count;i++){const unsigned char *r=c->data+12+96*i;if(mmsc_match(r,q)&&(mode<0||b6_le32(r+24)==(uint32_t)mode)){uint32_t weight=uniform?1:b6_le32(r+16);if(choice<weight){*out=b6_le32(r);return 1;}choice-=weight;}}
 return 0;
}
/* Pad axes are preferences; speed, style and explicit corpus mode stay strict.
   Preserve the exact selector and RNG stream whenever the exact cell exists. */
static inline int mmsc_select_nearest(const mmsc_catalog *c,const mmsc_request *q,int mode,uint32_t seed,uint32_t *out,uint32_t *eligible){
 if(!c||!c->data||!q||!out||!eligible||q->speed>2||q->light_dark>3||q->simple_complex>4||mode>4)return 0;
 if(mmsc_select_mode(c,q,mode,seed,out,eligible))return 1;
 unsigned best=UINT32_MAX;uint32_t count=0,first=0;uint64_t total=0;int uniform=1;
 for(uint32_t i=0;i<c->count;i++){
  const unsigned char *r=c->data+12+96*i;
  if(b6_le32(r+4)!=q->speed||(mode>=0&&b6_le32(r+24)!=(uint32_t)mode)||(q->style&&q->style[0]&&strcmp(q->style,(const char *)r+32)))continue;
  unsigned score=(unsigned)abs((int)b6_le32(r+8)-(int)q->light_dark)+(unsigned)abs((int)b6_le32(r+12)-(int)q->simple_complex);
  if(score<best){best=score;count=0;total=0;uniform=1;}
  if(score==best){uint32_t weight=b6_le32(r+16);if(!count)first=weight;else if(first!=weight)uniform=0;count++;total+=weight;}
 }
 *eligible=count;if(!count)return 0;seed=seed?seed:0x9e3779b9u;uint64_t choice=b6_below(&seed,uniform?count:total);
 for(uint32_t i=0;i<c->count;i++){
  const unsigned char *r=c->data+12+96*i;
  if(b6_le32(r+4)!=q->speed||(mode>=0&&b6_le32(r+24)!=(uint32_t)mode)||(q->style&&q->style[0]&&strcmp(q->style,(const char *)r+32)))continue;
  unsigned score=(unsigned)abs((int)b6_le32(r+8)-(int)q->light_dark)+(unsigned)abs((int)b6_le32(r+12)-(int)q->simple_complex);
  if(score!=best)continue;uint32_t weight=uniform?1:b6_le32(r+16);if(choice<weight){*out=b6_le32(r);return 1;}choice-=weight;
 }
 return 0;
}
static inline int mmsc_select(const mmsc_catalog *c,const mmsc_request *q,uint32_t seed,uint32_t *out,uint32_t *eligible){return mmsc_select_mode(c,q,-1,seed,out,eligible);}
#endif
