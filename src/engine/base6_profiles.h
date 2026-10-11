#ifndef TT_BASE6_PROFILES_H
#define TT_BASE6_PROFILES_H
#include "base6_sampler.h"
/* Four-bar halves can exceed the original 1,432-family profile count. */
#define B6_MAX_PROFILES 4096u
#define B6_MAX_GROUPS 128u
typedef struct {
    unsigned char *data;size_t size;
    uint32_t rows,groups;
    const unsigned char *counts,*group[B6_MAX_GROUPS],*row[B6_MAX_PROFILES];
} b6_profiles;
typedef struct {uint32_t profile,desired;double pout;int32_t target[49],spread[49];} b6_profile_choice;
static inline void b6_profiles_close(b6_profiles *p){free(p->data);memset(p,0,sizeof *p);}
static inline int b6_profiles_load(b6_profiles *p,const b6_tables *m,const char *path) {
    FILE *f;long size;size_t pos;uint32_t i;memset(p,0,sizeof *p);
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)||(size=ftell(f))<16||size>1024*1024||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    p->data=(unsigned char *)malloc((size_t)size);p->size=(size_t)size;
    if(!p->data){fclose(f);return 0;}
    if(fread(p->data,1,p->size,f)!=p->size){fclose(f);goto bad;}
    if(fclose(f))goto bad;
    if(memcmp(p->data,"TTMM6PF1",8))goto bad;
    p->rows=b6_le32(p->data+8);p->groups=b6_le32(p->data+12);
    if(!p->rows||p->rows>B6_MAX_PROFILES||!p->groups||p->groups>B6_MAX_GROUPS||p->rows!=b6_le32(m->profiles))goto bad;
    pos=16;
    if((size_t)p->rows*4>p->size-pos)goto bad;
    p->counts=p->data+pos;pos+=(size_t)p->rows*4;
    for(i=0;i<p->rows;i++)if(!b6_le32(p->counts+4*i)||b6_le32(p->counts+4*i)>128)goto bad;
    for(i=0;i<p->groups;i++) {
        uint32_t count,j;const unsigned char *g;
        if(p->size-pos<16)goto bad;
        g=p->data+pos;p->group[i]=g;count=b6_le32(g+12);pos+=16;
        if(!count||count>p->rows||(size_t)count*4+196>p->size-pos)goto bad;
        for(j=0;j<count;j++)if(b6_le32(p->data+pos+4*j)>=p->rows)goto bad;
        pos+=(size_t)count*4;
        for(j=0;j<49;j++)if(b6_i32(p->data+pos+4*j)<1)goto bad;
        pos+=196;
    }
    if(pos!=p->size)goto bad;
    pos=4;
    for(i=0;i<p->rows;i++) {
        uint32_t len=b6_le32(m->profiles+pos);pos+=4+len;
        p->row[i]=m->profiles+pos;pos+=216;
        if(!b6_le32(p->row[i]+12)||b6_i32(p->row[i]+16)<-1||b6_i32(p->row[i]+16)>1)goto bad;
    }
    return 1;
bad:b6_profiles_close(p);return 0;
}
static inline int b6_choose_profile(const b6_profiles *p,const int32_t co[3],uint32_t count,
                                    uint32_t *seed,b6_profile_choice *out) {
    const unsigned char *g=NULL;uint32_t i,n,nearest[B6_MAX_PROFILES],nn=0,best=UINT32_MAX;
    uint32_t uniform=1,first=0,end_uniform=1,end_first=0,end_n=0,end_sum=0;
    uint64_t total=0,end_total=0,end_out=0,draw;unsigned j;
    for(i=0;i<p->groups;i++)if(!b6_compare(p->group[i],co,3)){g=p->group[i];break;}
    if(!g)return 0;
    n=b6_le32(g+12);
    for(i=0;i<n;i++) {
        uint32_t id=b6_le32(g+16+4*i),c=b6_le32(p->counts+4*id),d=c>count?c-count:count-c;
        if(d<best){best=d;nn=0;}if(d==best)nearest[nn++]=id;
    }
    if(!nn)return 0;
    first=b6_le32(p->row[nearest[0]]+12);
    for(i=0;i<nn;i++){uint32_t weight=b6_le32(p->row[nearest[i]]+12);if(weight!=first)uniform=0;total+=weight;}
    draw=b6_below(seed,uniform?nn:total);
    if(uniform)out->profile=nearest[(uint32_t)draw];
    else {out->profile=nearest[nn-1];for(i=0;i<nn;i++){uint32_t weight=b6_le32(p->row[nearest[i]]+12);if(draw<weight){out->profile=nearest[i];break;}draw-=weight;}}
    for(i=0;i<n;i++) {
        uint32_t id=b6_le32(g+16+4*i),weight=b6_le32(p->row[id]+12);int ending=b6_i32(p->row[id]+16);
        if(ending<0)continue;
        if(!end_n)end_first=weight;else if(end_first!=weight)end_uniform=0;
        end_n++;end_sum+=(uint32_t)ending;end_total+=weight;end_out+=(uint64_t)weight*(uint32_t)ending;
    }
    out->pout=end_total?(double)end_out/(double)end_total:0.;out->desired=0;
    if(end_n)out->desired=b6_below(seed,end_uniform?end_n:end_total)<(end_uniform?end_sum:end_out);
    for(j=0;j<49;j++) {
        out->target[j]=b6_i32(p->row[out->profile]+20+4*j);
        out->spread[j]=b6_i32(g+16+4*n+4*j);
    }
    out->target[16]=out->target[17]=0;return 1;
}
#endif
