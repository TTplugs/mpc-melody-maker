#ifndef TT_BASE6_SAMPLER_H
#define TT_BASE6_SAMPLER_H
#include "base6_tables.h"
#include "base6_arithmetic.h"
static inline uint32_t b6_next(uint32_t *s) {
    uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;
}
static inline uint64_t b6_below(uint32_t *s,uint64_t n) {
    uint64_t r,threshold=(UINT64_MAX%n+1)%n;
    do {uint32_t hi=b6_next(s),lo=b6_next(s);r=((uint64_t)hi<<32)|lo;}while(r<threshold);
    return r%n;
}
static inline int b6_degree(int key,int mode,int pitch) {
    static const int modes[5][7]={{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,3,5,7,8,11},{0,2,4,5,7,9,11}};
    int x=pitch-key,o=x/12,pc=x%12,i,best=0,distance=99;
    if(pc<0){pc+=12;o--;}
    for(i=0;i<7;i++){int d=abs(modes[mode][i]-pc);if(d<distance){distance=d;best=i;}}
    return o*7+best;
}
static inline int b6_mod12(int x){int r=x%12;return r<0?r+12:r;}
static inline int b6_row_total(const unsigned char *r,uint32_t *out) {
    uint64_t sum=0;uint32_t i;
    if(r)for(i=0;i<b6_le32(r+40);i++) {
        uint64_t n=b6_le64(r+44+20*i+12);
        if(n>UINT32_MAX-sum)return 0;
        sum+=n;
    }
    *out=(uint32_t)sum;return 1;
}
static inline uint64_t b6_register(const b6_tables *m,int offset) {
    uint32_t lo=0,hi=b6_le32(m->reg);
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2;const unsigned char *r=m->reg+4+12*mid;int v=b6_i32(r);if(v<offset)lo=mid+1;else if(v>offset)hi=mid;else return b6_le64(r+4);}
    return 0;
}
/* Exactly one original Part6 note draw. Caller owns candidate state/history.
 * seed must already be initialized to seed-or-0x9e3779b9, as Python RNG.
 * history: older token then newer token; each token has three signed fields.
 * allow[128] applies the existing optional pitch policy, not new restrictions.
 */
static inline int b6_draw(const b6_tables *m,uint32_t smooth,const int32_t co[3],
                          int key,int mode,int prev,int root,int bass,int center,
                          const int32_t history[6],const unsigned char allow[128],
                          uint32_t *seed,uint64_t weights[128],int *pitch) {
    const unsigned char *rows[3]={0,0,0};uint32_t n0,n1;int order,p;
    uint64_t total=0,choice;
    if(mode<0||mode>4||key<0||key>11)return 0;
    for(order=0;order<3;order++) {
        int fallback;
        for(fallback=0;fallback<4;fallback++) {
            int32_t query[10];int j;
            query[0]=fallback==3?-1:co[0];query[1]=fallback>=2?-1:co[1];query[2]=fallback>=1?-1:co[2];query[3]=order;
            for(j=0;j<6;j++)query[4+j]=j<order*3?history[6-order*3+j]:-999;
            rows[order]=b6_find_row(m,query);if(rows[order])break;
        }
    }
    if(!b6_row_total(rows[0],&n0)||!b6_row_total(rows[1],&n1))return 0;
    for(p=0;p<128;p++) {
        int32_t token[3];uint64_t z,o,t,r;
        token[0]=prev<0?0:b6_degree(key,mode,p)-b6_degree(key,mode,prev);
        token[1]=b6_mod12(p-(root<0?key:root));token[2]=bass<0?12:b6_mod12(p-bass);
        z=b6_token_count(rows[0],token);o=b6_token_count(rows[1],token);t=b6_token_count(rows[2],token);r=b6_register(m,p-center);
        if(z>UINT32_MAX||o>UINT32_MAX||t>UINT32_MAX||r>UINT32_MAX)return 0;
        if(!b6_weight((uint32_t)t,(uint32_t)o,(uint32_t)z,n1,n0,(uint32_t)r,smooth,&weights[p]))return 0;
        if(allow&&!allow[p])weights[p]=0;
        if(weights[p]>UINT64_MAX-total)return 0;
        total+=weights[p];
    }
    if(!total)return 0;
    choice=b6_below(seed,total);
    for(p=0;p<128;p++){if(choice<weights[p]){*pitch=p;return 1;}choice-=weights[p];}
    return 0;
}
#endif
