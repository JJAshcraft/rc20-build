#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour bg        { 0xff1b1815 };
    const juce::Colour panel     { 0xff27221e };
    const juce::Colour panelEdge { 0xff3a332d };
    const juce::Colour text      { 0xffeee4d3 };
    const juce::Colour dim       { 0xff9d907f };
    const juce::Colour knobBody  { 0xff352e28 };

    const juce::Colour noise     { 0xffe0a458 }; // amber
    const juce::Colour wobble    { 0xff6fb3a8 }; // sea green
    const juce::Colour distort   { 0xffd9644a }; // brick
    const juce::Colour digital   { 0xff8f86d6 }; // lavender
    const juce::Colour space     { 0xff5f93c9 }; // denim
    const juce::Colour magnetic  { 0xffb7c26a }; // olive
    const juce::Colour master    { 0xffeee4d3 };
}

static const char* accentProp = "accent";

//==============================================================================
PatinaLookAndFeel::PatinaLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::ComboBox::backgroundColourId, Palette::knobBody);
    setColour (juce::ComboBox::outlineColourId, Palette::panelEdge);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::dim);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::panelEdge);
}

static juce::Colour accentOf (juce::Component& c)
{
    const auto v = c.getProperties()[accentProp];
    return v.isVoid() ? Palette::master : juce::Colour ((juce::uint32) (juce::int64) v);
}

void PatinaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = accentOf (s);
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float track = juce::jmax (3.0f, radius * 0.11f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path bgArc, valueArc;
    bgArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    valueArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour (Palette::panelEdge);
    g.strokePath (bgArc, stroke);
    g.setColour (s.isEnabled() ? accent : accent.withAlpha (0.35f));
    g.strokePath (valueArc, stroke);

    const float bodyR = radius - track * 2.6f;
    g.setGradientFill (juce::ColourGradient (Palette::knobBody.brighter (0.25f), centre.x, centre.y - bodyR,
                                             Palette::knobBody.darker (0.4f), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 3.0f, 3.0f, bodyR * 0.5f, 1.5f);
    g.setColour (Palette::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void PatinaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float,
                                          float, juce::Slider::SliderStyle, juce::Slider&)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 4.0f, (float) w, 8.0f);
    g.setColour (Palette::panelEdge);
    g.fillRoundedRectangle (r, 4.0f);

    juce::ColourGradient grad (Palette::noise, r.getX(), 0, Palette::space, r.getRight(), 0, false);
    grad.addColour (0.5, Palette::distort);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.withRight (pos), 4.0f);

    g.setColour (Palette::text);
    g.fillEllipse (pos - 9.0f, r.getCentreY() - 9.0f, 18.0f, 18.0f);
    g.setColour (Palette::bg);
    g.fillEllipse (pos - 3.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
}

void PatinaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto accent = accentOf (b);
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto c = r.withSizeKeepingCentre (d, d);

    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (c.expanded (2.0f));
    }
    g.setColour (on ? accent : Palette::knobBody.brighter (highlighted ? 0.3f : 0.1f));
    g.fillEllipse (c);

    // power glyph
    const auto ctr = c.getCentre();
    const float pr = d * 0.24f;
    juce::Path p;
    p.addCentredArc (ctr.x, ctr.y, pr, pr, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
    g.setColour (on ? Palette::bg : Palette::dim);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (ctr.x, ctr.y - pr - 1.0f, ctr.x, ctr.y - pr * 0.1f, 1.6f);
}

juce::Label* PatinaLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (12.0f));
    l->setColour (juce::Label::textColourId, Palette::dim);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, Palette::bg);
    l->setColour (juce::TextEditor::textColourId, Palette::text);
    return l;
}

