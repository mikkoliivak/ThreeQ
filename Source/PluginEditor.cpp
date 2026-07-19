/*
  ==============================================================================

    ThreeQ - a 3-band equalizer (Low Shelf / Mid Peak / High Shelf)

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour lowColour  (0xffff6b4a);
    const juce::Colour midColour  (0xff3ddc97);
    const juce::Colour highColour (0xff5b9bff);
    const juce::Colour backgroundColour (0xff0d0e14);
    const juce::Colour panelColour (0xff14161f);
}

//==============================================================================
ThreeQLookAndFeel::ThreeQLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colours::white);
}

void ThreeQLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.f;
    auto centre = bounds.getCentre();
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, radius, radius, 0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (backgroundArc, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    bool bipolar = slider.getProperties().contains ("bipolar") && (bool) slider.getProperties()["bipolar"];
    float startFillAngle = bipolar ? (rotaryStartAngle + rotaryEndAngle) / 2.f : rotaryStartAngle;

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.f, startFillAngle, toAngle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto knobRadius = radius * 0.6f;
    g.setColour (panelColour);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.f, knobRadius * 2.f);
    g.setColour (accent.withAlpha (0.5f));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.f, knobRadius * 2.f, 1.2f);

    juce::Path pointer;
    auto pointerLength = knobRadius * 0.82f;
    auto pointerThickness = 2.6f;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.7f, pointerThickness * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centre));
    g.setColour (juce::Colours::white);
    g.fillPath (pointer);
}

juce::Label* ThreeQLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

//==============================================================================
LabelledKnob::LabelledKnob (const juce::String& caption, juce::Colour accentColour)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    addAndMakeVisible (slider);

    captionLabel.setText (caption, juce::dontSendNotification);
    captionLabel.setJustificationType (juce::Justification::centred);
    captionLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    captionLabel.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (captionLabel);
}

void LabelledKnob::resized()
{
    auto bounds = getLocalBounds();
    captionLabel.setBounds (bounds.removeFromTop (16));
    slider.setBounds (bounds);
}

//==============================================================================
namespace
{
    const std::array<const char*, 7> eqParamIDs {
        "LowShelf Freq", "LowShelf Gain",
        "Peak Freq", "Peak Gain", "Peak Quality",
        "HighShelf Freq", "HighShelf Gain"
    };
}

ResponseCurveComponent::ResponseCurveComponent (ThreeQAudioProcessor& p) : audioProcessor (p)
{
    for (auto* paramID : eqParamIDs)
        audioProcessor.apvts.addParameterListener (paramID, this);

    startTimerHz (30);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    for (auto* paramID : eqParamIDs)
        audioProcessor.apvts.removeParameterListener (paramID, this);
}

void ResponseCurveComponent::parameterChanged (const juce::String&, float)
{
    parametersChanged.store (true);
}

void ResponseCurveComponent::timerCallback()
{
    if (parametersChanged.exchange (false))
        repaint();
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds, 10.f);

    auto plotBounds = bounds.reduced (12.f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (float db : { -24.f, -12.f, 12.f, 24.f })
    {
        auto lineY = juce::jmap (db, 24.f, -24.f, plotBounds.getY(), plotBounds.getBottom());
        g.drawHorizontalLine ((int) lineY, plotBounds.getX(), plotBounds.getRight());
    }

    for (float freq : { 30.f, 100.f, 300.f, 1000.f, 3000.f, 10000.f })
    {
        auto lineX = plotBounds.getX() + plotBounds.getWidth() * std::log10 (freq / 20.f) / std::log10 (1000.f);
        g.drawVerticalLine ((int) lineX, plotBounds.getY(), plotBounds.getBottom());
    }

    auto zeroY = juce::jmap (0.f, 24.f, -24.f, plotBounds.getY(), plotBounds.getBottom());
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine ((int) zeroY, plotBounds.getX(), plotBounds.getRight());

    auto chainSettings = getChainSettings (audioProcessor.apvts);
    auto sampleRate = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    auto lowCoeffs  = FilterCoefficients::makeLowShelfCoefficients (chainSettings, sampleRate);
    auto peakCoeffs = FilterCoefficients::makePeakCoefficients (chainSettings, sampleRate);
    auto highCoeffs = FilterCoefficients::makeHighShelfCoefficients (chainSettings, sampleRate);

    juce::Path responsePath;
    auto width = (int) plotBounds.getWidth();

    for (int i = 0; i < width; ++i)
    {
        auto normX = (float) i / (float) juce::jmax (1, width - 1);
        auto freq = 20.f * std::pow (1000.f, normX);

        double magnitude = 1.0;
        magnitude *= lowCoeffs->getMagnitudeForFrequency (freq, sampleRate);
        magnitude *= peakCoeffs->getMagnitudeForFrequency (freq, sampleRate);
        magnitude *= highCoeffs->getMagnitudeForFrequency (freq, sampleRate);

        auto magnitudeDb = juce::Decibels::gainToDecibels ((float) magnitude);
        auto y = juce::jmap (juce::jlimit (-24.f, 24.f, magnitudeDb), -24.f, 24.f, plotBounds.getBottom(), plotBounds.getY());

        if (i == 0)
            responsePath.startNewSubPath (plotBounds.getX() + (float) i, y);
        else
            responsePath.lineTo (plotBounds.getX() + (float) i, y);
    }

    auto fillPath = responsePath;
    fillPath.lineTo (plotBounds.getRight(), plotBounds.getBottom());
    fillPath.lineTo (plotBounds.getX(), plotBounds.getBottom());
    fillPath.closeSubPath();
    g.setColour (highColour.withAlpha (0.15f));
    g.fillPath (fillPath);

    g.setColour (juce::Colours::white);
    g.strokePath (responsePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.f, 1.f);
}

//==============================================================================
ThreeQAudioProcessorEditor::ThreeQAudioProcessorEditor (ThreeQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      responseCurveComponent (p),
      lowFreqKnob  ("FREQ", lowColour),
      lowGainKnob  ("GAIN", lowColour),
      midFreqKnob  ("FREQ", midColour),
      midGainKnob  ("GAIN", midColour),
      midQKnob     ("Q",    midColour),
      highFreqKnob ("FREQ", highColour),
      highGainKnob ("GAIN", highColour),
      lowFreqAttachment  (p.apvts, "LowShelf Freq",  lowFreqKnob.slider),
      lowGainAttachment  (p.apvts, "LowShelf Gain",  lowGainKnob.slider),
      midFreqAttachment  (p.apvts, "Peak Freq",      midFreqKnob.slider),
      midGainAttachment  (p.apvts, "Peak Gain",      midGainKnob.slider),
      midQAttachment     (p.apvts, "Peak Quality",   midQKnob.slider),
      highFreqAttachment (p.apvts, "HighShelf Freq", highFreqKnob.slider),
      highGainAttachment (p.apvts, "HighShelf Gain", highGainKnob.slider)
{
    setLookAndFeel (&lookAndFeel);

    lowGainKnob.slider.getProperties().set ("bipolar", true);
    midGainKnob.slider.getProperties().set ("bipolar", true);
    highGainKnob.slider.getProperties().set ("bipolar", true);

    auto setupFreqText = [] (juce::Slider& s)
    {
        s.textFromValueFunction = [] (double v)
        {
            if (v < 1000.0)
                return juce::String ((int) std::round (v)) + " Hz";
            return juce::String (v / 1000.0, 2) + " kHz";
        };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        s.updateText();
    };

    auto setupDbText = [] (juce::Slider& s)
    {
        s.textFromValueFunction = [] (double v)
        {
            return (v > 0.0 ? "+" : juce::String()) + juce::String (v, 1) + " dB";
        };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        s.updateText();
    };

    auto setupQText = [] (juce::Slider& s)
    {
        s.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
        s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        s.updateText();
    };

    setupFreqText (lowFreqKnob.slider);
    setupFreqText (midFreqKnob.slider);
    setupFreqText (highFreqKnob.slider);
    setupDbText (lowGainKnob.slider);
    setupDbText (midGainKnob.slider);
    setupDbText (highGainKnob.slider);
    setupQText (midQKnob.slider);

    auto setupBandLabel = [this] (juce::Label& l, const juce::String& text, juce::Colour colour)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, colour);
        addAndMakeVisible (l);
    };

    setupBandLabel (lowBandLabel, "LOW", lowColour);
    setupBandLabel (midBandLabel, "MID", midColour);
    setupBandLabel (highBandLabel, "HIGH", highColour);

    addAndMakeVisible (responseCurveComponent);

    for (auto* knob : { &lowFreqKnob, &lowGainKnob, &midFreqKnob, &midGainKnob, &midQKnob, &highFreqKnob, &highGainKnob })
        addAndMakeVisible (knob);

    setSize (780, 560);
}

ThreeQAudioProcessorEditor::~ThreeQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void ThreeQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto titleArea = getLocalBounds().removeFromTop (40).reduced (16, 6);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText ("ThreeQ", titleArea.removeFromLeft (120), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText ("3-BAND EQUALIZER", titleArea, juce::Justification::centredLeft);
}

void ThreeQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (40);

    auto curveArea = bounds.removeFromTop ((int) (bounds.getHeight() * 0.42f)).reduced (16, 8);
    responseCurveComponent.setBounds (curveArea);

    bounds.reduce (16, 8);

    auto colWidth = bounds.getWidth() / 3;
    auto lowArea  = bounds.removeFromLeft (colWidth);
    auto midArea  = bounds.removeFromLeft (colWidth);
    auto highArea = bounds;

    auto layoutBand = [] (juce::Rectangle<int> area, juce::Label& bandLabel, std::initializer_list<LabelledKnob*> knobs)
    {
        area.reduce (6, 0);
        bandLabel.setBounds (area.removeFromTop (22));
        area.removeFromTop (4);

        auto knobHeight = area.getHeight() / 3;
        for (auto* knob : knobs)
            knob->setBounds (area.removeFromTop (knobHeight));
    };

    layoutBand (lowArea,  lowBandLabel,  { &lowFreqKnob, &lowGainKnob });
    layoutBand (midArea,  midBandLabel,  { &midFreqKnob, &midGainKnob, &midQKnob });
    layoutBand (highArea, highBandLabel, { &highFreqKnob, &highGainKnob });
}
