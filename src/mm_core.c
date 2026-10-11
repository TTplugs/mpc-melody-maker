#include "mm_core.h"
#include "mm_crypto.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char id[48], style[32];
    uint8_t properties[8];
    uint32_t length,family;uint8_t variant,movement,chord_fit,flags;
    struct { uint32_t count; mm_note *notes; } parts[MM_PARTS];
} mm_clip;
struct mm_bank { unsigned count,version; mm_clip *clips; uint32_t movement[3][25]; };
static const struct { unsigned count; int notes[8]; } scales[MM_SCALE_COUNT] = {
    {7,{0,2,4,5,7,9,11}}, /* major / Ionian */
    {7,{0,2,3,5,7,8,10}}, /* natural minor / Aeolian */
    {5,{0,2,4,7,9}},      /* major pentatonic */
    {5,{0,3,5,7,10}},     /* minor pentatonic */
    {6,{0,3,5,6,7,10}},   /* minor blues */
    {7,{0,2,3,5,7,8,11}}, /* harmonic minor */
    {7,{0,2,3,5,7,9,11}}, /* ascending melodic minor, same set both directions */
    {7,{0,2,3,5,7,9,10}}, /* Dorian */
    {7,{0,1,3,5,7,8,10}}, /* Phrygian */
    {7,{0,2,4,6,7,9,11}}, /* Lydian */
    {7,{0,2,4,5,7,9,10}}, /* Mixolydian */
    {7,{0,1,3,5,6,8,10}}, /* Locrian */
    {5,{0,2,3,7,8}}, /* Hirajoshi */
    {5,{0,1,5,7,10}}, /* Insen */
    {5,{0,1,5,6,10}}, /* Iwato */
    {5,{0,2,5,7,9}}, /* Yo */
    {7,{0,2,3,6,7,8,11}}, /* Hungarian minor */
    {7,{0,1,4,5,7,8,11}}, /* Double harmonic */
    {7,{0,1,4,5,7,8,10}}, /* Phrygian dominant */
    {6,{0,2,4,6,8,10}}, /* Whole tone */
    {8,{0,2,3,5,6,8,9,11}}, /* Diminished whole/half */
    {5,{0,2,5,7,10}} /* Egyptian pentatonic */
};
static uint32_t rng(uint32_t *s) { uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; return *s=x; }
static float chance(uint32_t *s) { return (rng(s)>>8)*(1.0f/16777216.0f); }
static int read_u32(FILE *f, uint32_t *out) {
    unsigned char b[4]; if(fread(b,1,4,f)!=4) return 0;
    *out=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); return 1;
}
void mm_bank_free(mm_bank *b) {
    if(!b) return;
    for(unsigned i=0;i<b->count;i++) for(int p=0;p<MM_PARTS;p++) free(b->clips[i].parts[p].notes);
    free(b->clips); free(b);
}
unsigned mm_bank_count(const mm_bank *b) { return b?b->count:0; }
mm_bank *mm_bank_load(const char *path, char *error, unsigned cap) {
    unsigned char *memory=NULL;size_t memory_length=0;
    FILE *f=mm_library_open(path,&memory,&memory_length,error,cap); mm_bank *b=NULL; char magic[8]; uint32_t ppq,n;
    if(!f)return NULL;
    if(fread(magic,1,8,f)!=8 || (memcmp(magic,"TTMMB001",8)&&memcmp(magic,"TTMMB002",8)) || !read_u32(f,&ppq) || ppq!=MM_PPQ ||
       !read_u32(f,&n) || !n || n>100000) goto invalid;
    b=calloc(1,sizeof *b); if(!b) goto invalid;
    b->clips=calloc(n,sizeof *b->clips); if(!b->clips) goto invalid;
    b->count=n;b->version=!memcmp(magic,"TTMMB002",8)?2:1;
    for(unsigned i=0;i<n;i++) {
        mm_clip *c=&b->clips[i]; uint32_t counts[MM_PARTS];
        if(fread(c->id,1,48,f)!=48 || fread(c->style,1,32,f)!=32 || fread(c->properties,1,8,f)!=8)goto invalid;
        if(b->version==2){unsigned char meta[4];if(!read_u32(f,&c->family)||!c->family||fread(meta,1,4,f)!=4||meta[0]>3||meta[1]>1||meta[2]>2||meta[3]>1)goto invalid;c->variant=meta[0];c->movement=meta[1];c->chord_fit=meta[2];c->flags=meta[3];}
        else c->family=i+1;
        if(!read_u32(f,&c->length) || !c->length || c->length>MM_PPQ*256) goto invalid;
        c->id[47]=0; c->style[31]=0;
        if(c->properties[0]>2 || c->properties[4]>11 || c->properties[5]>1) goto invalid;
        for(int p=0;p<MM_PARTS;p++) if(!read_u32(f,&counts[p]) || counts[p]>MM_MAX_NOTES) goto invalid;
        if(!counts[0]) goto invalid;
        for(int p=0;p<MM_PARTS;p++) {
            c->parts[p].count=counts[p];
            if(!counts[p]) continue;
            c->parts[p].notes=calloc(counts[p],sizeof(mm_note)); if(!c->parts[p].notes) goto invalid;
            uint32_t previous_start=0;
            for(unsigned j=0;j<counts[p];j++) {
                mm_note *a=&c->parts[p].notes[j]; unsigned char tail[4];
                if(!read_u32(f,&a->start)||!read_u32(f,&a->duration)||fread(tail,1,4,f)!=4) goto invalid;
                a->pitch=tail[0]; a->velocity=tail[1];
                if(!a->duration || a->start>=c->length || a->duration>c->length-a->start ||
                   a->pitch>127 || !a->velocity || a->velocity>127 || a->start<previous_start) goto invalid;
                previous_start=a->start;
                if(p==0 && j) {
                    int delta=(int)a->pitch-c->parts[p].notes[j-1].pitch;
                    int prior=j>1?(int)c->parts[p].notes[j-1].pitch-c->parts[p].notes[j-2].pitch:0;
                    if(delta>=-12 && delta<=12) b->movement[(prior>0)-(prior<0)+1][delta+12]++;
                }
            }
        }
    }
    if(fgetc(f)!=EOF) goto invalid;
    mm_library_close(f,memory,memory_length); if(cap) error[0]=0; return b;
invalid:
    snprintf(error,cap,"Invalid or incomplete private library"); mm_library_close(f,memory,memory_length); mm_bank_free(b); return NULL;
}
static int snap(int pitch,int key,int scale,int lo,int hi) {
    if(pitch<lo) pitch=lo;
    if(pitch>hi) pitch=hi;
    for(int d=0;d<128;d++) for(int side=0;side<2;side++) {
        int q=pitch+(side?d:-d); if(q<lo||q>hi) continue;
        int pc=(q-key+120)%12;
        for(unsigned k=0;k<scales[scale].count;k++) if(pc==scales[scale].notes[k]) return q;
    }
    return lo;
}
/* Shared deterministic scale projection for legacy and candidate backends.
   Returns -1 for an invalid request; never changes timing or velocity. */
