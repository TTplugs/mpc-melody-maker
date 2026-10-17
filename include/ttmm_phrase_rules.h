/* TTplugs MPC Melody Maker — Harmonic Anticipation, Contour & Voice-Leading Rules.
   Pure C11, zero heap allocations, deterministic across x86_64 and ARM32. */
#ifndef TT_MM_PHRASE_RULES_H
#define TT_MM_PHRASE_RULES_H
#include "mm_core.h"

/* Maps normalized pitch class (C=0, D=2, E=4, F=5, G=7, A=9, B=11)
   to the 7-column diatonic shift index [B, A, G, F, E, D, C] (0..6). */
static const int8_t ttmm_diatonic_col[12] = {
    6, -1, 5, -1, 4, 3, -1, 2, -1, 1, -1, 0
};

/* Stable pitch-class bitmasks (bit p = pitch class p relative to tonic)
   for light (0 = major) and dark (1 = minor) across chord root offsets 0..11. */
static const uint16_t ttmm_stable_masks[2][12] = {
    /* Light / Major context */
    {
        0x0891u, /* 0 (I):    C, E, G, B */
        0x0224u, /* 1 (bII):  D, F, A */
        0x0224u, /* 2 (ii):   D, F, A */
        0x0890u, /* 3 (bIII): E, G, B */
        0x0890u, /* 4 (iii):  E, G, B */
        0x0221u, /* 5 (IV):   C, F, A */
        0x0224u, /* 6 (#iv):  D, F, A */
        0x0884u, /* 7 (V):    D, G, B */
        0x0221u, /* 8 (bVI):  C, F, A */
        0x0211u, /* 9 (vi):   C, E, A */
        0x0284u, /* 10 (bVII):D, G, A */
        0x0824u  /* 11 (vii°):D, F, B */
    },
    /* Dark / Minor context */
    {
        0x0089u, /* 0 (i):    C, Eb, G */
        0x0122u, /* 1 (bII):  Db, F, Ab */
        0x0124u, /* 2 (ii°):  D, F, Ab */
        0x0488u, /* 3 (III):  Eb, G, Bb */
        0x0089u, /* 4 (iii):  C, E, G */
        0x0121u, /* 5 (iv):   C, F, Ab */
        0x0089u, /* 6 (#iv):  C, Eb, G */
        0x0484u, /* 7 (v/V):  D, G, Bb */
        0x0109u, /* 8 (VI):   C, Eb, Ab */
        0x0221u, /* 9 (vi):   C, F, A */
        0x0424u, /* 10 (VII): D, F, Bb */
        0x0884u  /* 11 (vii): D, G, B */
    }
};

/* 7-degree diatonic chord shifts [B, A, G, F, E, D, C] for light (0) and dark (1)
   indexed by active chord root interval relative to tonic (0..11). */
static const int8_t ttmm_chord_shifts[2][12][7] = {
    /* Light (Major) */
    {
        { 0,  0,  0,  0,  0,  0,  0}, /* I */
        { 0, -1,  0,  0, -1, -1,  0}, /* bII */
        { 0,  0,  0,  0,  0,  0,  0}, /* ii */
        {-1,  0,  0,  0, -1,  0,  0}, /* bIII */
        { 0,  0,  0,  0,  0,  0,  0}, /* iii */
        { 0,  0,  0,  0,  0,  0,  0}, /* IV */
        { 0,  0,  0,  1,  0,  0,  0}, /* #iv */
        { 0,  0,  0,  0,  0,  0,  0}, /* V */
        {-1, -1,  0,  0, -1,  0,  0}, /* bVI */
        { 0,  0,  0,  0,  0,  0,  0}, /* vi */
        {-1,  0,  0,  0,  0,  0,  0}, /* bVII */
        { 0,  0,  0,  0,  0,  0,  0}  /* vii */
    },
    /* Dark (Minor) */
    {
        { 0,  0,  0,  0,  0,  0,  0}, /* i */
        { 0,  0,  0,  0,  0, -1,  0}, /* bII */
        { 0,  0,  0,  0,  0,  0,  0}, /* ii° */
        { 0,  0,  0,  0,  0,  0,  0}, /* III */
        { 0,  0,  0,  0,  1,  0,  0}, /* III+ */
        { 0,  0,  0,  0,  0,  0,  0}, /* iv */
        { 0,  0,  0,  1,  0,  0,  0}, /* #iv */
        { 1,  0,  0,  0,  0,  0,  0}, /* V (harmonic leading tone) */
        { 0,  0,  0,  0,  0,  0,  0}, /* VI */
        { 0,  1,  0,  0,  0,  0,  0}, /* vi (Dorian 6th) */
        { 0,  0,  0,  0,  0,  0,  0}, /* VII */
        { 1,  0,  0,  0,  0,  0,  0}  /* vii° */
    }
};

