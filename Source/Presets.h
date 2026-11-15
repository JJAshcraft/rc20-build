#pragma once

// Factory presets. Each row is one complete snapshot of every parameter.
// Noise type: 0 = Hiss, 1 = Vinyl, 2 = Hum. Amounts are 0..1, cuts in Hz, output in dB.
// Output gains are level-matched to the dry signal (measured by Tests/SmokeTest.cpp).

namespace patina
{
struct Stage { bool on; float amount; float extra; };

struct Preset
{
    const char* name;
    const char* category;
    float magnitude;
    bool noiseOn; float noiseAmt; int noiseType; float noiseTone;
    Stage wobble;    // extra = rate
    Stage distort;   // extra = tone
    Stage digital;   // extra = rate
    Stage space;     // extra = size
    Stage magnetic;  // extra = dropouts
    float lowCut, highCut, mix, outputDb;
};

constexpr bool on = true, off = false;
constexpr int hiss = 0, vinyl = 1, hum = 2;

// clang-format off
inline const Preset factoryPresets[] =
{
//  name                    category     mag    noise                         wobble             distort            digital            space              magnetic           lowCut  highCut  mix   out
    { "Default",            "Basics",    1.00f, on,  0.25f, vinyl, 0.50f, { on,  0.30f, 0.30f }, { on,  0.25f, 0.60f }, { off, 0.30f, 0.30f }, { off, 0.30f, 0.50f }, { on,  0.30f, 0.30f },   20.0f, 20000.0f, 1.00f,  -1.5f },
    { "Subtle Color",       "Basics",    0.35f, on,  0.20f, hiss,  0.60f, { on,  0.20f, 0.30f }, { on,  0.30f, 0.70f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.40f, 0.10f },   20.0f, 20000.0f, 1.00f,  -0.5f },
    { "Mix Bus Glow",       "Basics",    0.50f, on,  0.10f, hiss,  0.50f, { on,  0.08f, 0.30f }, { on,  0.20f, 0.75f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.35f, 0.00f },   20.0f, 20000.0f, 1.00f,  -1.0f },
    { "Tape Glue",          "Tape",      1.00f, off, 0.00f, hiss,  0.50f, { on,  0.06f, 0.30f }, { on,  0.15f, 0.70f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.50f, 0.00f },   20.0f, 20000.0f, 1.00f,  -3.5f },
    { "Warm Cassette",      "Tape",      1.00f, on,  0.30f, hiss,  0.55f, { on,  0.35f, 0.35f }, { on,  0.20f, 0.45f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.45f, 0.25f },   35.0f, 12000.0f, 1.00f,  -2.5f },
    { "Worn Cassette",      "Tape",      1.00f, on,  0.45f, hiss,  0.45f, { on,  0.60f, 0.40f }, { on,  0.30f, 0.35f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.70f, 0.60f },   60.0f,  8000.0f, 1.00f,  -3.0f },
    { "Broken Walkman",     "Tape",      1.00f, on,  0.50f, hiss,  0.40f, { on,  0.80f, 0.55f }, { on,  0.30f, 0.30f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.85f, 0.80f },   90.0f,  7000.0f, 1.00f,  -3.0f },
    { "Lost VHS Tape",      "Tape",      1.00f, on,  0.40f, hiss,  0.35f, { on,  0.55f, 0.20f }, { on,  0.20f, 0.40f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.65f, 0.50f },   45.0f,  9000.0f, 1.00f,  -3.5f },
    { "Dusty Crate",        "Vinyl",     1.00f, on,  0.35f, vinyl, 0.40f, { on,  0.20f, 0.20f }, { on,  0.20f, 0.50f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.20f, 0.15f },   40.0f, 11000.0f, 1.00f,  -1.0f },
    { "Needle Drop",        "Vinyl",     1.00f, on,  0.50f, vinyl, 0.50f, { off, 0.00f, 0.30f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },   20.0f, 20000.0f, 1.00f,  0.0f },
    { "Old Gramophone",     "Vinyl",     1.00f, on,  0.70f, vinyl, 0.30f, { on,  0.30f, 0.15f }, { on,  0.25f, 0.30f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.30f, 0.20f },  250.0f,  5000.0f, 1.00f,  0.0f },
    { "Lo-Fi Keys",         "Vinyl",     1.00f, on,  0.30f, vinyl, 0.45f, { on,  0.45f, 0.25f }, { on,  0.15f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.20f, 0.40f }, { on,  0.30f, 0.20f },   20.0f,  9000.0f, 1.00f,  -2.0f },
    { "Memory Loop",        "Vinyl",     1.00f, on,  0.30f, vinyl, 0.40f, { on,  0.40f, 0.30f }, { on,  0.20f, 0.45f }, { on,  0.25f, 0.30f }, { on,  0.30f, 0.50f }, { on,  0.40f, 0.40f },   30.0f, 10000.0f, 1.00f,  -3.0f },
    { "Boom Bap Drums",     "Digital",   1.00f, on,  0.20f, vinyl, 0.50f, { off, 0.00f, 0.30f }, { on,  0.45f, 0.50f }, { on,  0.35f, 0.25f }, { off, 0.00f, 0.50f }, { on,  0.20f, 0.10f },   30.0f, 14000.0f, 1.00f,  -1.0f },
    { "12-Bit Sampler",     "Digital",   1.00f, off, 0.00f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  0.15f, 0.55f }, { on,  0.33f, 0.35f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },   20.0f, 15000.0f, 1.00f,  0.0f },
    { "Handheld Chip",      "Digital",   1.00f, off, 0.00f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  0.20f, 0.60f }, { on,  0.75f, 0.50f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },  150.0f,  7000.0f, 1.00f,  0.0f },
    { "Bit Mangler",        "Digital",   1.00f, off, 0.00f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  0.50f, 0.70f }, { on,  0.85f, 0.70f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },   20.0f, 20000.0f, 0.80f,  0.0f },
    { "AM Radio",           "Radio",     1.00f, on,  0.25f, hiss,  0.30f, { off, 0.00f, 0.30f }, { on,  0.40f, 0.20f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.20f, 0.30f },  400.0f,  3500.0f, 1.00f,  4.0f },
    { "Telephone Line",     "Radio",     1.00f, on,  0.20f, hiss,  0.30f, { off, 0.00f, 0.30f }, { on,  0.35f, 0.30f }, { on,  0.30f, 0.20f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },  600.0f,  3000.0f, 1.00f,  8.5f },
    { "Late Night Radio",   "Radio",     1.00f, on,  0.30f, hiss,  0.40f, { on,  0.20f, 0.30f }, { on,  0.30f, 0.35f }, { off, 0.00f, 0.00f }, { on,  0.20f, 0.40f }, { on,  0.25f, 0.30f },  150.0f,  6000.0f, 1.00f,  -1.5f },
    { "Ground Loop",        "Radio",     1.00f, on,  0.35f, hum,   0.60f, { off, 0.00f, 0.30f }, { on,  0.30f, 0.45f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { on,  0.20f, 0.20f },   20.0f, 20000.0f, 1.00f,  -1.5f },
    { "Hazy Daydream",      "Ambient",   1.00f, on,  0.20f, hiss,  0.40f, { on,  0.45f, 0.20f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.55f, 0.70f }, { on,  0.30f, 0.20f },   20.0f,  7000.0f, 1.00f,  -5.5f },
    { "Ghost Room",         "Ambient",   1.00f, on,  0.15f, hiss,  0.30f, { on,  0.30f, 0.20f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.80f, 0.90f }, { on,  0.20f, 0.30f },  120.0f,  6000.0f, 1.00f,  -8.5f },
    { "Underwater",         "Ambient",   1.00f, off, 0.00f, hiss,  0.50f, { on,  0.50f, 0.15f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.40f, 0.60f }, { off, 0.00f, 0.00f },   20.0f,  1200.0f, 1.00f,  -2.0f },
    { "Seasick",            "Ambient",   1.00f, off, 0.00f, hiss,  0.50f, { on,  0.90f, 0.10f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.20f, 0.50f }, { on,  0.20f, 0.20f },   20.0f, 20000.0f, 1.00f,  -1.5f },
    { "Flutter Chorus",     "Ambient",   1.00f, off, 0.00f, hiss,  0.50f, { on,  0.50f, 0.85f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f }, { on,  0.15f, 0.40f }, { off, 0.00f, 0.00f },   20.0f, 20000.0f, 0.50f,  3.0f },
    { "Basement Show",      "Grit",      1.00f, on,  0.20f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  0.55f, 0.45f }, { off, 0.00f, 0.00f }, { on,  0.35f, 0.30f }, { off, 0.00f, 0.00f },   80.0f,  9000.0f, 1.00f,  -1.0f },
    { "Blown Speaker",      "Grit",      1.00f, off, 0.00f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  0.85f, 0.30f }, { off, 0.00f, 0.00f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },  200.0f,  6000.0f, 1.00f,  4.5f },
    { "Fuzz Box",           "Grit",      1.00f, off, 0.00f, hiss,  0.50f, { off, 0.00f, 0.30f }, { on,  1.00f, 0.55f }, { on,  0.20f, 0.20f }, { off, 0.00f, 0.50f }, { off, 0.00f, 0.00f },   20.0f, 20000.0f, 0.85f,  5.5f },
    { "Total Decay",        "Grit",      1.00f, on,  0.60f, hiss,  0.45f, { on,  0.75f, 0.45f }, { on,  0.60f, 0.40f }, { on,  0.60f, 0.60f }, { on,  0.50f, 0.70f }, { on,  0.90f, 0.90f },  100.0f,  6000.0f, 1.00f,  -4.0f },
};
// clang-format on

inline constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
} // namespace patina
