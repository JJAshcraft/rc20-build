#include "preset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Preset presets[PRESET_SLOTS];

int param_is_global(int id) { return id == P_P_SCALE || id == P_P_ROOT || id == P_P_OCT; }

static int find_key(const char *k)
{
    for (int i = 0; i < P_COUNT; i++)
        if (!strcmp(param_defs[i].key, k)) return i;
    return -1;
}

/* "key=value key=value ..." on top of the defaults */
static void apply_spec(float *v, const char *spec)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        int id = find_key(tok);
        if (id >= 0) v[id] = param_clamp(id, (float)atof(eq + 1));
    }
}

static const struct { const char *name, *spec; } factory[] = {
    {"INIT", ""},
    {"MOSS PAD", "a_table=2 a_pos=0.3 a_uni=4 a_det=0.35 b_on=1 b_table=3 b_pos=0.4 b_level=0.4 b_fine=-6 "
                 "f_type=1 f_cut=0.55 f_res=0.15 f_env=0.15 e1_a=0.68 e1_d=0.6 e1_s=0.85 e1_r=0.7 "
                 "e2_a=0.7 e2_s=0.5 lfo_dest=4 lfo_amt=0.3 lfo_rate=0.3 fx_cho=0.5 fx_rev=0.4 fx_rsize=0.75"},
    {"THORN BASS", "v_mode=1 v_glide=0.2 a_table=0 a_pos=0.66 sub_lvl=0.6 sub_shape=2 f_type=1 f_cut=0.3 "
                   "f_res=0.45 f_env=0.5 f_drive=0.3 e2_a=0 e2_d=0.42 e2_s=0.1 e1_a=0 e1_d=0.5 e1_s=0.9 "
                   "e1_r=0.25 fx_rev=0"},
    {"DEW PLUCK", "a_table=1 a_pos=0.2 b_on=1 b_table=6 b_pos=0.2 b_level=0.3 f_cut=0.35 f_env=0.6 "
                  "e2_a=0 e2_d=0.38 e2_s=0 e1_a=0 e1_d=0.48 e1_s=0 e1_r=0.42 fx_dly=0.3 fx_dtime=0.375 "
                  "fx_dfb=0.45 fx_rev=0.25"},
    {"ROOT LEAD", "v_mode=1 v_glide=0.25 a_table=4 a_pos=0.25 e2_pos=0.4 a_uni=2 a_det=0.15 f_cut=0.65 "
                  "f_res=0.3 f_env=0.2 e2_a=0.2 e2_d=0.6 e2_s=0.4 lfo_dest=1 lfo_amt=0.15 lfo_rate=0.55 "
                  "fx_dly=0.2 fx_dtime=0.3"},
    {"SPORE ARP", "p_arp=1 p_bpm=128 a_table=5 a_pos=0.2 e2_pos=0.5 f_cut=0.45 f_res=0.5 f_env=0.4 "
                  "e2_a=0 e2_d=0.32 e2_s=0 e1_a=0 e1_d=0.4 e1_s=0.2 e1_r=0.3 fx_dly=0.35 fx_dtime=0.36 "
                  "fx_dfb=0.5 fx_rev=0.2"},
    {"BARK KEYS", "a_table=3 a_pos=0.6 b_on=1 b_table=0 b_pos=0 b_semi=12 b_level=0.4 f_cut=0.6 f_env=0.1 "
                  "e1_a=0.15 e1_d=0.7 e1_s=0.6 e1_r=0.35 fx_cho=0.6 fx_rev=0.2"},
    {"GRIT SWELL", "a_table=7 a_pos=0.2 b_on=1 b_table=7 b_pos=0.8 b_fine=-12 lfo_shape=1 lfo_dest=2 "
                   "lfo_amt=0.6 lfo_rate=0.35 f_type=1 f_cut=0.5 f_res=0.35 f_env=0 e1_a=0.72 e1_r=0.72 "
                   "fx_rev=0.5 fx_rsize=0.85"},
    {"BELL FERN", "a_table=6 a_pos=0.5 e2_pos=-0.4 f_cut=0.9 f_env=0 e2_a=0 e2_d=0.6 e2_s=0 "
                  "e1_a=0 e1_d=0.68 e1_s=0 e1_r=0.62 fx_rev=0.45 fx_dly=0.15"},
    {"WIND", "a_on=0 noise_lvl=0.8 noise_col=0.7 f_type=2 f_cut=0.55 f_res=0.7 f_env=0 f_key=0 "
             "lfo_dest=5 lfo_amt=0.3 lfo_rate=0.2 e1_a=0.65 e1_r=0.72 fx_rev=0.6 fx_rsize=0.9"},
};