int mm_scale_pitch(int pitch,int key,int scale,int lo,int hi) {
    if(key<0||key>11||scale<0||scale>=MM_SCALE_COUNT||lo<0||hi>127||lo>hi)return -1;
    return snap(pitch,key,scale,lo,hi);
}
void mm_vary_phrase(mm_phrase *phrase,const mm_request *r){
    if(!phrase||r->scale<0||r->scale>=MM_SCALE_COUNT)return;
    uint32_t seed=r->seed?r->seed:1;
    /* Keep the motif's anchors, rhythm and harmonic companions. Change selected inner melody notes. */
    for(unsigned i=0;i<phrase->parts[0].count;i++){
        mm_note *n=&phrase->parts[0].notes[i];if(i%4==0||chance(&seed)>r->mutation)continue;
        int shift=(int)(rng(&seed)%5)-2;
        n->pitch=(uint8_t)snap(n->pitch+shift,r->key,r->scale,36,96);
    }
}

static const mm_clip *choose(const mm_bank *b,const mm_request *r,uint32_t *state) {
    const mm_clip *selected=NULL; int best=100000; unsigned ties=0;
    for(unsigned i=0;i<b->count;i++) {
        const mm_clip *c=&b->clips[i];
        if(b->version==2&&c->variant)continue;
        if(r->substyle && *r->substyle && strcmp(r->substyle,c->style)) continue;
        int score=abs(r->speed-(int)c->properties[0])*20;
        static const int complexity[4]={0,1,3,4};
        if(c->properties[3]) score+=abs(complexity[r->pad%4]-(int)c->properties[1])*2+abs(r->pad/4-(int)c->properties[2])*2;
        else score+=5;
        if(score<best) { best=score; selected=c; ties=1; }
        else if(score==best && rng(state)%++ties==0) selected=c;
    }
    return selected;
}
static int note_order(const void *x,const void *y) {
    const mm_note *a=x,*b=y; return (a->start>b->start)-(a->start<b->start);
}
static void finalize(mm_part *part,uint32_t length,int mono) {
    qsort(part->notes,part->count,sizeof(mm_note),note_order); unsigned kept=0;
    for(unsigned i=0;i<part->count;i++) {
        mm_note n=part->notes[i]; if(n.start>=length||!n.duration) continue;
        if(n.duration>length-n.start) n.duration=length-n.start;
        if(mono&&kept) {
            mm_note *prev=&part->notes[kept-1];
            if(prev->start==n.start) { *prev=n; continue; }
            if(prev->duration>n.start-prev->start) prev->duration=n.start-prev->start;
        }
        part->notes[kept++]=n;
    }
    part->count=kept;
}
/* A family supplies a motif, related pitch/rhythm transformations and harmonic context.
   We never splice a phrase from an unrelated family into this path. */
