/* Bramble: a wavetable synth for the R36S (and other SDL2 Linux handhelds). */
#include "app.h"
#include "preset.h"
#include "synth.h"
#include "wavetable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static App app;
static int running = 1;
static char data_dir[512] = ".";
static char path_presets[600], path_state[600], path_cfg[600];

/* ---------- config ---------- */
static int raw_map[BTN_COUNT] = {
    /* raw joystick fallback: a guess for the R36S / OGA-style gamepad;
     * check the INPUT TEST page and override in bramble.cfg */
    8, 9, 10, 11,   /* up down left right */
    1, 0, 2, 3,     /* a b x y */
    4, 5, 6, 7,     /* l1 r1 l2 r2 */
    14, 15,         /* l3 r3 */
    12, 13, 16,     /* select start fn */
};
static int raw_axis[AX_COUNT] = {0, 1, 2, 3};
static int cfg_force_raw, cfg_swap_ab, cfg_swap_xy, cfg_buffer = 512, cfg_invert_ry;
static const char *cfg_keys[BTN_COUNT] = {"up", "down", "left", "right", "a", "b", "x", "y", "l1", "r1",
                                          "l2", "r2", "l3", "r3", "select", "start", "fn"};

static void load_cfg(void)
{
    FILE *f = fopen(path_cfg, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n#")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        while (*k == ' ') k++;
        for (char *e = k + strlen(k); e > k && e[-1] == ' '; ) *--e = 0;
        int n = atoi(v);
        if (!strcmp(k, "force_raw")) cfg_force_raw = n;
        else if (!strcmp(k, "swap_ab")) cfg_swap_ab = n;
        else if (!strcmp(k, "swap_xy")) cfg_swap_xy = n;
        else if (!strcmp(k, "audio_buffer")) cfg_buffer = n;
        else if (!strcmp(k, "invert_ry")) cfg_invert_ry = n;
        else if (!strncmp(k, "raw_axis_", 9)) {
            static const char *an[] = {"lx", "ly", "rx", "ry"};
            for (int i = 0; i < AX_COUNT; i++) if (!strcmp(k + 9, an[i])) raw_axis[i] = n;
        } else if (!strncmp(k, "raw_", 4)) {
            for (int i = 0; i < BTN_COUNT; i++) if (!strcmp(k + 4, cfg_keys[i])) raw_map[i] = n;
        }
    }
    fclose(f);
}

/* ---------- parameters ---------- */
static void toast(const char *msg)
{
    snprintf(app.toast, sizeof app.toast, "%s", msg);
    app.toast_until = SDL_GetTicks() + 1400;
}

static void set_param(int id, float v)
{
    v = param_clamp(id, v);
    app.vals[id] = v;
    synth_params[id] = v;
}

static void push_all(void)
{
    for (int i = 0; i < P_COUNT; i++) synth_params[i] = app.vals[i];
}

static void load_preset(int slot)
{
    if (slot < 0 || slot >= PRESET_SLOTS || !presets[slot].used) return;
    preset_apply(slot, app.vals);
    push_all();
    app.preset_cur = slot;
    snprintf(app.preset_name, sizeof app.preset_name, "%s", presets[slot].name);
    if ((int)app.vals[P_P_ARP]) app.last_arp = (int)app.vals[P_P_ARP];
}

static void step_preset(int dir)
{
    for (int i = 1; i <= PRESET_SLOTS; i++) {
        int s = (app.preset_cur + dir * i + PRESET_SLOTS * 2) % PRESET_SLOTS;
        if (presets[s].used) {
            load_preset(s);
            char b[40];
            snprintf(b, sizeof b, "%02d %s", s + 1, presets[s].name);
            toast(b);
            return;
        }
    }
}

/* ---------- notes ---------- */
static int degree_note(int k)
{
    int scale = (int)app.vals[P_P_SCALE];
    return 12 * ((int)app.vals[P_P_OCT] + 1) + (int)app.vals[P_P_ROOT] + scale_degree(scale, k);
}

