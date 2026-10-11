#ifndef TT_BASE6_ARITHMETIC_H
#define TT_BASE6_ARITHMETIC_H
#include <stdint.h>
/* ARM32-portable exact nonnegative arithmetic; no __int128 or floating point.
 * Returns 0 on overflow/invalid input, 1 on success. Never silently wraps.
 */
typedef struct {uint32_t v[3];} b6_u96;
static inline b6_u96 b6_from32(uint32_t x) {
    b6_u96 a={{0,0,0}};a.v[0]=x;return a;
}
static inline int b6_mul32(b6_u96 *a,uint32_t x) {
    uint64_t carry=0;unsigned i;
    for(i=0;i<3;i++){uint64_t p=(uint64_t)a->v[i]*x+carry;a->v[i]=(uint32_t)p;carry=p>>32;}
    return carry==0;
}
static inline int b6_add(b6_u96 *a,b6_u96 b) {
    uint64_t carry=0;unsigned i;
    for(i=0;i<3;i++){uint64_t p=(uint64_t)a->v[i]+b.v[i]+carry;a->v[i]=(uint32_t)p;carry=p>>32;}
    return carry==0;
}
static inline int b6_div64(b6_u96 a,uint32_t divisor,uint64_t *out) {
    uint64_t rem=0;int i;
    if(!divisor || !out)return 0;
    for(i=2;i>=0;i--){uint64_t p=(rem<<32)|a.v[i];a.v[i]=(uint32_t)(p/divisor);rem=p%divisor;}
    if(a.v[2])return 0;
    *out=((uint64_t)a.v[1]<<32)|a.v[0];return 1;
}
/* Original Part6 order-2 smoothing formula; weighted smoothing=100,
 * original smoothing=1. Division occurs only AFTER all exact products.
 */
static inline int b6_weight(uint32_t t,uint32_t o,uint32_t z,
                            uint32_t total1,uint32_t total0,uint32_t reg,
                            uint32_t smooth,uint64_t *out) {
    b6_u96 a,b;uint32_t s2,s3;
    if((smooth!=1 && smooth!=100) || total1>UINT32_MAX-smooth ||
       total0>UINT32_MAX-smooth || reg>UINT32_MAX-smooth)return 0;
    s2=smooth*smooth;s3=s2*smooth;
    a=b6_from32(t);
    if(!b6_mul32(&a,total1+smooth)||!b6_mul32(&a,total0+smooth))return 0;
    b=b6_from32(o);
    if(!b6_mul32(&b,smooth)||!b6_mul32(&b,total0+smooth)||!b6_add(&a,b))return 0;
    b=b6_from32(z);
    if(!b6_mul32(&b,s2)||!b6_add(&a,b)||!b6_add(&a,b6_from32(s3)))return 0;
    if(!b6_mul32(&a,reg+smooth)||!b6_div64(a,smooth==100?100000000u:1u,out))return 0;
    if(!*out)*out=1;
    return 1;
}
#endif
