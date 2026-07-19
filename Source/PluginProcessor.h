/*
  ==============================================================================

    ThreeQ - a 3-band equalizer (Low Shelf / Mid Peak / High Shelf)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct ChainSettings
{
    float lowShelfFreq { 100.f }, lowShelfGainInDecibels { 0.f };
    float peakFreq { 1000.f }, peakGainInDecibels { 0.f }, peakQuality { 0.707f };
    float highShelfFreq { 5000.f }, highShelfGainInDecibels { 0.f };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

using Filter = juce::dsp::IIR::Filter<float>;
using Coefficients = Filter::CoefficientsPtr;

namespace FilterCoefficients
{
    Coefficients makeLowShelfCoefficients(const ChainSettings& chainSettings, double sampleRate);
    Coefficients makePeakCoefficients(const ChainSettings& chainSettings, double sampleRate);
    Coefficients makeHighShelfCoefficients(const ChainSettings& chainSettings, double sampleRate);
}

//==============================================================================
/**
*/
class ThreeQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ThreeQAudioProcessor();
    ~ThreeQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

    enum ChainPositions
    {
        LowShelf,
        Peak,
        HighShelf
    };

    using MonoChain = juce::dsp::ProcessorChain<Filter, Filter, Filter>;

private:
    MonoChain leftChain, rightChain;

    void updateLowShelfFilter(const ChainSettings& chainSettings);
    void updatePeakFilter(const ChainSettings& chainSettings);
    void updateHighShelfFilter(const ChainSettings& chainSettings);

    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeQAudioProcessor)
};
