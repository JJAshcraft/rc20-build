/* Bramble screen: 640x480 logical, drawn with SDL_Renderer. */
#include "app.h"
#include "font.h"
#include "preset.h"
#include "synth.h"
#include "wavetable.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define W 640
#define H 480

typedef struct { Uint8 r, g, b; } Col;
static const Col C_BG = {0x10, 0x14, 0x0f}, C_PANEL = {0x1a, 0x21, 0x19}, C_GRID = {0x2c, 0x37, 0x2a},
                 C_TEXT = {0xd8, 0xe6, 0xcf}, C_DIM = {0x7d, 0x8f, 0x74}, C_LEAF = {0x8f, 0xdc, 0x5a},
                 C_BERRY = {0xc7, 0x5b, 0x9b}, C_SUN = {0xe8, 0xb0, 0x4a}, C_DARK = {0x0a, 0x0d, 0x0a};

const char *const page_names[PG_COUNT] = {"OSC A", "OSC B", "SUB+NOISE", "FILTER", "ENVELOPES",
                                          "LFO+VOICE", "EFFECTS", "PLAY", "PRESETS", "INPUT TEST"};
static const int pg_a[] = {P_A_ON, P_A_TABLE, P_A_POS, P_A_SEMI, P_A_FINE, P_A_LEVEL, P_A_UNI, P_A_DET, P_A_BEND};
static const int pg_b[] = {P_B_ON, P_B_TABLE, P_B_POS, P_B_SEMI, P_B_FINE, P_B_LEVEL, P_B_UNI, P_B_DET, P_B_BEND};
static const int pg_s[] = {P_SUB_LVL, P_SUB_SHAPE, P_SUB_OCT, P_NOISE_LVL, P_NOISE_COL};
static const int pg_f[] = {P_F_TYPE, P_F_CUT, P_F_RES, P_F_DRIVE, P_F_KEY, P_F_ENV};
static const int pg_e[] = {P_E1_A, P_E1_D, P_E1_S, P_E1_R, P_E2_A, P_E2_D, P_E2_S, P_E2_R, P_E2_POS};
static const int pg_l[] = {P_LFO_SHAPE, P_LFO_RATE, P_LFO_DEST, P_LFO_AMT, P_V_MODE, P_V_GLIDE, P_V_VOL};
static const int pg_x[] = {P_FX_CHO, P_FX_DLY, P_FX_DTIME, P_FX_DFB, P_FX_REV, P_FX_RSIZE};
static const int pg_p[] = {P_P_SCALE, P_P_ROOT, P_P_OCT, P_P_ARP, P_P_BPM, P_P_CHORD};
const int *const page_params[PG_COUNT] = {pg_a, pg_b, pg_s, pg_f, pg_e, pg_l, pg_x, pg_p, NULL, NULL};
const int page_nparams[PG_COUNT] = {9, 9, 5, 6, 9, 7, 6, 6, 0, 0};

static SDL_Renderer *R;
static SDL_Texture *FONT;

static void color(Col c) { SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255); }
static void fill(int x, int y, int w, int h, Col c)
{
    SDL_Rect r = {x, y, w, h};
    color(c);
    SDL_RenderFillRect(R, &r);
}
static void frame(int x, int y, int w, int h, Col c)
{
    SDL_Rect r = {x, y, w, h};
    color(c);
    SDL_RenderDrawRect(R, &r);
}
static void line(int x0, int y0, int x1, int y1, Col c)
{
    color(c);
    SDL_RenderDrawLine(R, x0, y0, x1, y1);
}
static void text(int x, int y, int s, Col c, const char *str)
{
    SDL_SetTextureColorMod(FONT, c.r, c.g, c.b);
    for (; *str; str++, x += FONT_ADV * s) {
        int ch = (unsigned char)*str;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == ' ') continue;
        SDL_Rect src = {(ch & 127) * 6, 0, 5, 7}, dst = {x, y, 5 * s, 7 * s};
        SDL_RenderCopy(R, FONT, &src, &dst);
    }
}
static int text_w(int s, const char *str) { return (int)strlen(str) * FONT_ADV * s - s; }
static void text_r(int xr, int y, int s, Col c, const char *str) { text(xr - text_w(s, str), y, s, c, str); }
static void text_c(int xc, int y, int s, Col c, const char *str) { text(xc - text_w(s, str) / 2, y, s, c, str); }

