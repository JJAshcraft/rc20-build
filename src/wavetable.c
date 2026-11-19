/* Bramble wavetables: each frame is drawn in the time domain, taken to the
 * frequency domain, and rebuilt once per octave with the harmonics that octave
 * can play without aliasing. */
#include "wavetable.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float *tables; /* [table][mip][frame][WT_LEN] */

const float *wt_frame(int table, int mip, int frame)
{
    return tables + (((size_t)table * WT_MIPS + mip) * WT_FRAMES + frame) * WT_LEN;
}

int wt_mip_for(float freq, float sr)
{
    if (freq < 1) freq = 1;
    float maxh = (sr * 0.5f) / freq; /* highest harmonic below Nyquist */
    int m = 0;
    while (m < WT_MIPS - 1 && (float)((WT_LEN / 2) >> m) > maxh) m++;
    return m;
}

/* In-place radix-2 complex FFT; inverse when inv != 0 (unscaled). */
static void fft(double *re, double *im, int n, int inv)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / len * (inv ? 1 : -1);
        double wr = cos(ang), wi = sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = i + k + len / 2;
                double xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr; im[a] += xi;
                double t = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = t;
            }
        }
    }
}

static uint32_t rng = 0x9E3779B9u;
static double rnd(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / (double)0x1000000;
}

/* Draws frame f (t = f / (frames-1) in 0..1) of table k into out (WT_LEN samples).
 * Returns 1 if the frame was filled as a spectrum (re/im) instead. */
static int draw(int k, double t, double *out, double *sre, double *sim)
{
    const int N = WT_LEN;
    switch (k) {
    case 0: /* BASIC: sine -> triangle -> saw -> square */
        for (int i = 0; i < N; i++) {
            double p = (double)i / N;
            double sine = sin(2 * M_PI * p);
            double tri = p < 0.25 ? 4 * p : p < 0.75 ? 2 - 4 * p : 4 * p - 4;
            double saw = 2 * p - 1;
            double sq = p < 0.5 ? 1 : -1;
            double s = t * 3, a;
            if (s < 1) a = sine + (tri - sine) * s;
            else if (s < 2) a = tri + (saw - tri) * (s - 1);
            else a = saw + (sq - saw) * (s - 2);
            out[i] = a;
        }
        return 0;
    case 1: { /* PULSE: width 50% -> 3% */
        double w = 0.5 - 0.47 * t;
        for (int i = 0; i < N; i++) out[i] = ((double)i / N) < w ? 1 : -1;
        return 0;
    }
    case 2: { /* VOWEL: two formant peaks moving through the spectrum */
        double f1 = 3 + 9 * t, f2 = 10 + 30 * (1 - t) * (1 - t) + 6 * t;
        memset(sre, 0, sizeof(double) * N);
        memset(sim, 0, sizeof(double) * N);
        for (int h = 1; h < N / 2; h++) {
            double a = exp(-0.5 * pow((h - f1) / 2.0, 2)) + 0.7 * exp(-0.5 * pow((h - f2) / 4.0, 2));
            a += 0.15 / h;
            sim[h] = -a; sim[N - h] = a;
        }
        return 1;
    }
    case 3: { /* ORGAN: drawbars come in one by one */
        static const int bars[9] = {1, 2, 3, 4, 6, 8, 10, 12, 16};
        double nb = 1 + t * 8.0;
        memset(sre, 0, sizeof(double) * N);
        memset(sim, 0, sizeof(double) * N);
        for (int b = 0; b < 9; b++) {
            double a = nb >= b + 1 ? 1.0 : nb > b ? nb - b : 0;
            a *= 1.0 / (1 + b * 0.25);
            int h = bars[b];
            sim[h] -= a; sim[N - h] += a;
        }
        return 1;
    }
    case 4: { /* SYNC: a saw restarted by the master, slave ratio 1 -> 7 */
        double r = 1 + 6 * t;
        for (int i = 0; i < N; i++) {
            double p = (double)i / N;
            double s = fmod(p * r, 1.0);
            out[i] = (2 * s - 1) * (1 - p * 0.5);
        }
        return 0;
    }
    case 5: { /* FOLD: a sine pushed through a wave folder */
        double g = 1 + 7 * t;
        for (int i = 0; i < N; i++) out[i] = sin(g * sin(2 * M_PI * i / N) * M_PI * 0.5);
        return 0;
    }
    case 6: { /* METAL: phase modulation with a 3.5:1 feel, index rising */
        double idx = 6 * t;
        for (int i = 0; i < N; i++) {
            double p = 2 * M_PI * i / N;
            out[i] = sin(p + idx * sin(3 * p) + 0.5 * idx * sin(7 * p));
        }
        return 0;
    }
    default: { /* GRIT: 1/h saw spectrum that dissolves into random harmonics */
        uint32_t keep = rng;
        rng = 0x12345u; /* same random set for every frame, so frames morph */
        memset(sre, 0, sizeof(double) * N);
        memset(sim, 0, sizeof(double) * N);
        for (int h = 1; h < N / 2; h++) {
            double smooth = 1.0 / h;
            double rough = rnd() * rnd() * 2.0 / sqrt((double)h);
            double a = smooth * (1 - t) + rough * t;
            double ph = rnd() * 2 * M_PI * t;
            sre[h] = a * sin(ph); sim[h] = -a * cos(ph);
            sre[N - h] = sre[h]; sim[N - h] = -sim[h];
        }
        rng = keep;
        return 1;
    }
    }
}