static int family_pitch(int pitch,const mm_clip *base,const mm_request *r,int part,int shift){
    int lo=part==2?24:part==1?36:24,hi=part==2?60:part==1?96:108;
    return snap(pitch+r->key-base->properties[4]+shift,r->key,r->scale,lo,hi);
}
static const mm_note *aligned_note(const mm_clip *clip,int part,uint32_t tick,uint32_t canonical){
    const mm_note *best=NULL;uint32_t distance=UINT32_MAX;
    for(unsigned i=0;i<clip->parts[part].count;i++){
        const mm_note *note=&clip->parts[part].notes[i];uint32_t start=(uint64_t)note->start*canonical/clip->length;
        uint32_t gap=start>tick?start-tick:tick-start;if(gap<distance){distance=gap;best=note;}
    }return best;
}
static int family_generate(const mm_bank *b,const mm_request *r,mm_phrase *out){
    uint32_t seed=r->seed?r->seed:1;const mm_clip *base=choose(b,r,&seed),*members[4]={NULL};
    if(!base)return -1;
    members[0]=base;
    for(unsigned i=0;i<b->count;i++)if(b->clips[i].family==base->family)members[b->clips[i].variant]=&b->clips[i];
    memset(out,0,sizeof *out);out->length=r->bars*4*MM_PPQ;snprintf(out->source_id,sizeof out->source_id,"%s",base->id);
    int sum=0;for(unsigned i=0;i<base->parts[0].count;i++)sum+=base->parts[0].notes[i].pitch;
    int average=sum/(int)base->parts[0].count;
    int shift=12*(int)lround(((r->octave+1)*12+r->key-average-(r->key-base->properties[4]))/12.0);
    const uint32_t bar=4*MM_PPQ,canonical=((base->length+bar-1)/bar)*bar;
    /* Companion notes stay linked to the motif's original harmonic context. */
    for(int part=1;part<3;part++)for(uint32_t cycle=0;cycle<out->length;cycle+=canonical){
        for(unsigned i=0;i<base->parts[part].count&&out->parts[part].count<MM_MAX_NOTES;i++){
            mm_note note=base->parts[part].notes[i];note.start+=cycle;if(note.start>=out->length)continue;
            note.pitch=(uint8_t)family_pitch(note.pitch,base,r,part,0);out->parts[part].notes[out->parts[part].count++]=note;
        }
    }
    unsigned previous_variant=0;
    for(uint32_t begin=0;begin<out->length;begin+=bar){
        uint32_t local=begin%canonical,end=local+bar;unsigned variant=0;
        if(chance(&seed)<r->recombination){unsigned candidates[3],count=0;
            if(members[3])candidates[count++]=3;
            if(r->pad/4<=1&&members[2])candidates[count++]=2;
            if(r->pad/4>=2&&members[1])candidates[count++]=1;
            if(count)variant=candidates[rng(&seed)%count];
        }
        int held=0;for(unsigned n=0;n<base->parts[0].count;n++){const mm_note *note=&base->parts[0].notes[n];if(note->start<local&&note->start+note->duration>local){held=1;break;}}
        if(held)variant=previous_variant;
        previous_variant=variant;
        const mm_clip *donor=members[variant]?members[variant]:base;
        if(variant==3){
            unsigned base_indices[MM_MAX_NOTES],base_count=0,donor_count=0,rank=0;
            for(unsigned i=0;i<base->parts[0].count;i++)if(base->parts[0].notes[i].start>=local&&base->parts[0].notes[i].start<end)base_indices[base_count++]=i;
            for(unsigned i=0;i<donor->parts[0].count;i++){uint32_t tick=(uint64_t)donor->parts[0].notes[i].start*canonical/donor->length;if(tick>=local&&tick<end)donor_count++;}
            for(unsigned i=0;i<donor->parts[0].count&&out->parts[0].count<MM_MAX_NOTES;i++){
                mm_note note=donor->parts[0].notes[i];uint32_t tick=(uint64_t)note.start*canonical/donor->length;
                if(tick<local||tick>=end)continue;
                /* Rhythm donor contributes timing only; pitches follow the base contour. */
                if(!base_count)continue;
                unsigned mapped=(uint64_t)rank++*base_count/(donor_count?donor_count:1);if(mapped>=base_count)mapped=base_count-1;
                const mm_note *pitch=&base->parts[0].notes[base_indices[mapped]];
                note.start=begin+tick-local;note.duration=(uint64_t)note.duration*canonical/donor->length;
                note.pitch=(uint8_t)family_pitch(pitch->pitch,base,r,0,shift);
                out->parts[0].notes[out->parts[0].count++]=note;
            }
        }else{
            for(unsigned i=0;i<base->parts[0].count&&out->parts[0].count<MM_MAX_NOTES;i++){
                mm_note note=base->parts[0].notes[i];if(note.start<local||note.start>=end)continue;
                const mm_note *learned=variant?aligned_note(donor,0,note.start,canonical):NULL;
                int pitch=note.pitch;
                /* Light/Dark donor contributes pitch only, never its changed timing. */
                if(learned)pitch=learned->pitch;
                /* Learn small local changes from family members; retain phrase endpoints. */
                if(!variant&&i>0&&i+1<base->parts[0].count&&note.start%bar&&chance(&seed)<r->mutation){
                    unsigned kind=r->pad/4<=1?2:1;
                    if(members[kind]){const mm_note *related=aligned_note(members[kind],0,note.start,canonical);if(related){int delta=(int)related->pitch-note.pitch;int limit=base->movement?5:2;if(delta>limit)delta=limit;if(delta< -limit)delta=-limit;pitch+=delta;}}else{
                        /* For incomplete families, reuse neighbouring motif tones on weak beats. */
                        if(note.start%MM_PPQ){unsigned neighbour=(rng(&seed)&1)?i-1:i+1;int delta=(int)base->parts[0].notes[neighbour].pitch-note.pitch;int limit=base->movement?5:2;if(delta>limit)delta=limit;if(delta< -limit)delta=-limit;pitch+=delta;}
                    }
                }
                note.start=begin+note.start-local;note.pitch=(uint8_t)family_pitch(pitch,base,r,0,shift);
                out->parts[0].notes[out->parts[0].count++]=note;
            }
        }
    }
    for(int part=0;part<3;part++)finalize(&out->parts[part],out->length,part!=1);
    if(!out->parts[0].count)return -1;
    /* Keep existing chord context. Only a missing accompaniment uses the fallback. */
    for(int measure=0;measure<r->bars;measure++){
        uint32_t tick=measure*bar;int degree=0;
        for(unsigned n=0;n<out->parts[0].count;n++)if(out->parts[0].notes[n].start>=tick&&out->parts[0].notes[n].start<tick+bar){int pc=(out->parts[0].notes[n].pitch-r->key+120)%12;for(unsigned d=0;d<scales[r->scale].count;d++)if(scales[r->scale].notes[d]==pc)degree=d;break;}
        if(!base->parts[1].count)for(int k=0;k<3;k++){int d=degree+2*k,steps=scales[r->scale].count;out->parts[1].notes[out->parts[1].count++]=(mm_note){tick,bar-80,48+r->key+scales[r->scale].notes[d%steps]+12*(d/steps),74};}
        if(!base->parts[2].count)out->parts[2].notes[out->parts[2].count++]=(mm_note){tick,2*MM_PPQ,36+r->key+scales[r->scale].notes[degree],88};
    }
    /* Chord Fit on metric anchors; preserve passing notes on weak positions. */
    if(base->chord_fit<2)for(unsigned n=0;n<out->parts[0].count;n++){
        mm_note *note=&out->parts[0].notes[n];if(note->start%MM_PPQ)continue;
        int best=note->pitch,distance=128;
        for(unsigned j=0;j<out->parts[1].count;j++){const mm_note *chord=&out->parts[1].notes[j];if(note->start<chord->start||note->start>=chord->start+chord->duration)continue;
            for(int octave=-5;octave<=5;octave++){int pitch=chord->pitch+octave*12;if(pitch<24||pitch>108)continue;int gap=abs(pitch-note->pitch);if(gap<distance){distance=gap;best=pitch;}}
        }
        if(distance<=(base->chord_fit==0?12:2))note->pitch=best;
    }
    /* Do not return an entire stored melody, even after transposition. Preserve the
       first/last motif anchors when adjusting a candidate that matches a member. */
    for(unsigned attempt=0;attempt<16;attempt++){
        int copied=0;mm_part *melody=&out->parts[0];
        for(unsigned clip=0;clip<b->count&&!copied;clip++){
            const mm_clip *source=&b->clips[clip];if(source->parts[0].count!=melody->count)continue;
            int delta=(int)melody->notes[0].pitch-source->parts[0].notes[0].pitch,same=1;
            for(unsigned n=0;n<melody->count;n++){const mm_note *old=&source->parts[0].notes[n],*fresh=&melody->notes[n];if((int)fresh->pitch-old->pitch!=delta||fresh->start!=old->start||fresh->duration!=old->duration){same=0;break;}}
            copied=same;
        }
        if(!copied)return 0;
        if(attempt==15)return -1;
        unsigned index=melody->count>2?1+rng(&seed)%(melody->count-2):0;mm_note *note=&melody->notes[index];
        int direction=(attempt&1)?-1:1,pitch=snap(note->pitch+direction*2,r->key,r->scale,24,108);if(pitch==note->pitch)pitch=snap(note->pitch+direction*4,r->key,r->scale,24,108);note->pitch=pitch;
    }return -1;
}