static void note_button(int k, int down)
{
    if (down) {
        if (app.notes_n[k]) return;
        int ch = (int)app.vals[P_P_CHORD], n = 0, *out = app.notes_on[k];
        out[n++] = degree_note(k);
        if (ch == 1 || ch == 2) { out[n++] = degree_note(k + 2); out[n++] = degree_note(k + 4); }
        if (ch == 2) out[n++] = degree_note(k + 6);
        if (ch == 3) out[n++] = out[0] + 7;
        for (int i = 0; i < n; i++)
            if (out[i] >= 0 && out[i] < 128) synth_key_on(out[i]);
        app.notes_n[k] = n;
    } else {
        for (int i = 0; i < app.notes_n[k]; i++) synth_key_off(app.notes_on[k][i]);
        app.notes_n[k] = 0;
    }
}

static void release_all_buttons(void)
{
    for (int k = 0; k < 8; k++) note_button(k, 0);
}

/* ---------- controls ---------- */
static int prev_page = PG_OSCA;
static Uint32 rep_next[2];
static int rep_held[2];
static Uint32 rep_start[2];

static void adjust(int dir, int fast)
{
    int n = page_nparams[app.page];
    if (!n) return;
    int id = page_params[app.page][app.sel[app.page]];
    const ParamDef *d = &param_defs[id];
    float step = d->step;
    if (fast) step *= (d->disp == D_ENUM || d->disp == D_INT || d->disp == D_NOTE) ? 1 : 10;
    float v = app.vals[id] + dir * step;
    if (d->disp == D_ENUM || d->disp == D_NOTE) { /* enums wrap */
        int cnt = (int)(d->max - d->min) + 1;
        int iv = ((int)lroundf(app.vals[id] - d->min) + dir + cnt) % cnt;
        v = d->min + iv;
    }
    set_param(id, v);
}

static void change_page(int dir)
{
    release_all_buttons();
    app.page = (app.page + dir + PG_COUNT) % PG_COUNT;
    app.save_armed = -1;
}

static int deg_for_button(int b)
{
    switch (b) {
    case BTN_LEFT: return 0;
    case BTN_UP: return 1;
    case BTN_RIGHT: return 2;
    case BTN_DOWN: return 3;
    case BTN_Y: return 4;
    case BTN_X: return 5;
    case BTN_A: return 6;
    case BTN_B: return 7;
    default: return -1;
    }
}

static int audition[2][4], audition_n[2];

