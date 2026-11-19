#ifndef BRAMBLE_WAVETABLE_H
#define BRAMBLE_WAVETABLE_H

#define WT_TABLES 8
#define WT_FRAMES 32
#define WT_LEN 2048            /* samples per frame (power of two) */
#define WT_MIPS 10             /* mip m holds harmonics 1 .. (WT_LEN/2) >> m */

/* Builds every table. Returns 0 on success. */
int wt_init(void);

/* One band-limited frame: WT_LEN samples. */
const float *wt_frame(int table, int mip, int frame);

/* Picks the mip that keeps harmonics below Nyquist for a fundamental at freq. */
int wt_mip_for(float freq, float sample_rate);

#endif