int mm_vary_family(const mm_bank *b,mm_phrase *phrase,const mm_request *r){
    if(!b||b->version!=2)return -1;
    const mm_clip *base=NULL,*donor=NULL;
    for(unsigned i=0;i<b->count;i++)if(!strcmp(b->clips[i].id,phrase->source_id)){base=&b->clips[i];break;}
    if(!base)return -1;
    unsigned kind=r->pad/4<=1?2:1;uint32_t seed=r->seed?r->seed:1;
    for(unsigned i=0;i<b->count;i++)if(b->clips[i].family==base->family&&b->clips[i].variant==kind){donor=&b->clips[i];break;}
    if(!donor)return 0;
    for(unsigned i=1;i+1<phrase->parts[0].count;i++){
        mm_note *note=&phrase->parts[0].notes[i];if(note->start%(4*MM_PPQ)==0||chance(&seed)>r->mutation)continue;
        uint32_t tick=note->start%base->length;
        const mm_note *anchor=aligned_note(base,0,tick,base->length),*learned=aligned_note(donor,0,tick,base->length);
        if(!anchor||!learned)continue;
        int delta=(int)learned->pitch-anchor->pitch;int limit=base->movement?5:2;if(delta>limit)delta=limit;if(delta< -limit)delta=-limit;
        note->pitch=(uint8_t)snap(note->pitch+delta,r->key,r->scale,24,108);
    }
    return 0;
}

