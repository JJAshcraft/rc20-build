/* Bramble synth engine: 8-voice wavetable synth with sub, noise, a
 * state-variable filter, two envelopes, an LFO, an arpeggiator and chorus,
 * delay and reverb. Runs entirely in the audio callback. */
#include "synth.h"
#include "wavetable.h"
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK 32
#define MAX_UNI 4

volatile float synth_params[P_COUNT];
Perf synth_perf;

static float SR = 48000.0f;

/* ---------- event queue ---------- */
enum { EV_ON, EV_OFF, EV_PANIC };
typedef struct { int type, note; } Event;
#define QLEN 256
static Event queue[QLEN];
static atomic_uint q_head, q_tail;

static void push(int type, int note)
{
    unsigned h = atomic_load_explicit(&q_head, memory_order_relaxed);
    unsigned t = atomic_load_explicit(&q_tail, memory_order_acquire);
    if (h - t >= QLEN) return; /* full: drop */
    queue[h % QLEN].type = type;
    queue[h % QLEN].note = note;
    atomic_store_explicit(&q_head, h + 1, memory_order_release);
}
void synth_key_on(int n) { push(EV_ON, n); }
void synth_key_off(int n) { push(EV_OFF, n); }
void synth_panic(void) { push(EV_PANIC, 0); }

/* ---------- helpers ---------- */
static inline float P(int id) { return synth_params[id]; }
static inline float semis_to_ratio(float s) { return exp2f(s * (1.0f / 12.0f)); }
static inline float note_hz(float n) { return 440.0f * exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static inline float soft(float x)
{
    if (x > 3) return 1;
    if (x < -3) return -1;
    return x * (27 + x * x) / (27 + 9 * x * x);
}
static uint32_t nrng = 22222;
static inline float white(void)
{
    nrng ^= nrng << 13; nrng ^= nrng >> 17; nrng ^= nrng << 5;
    return (int32_t)nrng * (1.0f / 2147483648.0f);
}

/* ---------- envelopes ---------- */
enum { ENV_IDLE, ENV_A, ENV_D, ENV_S, ENV_R };
typedef struct { float v; int st; } Env;

static inline float coef_for(float secs) { return 1.0f - expf(-4.6f / (secs * SR)); }

static inline float env_step(Env *e, float a, float d, float s, float r)
{
    switch (e->st) {
    case ENV_A:
        e->v += 1.0f / (a * SR);
        if (e->v >= 1) { e->v = 1; e->st = ENV_D; }
        break;
    case ENV_D:
        e->v += (s - e->v) * coef_for(d);
        if (fabsf(e->v - s) < 1e-4f) e->st = ENV_S;
        break;
    case ENV_S: e->v = s; break;
    case ENV_R:
        e->v += (0 - e->v) * coef_for(r);
        if (e->v < 1e-4f) { e->v = 0; e->st = ENV_IDLE; }
        break;
    default: break;
    }
    return e->v;
}

/* ---------- filter (TPT state variable) ---------- */
typedef struct { float ic1, ic2; } Svf;
typedef struct { float a1, a2, a3, k; } SvfCo;

static inline void svf_co(SvfCo *c, float fc, float k)
{
    if (fc < 20) fc = 20;
    if (fc > SR * 0.45f) fc = SR * 0.45f;
    float g = tanf((float)M_PI * fc / SR);
    c->k = k;
    c->a1 = 1.0f / (1.0f + g * (g + k));
    c->a2 = g * c->a1;
    c->a3 = g * c->a2;
}
static inline void svf_run(Svf *s, const SvfCo *c, float v0, float *lp, float *bp, float *hp)
{
    float v3 = v0 - s->ic2;
    float v1 = c->a1 * s->ic1 + c->a2 * v3;
    float v2 = s->ic2 + c->a2 * s->ic1 + c->a3 * v3;
    s->ic1 = 2 * v1 - s->ic1;
    s->ic2 = 2 * v2 - s->ic2;
    *lp = v2; *bp = v1; *hp = v0 - c->k * v1 - v2;
}

/* ---------- voices ---------- */
typedef struct {
    int active, gate, note;
    uint32_t age;
    float pitch, pitch_target;   /* in MIDI note units */
    double ph[2][MAX_UNI];       /* osc phase 0..1 */
    double sub_ph;
    float noise_lp;
    Env e1, e2;
    Svf f[2][2];                 /* [channel][stage] */
} Voice;

static Voice voices[MAX_VOICES];
static uint32_t age_counter;
static float last_pitch = 60;
static int last_voice = 0;

static void voice_start(Voice *v, int note, int legato)
{
    float glide = P(P_V_GLIDE);
    v->note = note;
    v->pitch_target = (float)note;
    if (!legato) {
        if (!v->active || glide <= 0) v->pitch = glide > 0 ? last_pitch : (float)note;
        if (!v->active) {
            memset(v->f, 0, sizeof v->f);
            for (int o = 0; o < 2; o++)
                for (int u = 0; u < MAX_UNI; u++) v->ph[o][u] = (u * 0.37) - floor(u * 0.37);
            v->sub_ph = 0;
            v->e1.v = v->e2.v = 0;
        }
        v->e1.st = ENV_A;
        v->e2.st = ENV_A;
    }
    v->active = 1;
    v->gate = 1;
    v->age = ++age_counter;
    last_pitch = (float)note;
    last_voice = (int)(v - voices);
}

static void voice_release(Voice *v)
{
    v->gate = 0;
    v->e1.st = ENV_R;
    v->e2.st = ENV_R;
}

/* mono-mode note stack */
static int mono_stack[16], mono_n;

static void play_on(int note)
{
    if ((int)P(P_V_MODE) == 1) {
        Voice *v = &voices[0];
        for (int i = 0; i < mono_n; i++)
            if (mono_stack[i] == note) { memmove(&mono_stack[i], &mono_stack[i + 1], (mono_n - i - 1) * sizeof(int)); mono_n--; break; }
        if (mono_n < 16) mono_stack[mono_n++] = note;
        voice_start(v, note, v->active && v->gate);
        return;
    }
    Voice *best = NULL;
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].gate && voices[i].note == note) best = &voices[i];
    if (!best)
        for (int i = 0; i < MAX_VOICES; i++)
            if (!voices[i].active) { best = &voices[i]; break; }
    if (!best) { /* quietest releasing voice, else the oldest */
        float lv = 2;
        for (int i = 0; i < MAX_VOICES; i++)
            if (!voices[i].gate && voices[i].e1.v < lv) { lv = voices[i].e1.v; best = &voices[i]; }
    }
    if (!best) {
        best = &voices[0];
        for (int i = 1; i < MAX_VOICES; i++)
            if (voices[i].age < best->age) best = &voices[i];
    }
    voice_start(best, note, 0);
}

