#ifndef BRAMBLE_PRESET_H
#define BRAMBLE_PRESET_H

#include "params.h"

#define PRESET_SLOTS 32
#define PRESET_NAME 16

typedef struct {
    int used;
    char name[PRESET_NAME];
    float v[P_COUNT];
} Preset;

extern Preset presets[PRESET_SLOTS];

/* Parameters that are global play settings, not part of a sound. */
int param_is_global(int id);

void presets_factory(void);                 /* fills slots with the built-in sounds */
int presets_load(const char *path);         /* 0 ok, -1 no file */
int presets_save(const char *path);
void preset_capture(int slot, const float *vals, const char *name);
void preset_apply(int slot, float *vals);   /* keeps global params */

int state_save(const char *path, const float *vals, int slot);
int state_load(const char *path, float *vals, int *slot);

#endif
