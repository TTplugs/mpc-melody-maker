#ifndef MM_CORE_H
#define MM_CORE_H
#include <stdint.h>
#define MM_PPQ 960u
#define MM_MAX_NOTES 2048u
#define MM_PARTS 3
/* Stable target-scale IDs. Source bank key estimates remain major/minor. */
enum mm_scale {
    MM_MAJOR, MM_MINOR, MM_MAJOR_PENTATONIC, MM_MINOR_PENTATONIC, MM_MINOR_BLUES,
    MM_HARMONIC_MINOR, MM_MELODIC_MINOR, MM_DORIAN, MM_PHRYGIAN, MM_LYDIAN,
    MM_MIXOLYDIAN, MM_LOCRIAN, MM_HIRAJOSHI, MM_INSEN, MM_IWATO, MM_YO,
    MM_HUNGARIAN_MINOR, MM_DOUBLE_HARMONIC, MM_PHRYGIAN_DOMINANT, MM_WHOLE_TONE,
    MM_DIMINISHED_WH, MM_EGYPTIAN_PENTATONIC, MM_SCALE_COUNT
};
typedef struct { uint32_t start, duration; uint8_t pitch, velocity; } mm_note;
typedef struct { uint32_t count; mm_note notes[MM_MAX_NOTES]; } mm_part;
typedef struct {
    char source_id[48];
    uint32_t length;
    mm_part parts[MM_PARTS];
} mm_phrase;
typedef struct mm_bank mm_bank;
typedef struct {
    uint32_t seed;
    int key, scale, bars, speed, pad, octave;
    float mutation, recombination;
    const char *substyle;
} mm_request;
/* No I/O in generation. Each request owns its RNG, so results are repeatable. */
mm_bank *mm_bank_load(const char *path, char *error, unsigned error_size);
void mm_bank_free(mm_bank *bank);
unsigned mm_bank_count(const mm_bank *bank);
int mm_scale_pitch(int pitch, int key, int scale, int lo, int hi);
int mm_generate(const mm_bank *bank, const mm_request *request, mm_phrase *out);
void mm_vary_phrase(mm_phrase *phrase, const mm_request *request);
int mm_write_midi(const mm_phrase *phrase, const char *path, double bpm);
/* Channels are human-readable 1..16; the simple writer defaults to 2/3/4. */
int mm_write_midi_channels(const mm_phrase *phrase, const char *path, double bpm, const int channels[3]);

int mm_vary_family(const mm_bank *bank, mm_phrase *phrase, const mm_request *request);
#endif
