/* Bramble: parameter table shared by the engine, the UI and preset files. */
#ifndef BRAMBLE_PARAMS_H
#define BRAMBLE_PARAMS_H

enum disp {
    D_PCT,      /* 0..1 shown as 0..100 */
    D_BIP,      /* -1..1 shown as -100..100 */
    D_INT,      /* integer */
    D_ENUM,     /* integer index into labels */
    D_CUTOFF,   /* 0..1 -> 20 Hz .. 20 kHz */
    D_TIME,     /* 0..1 -> 1 ms .. 10 s */
    D_LFOHZ,    /* 0..1 -> 0.05 .. 20 Hz */
    D_SEMI,     /* semitones */
    D_CENTS,    /* cents */
    D_DLYMS,    /* 0..1 -> 20 .. 1000 ms */
    D_BPM,      /* bpm */
    D_NOTE,     /* 0..11 note name */
};

/*  id            key          label        min   max   def   step  disp     labels */
#define PARAM_LIST(X) \
    X(A_ON,       "a_on",      "OSC A",     0,    1,    1,    1,    D_ENUM,  L_ONOFF) \
    X(A_TABLE,    "a_table",   "TABLE",     0,    7,    0,    1,    D_ENUM,  L_TABLES) \
    X(A_POS,      "a_pos",     "POSITION",  0,    1,    0.33f,0.01f,D_PCT,   0) \
    X(A_SEMI,     "a_semi",    "SEMI",      -24,  24,   0,    1,    D_SEMI,  0) \
    X(A_FINE,     "a_fine",    "FINE",      -100, 100,  0,    1,    D_CENTS, 0) \
    X(A_LEVEL,    "a_level",   "LEVEL",     0,    1,    0.75f,0.01f,D_PCT,   0) \
    X(A_UNI,      "a_uni",     "UNISON",    1,    4,    1,    1,    D_INT,   0) \
    X(A_DET,      "a_det",     "DETUNE",    0,    1,    0.25f,0.01f,D_PCT,   0) \
    X(A_BEND,     "a_bend",    "BEND",      -1,   1,    0,    0.01f,D_BIP,   0) \
    X(B_ON,       "b_on",      "OSC B",     0,    1,    0,    1,    D_ENUM,  L_ONOFF) \
    X(B_TABLE,    "b_table",   "TABLE",     0,    7,    2,    1,    D_ENUM,  L_TABLES) \
    X(B_POS,      "b_pos",     "POSITION",  0,    1,    0,    0.01f,D_PCT,   0) \
    X(B_SEMI,     "b_semi",    "SEMI",      -24,  24,   0,    1,    D_SEMI,  0) \
    X(B_FINE,     "b_fine",    "FINE",      -100, 100,  7,    1,    D_CENTS, 0) \
    X(B_LEVEL,    "b_level",   "LEVEL",     0,    1,    0.6f, 0.01f,D_PCT,   0) \
    X(B_UNI,      "b_uni",     "UNISON",    1,    4,    1,    1,    D_INT,   0) \
    X(B_DET,      "b_det",     "DETUNE",    0,    1,    0.25f,0.01f,D_PCT,   0) \
    X(B_BEND,     "b_bend",    "BEND",      -1,   1,    0,    0.01f,D_BIP,   0) \
    X(SUB_LVL,    "sub_lvl",   "SUB LEVEL", 0,    1,    0,    0.01f,D_PCT,   0) \
    X(SUB_SHAPE,  "sub_shape", "SUB SHAPE", 0,    2,    0,    1,    D_ENUM,  L_SUBSHAPE) \
    X(SUB_OCT,    "sub_oct",   "SUB OCT",   -2,   -1,   -1,   1,    D_INT,   0) \
    X(NOISE_LVL,  "noise_lvl", "NOISE",     0,    1,    0,    0.01f,D_PCT,   0) \
    X(NOISE_COL,  "noise_col", "NOISE TONE",0,    1,    0.5f, 0.01f,D_PCT,   0) \
    X(F_TYPE,     "f_type",    "TYPE",      0,    4,    0,    1,    D_ENUM,  L_FTYPES) \
    X(F_CUT,      "f_cut",     "CUTOFF",    0,    1,    0.7f, 0.005f,D_CUTOFF,0) \
    X(F_RES,      "f_res",     "RESONANCE", 0,    1,    0.2f, 0.01f,D_PCT,   0) \
    X(F_DRIVE,    "f_drive",   "DRIVE",     0,    1,    0,    0.01f,D_PCT,   0) \
    X(F_KEY,      "f_key",     "KEY TRACK", 0,    1,    0.5f, 0.01f,D_PCT,   0) \
    X(F_ENV,      "f_env",     "ENV2>CUT",  -1,   1,    0.3f, 0.01f,D_BIP,   0) \
    X(E1_A,       "e1_a",      "AMP ATK",   0,    1,    0.05f,0.01f,D_TIME,  0) \
    X(E1_D,       "e1_d",      "AMP DEC",   0,    1,    0.5f, 0.01f,D_TIME,  0) \
    X(E1_S,       "e1_s",      "AMP SUS",   0,    1,    0.8f, 0.01f,D_PCT,   0) \
    X(E1_R,       "e1_r",      "AMP REL",   0,    1,    0.55f, 0.01f,D_TIME,  0) \
    X(E2_A,       "e2_a",      "ENV2 ATK",  0,    1,    0.05f,0.01f,D_TIME,  0) \
    X(E2_D,       "e2_d",      "ENV2 DEC",  0,    1,    0.5f, 0.01f,D_TIME,  0) \
    X(E2_S,       "e2_s",      "ENV2 SUS",  0,    1,    0.3f, 0.01f,D_PCT,   0) \
    X(E2_R,       "e2_r",      "ENV2 REL",  0,    1,    0.55f, 0.01f,D_TIME,  0) \
    X(E2_POS,     "e2_pos",    "ENV2>POS",  -1,   1,    0,    0.01f,D_BIP,   0) \
    X(LFO_SHAPE,  "lfo_shape", "LFO SHAPE", 0,    4,    0,    1,    D_ENUM,  L_LFOSHAPE) \
    X(LFO_RATE,   "lfo_rate",  "LFO RATE",  0,    1,    0.45f,0.01f,D_LFOHZ, 0) \
    X(LFO_DEST,   "lfo_dest",  "LFO DEST",  0,    6,    5,    1,    D_ENUM,  L_LFODEST) \
    X(LFO_AMT,    "lfo_amt",   "LFO AMOUNT",0,    1,    0,    0.01f,D_PCT,   0) \
    X(V_MODE,     "v_mode",    "VOICE",     0,    1,    0,    1,    D_ENUM,  L_VMODE) \
    X(V_GLIDE,    "v_glide",   "GLIDE",     0,    1,    0,    0.01f,D_PCT,   0) \
    X(V_VOL,      "v_vol",     "VOLUME",    0,    1,    0.7f, 0.01f,D_PCT,   0) \
    X(FX_CHO,     "fx_cho",    "CHORUS",    0,    1,    0,    0.01f,D_PCT,   0) \
    X(FX_DLY,     "fx_dly",    "DELAY MIX", 0,    1,    0,    0.01f,D_PCT,   0) \
    X(FX_DTIME,   "fx_dtime",  "DELAY TIME",0,    1,    0.4f, 0.01f,D_DLYMS, 0) \
    X(FX_DFB,     "fx_dfb",    "FEEDBACK",  0,    0.95f,0.4f, 0.01f,D_PCT,   0) \
    X(FX_REV,     "fx_rev",    "REVERB",    0,    1,    0.15f,0.01f,D_PCT,   0) \
    X(FX_RSIZE,   "fx_rsize",  "ROOM SIZE", 0,    1,    0.6f, 0.01f,D_PCT,   0) \
    X(P_SCALE,    "p_scale",   "SCALE",     0,    6,    1,    1,    D_ENUM,  L_SCALES) \
    X(P_ROOT,     "p_root",    "ROOT",      0,    11,   0,    1,    D_NOTE,  0) \
    X(P_OCT,      "p_oct",     "OCTAVE",    1,    6,    4,    1,    D_INT,   0) \
    X(P_ARP,      "p_arp",     "ARP",       0,    4,    0,    1,    D_ENUM,  L_ARP) \
    X(P_BPM,      "p_bpm",     "ARP BPM",   40,   240,  120,  1,    D_BPM,   0) \
    X(P_CHORD,    "p_chord",   "CHORD",     0,    3,    0,    1,    D_ENUM,  L_CHORD)