void presets_factory(void)
{
    memset(presets, 0, sizeof presets);
    for (size_t i = 0; i < sizeof factory / sizeof factory[0] && i < PRESET_SLOTS; i++) {
        Preset *p = &presets[i];
        p->used = 1;
        snprintf(p->name, PRESET_NAME, "%s", factory[i].name);
        for (int k = 0; k < P_COUNT; k++) p->v[k] = param_defs[k].def;
        apply_spec(p->v, factory[i].spec);
    }
}

void preset_capture(int slot, const float *vals, const char *name)
{
    if (slot < 0 || slot >= PRESET_SLOTS) return;
    Preset *p = &presets[slot];
    p->used = 1;
    snprintf(p->name, PRESET_NAME, "%s", name);
    memcpy(p->v, vals, sizeof(float) * P_COUNT);
}

void preset_apply(int slot, float *vals)
{
    if (slot < 0 || slot >= PRESET_SLOTS || !presets[slot].used) return;
    for (int i = 0; i < P_COUNT; i++)
        if (!param_is_global(i)) vals[i] = presets[slot].v[i];
}

/* File format, one preset per block:
 *   [3] DEW PLUCK
 *   a_table=1
 *   ...
 * Missing keys take their defaults, unknown keys are ignored. */
int presets_save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# Bramble presets\n");
    for (int s = 0; s < PRESET_SLOTS; s++) {
        if (!presets[s].used) continue;
        fprintf(f, "[%d] %s\n", s + 1, presets[s].name);
        for (int i = 0; i < P_COUNT; i++)
            if (!param_is_global(i)) fprintf(f, "%s=%g\n", param_defs[i].key, presets[s].v[i]);
    }
    return fclose(f) ? -1 : 0;
}

int presets_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(presets, 0, sizeof presets);
    char line[256];
    Preset *cur = NULL;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '[') {
            int slot = atoi(line + 1) - 1;
            char *nm = strchr(line, ']');
            cur = NULL;
            if (slot < 0 || slot >= PRESET_SLOTS) continue;
            cur = &presets[slot];
            cur->used = 1;
            for (int k = 0; k < P_COUNT; k++) cur->v[k] = param_defs[k].def;
            nm = nm ? nm + 1 : (char *)"";
            while (*nm == ' ') nm++;
            snprintf(cur->name, PRESET_NAME, "%s", nm);
        } else if (cur) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            int id = find_key(line);
            if (id >= 0) cur->v[id] = param_clamp(id, (float)atof(eq + 1));
        }
    }
    fclose(f);
    return 0;
}

int state_save(const char *path, const float *vals, int slot)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "slot=%d\n", slot);
    for (int i = 0; i < P_COUNT; i++) fprintf(f, "%s=%g\n", param_defs[i].key, vals[i]);
    return fclose(f) ? -1 : 0;
}

int state_load(const char *path, float *vals, int *slot)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (!strcmp(line, "slot")) { *slot = atoi(eq + 1); continue; }
        int id = find_key(line);
        if (id >= 0) vals[id] = param_clamp(id, (float)atof(eq + 1));
    }
    fclose(f);
    return 0;
}
