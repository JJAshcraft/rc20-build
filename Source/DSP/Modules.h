#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

// All of Patina's colouring stages. Each stage is header-only, processes a
// buffer in place, and takes its "amount" as a 0..1 value that is already
// scaled by the global Magnitude control.

namespace patina
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

inline float onePoleCoef (float cutoffHz, double sampleRate)
{
    return std::exp (-twoPi * cutoffHz / (float) sampleRate);
}

//==============================================================================
// Noise: hiss, vinyl crackle + rumble, or mains hum, added on top of the signal.
class Noise
{
public:
    enum Type { Hiss = 0, Vinyl, Hum };

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        lp.assign ((size_t) numChannels, 0.0f);
        rumble.assign ((size_t) numChannels, 0.0f);
        crackle.assign ((size_t) numChannels, 0.0f);
        crackleVel.assign ((size_t) numChannels, 0.0f);
        humPhase = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, int type, float tone)
    {
        if (amount <= 0.0f)
            return;

        const float level = amount * amount * 0.12f;             // up to roughly -18 dBFS
        const float cutoff = 800.0f * std::pow (20.0f, tone);     // 800 Hz .. 16 kHz
        const float a = onePoleCoef (cutoff, sampleRate);
        const float rumbleA = onePoleCoef (35.0f, sampleRate);
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();

        if (type == Hum)
        {
            const float inc = twoPi * 60.0f / (float) sampleRate;
            const float buzz = tone;  // tone adds upper harmonics ("buzzier" ground loop)
            for (int i = 0; i < n; ++i)
            {
                humPhase += inc;
                if (humPhase > twoPi) humPhase -= twoPi;
                float h = std::sin (humPhase)
                        + 0.5f * std::sin (2.0f * humPhase)
                        + 0.25f * std::sin (3.0f * humPhase)
                        + buzz * (0.2f * std::sin (5.0f * humPhase) + 0.12f * std::sin (7.0f * humPhase));
                h *= level * 0.45f;
                for (int ch = 0; ch < numCh; ++ch)
                    buffer.getWritePointer (ch)[i] += h;
            }
            return;
        }

        const float crackleProb = 0.0004f + amount * 0.0025f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float& s = lp[(size_t) ch];
            float& r = rumble[(size_t) ch];
            float& c = crackle[(size_t) ch];
            float& cv = crackleVel[(size_t) ch];

            for (int i = 0; i < n; ++i)
            {
                const float white = rng.nextFloat() * 2.0f - 1.0f;

                if (type == Hiss)
                {
                    s = white * (1.0f - a) + s * a;
                    d[i] += s * level * 2.0f;
                }
                else // Vinyl
                {
                    // sparse clicks: a kicked, quickly damped resonance
                    if (rng.nextFloat() < crackleProb)
                        cv += (rng.nextFloat() * 2.0f - 1.0f) * (0.3f + 0.7f * rng.nextFloat());
                    c += cv;
                    cv = cv * 0.35f - c * 0.55f;
                    c *= 0.6f;

                    r = white * (1.0f - rumbleA) + r * rumbleA;
                    s = white * (1.0f - a) + s * a;
                    d[i] += (c * 1.6f + r * 6.0f + s * 0.25f) * level;
                }
            }
        }
    }

private:
    double sampleRate = 44100.0;
    juce::Random rng { 0x5eed };
    std::vector<float> lp, rumble, crackle, crackleVel;
    float humPhase = 0.0f;
};

//==============================================================================
// Wobble: wow (slow) + flutter (fast) + random drift, via a modulated delay.
// Runs constantly so latency is fixed; at amount 0 it is a pure delay.
class Wobble
{
public:
    void prepare (double sr, int maxBlock, int numChannels)
    {
        sampleRate = sr;
        baseDelay = (int) std::round (sr * 0.006);              // 6 ms centre
        delay.prepare ({ sr, (juce::uint32) maxBlock, (juce::uint32) numChannels });
        delay.setMaximumDelayInSamples (baseDelay * 3);
        delay.reset();
        wowPhase = flutterPhase = 0.0f;
        drift = driftTarget = 0.0f;
        driftCounter = 0;
    }

    int getLatency() const { return baseDelay; }