#define X(id, key, label, mn, mx, df, st, dp, lb) P_##id,
enum { PARAM_LIST(X) P_COUNT };
#undef X

typedef struct {
    const char *key, *label;
    float min, max, def, step;
    int disp;
    const char *const *labels;
} ParamDef;

extern const ParamDef param_defs[P_COUNT];

/* Shared labels. */
extern const char *const L_ONOFF[], *const L_TABLES[], *const L_SUBSHAPE[], *const L_FTYPES[],
    *const L_LFOSHAPE[], *const L_LFODEST[], *const L_VMODE[], *const L_SCALES[], *const L_ARP[],
    *const L_CHORD[];

enum { LFO_D_NONE, LFO_D_PITCH, LFO_D_POSA, LFO_D_POSB, LFO_D_POSAB, LFO_D_CUT, LFO_D_AMP };

/* Scales: notes within one octave; degree k of a scale is
 * 12 * (k / len) + notes[k % len]. */
extern const int scale_notes[7][12];
extern const int scale_len[7];
int scale_degree(int scale, int k);

void param_format(int id, float v, char *out, int n);
float param_clamp(int id, float v);

/* Value conversions used by the engine and the UI. */
float conv_cutoff_hz(float n);
float conv_time_s(float n);
float conv_lfo_hz(float n);
float conv_delay_s(float n);

#endif