static void button(int b, int down)
{
    if (b < 0 || b >= BTN_COUNT) return;
    if (app.btn[b] == down) return;
    app.btn[b] = down;

    if (down && ((b == BTN_SELECT && app.btn[BTN_START]) || (b == BTN_START && app.btn[BTN_SELECT]))) {
        running = 0;
        return;
    }

    int nav = app.page == PG_PRESET || app.page == PG_INPUT;

    if (b == BTN_SELECT && down) {
        release_all_buttons();
        if (nav) app.page = prev_page;
        app.mode_play = !app.mode_play;
        return;
    }
    if (b == BTN_START && down) {
        release_all_buttons();
        if (app.page == PG_PRESET) app.page = prev_page;
        else { prev_page = app.page == PG_INPUT ? PG_OSCA : app.page; app.page = PG_PRESET; app.preset_sel = app.preset_cur; }
        app.save_armed = -1;
        return;
    }
    if ((b == BTN_L3 || b == BTN_R3) && down && app.page != PG_INPUT) {
        release_all_buttons();
        step_preset(b == BTN_L3 ? -1 : 1);
        return;
    }

    if (app.page == PG_INPUT) {
        if (down && (b == BTN_L1 || b == BTN_R1)) change_page(b == BTN_L1 ? -1 : 1);
        return;
    }

    if (app.page == PG_PRESET) {
        if (!down) return;
        switch (b) {
        case BTN_UP: app.preset_sel = (app.preset_sel + PRESET_SLOTS - 1) % PRESET_SLOTS; app.save_armed = -1; break;
        case BTN_DOWN: app.preset_sel = (app.preset_sel + 1) % PRESET_SLOTS; app.save_armed = -1; break;
        case BTN_LEFT: app.preset_sel = (app.preset_sel + PRESET_SLOTS - 8) % PRESET_SLOTS; app.save_armed = -1; break;
        case BTN_RIGHT: app.preset_sel = (app.preset_sel + 8) % PRESET_SLOTS; app.save_armed = -1; break;
        case BTN_A:
            if (presets[app.preset_sel].used) { load_preset(app.preset_sel); toast("LOADED"); }
            break;
        case BTN_X:
            if (app.save_armed != app.preset_sel) { app.save_armed = app.preset_sel; toast("X AGAIN TO SAVE"); }
            else {
                char name[PRESET_NAME + 8];
                if (presets[app.preset_sel].used) snprintf(name, sizeof name, "%.15s", presets[app.preset_sel].name);
                else snprintf(name, sizeof name, "USER %02d", app.preset_sel + 1);
                preset_capture(app.preset_sel, app.vals, name);
                app.preset_cur = app.preset_sel;
                snprintf(app.preset_name, sizeof app.preset_name, "%s", name);
                toast(presets_save(path_presets) == 0 ? "SAVED" : "SAVE FAILED");
                app.save_armed = -1;
            }
            break;
        case BTN_B: app.page = prev_page; break;
        case BTN_L1: case BTN_R1: change_page(b == BTN_L1 ? -1 : 1); break;
        }
        return;
    }

    if (app.mode_play) {
        int k = deg_for_button(b);
        if (k >= 0) { note_button(k, down); return; }
        if (!down) return;
        switch (b) {
        case BTN_L1: case BTN_R1: {
            set_param(P_P_OCT, app.vals[P_P_OCT] + (b == BTN_L1 ? -1 : 1));
            char m[24];
            snprintf(m, sizeof m, "OCTAVE %d", (int)app.vals[P_P_OCT]);
            toast(m);
            break;
        }
        case BTN_L2:
            app.hold = !app.hold;
            synth_perf.hold = app.hold;
            toast(app.hold ? "HOLD ON" : "HOLD OFF");
            break;
        case BTN_R2:
            if ((int)app.vals[P_P_ARP]) { app.last_arp = (int)app.vals[P_P_ARP]; set_param(P_P_ARP, 0); toast("ARP OFF"); }
            else { set_param(P_P_ARP, app.last_arp ? app.last_arp : 1); toast("ARP ON"); }
            break;
        }
        return;
    }

    /* edit mode */
    int n = page_nparams[app.page];
    switch (b) {
    case BTN_UP: if (down && n) app.sel[app.page] = (app.sel[app.page] + n - 1) % n; break;
    case BTN_DOWN: if (down && n) app.sel[app.page] = (app.sel[app.page] + 1) % n; break;
    case BTN_LEFT: case BTN_RIGHT: {
        int i = b == BTN_RIGHT;
        rep_held[i] = down;
        if (down) {
            adjust(i ? 1 : -1, app.btn[BTN_B]);
            rep_start[i] = SDL_GetTicks();
            rep_next[i] = rep_start[i] + 320;
        }
        break;
    }
    case BTN_L1: case BTN_R1: if (down) change_page(b == BTN_L1 ? -1 : 1); break;
    case BTN_Y:
        if (down && n) {
            int id = page_params[app.page][app.sel[app.page]];
            set_param(id, param_defs[id].def);
        }
        break;
    case BTN_A: case BTN_X: {
        int i = b == BTN_X;
        if (down) {
            int root = degree_note(0);
            audition_n[i] = 0;
            audition[i][audition_n[i]++] = root;
            if (i) { audition[i][audition_n[i]++] = degree_note(2); audition[i][audition_n[i]++] = degree_note(4); }
            for (int k = 0; k < audition_n[i]; k++) synth_key_on(audition[i][k]);
        } else {
            for (int k = 0; k < audition_n[i]; k++) synth_key_off(audition[i][k]);
            audition_n[i] = 0;
        }
        break;
    }
    case BTN_L2: if (down) { app.hold = !app.hold; synth_perf.hold = app.hold; toast(app.hold ? "HOLD ON" : "HOLD OFF"); } break;
    default: break;
    }
}