void ui_init(App *a)
{
    R = a->ren;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 128 * 6, 8, 32, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 255, 255, 255, 0));
    Uint32 on = SDL_MapRGBA(s->format, 255, 255, 255, 255);
    SDL_LockSurface(s);
    for (int c = 0; c < 128; c++) {
        const unsigned char *g = font_glyph(c);
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 5; x++)
                if (g[y] & (0x10 >> x))
                    *(Uint32 *)((Uint8 *)s->pixels + y * s->pitch + (c * 6 + x) * 4) = on;
    }
    SDL_UnlockSurface(s);
    FONT = SDL_CreateTextureFromSurface(R, s);
    SDL_SetTextureBlendMode(FONT, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
    a->font = FONT;
}

/* ---------- visualisations (panel at px,py size pw x ph) ---------- */
#define PX 352
#define PY 76
#define PW 272
#define PH 224

static void draw_wave(const float *f0, const float *f1, float fm, int x, int y, int w, int h, Col c)
{
    SDL_Point pts[129];
    for (int i = 0; i <= 128; i++) {
        int k = (i * WT_LEN / 128) & (WT_LEN - 1);
        float v = f0[k] + (f1[k] - f0[k]) * fm;
        pts[i].x = x + i * w / 128;
        pts[i].y = y + h / 2 - (int)(v * h * 0.45f);
    }
    color(c);
    SDL_RenderDrawLines(R, pts, 129);
}

static void viz_table(App *a, int osc)
{
    int b = osc ? P_B_ON - P_A_ON : 0;
    int tab = (int)a->vals[P_A_TABLE + b];
    float pos = a->vals[P_A_POS + b];
    /* stacked frames, back to front */
    for (int k = 7; k >= 0; k--) {
        int fr = k * (WT_FRAMES - 1) / 7;
        const float *f = wt_frame(tab, 3, fr);
        int off = k * 10;
        Col c = (fabsf((float)fr / (WT_FRAMES - 1) - pos) < 0.08f) ? C_DIM : C_GRID;
        draw_wave(f, f, 0, PX + 16 + off, PY + 96 - off, PW - 110, 110, c);
    }
    float fp = pos * (WT_FRAMES - 1);
    int fi = (int)fp;
    if (fi >= WT_FRAMES - 1) fi = WT_FRAMES - 2;
    int off = (int)(pos * 70);
    draw_wave(wt_frame(tab, 3, fi), wt_frame(tab, 3, fi + 1), fp - fi, PX + 16 + off, PY + 96 - off, PW - 110, 110,
              a->vals[P_A_ON + b] > 0.5f ? C_LEAF : C_DIM);
    char buf[32];
    snprintf(buf, sizeof buf, "%s  FRAME %d", L_TABLES[tab], (int)lroundf(fp) + 1);
    text(PX + 10, PY + PH - 22, 2, C_DIM, buf);
}

static float filt_mag(int type, float w, float k)
{
    float re = 1 - w * w, im = k * w;
    float den = sqrtf(re * re + im * im) + 1e-9f;
    switch (type) {
    case 0: return 1 / den;
    case 2: return w / den;
    case 3: return w * w / den;
    default: return fabsf(1 - w * w) / den;
    }
}

