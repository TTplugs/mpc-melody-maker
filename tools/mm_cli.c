#include "mm_core.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    if(argc<3){fprintf(stderr,"Usage: %s private-bank output.mid [seed] [pad 0..15] [scale 0..21]\n",argv[0]);return 2;}
    char error[128]; mm_bank *bank=mm_bank_load(argv[1],error,sizeof error);
    if(!bank){fprintf(stderr,"%s\n",error);return 1;}
    mm_request request={.seed=argc>3?(uint32_t)strtoul(argv[3],NULL,10):1,.key=9,.scale=argc>5?atoi(argv[5]):MM_MINOR,.bars=4,
                        .speed=1,.pad=argc>4?atoi(argv[4]):5,.octave=4,.mutation=.25f,.recombination=.35f};
    mm_phrase *phrase=calloc(1,sizeof *phrase);int rc=phrase?mm_generate(bank,&request,phrase):-1;
    if(!rc)rc=mm_write_midi(phrase,argv[2],90);
    if(!rc)printf("Hip Hop: library=%u source=%s notes=%u/%u/%u\n",mm_bank_count(bank),phrase->source_id,
                  phrase->parts[0].count,phrase->parts[1].count,phrase->parts[2].count);
    free(phrase);mm_bank_free(bank);return rc?1:0;
}
