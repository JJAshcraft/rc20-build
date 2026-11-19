/* Headless test: renders each factory preset to a WAV and checks the output.
 *   render OUT_DIR [SECONDS]
 * Prints peak level, NaN/Inf count and real-time ratio per preset; exits 1 on a bad result. */
#include "../src/preset.h"
#include "../src/synth.h"
#include "../src/wavetable.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SR 48000

static void wav_write(const char *path, const float *buf, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    uint32_t v;
    uint16_t s;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = SR; fwrite(&v, 4, 1, f); v = SR * 4; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
    for (int i = 0; i < frames * 2; i++) {
        float x = buf[i] * 32767.0f;
        int16_t q = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    float secs = argc > 2 ? (float)atof(argv[2]) : 3.0f;
    int frames = (int)(secs * SR);
    double t0 = now();
    if (wt_init()) { fprintf(stderr, "wavetable init failed\n"); return 1; }
    printf("wavetables built in %.2f s\n", now() - t0);
    if (synth_init(SR)) { fprintf(stderr, "synth init failed\n"); return 1; }
    presets_factory();

    float *buf = (float *)malloc(sizeof(float) * 2 * frames);
    int bad = 0;
    for (int p = 0; p < PRESET_SLOTS; p++) {
        if (!presets[p].used) continue;
        float vals[P_COUNT];
        for (int i = 0; i < P_COUNT; i++) vals[i] = param_defs[i].def;
        preset_apply(p, vals);
        for (int i = 0; i < P_COUNT; i++) synth_params[i] = vals[i];
        synth_panic();
        float flush[256];
        for (int i = 0; i < 400; i++) synth_render(flush, 128); /* let tails die */

        /* a C minor-ish chord held for half the time, then a short melody */
        int chord[4] = {48, 55, 60, 63};
        int mono = (int)vals[P_V_MODE] == 1;
        double cpu = 0;
        int pos = 0, step = 0;
        while (pos < frames) {
            if (pos == 0) {
                for (int k = 0; k < (mono ? 1 : 4); k++) synth_key_on(chord[k]);
            }
            if (pos >= frames / 2 && step == 0) {
                for (int k = 0; k < 4; k++) synth_key_off(chord[k]);
                step = 1;
            }
            int n = 256;
            if (pos + n > frames) n = frames - pos;
            double a = now();
            synth_render(buf + pos * 2, n);
            cpu += now() - a;
            pos += n;
        }
        float peak = 0;
        int nan = 0;
        double rms = 0;
        for (int i = 0; i < frames * 2; i++) {
            if (!isfinite(buf[i])) { nan++; continue; }
            if (fabsf(buf[i]) > peak) peak = fabsf(buf[i]);
            rms += buf[i] * buf[i];
        }
        rms = sqrt(rms / (frames * 2));
        char path[512], fname[64];
        snprintf(fname, sizeof fname, "%s", presets[p].name);
        for (char *c = fname; *c; c++) if (*c == ' ') *c = '_';
        snprintf(path, sizeof path, "%s/%02d_%s.wav", dir, p + 1, fname);
        wav_write(path, buf, frames);
        double rt = cpu / secs;
        int ok = nan == 0 && peak > 0.01f && peak <= 1.0f;
        if (!ok) bad = 1;
        printf("%-12s peak %.3f rms %.3f nan %d cpu %.1f%% of real time %s\n", presets[p].name, peak, rms, nan,
               rt * 100, ok ? "ok" : "BAD");
    }
    /* worst case: 8 voices, both oscillators at 4x unison, sub, noise, LP24, all effects */
    {
        for (int i = 0; i < P_COUNT; i++) synth_params[i] = param_defs[i].def;
        synth_params[P_A_UNI] = synth_params[P_B_UNI] = 4;
        synth_params[P_B_ON] = 1;
        synth_params[P_SUB_LVL] = synth_params[P_NOISE_LVL] = 0.3f;
        synth_params[P_F_TYPE] = 1;
        synth_params[P_F_DRIVE] = 0.5f;
        synth_params[P_FX_CHO] = synth_params[P_FX_DLY] = synth_params[P_FX_REV] = 0.5f;
        synth_params[P_E1_S] = 1;
        synth_panic();
        for (int k = 0; k < 8; k++) synth_key_on(48 + k * 3);
        double a = now();
        for (int pos = 0; pos < frames; pos += 256) synth_render(buf, 256);
        double rt = (now() - a) / secs;
        printf("WORST CASE   8 voices x 2 osc x 4 unison + fx: cpu %.1f%% of real time\n", rt * 100);
    }
    free(buf);
    return bad;
}