static void viz_filter(App *a)
{
    int type = (int)a->vals[P_F_TYPE];
    float fc = conv_cutoff_hz(a->vals[P_F_CUT]), res = a->vals[P_F_RES];
    for (int d = -24; d <= 12; d += 12) {
        int y = PY + 30 + (int)((12 - d) * 4.0f);
        line(PX + 8, y, PX + PW - 8, y, C_GRID);
    }
    for (int f = 100; f <= 10000; f *= 10) {
        int x = PX + 8 + (int)(logf(f / 20.0f) / logf(1000.0f) * (PW - 16));
        line(x, PY + 20, x, PY + PH - 30, C_GRID);
    }
    SDL_Point pts[PW];
    int n = 0;
    for (int i = 0; i < PW - 16; i++) {
        float f = 20.0f * powf(1000.0f, (float)i / (PW - 17));
        float w = f / fc, m;
        if (type == 1) m = filt_mag(0, w, 1.4142f) * filt_mag(0, w, 2.0f - 1.94f * res);
        else m = filt_mag(type == 4 ? 4 : type == 0 ? 0 : type, w, 2.0f - 1.96f * res);
        float db = 20 * log10f(m + 1e-6f);
        if (db < -30) db = -30;
        if (db > 24) db = 24;
        pts[n].x = PX + 8 + i;
        pts[n].y = PY + 30 + (int)((12 - db) * 4.0f);
        n++;
    }
    color(C_LEAF);
    SDL_RenderDrawLines(R, pts, n);
    char buf[32];
    param_format(P_F_CUT, a->vals[P_F_CUT], buf, sizeof buf);
    text(PX + 10, PY + PH - 22, 2, C_DIM, L_FTYPES[type]);
    text_r(PX + PW - 10, PY + PH - 22, 2, C_DIM, buf);
}

static void env_shape(float an, float dn, float s, float rn, int x, int y, int w, int h, Col c)
{
    float wa = 10 + an * 50, wd = 10 + dn * 50, ws = 40, wr = 10 + rn * 50;
    float scale = (w - 4) / (wa + wd + ws + wr);
    wa *= scale; wd *= scale; ws *= scale; wr *= scale;
    SDL_Point p[64];
    int n = 0;
    p[n++] = (SDL_Point){x, y + h};
    p[n++] = (SDL_Point){x + (int)wa, y};
    for (int i = 1; i <= 16; i++) {
        float t = i / 16.0f, v = s + (1 - s) * expf(-4.6f * t);
        p[n++] = (SDL_Point){x + (int)(wa + wd * t), y + h - (int)(v * h)};
    }
    p[n++] = (SDL_Point){x + (int)(wa + wd + ws), y + h - (int)(s * h)};
    for (int i = 1; i <= 16; i++) {
        float t = i / 16.0f, v = s * expf(-4.6f * t);
        p[n++] = (SDL_Point){x + (int)(wa + wd + ws + wr * t), y + h - (int)(v * h)};
    }
    color(c);
    SDL_RenderDrawLines(R, p, n);
}

static void viz_env(App *a)
{
    text(PX + 10, PY + 10, 2, C_LEAF, "AMP");
    env_shape(a->vals[P_E1_A], a->vals[P_E1_D], a->vals[P_E1_S], a->vals[P_E1_R], PX + 10, PY + 30, PW - 20, 70,
              C_LEAF);
    text(PX + 10, PY + 116, 2, C_BERRY, "ENV2");
    env_shape(a->vals[P_E2_A], a->vals[P_E2_D], a->vals[P_E2_S], a->vals[P_E2_R], PX + 10, PY + 136, PW - 20, 70,
              C_BERRY);
}

static void viz_lfo(App *a)
{
    int shape = (int)a->vals[P_LFO_SHAPE];
    SDL_Point p[PW];
    int n = 0, h = 140, y0 = PY + 20;
    unsigned seed = 7;
    float hold = 0;
    for (int i = 0; i < PW - 20; i++) {
        float ph = fmodf(i / (float)(PW - 20) * 2.0f, 1.0f), v;
        switch (shape) {
        case 0: v = sinf(6.2831853f * ph); break;
        case 1: v = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph; break;
        case 2: v = 1 - 2 * ph; break;
        case 3: v = ph < 0.5f ? 1 : -1; break;
        default:
            if (i % ((PW - 20) / 8) == 0) { seed = seed * 1103515245u + 12345u; hold = ((seed >> 16) & 255) / 127.5f - 1; }
            v = hold;
        }
        p[n].x = PX + 10 + i;
        p[n].y = y0 + h / 2 - (int)(v * h * 0.42f);
        n++;
    }
    line(PX + 10, y0 + h / 2, PX + PW - 10, y0 + h / 2, C_GRID);
    color(C_BERRY);
    SDL_RenderDrawLines(R, p, n);
    float lv = synth_lfo_value();
    int bx = PX + PW - 30, by = y0 + h / 2 - (int)(lv * h * 0.42f);
    fill(bx - 5, by - 5, 10, 10, C_SUN);
    char buf[48];
    param_format(P_LFO_RATE, a->vals[P_LFO_RATE], buf, sizeof buf);
    text(PX + 10, PY + PH - 22, 2, C_DIM, L_LFODEST[(int)a->vals[P_LFO_DEST]]);
    text_r(PX + PW - 10, PY + PH - 22, 2, C_DIM, buf);
}