static void edit_repeat(void)
{
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < 2; i++) {
        if (!rep_held[i] || app.mode_play || app.page >= PG_PRESET) { rep_held[i] = 0; continue; }
        if (now >= rep_next[i]) {
            int fast = app.btn[BTN_B] || now - rep_start[i] > 1500;
            adjust(i ? 1 : -1, fast);
            rep_next[i] = now + 60;
        }
    }
}

static float dz(float v) { return fabsf(v) < 0.15f ? 0 : (v - (v > 0 ? 0.15f : -0.15f)) / 0.85f; }

static void sticks(float dt)
{
    float lx = dz(app.ax[AX_LX]), ly = dz(app.ax[AX_LY]), rx = dz(app.ax[AX_RX]), ry = dz(app.ax[AX_RY]);
    if (cfg_invert_ry) ry = -ry;
    synth_perf.bend = rx * 2.0f;
    synth_perf.cut = -ry * 36.0f;
    if (app.mode_play || app.page >= PG_PRESET) {
        synth_perf.pos = lx * 0.5f;
        synth_perf.vib = ly < 0 ? -ly : 0;
    } else {
        synth_perf.pos = 0;
        synth_perf.vib = 0;
        int n = page_nparams[app.page];
        if (n && fabsf(lx) > 0.2f) {
            int id = page_params[app.page][app.sel[app.page]];
            const ParamDef *d = &param_defs[id];
            if (d->disp != D_ENUM && d->disp != D_NOTE) {
                static float acc[P_COUNT];
                acc[id] += lx * lx * lx * (d->max - d->min) * 0.6f * dt;
                if (fabsf(acc[id]) >= d->step * 0.999f) {
                    float s = truncf(acc[id] / d->step) * d->step;
                    acc[id] -= s;
                    set_param(id, app.vals[id] + s);
                }
            }
        }
    }
}

/* ---------- SDL input ---------- */
static SDL_GameController *pad;
static SDL_Joystick *joy;
static Uint8 hat_state;

static void open_pads(void)
{
    if (pad || joy) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!cfg_force_raw && SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) { app.pad_name = SDL_GameControllerName(pad); return; }
        }
    }
    if (SDL_NumJoysticks() > 0) {
        joy = SDL_JoystickOpen(0);
        if (joy) app.pad_name = SDL_JoystickName(joy);
    }
}

static int map_pad_button(int sb)
{
    int b;
    switch (sb) {
    case SDL_CONTROLLER_BUTTON_A: b = BTN_B; break;   /* south */
    case SDL_CONTROLLER_BUTTON_B: b = BTN_A; break;   /* east */
    case SDL_CONTROLLER_BUTTON_X: b = BTN_Y; break;   /* west */
    case SDL_CONTROLLER_BUTTON_Y: b = BTN_X; break;   /* north */
    case SDL_CONTROLLER_BUTTON_BACK: return BTN_SELECT;
    case SDL_CONTROLLER_BUTTON_START: return BTN_START;
    case SDL_CONTROLLER_BUTTON_GUIDE: return BTN_FN;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return BTN_L3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return BTN_R3;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return BTN_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_R1;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    default: return -1;
    }
    if (cfg_swap_ab && (b == BTN_A || b == BTN_B)) b = b == BTN_A ? BTN_B : BTN_A;
    if (cfg_swap_xy && (b == BTN_X || b == BTN_Y)) b = b == BTN_X ? BTN_Y : BTN_X;
    return b;
}

static int map_key(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP: return BTN_UP;
    case SDLK_DOWN: return BTN_DOWN;
    case SDLK_LEFT: return BTN_LEFT;
    case SDLK_RIGHT: return BTN_RIGHT;
    case SDLK_x: return BTN_A;
    case SDLK_z: return BTN_B;
    case SDLK_s: return BTN_X;
    case SDLK_a: return BTN_Y;
    case SDLK_q: return BTN_L1;
    case SDLK_w: return BTN_R1;
    case SDLK_1: return BTN_L2;
    case SDLK_2: return BTN_R2;
    case SDLK_3: return BTN_L3;
    case SDLK_4: return BTN_R3;
    case SDLK_BACKSPACE: case SDLK_RSHIFT: return BTN_SELECT;
    case SDLK_RETURN: return BTN_START;
    default: return -1;
    }
}