static void play_off(int note)
{
    if ((int)P(P_V_MODE) == 1) {
        for (int i = 0; i < mono_n; i++)
            if (mono_stack[i] == note) { memmove(&mono_stack[i], &mono_stack[i + 1], (mono_n - i - 1) * sizeof(int)); mono_n--; break; }
        Voice *v = &voices[0];
        if (!v->gate || v->note != note) return;
        if (mono_n > 0) voice_start(v, mono_stack[mono_n - 1], 1);
        else voice_release(v);
        return;
    }
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active && voices[i].gate && voices[i].note == note) voice_release(&voices[i]);
}

static void all_release(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active) voice_release(&voices[i]);
    mono_n = 0;
}

/* ---------- keys, hold and arpeggiator ---------- */
static unsigned char phys_down[128];
static int held[32], held_n;      /* sounding keys in press order */
static int arp_mode_prev, hold_prev, vmode_prev;
static double arp_clock;          /* samples until next step */
static int arp_idx, arp_dir = 1, arp_note = -1;
static double arp_gate_left;

static int is_held(int n)
{
    for (int i = 0; i < held_n; i++) if (held[i] == n) return 1;
    return 0;
}
static void held_add(int n)
{
    if (is_held(n) || held_n >= 32) return;
    held[held_n++] = n;
    if ((int)P(P_P_ARP) == 0) play_on(n);
    else if (held_n == 1) arp_clock = 0; /* first key starts the arp on the beat */
}
static void held_remove(int n)
{
    for (int i = 0; i < held_n; i++)
        if (held[i] == n) {
            memmove(&held[i], &held[i + 1], (held_n - i - 1) * sizeof(int));
            held_n--;
            if ((int)P(P_P_ARP) == 0) play_off(n);
            return;
        }
}