static void viz_scope(void)
{
    float s[SCOPE_LEN];
    synth_scope(s, SCOPE_LEN);
    int start = 0;
    for (int i = 1; i < SCOPE_LEN - 300; i++)
        if (s[i - 1] <= 0 && s[i] > 0) { start = i; break; }
    SDL_Point p[PW];
    int n = 0, mid = PY + PH / 2 - 10;
    line(PX + 10, mid, PX + PW - 10, mid, C_GRID);
    for (int i = 0; i < PW - 20; i++) {
        float v = s[start + i];
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        p[n].x = PX + 10 + i;
        p[n].y = mid - (int)(v * 90);
        n++;
    }
    color(C_LEAF);
    SDL_RenderDrawLines(R, p, n);
}

static void viz_preset(App *a)
{
    int s = a->preset_sel;
    char buf[48];
    snprintf(buf, sizeof buf, "SLOT %d", s + 1);
    text(PX + 10, PY + 10, 2, C_DIM, buf);
    if (!presets[s].used) {
        text(PX + 10, PY + 40, 3, C_DIM, "EMPTY");
        text(PX + 10, PY + 80, 2, C_DIM, "X: SAVE HERE");
        return;
    }
    const Preset *p = &presets[s];
    text(PX + 10, PY + 40, 3, C_LEAF, p->name);
    int y = PY + 84;
    snprintf(buf, sizeof buf, "A: %s", p->v[P_A_ON] > 0.5f ? L_TABLES[(int)p->v[P_A_TABLE]] : "OFF");
    text(PX + 10, y, 2, C_TEXT, buf); y += 22;
    snprintf(buf, sizeof buf, "B: %s", p->v[P_B_ON] > 0.5f ? L_TABLES[(int)p->v[P_B_TABLE]] : "OFF");
    text(PX + 10, y, 2, C_TEXT, buf); y += 22;
    snprintf(buf, sizeof buf, "FILTER: %s", L_FTYPES[(int)p->v[P_F_TYPE]]);
    text(PX + 10, y, 2, C_TEXT, buf); y += 22;
    snprintf(buf, sizeof buf, "VOICE: %s", L_VMODE[(int)p->v[P_V_MODE]]);
    text(PX + 10, y, 2, C_TEXT, buf); y += 22;
    if ((int)p->v[P_P_ARP]) {
        snprintf(buf, sizeof buf, "ARP: %s", L_ARP[(int)p->v[P_P_ARP]]);
        text(PX + 10, y, 2, C_TEXT, buf);
    }
}

static const char *const btn_names[BTN_COUNT] = {"UP", "DN", "LT", "RT", "A", "B", "X", "Y", "L1", "R1",
                                                 "L2", "R2", "L3", "R3", "SE", "ST", "FN"};

