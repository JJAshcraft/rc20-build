#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int chunkSize = 32; // parameters are smoothed at this granularity

juce::NormalisableRange<float> freqRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}
} // namespace

PatinaProcessor::PatinaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PatinaState", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PatinaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; }));
    };
    auto onOff = [] (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };

    layout.add (pct (ParamID::magnitude, "Magnitude", 1.0f));

    layout.add (onOff (ParamID::noiseOn, "Noise On", true));
    layout.add (pct (ParamID::noiseAmt, "Noise Amount", 0.25f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::noiseType, 1 }, "Noise Type",
                                                        StringArray { "Hiss", "Vinyl", "Hum" }, 1));
    layout.add (pct (ParamID::noiseTone, "Noise Tone", 0.5f));

    layout.add (onOff (ParamID::wobbleOn, "Wobble On", true));
    layout.add (pct (ParamID::wobbleAmt, "Wobble Amount", 0.3f));
    layout.add (pct (ParamID::wobbleRate, "Wobble Rate", 0.3f));

    layout.add (onOff (ParamID::distortOn, "Distort On", true));
    layout.add (pct (ParamID::distortAmt, "Distort Amount", 0.25f));
    layout.add (pct (ParamID::distortTone, "Distort Tone", 0.6f));

    layout.add (onOff (ParamID::digitalOn, "Digital On", false));
    layout.add (pct (ParamID::digitalAmt, "Digital Amount", 0.3f));
    layout.add (pct (ParamID::digitalRate, "Digital Rate", 0.3f));

    layout.add (onOff (ParamID::spaceOn, "Space On", false));
    layout.add (pct (ParamID::spaceAmt, "Space Amount", 0.3f));
    layout.add (pct (ParamID::spaceSize, "Space Size", 0.5f));

    layout.add (onOff (ParamID::magneticOn, "Magnetic On", true));
    layout.add (pct (ParamID::magneticAmt, "Magnetic Amount", 0.3f));
    layout.add (pct (ParamID::magneticDrop, "Magnetic Dropouts", 0.3f));

    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                       : String (roundToInt (v)) + " Hz"; };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lowCut, 1 }, "Low Cut",
                                                       freqRange (20.0f, 1000.0f), 20.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::highCut, 1 }, "High Cut",
                                                       freqRange (1000.0f, 20000.0f), 20000.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    layout.add (pct (ParamID::mix, "Mix", 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

bool PatinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PatinaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = getTotalNumOutputChannels();

    noise.prepare (sampleRate, numCh);
    wobble.prepare (sampleRate, chunkSize, numCh);
    magnetic.prepare (sampleRate, numCh);
    distort.prepare (sampleRate, numCh);
    digital.prepare (numCh);
    space.prepare (sampleRate, chunkSize, numCh);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) chunkSize, (juce::uint32) numCh };
    lowCutFilter.prepare (spec);
    lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.prepare (spec);
    highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    // keep the dry path time-aligned with the (always running) wobble delay
    dryDelay.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryDelay.setMaximumDelayInSamples (wobble.getLatency() + 1);
    dryDelay.setDelay ((float) wobble.getLatency());
    dryDelay.reset();
    dryBuffer.setSize (numCh, chunkSize);
    setLatencySamples (wobble.getLatency());

    auto init = [&] (juce::SmoothedValue<float>& s, const char* id, double secs = 0.05)
    {
        s.reset (sampleRate / chunkSize, secs);
        s.setCurrentAndTargetValue (apvts.getRawParameterValue (id)->load());
    };
    init (sm.magnitude, ParamID::magnitude);
    init (sm.noise, ParamID::noiseAmt);
    init (sm.wobble, ParamID::wobbleAmt);
    init (sm.distort, ParamID::distortAmt);
    init (sm.digital, ParamID::digitalAmt);
    init (sm.space, ParamID::spaceAmt);
    init (sm.magnetic, ParamID::magneticAmt);
    init (sm.mix, ParamID::mix);
    init (sm.output, ParamID::output);
    init (sm.lowCut, ParamID::lowCut);
    init (sm.highCut, ParamID::highCut);
}