//==============================================================================
LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                            const juce::String& text, juce::Colour accent, bool large)
    : attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.getProperties().set (accentProp, (juce::int64) accent.getARGB());
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (paramId)->convertFrom0to1 (
                                                state.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (slider);

    caption.setText (text.toUpperCase(), juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (juce::FontOptions (large ? 12.5f : 11.0f, juce::Font::bold));
    caption.setColour (juce::Label::textColourId, large ? Palette::text : Palette::dim);
    addAndMakeVisible (caption);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& state, const juce::String& t, juce::Colour a,
                          const juce::String& onId, const juce::String& amtId,
                          const juce::String& extraId, const juce::String& extraCaption, bool extraIsChoice)
    : title (t), accent (a),
      powerAttachment (state, onId, power),
      amount (state, amtId, "Amount", a, true)
{
    power.getProperties().set (accentProp, (juce::int64) a.getARGB());
    power.setTooltip ("Enable " + title);
    addAndMakeVisible (power);
    addAndMakeVisible (amount);

    auto updateEnablement = [this]
    {
        const bool on = power.getToggleState();
        amount.slider.setEnabled (on);
        if (extraKnob) extraKnob->slider.setEnabled (on);
        extraChoice.setEnabled (on);
        repaint();
    };
    power.onStateChange = updateEnablement;

    if (extraIsChoice)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (extraId)))
            extraChoice.addItemList (p->choices, 1);
        choiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, extraId, extraChoice);
        extraChoice.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (extraChoice);
        extraChoiceCaption.setText (extraCaption.toUpperCase(), juce::dontSendNotification);
        extraChoiceCaption.setJustificationType (juce::Justification::centred);
        extraChoiceCaption.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        extraChoiceCaption.setColour (juce::Label::textColourId, Palette::dim);
        addAndMakeVisible (extraChoiceCaption);
    }
    else
    {
        extraKnob = std::make_unique<LabelledKnob> (state, extraId, extraCaption, a, false);
        addAndMakeVisible (*extraKnob);
    }
    updateEnablement();
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    const bool on = power.getToggleState();
    g.setColour (on ? accent : accent.withAlpha (0.3f));
    g.fillRoundedRectangle (r.getX() + 14.0f, r.getY(), 36.0f, 3.0f, 1.5f);

    g.setColour (on ? Palette::text : Palette::dim);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), juce::Rectangle<int> (14, 10, getWidth() - 60, 26),
                juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    power.setBounds (r.getRight() - 26, r.getY(), 26, 26);
    r.removeFromTop (34);

    auto right = r.removeFromRight (juce::roundToInt (r.getWidth() * 0.4f));
    amount.setBounds (r.reduced (4, 0));

    if (extraKnob)
        extraKnob->setBounds (right.withSizeKeepingCentre (right.getWidth(), juce::jmin (right.getHeight(), 100)));
    else
    {
        auto c = right.withSizeKeepingCentre (right.getWidth(), 46);
        extraChoiceCaption.setBounds (c.removeFromTop (16));
        extraChoice.setBounds (c.reduced (2, 2));
    }
}