static void page_input(App *a)
{
    char buf[64];
    snprintf(buf, sizeof buf, "PAD: %.28s", a->pad_name ? a->pad_name : "NONE (KEYBOARD)");
    text(24, 80, 2, C_TEXT, buf);
    for (int i = 0; i < BTN_COUNT; i++) {
        int x = 24 + (i % 6) * 52, y = 110 + (i / 6) * 40;
        fill(x, y, 46, 32, a->btn[i] ? C_LEAF : C_PANEL);
        text_c(x + 23, y + 9, 2, a->btn[i] ? C_DARK : C_DIM, btn_names[i]);
    }
    for (int i = 0; i < AX_COUNT; i++) {
        static const char *an[] = {"LX", "LY", "RX", "RY"};
        int y = 240 + i * 22;
        text(24, y, 2, C_DIM, an[i]);
        fill(60, y + 2, 260, 10, C_PANEL);
        fill(190 + (int)(a->ax[i] * 128) - 3, y, 6, 14, C_SUN);
    }
    snprintf(buf, sizeof buf, "RAW BUTTON %d  HAT %d", a->raw_btn, a->raw_hat);
    text(PX + 10, PY + 10, 2, C_TEXT, buf);
    snprintf(buf, sizeof buf, "RAW AXIS %d = %+.2f", a->raw_axis, a->raw_axis_val);
    text(PX + 10, PY + 34, 2, C_TEXT, buf);
    text(PX + 10, PY + 70, 2, C_DIM, "PRESS ANYTHING.");
    text(PX + 10, PY + 92, 2, C_DIM, "RAW NUMBERS GO IN");
    text(PX + 10, PY + 114, 2, C_DIM, "BRAMBLE.CFG IF A");
    text(PX + 10, PY + 136, 2, C_DIM, "BUTTON IS WRONG.");
}

/* ---------- main pieces ---------- */
static void draw_header(App *a)
{
    fill(0, 0, W, 66, C_DARK);
    text(16, 12, 3, C_LEAF, "BRAMBLE");
    text(176, 12, 3, C_TEXT, page_names[a->page]);
    const char *mode = a->mode_play ? "PLAY" : "EDIT";
    fill(W - 96, 8, 80, 28, a->mode_play ? C_BERRY : C_LEAF);
    text_c(W - 56, 12, 3, C_DARK, mode);
    char buf[64];
    snprintf(buf, sizeof buf, "%02d %s", a->preset_cur + 1, a->preset_name);
    text(16, 44, 2, C_DIM, buf);
    int x = W - 16;
    snprintf(buf, sizeof buf, "CPU %d%%", (int)(a->cpu * 100));
    text_r(x, 44, 2, a->cpu > 0.8f ? C_SUN : C_DIM, buf);
    x -= text_w(2, buf) + 16;
    if ((int)a->vals[P_P_ARP]) { text_r(x, 44, 2, C_SUN, "ARP"); x -= 52; }
    if (a->hold) { text_r(x, 44, 2, C_SUN, "HOLD"); x -= 64; }
    /* page dots */
    for (int i = 0; i < PG_COUNT; i++) fill(176 + i * 14, 38, 8, 4, i == a->page ? C_LEAF : C_GRID);
}

static void draw_params(App *a)
{
    const int *ids = page_params[a->page];
    int n = page_nparams[a->page];
    char buf[32];
    for (int r = 0; r < n; r++) {
        int id = ids[r], y = 76 + r * 28;
        const ParamDef *d = &param_defs[id];
        int sel = r == a->sel[a->page];
        if (sel) {
            fill(16, y - 2, 326, 26, C_PANEL);
            fill(16, y - 2, 4, 26, a->mode_play ? C_DIM : C_LEAF);
        }
        text(28, y + 2, 2, sel ? C_TEXT : C_DIM, d->label);
        param_format(id, a->vals[id], buf, sizeof buf);
        text_r(334, y + 2, 2, sel ? C_LEAF : C_TEXT, buf);
        float nrm = (a->vals[id] - d->min) / (d->max - d->min);
        fill(28, y + 19, 306, 2, C_GRID);
        if (d->min < 0 && d->max > 0) {
            int c = 28 + 153, e = 28 + (int)(nrm * 306);
            fill(e < c ? e : c, y + 19, e < c ? c - e : e - c, 2, sel ? C_LEAF : C_DIM);
        } else fill(28, y + 19, (int)(nrm * 306), 2, sel ? C_LEAF : C_DIM);
    }
}

static void draw_preset_list(App *a)
{
    int top = a->preset_sel - 4;
    if (top < 0) top = 0;
    if (top > PRESET_SLOTS - 9) top = PRESET_SLOTS - 9;
    char buf[40];
    for (int r = 0; r < 9; r++) {
        int s = top + r, y = 76 + r * 28;
        int sel = s == a->preset_sel;
        if (sel) {
            fill(16, y - 2, 326, 26, C_PANEL);
            fill(16, y - 2, 4, 26, C_LEAF);
        }
        snprintf(buf, sizeof buf, "%02d %s", s + 1, presets[s].used ? presets[s].name : "---");
        text(28, y + 2, 2, s == a->preset_cur ? C_LEAF : sel ? C_TEXT : C_DIM, buf);
    }
}

