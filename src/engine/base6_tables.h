#ifndef TT_BASE6_TABLES_H
#define TT_BASE6_TABLES_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    unsigned char *data;
    size_t size, transitions_size,register_size,profiles_size;
    const unsigned char *transitions,*reg,*profiles,*dispersion;
    uint32_t *offsets,rows;
} b6_tables;
static inline uint32_t b6_le32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline int32_t b6_i32(const unsigned char *p) {
    uint32_t u=b6_le32(p);
    return (int32_t)(u<=INT32_MAX?(int64_t)u:(int64_t)u-4294967296LL);
}
static inline uint64_t b6_le64(const unsigned char *p) {
    return (uint64_t)b6_le32(p)|((uint64_t)b6_le32(p+4)<<32);
}
static inline int b6_compare(const unsigned char *p,const int32_t *key,unsigned n) {
    unsigned i;
    for(i=0;i<n;i++){int32_t v=b6_i32(p+4*i);if(v!=key[i])return v<key[i]?-1:1;}
    return 0;
}
static inline void b6_tables_close(b6_tables *m) {
    free(m->offsets);free(m->data);memset(m,0,sizeof *m);
}
/* Call on a zero-initialized/unloaded model, outside audio. Owns its bytes.
 * File size capped at 64 MiB. Does not load companion theory/source library.
 */
static inline int b6_tables_load(b6_tables *m,const char *path) {
    FILE *f;long size;size_t pos=8;unsigned seen=0;uint32_t i;
    memset(m,0,sizeof *m);
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END) || (size=ftell(f))<8 || size>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    m->data=(unsigned char *)malloc((size_t)size);m->size=(size_t)size;
    if(!m->data){fclose(f);return 0;}
    if(fread(m->data,1,m->size,f)!=m->size){fclose(f);goto bad;}
    if(fclose(f))goto bad;
    if(memcmp(m->data,"TTMM6TB1",8))goto bad;
    while(pos<m->size) {
        uint32_t n;uint64_t bytes;const unsigned char *name,*section;unsigned bit=0;
        if(m->size-pos<12)goto bad;
        n=b6_le32(m->data+pos);bytes=b6_le64(m->data+pos+4);pos+=12;
        if(n>m->size-pos)goto bad;
        name=m->data+pos;pos+=n;
        if(bytes>m->size-pos)goto bad;
        section=m->data+pos;pos+=(size_t)bytes;
        if(n==11&&!memcmp(name,"transitions",11)){bit=1;m->transitions=section;m->transitions_size=(size_t)bytes;}
        else if(n==8&&!memcmp(name,"register",8)){bit=2;m->reg=section;m->register_size=(size_t)bytes;}
        else if(n==8&&!memcmp(name,"profiles",8)){bit=4;m->profiles=section;m->profiles_size=(size_t)bytes;}
        else if(n==10&&!memcmp(name,"dispersion",10)){bit=8;m->dispersion=section;if(bytes!=16)goto bad;}
        else goto bad;
        if(seen&bit)goto bad;
        seen|=bit;
    }
    if(seen!=15 || m->transitions_size<4 || m->register_size<4 || m->profiles_size<4)goto bad;
    m->rows=b6_le32(m->transitions);
    if(!m->rows || m->rows>(m->transitions_size-4)/44)goto bad;
    m->offsets=(uint32_t *)calloc(m->rows,sizeof *m->offsets);if(!m->offsets)goto bad;
    pos=4;
    for(i=0;i<m->rows;i++) {
        uint32_t count,j;const unsigned char *row;int32_t key[10];unsigned k;
        if(m->transitions_size-pos<44)goto bad;
        row=m->transitions+pos;m->offsets[i]=(uint32_t)pos;
        for(k=0;k<10;k++)key[k]=b6_i32(row+4*k);
        if(key[3]<0 || key[3]>2)goto bad;
        if(i && b6_compare(m->transitions+m->offsets[i-1],key,10)>=0)goto bad;
        count=b6_le32(row+40);pos+=44;
        if(!count || count>(m->transitions_size-pos)/20)goto bad;
        for(j=0;j<count;j++) {
            const unsigned char *entry=m->transitions+pos+20*j;int32_t token[3];
            for(k=0;k<3;k++)token[k]=b6_i32(entry+4*k);
            if(!b6_le64(entry+12) || (j&&b6_compare(entry-20,token,3)>=0))goto bad;
        }
        pos+=(size_t)count*20;
    }
    if(pos!=m->transitions_size)goto bad;
    {
        uint32_t count=b6_le32(m->reg);
        if((uint64_t)count*12+4!=m->register_size)goto bad;
        for(i=0;i<count;i++) {
            const unsigned char *r=m->reg+4+12*i;
            if(!b6_le64(r+4) || (i&&b6_i32(r-12)>=b6_i32(r)))goto bad;
        }
    }
    /* Profile records have UTF-8 identifier + fixed 216-byte payload.
     * Validate framing here; semantic interpretation is a separate stage. */
    pos=4;
    for(i=0;i<b6_le32(m->profiles);i++) {
        uint32_t len;
        if(m->profiles_size-pos<4)goto bad;
        len=b6_le32(m->profiles+pos);pos+=4;
        if(!len || len>m->profiles_size-pos)goto bad;
        pos+=len;if(m->profiles_size-pos<216)goto bad;pos+=216;
    }
    if(pos!=m->profiles_size)goto bad;
    return 1;
bad:b6_tables_close(m);return 0;
}
static inline const unsigned char *b6_find_row(const b6_tables *m,const int32_t key[10]) {
    uint32_t lo=0,hi=m->rows;
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2;const unsigned char *r=m->transitions+m->offsets[mid];int c=b6_compare(r,key,10);if(c<0)lo=mid+1;else if(c>0)hi=mid;else return r;}
    return NULL;
}
static inline uint64_t b6_token_count(const unsigned char *row,const int32_t token[3]) {
    uint32_t lo=0,hi;
    if(!row)return 0;
    hi=b6_le32(row+40);
    while(lo<hi){uint32_t mid=lo+(hi-lo)/2;const unsigned char *r=row+44+20*mid;int c=b6_compare(r,token,3);if(c<0)lo=mid+1;else if(c>0)hi=mid;else return b6_le64(r+12);}
    return 0;
}
#endif