static void handle_events(void)
{
    unsigned t = atomic_load_explicit(&q_tail, memory_order_relaxed);
    unsigned h = atomic_load_explicit(&q_head, memory_order_acquire);
    for (; t != h; t++) {
        Event e = queue[t % QLEN];
        int n = e.note & 127;
        if (e.type == EV_PANIC) {
            memset(phys_down, 0, sizeof phys_down);
            held_n = 0;
            arp_note = -1;
            all_release();
            continue;
        }
        if (e.type == EV_ON) {
            int any = 0;
            for (int i = 0; i < 128; i++) any |= phys_down[i];
            if (synth_perf.hold && !any) /* a fresh chord replaces the held one */
                while (held_n) held_remove(held[held_n - 1]);
            phys_down[n] = 1;
            held_add(n);
        } else {
            phys_down[n] = 0;
            if (!synth_perf.hold) held_remove(n);
        }
    }
    atomic_store_explicit(&q_tail, t, memory_order_release);

    int hold = synth_perf.hold;
    if (hold_prev && !hold) /* hold released: drop keys no longer pressed */
        for (int i = held_n - 1; i >= 0; i--)
            if (!phys_down[held[i]]) held_remove(held[i]);
    hold_prev = hold;

    int arp = (int)P(P_P_ARP), vm = (int)P(P_V_MODE);
    if (arp != arp_mode_prev || vm != vmode_prev) { /* mode change: restart cleanly */
        all_release();
        arp_note = -1;
        if (arp == 0)
            for (int i = 0; i < held_n; i++) play_on(held[i]);
        arp_clock = 0;
        arp_mode_prev = arp;
        vmode_prev = vm;
    }
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static void arp_tick(int frames)
{
    int mode = (int)P(P_P_ARP);
    if (mode == 0) return;
    double step = SR * 60.0 / P(P_P_BPM) / 4.0; /* sixteenth notes */
    if (arp_note >= 0) {
        arp_gate_left -= frames;
        if (arp_gate_left <= 0) { play_off(arp_note); arp_note = -1; }
    }
    arp_clock -= frames;
    if (arp_clock > 0) return;
    arp_clock += step;
    if (arp_clock < 0) arp_clock = step;
    if (held_n == 0) { arp_idx = 0; arp_dir = 1; return; }
    int sorted[32], n = held_n;
    memcpy(sorted, held, n * sizeof(int));
    qsort(sorted, n, sizeof(int), cmp_int);
    int pick;
    switch (mode) {
    case 1: pick = arp_idx % n; arp_idx = (arp_idx + 1) % n; break;
    case 2: pick = (n - 1) - (arp_idx % n); arp_idx = (arp_idx + 1) % n; break;
    case 3:
        if (arp_idx >= n) arp_idx = n - 1;
        pick = arp_idx;
        if (n > 1) {
            if (arp_idx + arp_dir >= n || arp_idx + arp_dir < 0) arp_dir = -arp_dir;
            arp_idx += arp_dir;
        }
        break;
    default: pick = rand() % n; break;
    }
    if (arp_note >= 0) play_off(arp_note);
    arp_note = sorted[pick];
    play_on(arp_note);
    arp_gate_left = step * 0.6;
}

/* ---------- LFO ---------- */
static double lfo_ph;
static float lfo_val, lfo_rand, lfo_out;
static double vib_ph;

static void lfo_tick(int frames)
{
    double inc = conv_lfo_hz(P(P_LFO_RATE)) / SR * frames;
    lfo_ph += inc;
    if (lfo_ph >= 1) { lfo_ph -= floor(lfo_ph); lfo_rand = white(); }
    float p = (float)lfo_ph;
    switch ((int)P(P_LFO_SHAPE)) {
    case 0: lfo_val = sinf(2 * (float)M_PI * p); break;
    case 1: lfo_val = p < 0.5f ? 4 * p - 1 : 3 - 4 * p; break;
    case 2: lfo_val = 1 - 2 * p; break;
    case 3: lfo_val = p < 0.5f ? 1 : -1; break;
    default: lfo_val = lfo_rand; break;
    }
    lfo_out = lfo_val;
    vib_ph += 5.5 / SR * frames;
    if (vib_ph >= 1) vib_ph -= 1;
}

/* ---------- oscillator ---------- */
static inline float bend_phase(float p, float c)
{
    return p < c ? p * 0.5f / c : 0.5f + (p - c) * 0.5f / (1.0f - c);
}

static inline float wt_read(const float *f0, const float *f1, float fm, float p)
{
    float x = p * WT_LEN;
    int i = (int)x;
    float fr = x - i;
    i &= WT_LEN - 1;
    int j = (i + 1) & (WT_LEN - 1);
    float a = f0[i] + (f0[j] - f0[i]) * fr;
    float b = f1[i] + (f1[j] - f1[i]) * fr;
    return a + (b - a) * fm;
}

/* ---------- effects ---------- */
#define CHO_LEN 4096
static float cho_buf[2][CHO_LEN];
static int cho_w;
static double cho_ph;

#define DLY_LEN 96000
static float *dly_buf[2];
static int dly_w;
static float dly_time_s = 0.4f, dly_lp[2];

#define NCOMB 4
#define NAP 2
static const int comb_len[NCOMB] = {1116, 1277, 1422, 1557};
static const int ap_len[NAP] = {556, 341};
static float *comb_buf[2][NCOMB], *ap_buf[2][NAP];
static int comb_n[2][NCOMB], ap_n[2][NAP], comb_i[2][NCOMB], ap_i[2][NAP];
static float comb_lp[2][NCOMB];

static float scope[SCOPE_LEN];
static atomic_int scope_w;
static float sm_cut = -1, sm_vol = 0;

int synth_init(float sr)
{
    SR = sr;
    for (int i = 0; i < P_COUNT; i++) synth_params[i] = param_defs[i].def;
    memset(voices, 0, sizeof voices);
    for (int c = 0; c < 2; c++) {
        dly_buf[c] = (float *)calloc(DLY_LEN, sizeof(float));
        if (!dly_buf[c]) return -1;
        for (int k = 0; k < NCOMB; k++) {
            comb_n[c][k] = (int)((comb_len[k] + c * 23) * sr / 44100.0f);
            comb_buf[c][k] = (float *)calloc(comb_n[c][k], sizeof(float));
            if (!comb_buf[c][k]) return -1;
        }
        for (int k = 0; k < NAP; k++) {
            ap_n[c][k] = (int)((ap_len[k] + c * 23) * sr / 44100.0f);
            ap_buf[c][k] = (float *)calloc(ap_n[c][k], sizeof(float));
            if (!ap_buf[c][k]) return -1;
        }
    }
    arp_mode_prev = (int)P(P_P_ARP);
    vmode_prev = (int)P(P_V_MODE);
    return 0;
}

static void render_voice(Voice *v, float *L, float *R, int n, float lfo, float pitch_mod, float cut_semis_mod,
                         float pos_mod, float amp_mod)
{
    /* glide toward target, once per block */
    float gl = P(P_V_GLIDE);
    if (gl > 0) {
        float c = 1.0f - expf(-(float)n / (gl * gl * SR * 0.5f + 1.0f));
        v->pitch += (v->pitch_target - v->pitch) * c;
    } else v->pitch = v->pitch_target;

    float base = v->pitch + pitch_mod;
    float e2 = v->e2.v;

    /* oscillator setup */
    const float *fr0[2], *fr1[2];
    float fm[2], cpt[2];
    double inc[2][MAX_UNI];
    float panl[2][MAX_UNI], panr[2][MAX_UNI];
    int uni[2], on[2];
    for (int o = 0; o < 2; o++) {
        int b = o ? P_B_ON - P_A_ON : 0;
        on[o] = P(P_A_ON + b) > 0.5f && P(P_A_LEVEL + b) > 0;
        if (!on[o]) continue;
        float note = base + P(P_A_SEMI + b) + P(P_A_FINE + b) * 0.01f;
        float hz = note_hz(note);
        int tab = (int)P(P_A_TABLE + b);
        int mip = wt_mip_for(hz * 1.03f, SR);
        float pos = P(P_A_POS + b) + P(P_E2_POS) * e2 + pos_mod;
        if (o == 0 && ((int)P(P_LFO_DEST) == LFO_D_POSA || (int)P(P_LFO_DEST) == LFO_D_POSAB))
            pos += lfo * P(P_LFO_AMT) * 0.5f;
        if (o == 1 && ((int)P(P_LFO_DEST) == LFO_D_POSB || (int)P(P_LFO_DEST) == LFO_D_POSAB))
            pos += lfo * P(P_LFO_AMT) * 0.5f;
        if (pos < 0) pos = 0;
        if (pos > 1) pos = 1;
        float fp = pos * (WT_FRAMES - 1);
        int fi = (int)fp;
        if (fi >= WT_FRAMES - 1) fi = WT_FRAMES - 2;
        fm[o] = fp - fi;
        fr0[o] = wt_frame(tab, mip, fi);
        fr1[o] = wt_frame(tab, mip, fi + 1);
        uni[o] = (int)P(P_A_UNI + b);
        if (uni[o] < 1) uni[o] = 1;
        float det = P(P_A_DET + b) * 40.0f; /* cents */
        float g = P(P_A_LEVEL + b) / sqrtf((float)uni[o]);
        cpt[o] = 0.5f - P(P_A_BEND + b) * 0.45f;
        for (int u = 0; u < uni[o]; u++) {
            float sp = uni[o] > 1 ? (2.0f * u / (uni[o] - 1) - 1.0f) : 0.0f;
            inc[o][u] = hz * semis_to_ratio(sp * det * 0.01f) / SR;
            float pan = sp * 0.8f;
            panl[o][u] = g * sqrtf(0.5f * (1 - pan));
            panr[o][u] = g * sqrtf(0.5f * (1 + pan));
        }
    }
    float sub_lvl = P(P_SUB_LVL), noise_lvl = P(P_NOISE_LVL);
    int sub_shape = (int)P(P_SUB_SHAPE);
    double sub_inc = note_hz(base + 12.0f * P(P_SUB_OCT)) / SR;
    float ncol = 0.02f + 0.98f * P(P_NOISE_COL) * P(P_NOISE_COL);

    /* filter setup */
    int ftype = (int)P(P_F_TYPE);
    float cut_semi = P(P_F_KEY) * (v->pitch - 60) + P(P_F_ENV) * e2 * 72.0f + cut_semis_mod;
    float fc = sm_cut * semis_to_ratio(cut_semi);
    float res = P(P_F_RES);
    SvfCo c1, c2;
    if (ftype == 1) {
        svf_co(&c1, fc, 1.4142f);
        svf_co(&c2, fc, 2.0f - 1.94f * res);
    } else svf_co(&c1, fc, 2.0f - 1.96f * res);
    float drive = 1.0f + P(P_F_DRIVE) * 8.0f;
    float dcomp = 1.0f / (1.0f + P(P_F_DRIVE) * 1.5f);

    float a1 = conv_time_s(P(P_E1_A)), d1 = conv_time_s(P(P_E1_D)), s1 = P(P_E1_S), r1 = conv_time_s(P(P_E1_R));
    float a2 = conv_time_s(P(P_E2_A)), d2 = conv_time_s(P(P_E2_D)), s2 = P(P_E2_S), r2 = conv_time_s(P(P_E2_R));

    for (int i = 0; i < n; i++) {
        float l = 0, r = 0;
        for (int o = 0; o < 2; o++) {
            if (!on[o]) continue;
            for (int u = 0; u < uni[o]; u++) {
                double ph = v->ph[o][u] + inc[o][u];
                if (ph >= 1) ph -= 1;
                v->ph[o][u] = ph;
                float p = bend_phase((float)ph, cpt[o]);
                float s = wt_read(fr0[o], fr1[o], fm[o], p);
                l += s * panl[o][u];
                r += s * panr[o][u];
            }
        }
        float mono = 0;
        if (sub_lvl > 0) {
            v->sub_ph += sub_inc;
            if (v->sub_ph >= 1) v->sub_ph -= 1;
            float p = (float)v->sub_ph, s;
            if (sub_shape == 0) s = sinf(2 * (float)M_PI * p);
            else if (sub_shape == 1) s = p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
            else {
                float dt = (float)sub_inc;
                s = p < 0.5f ? 1.0f : -1.0f;
                /* polyBLEP at both edges */
                float t = p;
                if (t < dt) { t /= dt; s += t + t - t * t - 1; }
                else if (t > 1 - dt) { t = (t - 1) / dt; s += t * t + t + t + 1; }
                t = p + 0.5f; if (t >= 1) t -= 1;
                if (t < dt) { t /= dt; s -= t + t - t * t - 1; }
                else if (t > 1 - dt) { t = (t - 1) / dt; s -= t * t + t + t + 1; }
                s *= 0.7f;
            }
            mono += s * sub_lvl;
        }
        if (noise_lvl > 0) {
            v->noise_lp += (white() - v->noise_lp) * ncol;
            mono += v->noise_lp * noise_lvl * (1.0f / sqrtf(ncol)) * 0.5f;
        }
        l = (l + mono) * 0.5f;
        r = (r + mono) * 0.5f;

        if (drive > 1.0f) { l = soft(l * drive) * dcomp; r = soft(r * drive) * dcomp; }
        float in[2] = {l, r}, outv[2];
        for (int ch = 0; ch < 2; ch++) {
            float lp, bp, hp, y;
            svf_run(&v->f[ch][0], &c1, in[ch], &lp, &bp, &hp);
            switch (ftype) {
            case 0: y = lp; break;
            case 1: { float lp2, bp2, hp2; svf_run(&v->f[ch][1], &c2, lp, &lp2, &bp2, &hp2); y = lp2; } break;
            case 2: y = bp; break;
            case 3: y = hp; break;
            default: y = lp + hp; break;
            }
            outv[ch] = y;
        }

        float e1 = env_step(&v->e1, a1, d1, s1, r1);
        env_step(&v->e2, a2, d2, s2, r2);
        float g = e1 * amp_mod;
        L[i] += outv[0] * g;
        R[i] += outv[1] * g;
    }
    if (v->e1.st == ENV_IDLE) v->active = 0;
}

static void fx_process(float *L, float *R, int n)
{
    float cho = P(P_FX_CHO), dmix = P(P_FX_DLY), dfb = P(P_FX_DFB), rev = P(P_FX_REV);
    float size = P(P_FX_RSIZE);
    float dtarget = conv_delay_s(P(P_FX_DTIME));
    float cfb = 0.70f + size * 0.27f;
    for (int i = 0; i < n; i++) {
        float l = L[i], r = R[i];
        if (cho > 0) {
            cho_ph += 0.6 / SR;
            if (cho_ph >= 1) cho_ph -= 1;
            cho_buf[0][cho_w] = l;
            cho_buf[1][cho_w] = r;
            float wet[2];
            for (int c = 0; c < 2; c++) {
                float m = sinf(2 * (float)M_PI * ((float)cho_ph + c * 0.25f));
                float d = (0.012f + 0.003f * m) * SR;
                float rp = cho_w - d;
                while (rp < 0) rp += CHO_LEN;
                int i0 = (int)rp, i1 = (i0 + 1) % CHO_LEN;
                float f = rp - i0;
                wet[c] = cho_buf[c][i0] + (cho_buf[c][i1] - cho_buf[c][i0]) * f;
            }
            cho_w = (cho_w + 1) % CHO_LEN;
            l = l * (1 - 0.4f * cho) + wet[1] * 0.8f * cho;
            r = r * (1 - 0.4f * cho) + wet[0] * 0.8f * cho;
        }
        if (dmix > 0 || dly_lp[0] != 0 || dly_lp[1] != 0) {
            dly_time_s += (dtarget - dly_time_s) * 0.0005f;
            float rp = dly_w - dly_time_s * SR;
            while (rp < 0) rp += DLY_LEN;
            int i0 = (int)rp, i1 = (i0 + 1) % DLY_LEN;
            float f = rp - i0;
            float dl = dly_buf[0][i0] + (dly_buf[0][i1] - dly_buf[0][i0]) * f;
            float dr = dly_buf[1][i0] + (dly_buf[1][i1] - dly_buf[1][i0]) * f;
            dly_lp[0] += (dl - dly_lp[0]) * 0.35f;
            dly_lp[1] += (dr - dly_lp[1]) * 0.35f;
            if (fabsf(dly_lp[0]) < 1e-9f) dly_lp[0] = 0;
            if (fabsf(dly_lp[1]) < 1e-9f) dly_lp[1] = 0;
            /* ping-pong: input enters left, each side feeds the other */
            dly_buf[0][dly_w] = (l + r) * 0.5f * (dmix > 0) + dly_lp[1] * dfb;
            dly_buf[1][dly_w] = dly_lp[0] * dfb;
            dly_w = (dly_w + 1) % DLY_LEN;
            l += dl * dmix;
            r += dr * dmix;
        }
        if (rev > 0) {
            float in = (l + r) * 0.03f, o[2];
            for (int c = 0; c < 2; c++) {
                float acc = 0;
                for (int k = 0; k < NCOMB; k++) {
                    float y = comb_buf[c][k][comb_i[c][k]];
                    comb_lp[c][k] += (y - comb_lp[c][k]) * 0.75f;
                    comb_buf[c][k][comb_i[c][k]] = in + comb_lp[c][k] * cfb;
                    if (++comb_i[c][k] >= comb_n[c][k]) comb_i[c][k] = 0;
                    acc += y;
                }
                for (int k = 0; k < NAP; k++) {
                    float b = ap_buf[c][k][ap_i[c][k]];
                    float y = b - acc;
                    ap_buf[c][k][ap_i[c][k]] = acc + b * 0.5f;
                    if (++ap_i[c][k] >= ap_n[c][k]) ap_i[c][k] = 0;
                    acc = y;
                }
                o[c] = acc;
            }
            l += o[0] * rev * 1.5f;
            r += o[1] * rev * 1.5f;
        }
        L[i] = l;
        R[i] = r;
    }
}

void synth_render(float *out, int frames)
{
    float L[BLOCK], R[BLOCK];
    int done = 0;
    while (done < frames) {
        int n = frames - done;
        if (n > BLOCK) n = BLOCK;
        handle_events();
        arp_tick(n);
        lfo_tick(n);

        float target_cut = conv_cutoff_hz(P(P_F_CUT));
        if (sm_cut < 0) sm_cut = target_cut;
        sm_cut += (target_cut - sm_cut) * 0.25f;

        int dest = (int)P(P_LFO_DEST);
        float amt = P(P_LFO_AMT);
        float pitch_mod = synth_perf.bend + synth_perf.vib * 0.6f * sinf(2 * (float)M_PI * (float)vib_ph);
        float cut_mod = synth_perf.cut;
        float amp_mod = 1.0f;
        if (dest == LFO_D_PITCH) pitch_mod += lfo_out * amt * amt * 12.0f;
        if (dest == LFO_D_CUT) cut_mod += lfo_out * amt * 48.0f;
        if (dest == LFO_D_AMP) amp_mod = 1.0f - amt * (0.5f + 0.5f * lfo_out);

        memset(L, 0, sizeof(float) * n);
        memset(R, 0, sizeof(float) * n);
        for (int v = 0; v < MAX_VOICES; v++)
            if (voices[v].active) render_voice(&voices[v], L, R, n, lfo_out, pitch_mod, cut_mod, synth_perf.pos, amp_mod);

        fx_process(L, R, n);

        float tv = P(P_V_VOL);
        tv *= tv;
        int sw = atomic_load_explicit(&scope_w, memory_order_relaxed);
        for (int i = 0; i < n; i++) {
            sm_vol += (tv - sm_vol) * 0.002f;
            float l = soft(L[i] * sm_vol * 2.0f), r = soft(R[i] * sm_vol * 2.0f);
            out[(done + i) * 2] = l;
            out[(done + i) * 2 + 1] = r;
            scope[sw] = (l + r) * 0.5f;
            sw = (sw + 1) % SCOPE_LEN;
        }
        atomic_store_explicit(&scope_w, sw, memory_order_relaxed);
        done += n;
    }
}

void synth_scope(float *dst, int n)
{
    if (n > SCOPE_LEN) n = SCOPE_LEN;
    int w = atomic_load_explicit(&scope_w, memory_order_relaxed);
    for (int i = 0; i < n; i++) dst[i] = scope[(w - n + i + SCOPE_LEN) % SCOPE_LEN];
}

int synth_active_notes(unsigned char *notes, int max)
{
    int c = 0;
    for (int i = 0; i < MAX_VOICES && c < max; i++)
        if (voices[i].active && voices[i].gate) notes[c++] = (unsigned char)voices[i].note;
    return c;
}

float synth_lfo_value(void) { return lfo_out; }
float synth_env2_value(void) { return voices[last_voice].e2.v; }
