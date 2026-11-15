#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PatinaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PatinaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// A knob with a caption underneath.
struct LabelledKnob : public juce::Component
{
    LabelledKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                  const juce::String& caption, juce::Colour accent, bool large);
    void resized() override;

    juce::Slider slider;
    juce::Label caption;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// One colouring stage: power switch, title, big amount knob, and one extra control.
class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState&, const juce::String& title, juce::Colour accent,
                 const juce::String& onId, const juce::String& amtId,
                 const juce::String& extraId, const juce::String& extraCaption, bool extraIsChoice = false);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    juce::Colour accent;
    juce::ToggleButton power;
    juce::AudioProcessorValueTreeState::ButtonAttachment powerAttachment;
    LabelledKnob amount;
    std::unique_ptr<LabelledKnob> extraKnob;
    juce::ComboBox extraChoice;
    juce::Label extraChoiceCaption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;
};

class PatinaEditor : public juce::AudioProcessorEditor
{
public:
    explicit PatinaEditor (PatinaProcessor&);
    ~PatinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PatinaLookAndFeel lnf;
    PatinaProcessor& processor;

    juce::Slider magnitude;
    juce::Label magnitudeCaption;
    juce::AudioProcessorValueTreeState::SliderAttachment magnitudeAttachment;

    ModulePanel noise, wobble, distort, digital, space, magnetic;
    LabelledKnob lowCut, highCut, mix, output;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaEditor)
};
