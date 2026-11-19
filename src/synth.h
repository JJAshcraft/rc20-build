#ifndef BRAMBLE_SYNTH_H
#define BRAMBLE_SYNTH_H

#include "params.h"

#define MAX_VOICES 8
#define SCOPE_LEN 1024

/* Written by the UI thread, read by the audio thread (single float stores). */
extern volatile float synth_params[P_COUNT];

typedef struct {
    volatile float bend;  /* semitones */
    volatile float cut;   /* cutoff offset in semitones */
    volatile float pos;   /* wavetable position offset, -1..1 */
    volatile float vib;   /* vibrato depth 0..1 */
    volatile int hold;    /* 1 = keys stay on after release */
} Perf;
extern Perf synth_perf;

int synth_init(float sample_rate);

/* UI thread -> audio thread (lock-free queue). */
void synth_key_on(int note);
void synth_key_off(int note);
void synth_panic(void);

/* Audio thread: interleaved stereo float. */
void synth_render(float *out, int frames);

/* For the UI. */
void synth_scope(float *dst, int n);              /* last n mono samples */
int synth_active_notes(unsigned char *notes, int max); /* notes currently sounding */
float synth_lfo_value(void);
float synth_env2_value(void);                     /* most recent voice's ENV2 */

#endif