static void draw_keys(App *a)
{
    static const char *lab[8] = {"{", "^", "}", "~", "Y", "X", "A", "B"};
    static const char *nn[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    unsigned char act[MAX_VOICES];
    int na = synth_active_notes(act, MAX_VOICES);
    int scale = (int)a->vals[P_P_SCALE], root = (int)a->vals[P_P_ROOT], oct = (int)a->vals[P_P_OCT];
    for (int k = 0; k < 8; k++) {
        int note = 12 * (oct + 1) + root + scale_degree(scale, k);
        int lit = a->notes_n[k] > 0;
        for (int i = 0; i < na && !lit; i++) lit = act[i] == note;
        int x = 19 + k * 76, y = 368;
        fill(x, y, 70, 58, lit ? C_LEAF : C_PANEL);
        if (!a->mode_play) frame(x, y, 70, 58, C_GRID);
        char buf[16];
        snprintf(buf, sizeof buf, "%s%d", nn[note % 12], note / 12 - 1);
        text_c(x + 35, y + 8, 3, lit ? C_DARK : a->mode_play ? C_TEXT : C_DIM, buf);
        text_c(x + 35, y + 38, 2, lit ? C_DARK : C_DIM, lab[k]);
    }
    line(19 + 4 * 76 - 4, 372, 19 + 4 * 76 - 4, 422, C_GRID);
}

static void draw_footer(App *a)
{
    const char *h;
    if (a->page == PG_PRESET) h = "A:LOAD  X:SAVE  ^~:SLOT  START:BACK";
    else if (a->page == PG_INPUT) h = "L1/R1:PAGE  START+SELECT:QUIT";
    else if (a->mode_play) h = "SEL:EDIT L1R1:OCT L2:HOLD R2:ARP L3R3:SOUND";
    else h = "SEL:PLAY L1R1:PAGE {}:SET B+:FAST A:HEAR Y:RESET";
    text(16, 446, 2, C_DIM, h);
}

void ui_splash(App *a)
{
    (void)a;
    color(C_BG);
    SDL_RenderClear(R);
    text_c(W / 2, 190, 6, C_LEAF, "BRAMBLE");
    text_c(W / 2, 250, 2, C_DIM, "GROWING WAVETABLES...");
    SDL_RenderPresent(R);
}

void ui_draw(App *a)
{
    color(C_BG);
    SDL_RenderClear(R);
    draw_header(a);

    if (a->page == PG_INPUT) page_input(a);
    else if (a->page == PG_PRESET) draw_preset_list(a);
    else draw_params(a);

    if (a->page != PG_INPUT) {
        fill(PX, PY, PW, PH, C_DARK);
        frame(PX, PY, PW, PH, C_GRID);
        switch (a->page) {
        case PG_OSCA: viz_table(a, 0); break;
        case PG_OSCB: viz_table(a, 1); break;
        case PG_FILTER: viz_filter(a); break;
        case PG_ENV: viz_env(a); break;
        case PG_LFO: viz_lfo(a); break;
        case PG_PRESET: viz_preset(a); break;
        default: viz_scope(); break;
        }
    }
    /* big readout of the selected parameter */
    if (page_nparams[a->page] > 0) {
        int id = page_params[a->page][a->sel[a->page]];
        char buf[32];
        param_format(id, a->vals[id], buf, sizeof buf);
        text(PX, 312, 2, C_DIM, param_defs[id].label);
        text_r(PX + PW, 330, 3, C_LEAF, buf);
    }

    draw_keys(a);
    draw_footer(a);

    if (a->toast[0] && SDL_GetTicks() < a->toast_until) {
        int w = text_w(3, a->toast) + 40;
        fill((W - w) / 2, 200, w, 50, C_SUN);
        text_c(W / 2, 214, 3, C_DARK, a->toast);
    }
    SDL_RenderPresent(R);
}