/* Harmonic Anticipation (Melody Trigger):
   When a syncopated note lands at 1.5 or 1.75 beats (1440 or 1680 ticks within
   a 1920-tick 2-beat harmonic slot) and holds across or pauses before the slot
   boundary, it anticipates the harmonic context of the NEXT 2-beat slot. */
static inline uint32_t ttmm_resolve_trigger_tick(uint32_t start, uint32_t duration,
                                                 uint32_t next_start, uint32_t length,
                                                 int speed) {
    const uint32_t slot_ticks = 2u * MM_PPQ; /* 1920 ticks = 2 beats */
    if (!length || start >= length) return start;
    uint32_t in_slot = start % slot_ticks;
    /* Anticipation positions: 1.5 beats (1440) or 1.75 beats (1680) in 2-beat slot */
    if (in_slot != 1440u && in_slot != 1680u) return start;
    uint32_t gap = (next_start > start) ? (next_start - start) : duration;
    uint32_t min_gap = (speed == 0) ? 1920u : 960u;
    if (gap < min_gap && start + duration <= ((start / slot_ticks) + 1u) * slot_ticks) {
        return start;
    }
    uint32_t next_slot_tick = ((start / slot_ticks) + 1u) * slot_ticks;
    return (next_slot_tick < length) ? next_slot_tick : 0u;
}

/* Preserves the repeated vs. non-repeated pitch contour invariant after scale
   projection: if two consecutive melody notes were distinct before projection
   (orig_prev != orig_curr) but collapsed onto the same pitch after projection
   (proj_prev == proj_curr), nudges proj_curr to the adjacent target-scale tone
   in the original melodic direction whenever within [lo, hi]. */
static inline int ttmm_preserve_repeated_contour(int orig_prev, int orig_curr,
                                                 int proj_prev, int proj_curr,
                                                 int key, int scale, int lo, int hi) {
    if (orig_prev == orig_curr || proj_prev != proj_curr) return proj_curr;
    int dir = (orig_curr > orig_prev) ? 1 : -1;
    for (int step = 1; step <= 4; step++) {
        int candidate_in = proj_curr + dir * step;
        if (candidate_in < lo || candidate_in > hi) break;
        int snapped = mm_scale_pitch(candidate_in, key, scale, lo, hi);
        if (snapped >= lo && snapped <= hi && snapped != proj_prev) {
            return snapped;
        }
    }
    return proj_curr;
}

/* Inter-phrase Flowing voice-leading check:
   Returns 1 if the transition from prev_phrase_last_pitch to curr_phrase_first_pitch
   satisfies the flowing stepwise threshold (<= 2 semitones, or <= 4 in relaxed pass). */
static inline int ttmm_check_flowing_transition(int prev_last_pitch, int curr_first_pitch,
                                                int relax_level) {
    if (prev_last_pitch < 0 || curr_first_pitch < 0) return 1;
    int dist = curr_first_pitch - prev_last_pitch;
    if (dist < 0) dist = -dist;
    int max_leap = (relax_level <= 0) ? 2 : (relax_level == 1) ? 4 : 7;
    return dist <= max_leap;
}

#endif /* TT_MM_PHRASE_RULES_H */
