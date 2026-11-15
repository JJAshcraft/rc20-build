#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Modules.h"
#include "Presets.h"

namespace ParamID
{
    inline constexpr auto magnitude   = "magnitude";
    inline constexpr auto noiseOn     = "noise_on";
    inline constexpr auto noiseAmt    = "noise_amt";
    inline constexpr auto noiseType   = "noise_type";
    inline constexpr auto noiseTone   = "noise_tone";
    inline constexpr auto wobbleOn    = "wobble_on";
    inline constexpr auto wobbleAmt   = "wobble_amt";
    inline constexpr auto wobbleRate  = "wobble_rate";
    inline constexpr auto distortOn   = "distort_on";
    inline constexpr auto distortAmt  = "distort_amt";
    inline constexpr auto distortTone = "distort_tone";
    inline constexpr auto digitalOn   = "digital_on";
    inline constexpr auto digitalAmt  = "digital_amt";
    inline constexpr auto digitalRate = "digital_rate";
    inline constexpr auto spaceOn     = "space_on";
    inline constexpr auto spaceAmt    = "space_amt";
    inline constexpr auto spaceSize   = "space_size";
    inline constexpr auto magneticOn  = "magnetic_on";
    inline constexpr auto magneticAmt = "magnetic_amt";
    inline constexpr auto magneticDrop= "magnetic_drop";
    inline constexpr auto lowCut      = "low_cut";
    inline constexpr auto highCut     = "high_cut";
    inline constexpr auto mix         = "mix";
    inline constexpr auto output      = "output";
}

class PatinaProcessor : public juce::AudioProcessor
{
public:
    PatinaProcessor();
    ~PatinaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Patina"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return patina::numFactoryPresets; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);
    std::function<void()> onPresetChanged; // called on the message thread

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void processChunk (juce::AudioBuffer<float>& chunk);
    float amt (const char* onId, juce::SmoothedValue<float>& s) const;

    struct Smoothers
    {
        juce::SmoothedValue<float> magnitude, noise, wobble, distort, digital, space, magnetic,
                                   mix, output, lowCut, highCut;
    } sm;

    patina::Noise noise;
    patina::Wobble wobble;
    patina::Magnetic magnetic;
    patina::Distort distort;
    patina::Digital digital;
    patina::Space space;
    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer;
    float currentMagnitude = 1.0f;
    int currentPreset = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PatinaProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaProcessor)
};
