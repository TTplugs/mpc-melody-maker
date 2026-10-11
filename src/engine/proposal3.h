#ifndef TT_MUTATION3_H
#define TT_MUTATION3_H
#include <stdint.h>
#include <stddef.h>
#define M3_NOTES 128
#define M3_PATTERNS 5728
#define M3_ROWS (2*4*3*13*31*12)
#define M3_SMALL (2*4*3)
#define M3_NO_COMPATIBLE_RHYTHM 1
#define M3_NEAREST_N 2
#define M3_MISSING_HARMONY 4
typedef struct { int32_t start,duration,pitch,velocity; uint32_t chord; int32_t root,bass; } m3_note;
typedef struct {int32_t start,end,count,pitches[16];uint32_t mask;} m3_chord;
typedef struct { int32_t count,length,speed,movement,groove,key,mode; m3_note notes[M3_NOTES];int32_t chord_count;m3_chord chords[512]; } m3_phrase;
typedef struct { uint32_t seed; int32_t operation,intensity,dark,candidates,passes; } m3_params;
typedef struct { int32_t count,length,speed,groove; int32_t starts[M3_NOTES],durations[M3_NOTES]; } m3_pattern;
typedef struct {
 uint16_t trans[M3_ROWS],reverse[M3_ROWS];
 uint16_t reg[M3_SMALL*255],jump[M3_SMALL*255],bass[M3_SMALL*13],hit[M3_SMALL*2];
 int32_t targets[M3_SMALL*12],scales[M3_SMALL*12];
 uint16_t tension[5*12*12],distance[4096*12];
 int32_t expectation[4],rhythm_targets[4*3*4],pattern_count;
 m3_pattern patterns[M3_PATTERNS];
 uint32_t back[M3_NOTES][12],emission[M3_NOTES][12];
 m3_phrase work; uint32_t weights[128]; int32_t mask[128];
} m3_context;
uint32_t m3_context_size(void);
uint32_t m3_pad_seed(uint32_t seed,uint32_t touches);
int m3_load(m3_context *c,const char *path);
int m3_generate(m3_context *c,const m3_phrase *base,const m3_params *params,m3_phrase *out);
int m3_score(m3_context *c,const m3_phrase *base,const m3_phrase *p,int dark);
void m3_features(m3_context *c,const m3_phrase *base,const m3_phrase *p,int32_t *features);
#endif