static void handle_event(SDL_Event *e)
{
    switch (e->type) {
    case SDL_QUIT: running = 0; break;
    case SDL_KEYDOWN: case SDL_KEYUP:
        if (e->key.repeat) break;
        if (e->key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
        button(map_key(e->key.keysym.sym), e->type == SDL_KEYDOWN);
        break;
    case SDL_CONTROLLERDEVICEADDED: case SDL_JOYDEVICEADDED: open_pads(); break;
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
        app.raw_btn = e->cbutton.button;
        button(map_pad_button(e->cbutton.button), e->type == SDL_CONTROLLERBUTTONDOWN);
        break;
    case SDL_CONTROLLERAXISMOTION: {
        float v = e->caxis.value / 32767.0f;
        app.raw_axis = e->caxis.axis;
        app.raw_axis_val = v;
        switch (e->caxis.axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: app.ax[AX_LX] = v; break;
        case SDL_CONTROLLER_AXIS_LEFTY: app.ax[AX_LY] = v; break;
        case SDL_CONTROLLER_AXIS_RIGHTX: app.ax[AX_RX] = v; break;
        case SDL_CONTROLLER_AXIS_RIGHTY: app.ax[AX_RY] = v; break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: button(BTN_L2, v > 0.5f); break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: button(BTN_R2, v > 0.5f); break;
        }
        break;
    }
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
        if (pad || !joy) break;
        app.raw_btn = e->jbutton.button;
        for (int i = 0; i < BTN_COUNT; i++)
            if (raw_map[i] == e->jbutton.button) button(i, e->type == SDL_JOYBUTTONDOWN);
        break;
    case SDL_JOYAXISMOTION:
        if (pad || !joy) break;
        app.raw_axis = e->jaxis.axis;
        app.raw_axis_val = e->jaxis.value / 32767.0f;
        for (int i = 0; i < AX_COUNT; i++)
            if (raw_axis[i] == e->jaxis.axis) app.ax[i] = e->jaxis.value / 32767.0f;
        break;
    case SDL_JOYHATMOTION: {
        if (pad || !joy) break;
        Uint8 h = e->jhat.value;
        app.raw_hat = h;
        static const struct { Uint8 m; int b; } hm[4] = {
            {SDL_HAT_UP, BTN_UP}, {SDL_HAT_DOWN, BTN_DOWN}, {SDL_HAT_LEFT, BTN_LEFT}, {SDL_HAT_RIGHT, BTN_RIGHT}};
        for (int i = 0; i < 4; i++)
            if ((h ^ hat_state) & hm[i].m) button(hm[i].b, (h & hm[i].m) != 0);
        hat_state = h;
        break;
    }
    }
}

/* ---------- audio ---------- */
static float cpu_acc;

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    Uint64 t0 = SDL_GetPerformanceCounter();
    int frames = len / (int)(2 * sizeof(float));
    synth_render((float *)stream, frames);
    double used = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();
    double avail = (double)frames / 48000.0;
    cpu_acc += (float)((used / avail) - cpu_acc) * 0.1f;
}

/* ---------- main ---------- */
static void usage(void)
{
    printf("bramble [--data DIR] [--window]\n"
           "        [--shot FILE.bmp [--page N] [--play] [--chord] [--script \"r1 right +a -a\"]]\n");
}

