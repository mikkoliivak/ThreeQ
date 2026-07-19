/*
  ==============================================================================

    ThreeQ - a 3-band equalizer (Low Shelf / Mid Peak / High Shelf)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class ThreeQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ThreeQLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider& slider) override;

    juce::Label* createSliderTextBox (juce::Slider& slider) override;
};

//==============================================================================
struct EQRotarySlider : juce::Slider
{
    EQRotarySlider() : juce::Slider (juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                      juce::Slider::TextEntryBoxPosition::TextBoxBelow)
    {
        setTextBoxStyle (juce::Slider::TextBoxBelow, true, 68, 18);
    }
};

//==============================================================================
// Draws a small labelled knob: a caption above an EQRotarySlider.
struct LabelledKnob : juce::Component
{
    LabelledKnob (const juce::String& caption, juce::Colour accentColour);

    void resized() override;

    EQRotarySlider slider;
    juce::Label captionLabel;
};

//==============================================================================
// Live magnitude-response graph for the current filter settings.
class ResponseCurveComponent : public juce::Component,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
public:
    explicit ResponseCurveComponent (ThreeQAudioProcessor&);
    ~ResponseCurveComponent() override;

    void paint (juce::Graphics& g) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    ThreeQAudioProcessor& audioProcessor;
    std::atomic<bool> parametersChanged { true };
};

//==============================================================================
/**
*/
class ThreeQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ThreeQAudioProcessorEditor (ThreeQAudioProcessor&);
    ~ThreeQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ThreeQAudioProcessor& audioProcessor;

    ThreeQLookAndFeel lookAndFeel;

    ResponseCurveComponent responseCurveComponent;

    LabelledKnob lowFreqKnob, lowGainKnob;
    LabelledKnob midFreqKnob, midGainKnob, midQKnob;
    LabelledKnob highFreqKnob, highGainKnob;

    juce::Label lowBandLabel, midBandLabel, highBandLabel;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment lowFreqAttachment, lowGainAttachment;
    Attachment midFreqAttachment, midGainAttachment, midQAttachment;
    Attachment highFreqAttachment, highGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeQAudioProcessorEditor)
};