int mm_generate(const mm_bank *b,const mm_request *r,mm_phrase *out) {
    if(!b||!r||!out||r->key<0||r->key>11||r->scale<0||r->scale>=MM_SCALE_COUNT||r->bars<1||r->bars>8||
       r->speed<0||r->speed>2||r->pad<0||r->pad>15||r->octave<2||r->octave>6||
       !isfinite(r->mutation)||!isfinite(r->recombination)||r->mutation<0||r->mutation>1||r->recombination<0||r->recombination>1) return -1;
    return family_generate(b,r,out);
}

typedef struct { uint32_t tick; uint8_t status,pitch,velocity; } event;
static int ev_order(const void *x,const void *y) {
    const event *a=x,*b=y;
    if(a->tick!=b->tick)return a->tick<b->tick?-1:1;
    return (a->status&0xf0)-(b->status&0xf0);
}
static unsigned put_vlq(unsigned char *buf,uint32_t n) {
    unsigned char tmp[5]; unsigned count=0;
    do {tmp[count++]=n&127;n>>=7;}while(n);
    for(unsigned i=0;i<count;i++)buf[i]=tmp[count-i-1]|(i+1<count?128:0);
    return count;
}
static int put32(FILE *f,uint32_t n) {
    unsigned char b[4]={(uint8_t)(n>>24),(uint8_t)(n>>16),(uint8_t)(n>>8),(uint8_t)n};return fwrite(b,1,4,f)==4;
}
int mm_write_midi(const mm_phrase *phrase,const char *path,double bpm) {
    const int channels[3]={2,3,4};
    return mm_write_midi_channels(phrase,path,bpm,channels);
}
int mm_write_midi_channels(const mm_phrase *phrase,const char *path,double bpm,const int channels[3]) {
    if(!phrase||!path||!isfinite(bpm)||bpm<20||bpm>400||!phrase->length)return -1;
    if(!channels)return -1;
    for(int p=0;p<MM_PARTS;p++)if(channels[p]<1||channels[p]>16)return -1;
    for(int p=0;p<MM_PARTS;p++) {
        if(phrase->parts[p].count>MM_MAX_NOTES)return -1;
        for(unsigned i=0;i<phrase->parts[p].count;i++) {
            const mm_note *n=&phrase->parts[p].notes[i];
            if(n->pitch>127||!n->velocity||n->velocity>127||!n->duration||n->start>=phrase->length||n->duration>phrase->length-n->start)return -1;
        }
    }
    event *events=calloc(MM_MAX_NOTES*2,sizeof *events); unsigned char *buffer=malloc(MM_MAX_NOTES*16+64);
    char *temporary=malloc(strlen(path)+5); if(!events||!buffer||!temporary){free(events);free(buffer);free(temporary);return -1;}
    sprintf(temporary,"%s.new",path); FILE *f=fopen(temporary,"wb"); int ok=f!=NULL;
    const unsigned char header[]={'M','T','h','d',0,0,0,6,0,1,0,3,3,192};
    if(ok)ok=fwrite(header,1,sizeof header,f)==sizeof header;
    for(int p=0;p<MM_PARTS&&ok;p++) {
        unsigned count=0,pos=0; uint32_t current=0;
        for(unsigned i=0;i<phrase->parts[p].count;i++) {
            const mm_note *n=&phrase->parts[p].notes[i];
            events[count++]=(event){n->start,(uint8_t)(0x90+channels[p]-1),n->pitch,n->velocity};
            events[count++]=(event){n->start+n->duration,(uint8_t)(0x80+channels[p]-1),n->pitch,0};
        }
        qsort(events,count,sizeof *events,ev_order);
        if(!p) {
            uint32_t tempo=(uint32_t)lround(60000000.0/bpm);
            unsigned char meta[]={0,255,81,3,(uint8_t)(tempo>>16),(uint8_t)(tempo>>8),(uint8_t)tempo};
            memcpy(buffer,meta,sizeof meta);pos=sizeof meta;
        }
        for(unsigned i=0;i<count;i++) {
            pos+=put_vlq(buffer+pos,events[i].tick-current);current=events[i].tick;
            buffer[pos++]=events[i].status;buffer[pos++]=events[i].pitch;buffer[pos++]=events[i].velocity;
        }
        pos+=put_vlq(buffer+pos,phrase->length-current);buffer[pos++]=255;buffer[pos++]=47;buffer[pos++]=0;
        ok=fwrite("MTrk",1,4,f)==4&&put32(f,pos)&&fwrite(buffer,1,pos,f)==pos;
    }
    if(f&&fclose(f))ok=0;
    if(ok&&rename(temporary,path))ok=0;
    if(!ok)remove(temporary);
    free(events);free(buffer);free(temporary);return ok?0:-1;
}