int wt_init(void)
{
    const int N = WT_LEN;
    size_t total = (size_t)WT_TABLES * WT_MIPS * WT_FRAMES * WT_LEN;
    tables = (float *)malloc(total * sizeof(float));
    double *re = (double *)malloc(sizeof(double) * N), *im = (double *)malloc(sizeof(double) * N);
    double *sre = (double *)malloc(sizeof(double) * N), *sim = (double *)malloc(sizeof(double) * N);
    double *wr = (double *)malloc(sizeof(double) * N), *wi = (double *)malloc(sizeof(double) * N);
    if (!tables || !re || !im || !sre || !sim || !wr || !wi) return -1;

    for (int k = 0; k < WT_TABLES; k++) {
        for (int f = 0; f < WT_FRAMES; f++) {
            double t = (double)f / (WT_FRAMES - 1);
            if (draw(k, t, re, sre, sim)) {
                memcpy(re, sre, sizeof(double) * N);
                memcpy(im, sim, sizeof(double) * N);
            } else {
                memset(im, 0, sizeof(double) * N);
                fft(re, im, N, 0);
            }
            re[0] = im[0] = 0; /* no DC */
            re[N / 2] = im[N / 2] = 0;
            double gain = 0;
            for (int m = 0; m < WT_MIPS; m++) {
                int hmax = (N / 2) >> m;
                if (hmax < 1) hmax = 1;
                for (int i = 0; i < N; i++) {
                    int h = i <= N / 2 ? i : N - i;
                    int keep = h > 0 && h <= hmax && h < N / 2;
                    wr[i] = keep ? re[i] : 0;
                    wi[i] = keep ? im[i] : 0;
                }
                fft(wr, wi, N, 1);
                if (m == 0) { /* normalise all mips by the full-band peak */
                    double pk = 1e-9;
                    for (int i = 0; i < N; i++) if (fabs(wr[i]) > pk) pk = fabs(wr[i]);
                    gain = 0.95 / pk;
                }
                float *dst = (float *)wt_frame(k, m, f);
                for (int i = 0; i < N; i++) dst[i] = (float)(wr[i] * gain);
            }
        }
    }
    free(re); free(im); free(sre); free(sim); free(wr); free(wi);
    return 0;
}
