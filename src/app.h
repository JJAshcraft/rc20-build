#ifndef BRAMBLE_APP_H
#define BRAMBLE_APP_H

#include "params.h"
#include <SDL.h>

/* Buttons by their R36S printed labels (Nintendo layout: A right, B bottom,
 * X top, Y left). */
enum {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_X, BTN_Y,
    BTN_L1, BTN_R1, BTN_L2, BTN_R2, BTN_L3, BTN_R3,
    BTN_SELECT, BTN_START, BTN_FN,
    BTN_COUNT
};
enum { AX_LX, AX_LY, AX_RX, AX_RY, AX_COUNT };

enum {
    PG_OSCA, PG_OSCB, PG_SUB, PG_FILTER, PG_ENV, PG_LFO, PG_FX, PG_PLAY, PG_PRESET, PG_INPUT,
    PG_COUNT
};

typedef struct {
    SDL_Renderer *ren;
    SDL_Texture *font;
    int page, mode_play;     /* mode_play: 1 = buttons are notes, 0 = edit */
    int sel[PG_COUNT];       /* selected row per page */
    float vals[P_COUNT];     /* UI copy of the parameters */
    int preset_cur, preset_sel, save_armed;
    char preset_name[24];
    int btn[BTN_COUNT];
    float ax[AX_COUNT];
    int notes_on[8][4], notes_n[8]; /* notes each note button started */
    int hold, last_arp;
    char toast[64];
    Uint32 toast_until;
    float cpu;
    int raw_btn, raw_axis, raw_hat;  /* input test page */
    float raw_axis_val;
    const char *pad_name;
} App;

extern const char *const page_names[PG_COUNT];
extern const int *const page_params[PG_COUNT];
extern const int page_nparams[PG_COUNT];

void ui_init(App *a);
void ui_draw(App *a);
void ui_splash(App *a);

#endif