float PatinaProcessor::amt (const char* onId, juce::SmoothedValue<float>& s) const
{
    const float v = s.getNextValue();
    return apvts.getRawParameterValue (onId)->load() > 0.5f ? v * currentMagnitude : 0.0f;
}

void PatinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto target = [this] (juce::SmoothedValue<float>& s, const char* id)
    { s.setTargetValue (apvts.getRawParameterValue (id)->load()); };
    target (sm.magnitude, ParamID::magnitude);
    target (sm.noise, ParamID::noiseAmt);
    target (sm.wobble, ParamID::wobbleAmt);
    target (sm.distort, ParamID::distortAmt);
    target (sm.digital, ParamID::digitalAmt);
    target (sm.space, ParamID::spaceAmt);
    target (sm.magnetic, ParamID::magneticAmt);
    target (sm.mix, ParamID::mix);
    target (sm.output, ParamID::output);
    target (sm.lowCut, ParamID::lowCut);
    target (sm.highCut, ParamID::highCut);

    const int numCh = buffer.getNumChannels();
    for (int start = 0; start < buffer.getNumSamples(); start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, start, n);
        processChunk (chunk);
    }
}

void PatinaProcessor::processChunk (juce::AudioBuffer<float>& chunk)
{
    const int numCh = chunk.getNumChannels();
    const int n = chunk.getNumSamples();
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // dry copy, delayed to match the wobble latency
    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto* in = chunk.getReadPointer (ch);
        auto* dry = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (ch, in[i]);
            dry[i] = dryDelay.popSample (ch);
        }
    }

    currentMagnitude = sm.magnitude.getNextValue();
    const float wobbleAmt   = amt (ParamID::wobbleOn, sm.wobble);
    const float magneticAmt = amt (ParamID::magneticOn, sm.magnetic);
    const float distortAmt  = amt (ParamID::distortOn, sm.distort);
    const float digitalAmt  = amt (ParamID::digitalOn, sm.digital);
    const float spaceAmt    = amt (ParamID::spaceOn, sm.space);
    const float noiseAmt    = amt (ParamID::noiseOn, sm.noise);

    // signal chain: tape transport -> tape -> drive -> converter -> tone -> room -> noise floor
    wobble.process (chunk, wobbleAmt, raw (ParamID::wobbleRate));
    magnetic.process (chunk, magneticAmt, raw (ParamID::magneticDrop));
    distort.process (chunk, distortAmt, raw (ParamID::distortTone));
    digital.process (chunk, digitalAmt, raw (ParamID::digitalRate));

    // tone filters drop out of the path when parked at their extremes
    const float lc = sm.lowCut.getNextValue();
    const float hc = sm.highCut.getNextValue();
    juce::dsp::AudioBlock<float> block (chunk);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    if (lc > 20.5f)  { lowCutFilter.setCutoffFrequency (lc);  lowCutFilter.process (ctx); }
    else               lowCutFilter.reset();
    if (hc < 19900.0f) { highCutFilter.setCutoffFrequency (hc); highCutFilter.process (ctx); }
    else                 highCutFilter.reset();

    space.process (chunk, spaceAmt, raw (ParamID::spaceSize));
    noise.process (chunk, noiseAmt, (int) raw (ParamID::noiseType), raw (ParamID::noiseTone));

    const float mix = sm.mix.getNextValue();
    const float out = juce::Decibels::decibelsToGain (sm.output.getNextValue());
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = chunk.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            wet[i] = (dry[i] + (wet[i] - dry[i]) * mix) * out;
    }
}

juce::AudioProcessorEditor* PatinaProcessor::createEditor()
{
    return new PatinaEditor (*this);
}

void PatinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PatinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PatinaProcessor();
}