//==============================================================================
PatinaEditor::PatinaEditor (PatinaProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      magnitudeAttachment (p.apvts, ParamID::magnitude, magnitude),
      noise    (p.apvts, "Noise",    Palette::noise,    ParamID::noiseOn,    ParamID::noiseAmt,    ParamID::noiseType,   "Type", true),
      wobble   (p.apvts, "Wobble",   Palette::wobble,   ParamID::wobbleOn,   ParamID::wobbleAmt,   ParamID::wobbleRate,  "Rate"),
      distort  (p.apvts, "Distort",  Palette::distort,  ParamID::distortOn,  ParamID::distortAmt,  ParamID::distortTone, "Tone"),
      digital  (p.apvts, "Digital",  Palette::digital,  ParamID::digitalOn,  ParamID::digitalAmt,  ParamID::digitalRate, "Rate"),
      space    (p.apvts, "Space",    Palette::space,    ParamID::spaceOn,    ParamID::spaceAmt,    ParamID::spaceSize,   "Size"),
      magnetic (p.apvts, "Magnetic", Palette::magnetic, ParamID::magneticOn, ParamID::magneticAmt, ParamID::magneticDrop,"Dropouts"),
      lowCut  (p.apvts, ParamID::lowCut,  "Low Cut",  Palette::master, false),
      highCut (p.apvts, ParamID::highCut, "High Cut", Palette::master, false),
      mix     (p.apvts, ParamID::mix,     "Mix",      Palette::master, false),
      output  (p.apvts, ParamID::output,  "Output",   Palette::master, false)
{
    setLookAndFeel (&lnf);

    magnitude.setSliderStyle (juce::Slider::LinearHorizontal);
    magnitude.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    magnitude.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    magnitude.setDoubleClickReturnValue (true, 1.0);
    addAndMakeVisible (magnitude);
    magnitudeCaption.setText ("MAGNITUDE", juce::dontSendNotification);
    magnitudeCaption.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    magnitudeCaption.setColour (juce::Label::textColourId, Palette::dim);
    magnitudeCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (magnitudeCaption);

    for (auto* c : std::initializer_list<juce::Component*> { &noise, &wobble, &distort, &digital, &space, &magnetic,
                                                             &lowCut, &highCut, &mix, &output })
        addAndMakeVisible (c);

    // preset browser, grouped by category
    juce::String lastCategory;
    for (int i = 0; i < patina::numFactoryPresets; ++i)
    {
        const auto& preset = patina::factoryPresets[i];
        if (lastCategory != preset.category)
        {
            if (i > 0) presetBox.addSeparator();
            presetBox.addSectionHeading (preset.category);
            lastCategory = preset.category;
        }
        presetBox.addItem (preset.name, i + 1);
    }
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != processor.getCurrentProgram())
            processor.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    auto step = [this] (int delta)
    {
        const int n = patina::numFactoryPresets;
        processor.setCurrentProgram ((processor.getCurrentProgram() + delta + n) % n);
        syncPresetBox();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (+1); };
    for (auto* b : { &prevPreset, &nextPreset })
    {
        b->setColour (juce::TextButton::buttonColourId, Palette::knobBody);
        b->setColour (juce::TextButton::textColourOffId, Palette::text);
        b->setColour (juce::ComboBox::outlineColourId, Palette::panelEdge);
        addAndMakeVisible (b);
    }
    processor.onPresetChanged = [this] { syncPresetBox(); };
    syncPresetBox();

    setSize (900, 600);
}

PatinaEditor::~PatinaEditor()
{
    processor.onPresetChanged = nullptr;
    setLookAndFeel (nullptr);
}

void PatinaEditor::syncPresetBox()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PatinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("patina", 24, 16, 200, 40, juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("VINTAGE COLOUR PROCESSOR", 24, 52, 260, 16, juce::Justification::centredLeft);

    // divider above the master strip
    g.setColour (Palette::panelEdge);
    g.fillRect (24, getHeight() - 130, getWidth() - 48, 1);
}

void PatinaEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 16);

    auto header = r.removeFromTop (60);
    auto mag = header.removeFromRight (330).withSizeKeepingCentre (330, 30);
    header.removeFromLeft (200);
    auto presets = header.withSizeKeepingCentre (juce::jmin (300, header.getWidth() - 20), 30);
    prevPreset.setBounds (presets.removeFromLeft (30));
    nextPreset.setBounds (presets.removeFromRight (30));
    presetBox.setBounds (presets.reduced (4, 0));
    magnitudeCaption.setBounds (mag.removeFromLeft (84));
    magnitude.setBounds (mag.withTrimmedLeft (10));

    r.removeFromTop (12);
    auto footer = r.removeFromBottom (110);
    r.removeFromBottom (12);

    const int gap = 12;
    const int colW = (r.getWidth() - 2 * gap) / 3;
    const int rowH = (r.getHeight() - gap) / 2;
    ModulePanel* panels[] = { &noise, &wobble, &distort, &digital, &space, &magnetic };
    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3, row = i / 3;
        panels[i]->setBounds (r.getX() + col * (colW + gap), r.getY() + row * (rowH + gap), colW, rowH);
    }

    footer.removeFromTop (14);
    auto strip = footer.withSizeKeepingCentre (4 * 110, footer.getHeight());
    for (auto* k : { &lowCut, &highCut, &mix, &output })
        k->setBounds (strip.removeFromLeft (110).reduced (8, 0));
}