    void process (juce::AudioBuffer<float>& buffer, float amount, float rate)
    {
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        const float wowHz = 0.15f + rate * 2.5f;             // 0.15 .. 2.65 Hz
        const float flutterHz = 6.0f + rate * 10.0f;         // 6 .. 16 Hz
        const float wowInc = twoPi * wowHz / (float) sampleRate;
        const float flutterInc = twoPi * flutterHz / (float) sampleRate;
        const float depth = amount * (float) baseDelay * 0.75f;
        const float driftA = onePoleCoef (1.5f, sampleRate);

        for (int i = 0; i < n; ++i)
        {
            if (--driftCounter <= 0)
            {
                driftTarget = rng.nextFloat() * 2.0f - 1.0f;
                driftCounter = (int) (sampleRate * (0.2 + 0.6 * rng.nextDouble()));
            }
            drift = driftTarget * (1.0f - driftA) + drift * driftA;

            wowPhase += wowInc;           if (wowPhase > twoPi) wowPhase -= twoPi;
            flutterPhase += flutterInc;   if (flutterPhase > twoPi) flutterPhase -= twoPi;

            const float mod = 0.65f * std::sin (wowPhase)
                            + 0.10f * std::sin (flutterPhase)
                            + 0.25f * drift;
            const float d = (float) baseDelay + depth * mod;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = buffer.getWritePointer (ch);
                delay.pushSample (ch, x[i]);
                x[i] = delay.popSample (ch, d);
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int baseDelay = 264;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay { 4096 };
    juce::Random rng { 0xb0b };
    float wowPhase = 0.0f, flutterPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
};

//==============================================================================
// Magnetic: tape-style level dropouts with HF loss, plus gentle tape saturation.
class Magnetic
{
public:
    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        lp.assign ((size_t) numChannels, 0.0f);
        gain = target = 1.0f;
        countdown = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, float dropouts)
    {
        if (amount <= 0.0f)
            return;

        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        const float glide = 1.0f - onePoleCoef (60.0f, sampleRate);
        const float drive = 1.0f + amount * 2.5f;
        const float makeup = 1.0f / std::tanh (drive);

        for (int i = 0; i < n; ++i)
        {
            if (--countdown <= 0)
            {
                const bool dropout = rng.nextFloat() < dropouts * 0.6f;
                target = dropout ? 1.0f - amount * (0.35f + 0.55f * rng.nextFloat())
                                 : 1.0f - amount * 0.06f * rng.nextFloat();
                countdown = (int) (sampleRate * (0.02 + 0.35 * rng.nextDouble()));
            }
            gain += (target - gain) * glide;

            // the deeper the dropout, the duller it gets
            if ((i & 15) == 0)
                lpA = onePoleCoef (1500.0f + 18500.0f * gain * gain, sampleRate);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = buffer.getWritePointer (ch);
                float& s = lp[(size_t) ch];
                s = x[i] * (1.0f - lpA) + s * lpA;
                const float sat = std::tanh (s * drive) * makeup;
                x[i] = juce::jmap (amount, s, sat) * gain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    juce::Random rng { 0x7a9e };
    std::vector<float> lp;
    float gain = 1.0f, target = 1.0f, lpA = 0.0f;
    int countdown = 0;
};

//==============================================================================
// Distort: asymmetric tanh drive (adds even + odd harmonics) and a tone filter.
class Distort
{
public:
    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        lp.assign ((size_t) numChannels, 0.0f);
        dc.assign ((size_t) numChannels, 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, float tone)
    {
        if (amount <= 0.0f)
            return;

        const float drive = juce::Decibels::decibelsToGain (amount * 30.0f);
        const float bias = 0.25f * amount;
        const float biasOffset = std::tanh (bias);
        const float makeup = 1.0f / std::pow (drive, 0.75f);
        const float blend = juce::jmin (1.0f, amount * 3.0f);
        const float a = onePoleCoef (1500.0f * std::pow (12.0f, tone), sampleRate); // 1.5k..18k
        const float dcA = onePoleCoef (10.0f, sampleRate);
        const int n = buffer.getNumSamples();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            float& s = lp[(size_t) ch];
            float& d = dc[(size_t) ch];
            for (int i = 0; i < n; ++i)
            {
                float y = (std::tanh (x[i] * drive + bias) - biasOffset) * makeup;
                d = y * (1.0f - dcA) + d * dcA;   // remove the DC the bias creates
                y -= d;
                s = y * (1.0f - a) + s * a;
                x[i] = juce::jmap (blend, x[i], s);
            }
        }
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> lp, dc;
};

//==============================================================================
// Digital: bit-depth reduction and sample-and-hold rate reduction.
class Digital
{
public:
    void prepare (int numChannels)
    {
        held.assign ((size_t) numChannels, 0.0f);
        counter = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, float rateAmt)
    {
        if (amount <= 0.0f)
            return;

        const float bits = 16.0f - 12.0f * amount;                  // 16 .. 4 bits
        const float steps = std::pow (2.0f, bits - 1.0f);
        const float holdLen = 1.0f + rateAmt * amount * 31.0f;      // 1 .. 32 samples
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            counter += 1.0f;
            const bool grab = counter >= holdLen;
            if (grab)
                counter -= holdLen;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = buffer.getWritePointer (ch);
                if (grab || holdLen <= 1.0f)
                    held[(size_t) ch] = std::round (x[i] * steps) / steps;
                x[i] = held[(size_t) ch];
            }
        }
    }

private:
    std::vector<float> held;
    float counter = 0.0f;
};

//==============================================================================
// Space: a darkened reverb.
class Space
{
public:
    void prepare (double sr, int maxBlock, int numChannels)
    {
        reverb.setSampleRate (sr);
        reverb.reset();
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) numChannels };
        darken.prepare (spec);
        darken.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        darken.setCutoffFrequency (6000.0f);
        wet.setSize (numChannels, maxBlock);
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, float size)
    {
        if (amount <= 0.0f)
            return;

        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + 0.68f * size;
        p.damping = 0.6f;
        p.width = 1.0f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        reverb.setParameters (p);

        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
            wet.copyFrom (ch, 0, buffer, ch, 0, n);

        if (numCh >= 2)
            reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), n);
        else
            reverb.processMono (wet.getWritePointer (0), n);

        juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        darken.process (juce::dsp::ProcessContextReplacing<float> (block));

        const float wetGain = amount * 0.6f;
        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, wet, ch, 0, n, wetGain);
    }

private:
    juce::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> darken;
    juce::AudioBuffer<float> wet;
};

} // namespace patina
