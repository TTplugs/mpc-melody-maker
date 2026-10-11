#include "mm_creation.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
static int put(FILE *f,uint32_t n){unsigned char b[4]={(unsigned char)n,(unsigned char)(n>>8),(unsigned char)(n>>16),(unsigned char)(n>>24)};return fwrite(b,1,4,f)==4;}
static int get(FILE *f,uint32_t *n){unsigned char b[4];if(fread(b,1,4,f)!=4)return 0;*n=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return 1;}
int mm_creation_write(const char *path,const mm_creation *c){
    char temporary[1400];if(snprintf(temporary,sizeof temporary,"%s.new",path)>=(int)sizeof temporary)return -1;
    FILE *f=fopen(temporary,"wb");if(!f)return -1;chmod(temporary,0600);
    int ok=fwrite("TTMMC001",1,8,f)==8&&put(f,c->favorite?1:0);
    for(int i=0;i<MM_CREATION_SETTINGS;i++){uint32_t bits;memcpy(&bits,&c->settings[i],4);ok=ok&&put(f,bits);}
    ok=ok&&fwrite(c->phrase.source_id,1,48,f)==48&&put(f,c->phrase.length);
    for(int part=0;part<3;part++){
        const mm_part *p=&c->phrase.parts[part];if(p->count>MM_MAX_NOTES)ok=0;
        ok=ok&&put(f,p->count);
        for(unsigned i=0;ok&&i<p->count;i++){const mm_note *n=&p->notes[i];ok=put(f,n->start)&&put(f,n->duration)&&put(f,n->pitch|((uint32_t)n->velocity<<8));}
    }
    if(fclose(f))ok=0;
    if(!ok||rename(temporary,path)){unlink(temporary);return -1;}return 0;
}
int mm_creation_read(const char *path,mm_creation *c){
    FILE *f=fopen(path,"rb");if(!f)return -1;memset(c,0,sizeof *c);char magic[8];uint32_t v;
    if(fread(magic,1,8,f)!=8||memcmp(magic,"TTMMC001",8)||!get(f,&v)||v>1)goto bad;
    c->favorite=(int)v;
    for(int i=0;i<MM_CREATION_SETTINGS;i++){if(!get(f,&v))goto bad;memcpy(&c->settings[i],&v,4);if(!isfinite(c->settings[i])||c->settings[i]<0||c->settings[i]>(i==1?MM_SCALE_COUNT-1:1))goto bad;}
    if(fread(c->phrase.source_id,1,48,f)!=48||!get(f,&c->phrase.length)||!c->phrase.length||c->phrase.length>MM_PPQ*32)goto bad;
    c->phrase.source_id[47]=0;
    for(int part=0;part<3;part++){
        mm_part *p=&c->phrase.parts[part];if(!get(f,&p->count)||p->count>MM_MAX_NOTES)goto bad;
        unsigned last=0;
        for(unsigned i=0;i<p->count;i++){mm_note *n=&p->notes[i];if(!get(f,&n->start)||!get(f,&n->duration)||!get(f,&v)||v>0x7f7f)goto bad;n->pitch=v&255;n->velocity=v>>8;
            if(n->pitch>127||!n->velocity||!n->duration||n->start<last||n->start>=c->phrase.length||n->duration>c->phrase.length-n->start)goto bad;
            last=n->start;}
    }
    if(!c->phrase.parts[0].count||fgetc(f)!=EOF)goto bad;
    fclose(f);return 0;
bad:fclose(f);return -1;
}
