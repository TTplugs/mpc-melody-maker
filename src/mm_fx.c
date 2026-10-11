#include "mm_fx.h"
#include <stdlib.h>
#include <math.h>
static int compare(const void *a,const void *b){const mm_note *x=a,*y=b;return x->start<y->start?-1:x->start>y->start?1:(int)x->pitch-(int)y->pitch;}
static unsigned noise(unsigned n){n^=n>>16;n*=0x7feb352d;n^=n>>15;n*=0x846ca68b;return n^(n>>16);}
static void add(mm_part *p,mm_note n,unsigned length){if(n.start>=length||!n.duration||p->count>=MM_MAX_NOTES)return;if(n.duration>length-n.start)n.duration=length-n.start;if(n.velocity<1)n.velocity=1;p->notes[p->count++]=n;}
void mm_fx_apply(const mm_phrase *dry,mm_phrase *wet,const mm_fx chain[MM_FX_SLOTS]){
    *wet=*dry;
    for(int slot=0;slot<MM_FX_SLOTS;slot++){
        const mm_fx *fx=&chain[slot];if(!fx->enabled||!fx->type)continue;
        float amount=fminf(1,fmaxf(0,fx->amount));
        for(int part=0;part<3;part++){
            if(fx->target&&fx->target!=part+1)continue;
            if((fx->type==6||fx->type==7||fx->type==10)&&part!=1)continue;
            if(fx->type==8&&part!=2)continue;
            mm_part source=wet->parts[part],*out=&wet->parts[part];out->count=0;
            unsigned step=amount<.34f?480:amount<.67f?240:120;
            for(unsigned i=0;i<source.count;i++){
                mm_note n=source.notes[i];unsigned random=noise(n.start+n.pitch*127u+i*7919u+slot*31u);
                if(fx->type==9&&random%1000<(unsigned)(amount*800))continue;
                if(fx->type==1&&(n.start/480)%2)n.start+=(unsigned)(amount*240);
                if(fx->type==2)n.duration=(unsigned)fmaxf(1,n.duration*(1-amount*.9f));
                if(fx->type==3||fx->type==4){int velocity=n.velocity+(int)((random>>8)%61)-30;velocity=(int)lroundf(n.velocity+(velocity-n.velocity)*amount);n.velocity=(unsigned char)(velocity<1?1:velocity>127?127:velocity);}
                if(fx->type==4){int start=(int)n.start+(int)(((int)(random%161)-80)*amount);n.start=start<0?0:(unsigned)start;}
                if(fx->type==10){unsigned rank=0;for(unsigned j=0;j<source.count;j++)if(source.notes[j].start==source.notes[i].start&&source.notes[j].pitch<n.pitch)rank++;n.start+=(unsigned)(rank*amount*80);}
                if(fx->type==11){int velocity=n.velocity+((n.start%960)<120?1:-1)*(int)(amount*25);n.velocity=(unsigned char)(velocity<1?1:velocity>127?127:velocity);}
                if(fx->type==6||fx->type==7){
                    unsigned first=i;while(first&&source.notes[first-1].start==n.start)first--;
                    if(first!=i)continue;
                    unsigned last=i+1;while(last<source.count&&source.notes[last].start==n.start)last++;
                    for(unsigned pos=0,t=n.start;t<n.start+n.duration&&t<wet->length;t+=step,pos++){
                        unsigned rank=pos%(last-first);if(fx->type==7)rank=last-first-1-rank;
                        mm_note arp=source.notes[first+rank];arp.start=t;arp.duration=step*3/4;add(out,arp,wet->length);
                    }continue;
                }
                if(fx->type==8){
                    for(unsigned pos=0,t=n.start;t<n.start+n.duration&&t<wet->length;t+=step,pos++){
                        mm_note bass=n;bass.start=t;bass.duration=step*3/4;
                        if(pos%4==3&&bass.pitch<=115)bass.pitch+=12;
                        add(out,bass,wet->length);
                    }continue;
                }
                add(out,n,wet->length);
                if(fx->type==5){unsigned repeats=1+(unsigned)(amount*3);for(unsigned repeat=1;repeat<=repeats;repeat++){
                    mm_note echo=n;echo.start+=step*repeat;echo.duration=echo.duration<step?echo.duration:step*3/4;
                    echo.velocity=(unsigned char)(n.velocity/(repeat+1));add(out,echo,wet->length);
                }}
            }
            qsort(out->notes,out->count,sizeof(mm_note),compare);
        }
    }
    /* Melody and bass remain single voices after timing and repeat effects. */
    for(int part=0;part<3;part++)if(part!=1){
        mm_part *voice=&wet->parts[part];qsort(voice->notes,voice->count,sizeof(mm_note),compare);
        unsigned kept=0;
        for(unsigned i=0;i<voice->count;i++){
            mm_note note=voice->notes[i];if(!note.duration||note.start>=wet->length)continue;
            if(kept){mm_note *previous=&voice->notes[kept-1];
                if(previous->start==note.start){*previous=note;continue;}
                unsigned gap=note.start-previous->start;if(previous->duration>gap)previous->duration=gap;
            }
            voice->notes[kept++]=note;
        }
        voice->count=kept;
    }

}
