#include "params.h"
#include <math.h>
#include <stdio.h>

const char *const L_ONOFF[] = {"OFF", "ON"};
const char *const L_TABLES[] = {"BASIC", "PULSE", "VOWEL", "ORGAN", "SYNC", "FOLD", "METAL", "GRIT"};
const char *const L_SUBSHAPE[] = {"SINE", "TRI", "SQUARE"};
const char *const L_FTYPES[] = {"LP 12", "LP 24", "BAND", "HP 12", "NOTCH"};
const char *const L_LFOSHAPE[] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
const char *const L_LFODEST[] = {"NONE", "PITCH", "POS A", "POS B", "POS A+B", "CUTOFF", "VOLUME"};
const char *const L_VMODE[] = {"POLY", "MONO"};
const char *const L_SCALES[] = {"MAJOR", "MINOR", "DORIAN", "PENTA MAJ", "PENTA MIN", "BLUES", "CHROMATIC"};
const char *const L_ARP[] = {"OFF", "UP", "DOWN", "UP DOWN", "RANDOM"};
const char *const L_CHORD[] = {"OFF", "TRIAD", "SEVENTH", "FIFTH"};

const int scale_notes[7][12] = {
    {0, 2, 4, 5, 7, 9, 11},
    {0, 2, 3, 5, 7, 8, 10},
    {0, 2, 3, 5, 7, 9, 10},
    {0, 2, 4, 7, 9},
    {0, 3, 5, 7, 10},
    {0, 3, 5, 6, 7, 10},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
};
const int scale_len[7] = {7, 7, 7, 5, 5, 6, 12};

int scale_degree(int scale, int k)
{
    if (scale < 0 || scale > 6) scale = 0;
    int n = scale_len[scale];
    int oct = k >= 0 ? k / n : -((-k + n - 1) / n);
    return 12 * oct + scale_notes[scale][k - oct * n];
}

#define X(id, key, label, mn, mx, df, st, dp, lb) {key, label, mn, mx, df, st, dp, lb},
const ParamDef param_defs[P_COUNT] = {PARAM_LIST(X)};
#undef X

float conv_cutoff_hz(float n) { return 20.0f * powf(1000.0f, n); }
float conv_time_s(float n) { return 0.001f * powf(10000.0f, n); }
float conv_lfo_hz(float n) { return 0.05f * powf(400.0f, n); }
float conv_delay_s(float n) { return 0.02f + 0.98f * n; }

float param_clamp(int id, float v)
{
    const ParamDef *d = &param_defs[id];
    if (v < d->min) v = d->min;
    if (v > d->max) v = d->max;
    if (d->disp == D_ENUM || d->disp == D_INT || d->disp == D_SEMI || d->disp == D_NOTE ||
        d->disp == D_BPM || d->disp == D_CENTS)
        v = roundf(v);
    return v;
}

void param_format(int id, float v, char *out, int n)
{
    static const char *notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const ParamDef *d = &param_defs[id];
    float x;
    switch (d->disp) {
    case D_PCT: snprintf(out, n, "%d%%", (int)lroundf(v * 100)); break;
    case D_BIP: snprintf(out, n, "%+d%%", (int)lroundf(v * 100)); break;
    case D_INT: snprintf(out, n, "%d", (int)v); break;
    case D_ENUM: snprintf(out, n, "%s", d->labels[(int)v]); break;
    case D_CUTOFF:
        x = conv_cutoff_hz(v);
        if (x >= 1000) snprintf(out, n, "%.1f KHZ", x / 1000);
        else snprintf(out, n, "%d HZ", (int)x);
        break;
    case D_TIME:
        x = conv_time_s(v);
        if (x >= 1) snprintf(out, n, "%.2f S", x);
        else snprintf(out, n, "%d MS", (int)(x * 1000));
        break;
    case D_LFOHZ: snprintf(out, n, "%.2f HZ", conv_lfo_hz(v)); break;
    case D_SEMI: snprintf(out, n, "%+d ST", (int)v); break;
    case D_CENTS: snprintf(out, n, "%+d CT", (int)v); break;
    case D_DLYMS: snprintf(out, n, "%d MS", (int)(conv_delay_s(v) * 1000)); break;
    case D_BPM: snprintf(out, n, "%d BPM", (int)v); break;
    case D_NOTE: snprintf(out, n, "%s", notes[((int)v) % 12]); break;
    default: snprintf(out, n, "%.2f", v);
    }
}