int main(int argc, char **argv)
{
    int windowed = 0, shot_page = -1, shot_play = 0, shot_chord = 0;
    const char *shot = NULL, *shot_script = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(data_dir, sizeof data_dir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--window")) windowed = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--page") && i + 1 < argc) shot_page = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--play")) shot_play = 1;
        else if (!strcmp(argv[i], "--chord")) shot_chord = 1;
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) shot_script = argv[++i];
        else { usage(); return argv[i][1] == 'h' ? 0 : 1; }
    }
    if (getenv("BRAMBLE_HOME") && !strcmp(data_dir, "."))
        snprintf(data_dir, sizeof data_dir, "%s", getenv("BRAMBLE_HOME"));
    snprintf(path_presets, sizeof path_presets, "%s/bramble_presets.txt", data_dir);
    snprintf(path_state, sizeof path_state, "%s/bramble_state.txt", data_dir);
    snprintf(path_cfg, sizeof path_cfg, "%s/bramble.cfg", data_dir);
    load_cfg();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);

    /* video */
    Uint32 wflags = windowed || shot ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_Window *win = SDL_CreateWindow("Bramble", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, wflags);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, shot ? SDL_RENDERER_SOFTWARE
                                                       : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, 640, 480);
    app.ren = ren;
    ui_init(&app);
    ui_splash(&app);

    if (wt_init() || synth_init(48000.0f)) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    /* audio */
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = (Uint16)cfg_buffer;
    want.callback = audio_cb;
    SDL_AudioDeviceID dev = shot ? 0 : SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev && !shot) fprintf(stderr, "no audio: %s\n", SDL_GetError());

    /* sounds and state */
    if (presets_load(path_presets) != 0) presets_factory();
    for (int i = 0; i < P_COUNT; i++) app.vals[i] = param_defs[i].def;
    app.save_armed = -1;
    app.last_arp = 1;
    int slot = 1;
    if (!shot && state_load(path_state, app.vals, &slot) == 0) {
        app.preset_cur = slot >= 0 && slot < PRESET_SLOTS ? slot : 0;
        snprintf(app.preset_name, sizeof app.preset_name, "%s",
                 presets[app.preset_cur].used ? presets[app.preset_cur].name : "");
        push_all();
    } else load_preset(presets[slot].used ? slot : 0);
    if ((int)app.vals[P_P_ARP]) app.last_arp = (int)app.vals[P_P_ARP];

    open_pads();
    if (dev) SDL_PauseAudioDevice(dev, 0);

    if (shot) {
        if (shot_page >= 0 && shot_page < PG_COUNT) app.page = shot_page;
        app.mode_play = shot_play;
        if (shot_chord) { note_button(0, 1); note_button(2, 1); note_button(4, 1); }
        float buf[2 * 800];
        if (shot_script) { /* "r1 right +a -a" : tap, press, release, by bramble.cfg names */
            char sc[1024];
            snprintf(sc, sizeof sc, "%s", shot_script);
            for (char *t = strtok(sc, " "); t; t = strtok(NULL, " ")) {
                int mode = *t == '+' ? 1 : *t == '-' ? 2 : 0;
                const char *nm = mode ? t + 1 : t;
                int b = -1;
                for (int i = 0; i < BTN_COUNT; i++) if (!strcmp(nm, cfg_keys[i])) b = i;
                if (b < 0) { fprintf(stderr, "script: unknown button %s\n", nm); return 1; }
                if (mode != 2) button(b, 1);
                synth_render(buf, 800);
                if (mode != 1) button(b, 0);
                synth_render(buf, 800);
            }
            int id = page_nparams[app.page] ? page_params[app.page][app.sel[app.page]] : -1;
            char v[32] = "-";
            if (id >= 0) param_format(id, app.vals[id], v, sizeof v);
            printf("page=%s mode=%s preset=%d sel=%s value=%s hold=%d arp=%d oct=%d\n", page_names[app.page],
                   app.mode_play ? "play" : "edit", app.preset_cur + 1, id >= 0 ? param_defs[id].key : "-", v,
                   app.hold, (int)app.vals[P_P_ARP], (int)app.vals[P_P_OCT]);
        }
        for (int f = 0; f < 40; f++) {
            synth_render(buf, 800);
            ui_draw(&app);
        }
        app.cpu = 0.07f;
        ui_draw(&app);
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        int rc = SDL_SaveBMP(s, shot);
        SDL_FreeSurface(s);
        SDL_Quit();
        return rc ? 1 : 0;
    }

    Uint32 last = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(&e);
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        edit_repeat();
        sticks(dt);
        app.cpu = cpu_acc;
        ui_draw(&app);
        if (SDL_GetTicks() - now < 8) SDL_Delay(8); /* in case vsync is off */
    }

    release_all_buttons();
    state_save(path_state, app.vals, app.preset_cur);
    if (dev) SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
