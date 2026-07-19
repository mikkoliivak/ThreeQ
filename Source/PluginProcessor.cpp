/*
  ==============================================================================

    ThreeQ - a 3-band equalizer (Low Shelf / Mid Peak / High Shelf)

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ThreeQAudioProcessor::ThreeQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ThreeQAudioProcessor::~ThreeQAudioProcessor()
{
}

//==============================================================================
const juce::String ThreeQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ThreeQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ThreeQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ThreeQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ThreeQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ThreeQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ThreeQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ThreeQAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String ThreeQAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void ThreeQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void ThreeQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();
}

void ThreeQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ThreeQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ThreeQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    // Process each channel through its own mono chain. Guards against mono
    // buffers (e.g. a mono track in Ableton) where a hardcoded L/R split
    // would read past the end of the block.
    for (size_t channel = 0; channel < juce::jmin ((size_t) 2, block.getNumChannels()); ++channel)
    {
        auto channelBlock = block.getSingleChannelBlock (channel);
        juce::dsp::ProcessContextReplacing<float> context (channelBlock);

        if (channel == 0)
            leftChain.process (context);
        else
            rightChain.process (context);
    }
}

//==============================================================================
bool ThreeQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ThreeQAudioProcessor::createEditor()
{
    return new ThreeQAudioProcessorEditor (*this);
}

//==============================================================================
void ThreeQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void ThreeQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowShelfFreq = apvts.getRawParameterValue("LowShelf Freq")->load();
    settings.lowShelfGainInDecibels = apvts.getRawParameterValue("LowShelf Gain")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.highShelfFreq = apvts.getRawParameterValue("HighShelf Freq")->load();
    settings.highShelfGainInDecibels = apvts.getRawParameterValue("HighShelf Gain")->load();

    return settings;
}

namespace FilterCoefficients
{
    Coefficients makeLowShelfCoefficients(const ChainSettings& chainSettings, double sampleRate)
    {
        return juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate,
                                                                   chainSettings.lowShelfFreq,
                                                                   0.707f,
                                                                   juce::Decibels::decibelsToGain(chainSettings.lowShelfGainInDecibels));
    }

    Coefficients makePeakCoefficients(const ChainSettings& chainSettings, double sampleRate)
    {
        return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                     chainSettings.peakFreq,
                                                                     chainSettings.peakQuality,
                                                                     juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    }

    Coefficients makeHighShelfCoefficients(const ChainSettings& chainSettings, double sampleRate)
    {
        return juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate,
                                                                    chainSettings.highShelfFreq,
                                                                    0.707f,
                                                                    juce::Decibels::decibelsToGain(chainSettings.highShelfGainInDecibels));
    }
}

void ThreeQAudioProcessor::updateLowShelfFilter(const ChainSettings& chainSettings)
{
    auto coefficients = FilterCoefficients::makeLowShelfCoefficients(chainSettings, getSampleRate());
    *leftChain.get<ChainPositions::LowShelf>().coefficients = *coefficients;
    *rightChain.get<ChainPositions::LowShelf>().coefficients = *coefficients;
}

void ThreeQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    auto coefficients = FilterCoefficients::makePeakCoefficients(chainSettings, getSampleRate());
    *leftChain.get<ChainPositions::Peak>().coefficients = *coefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *coefficients;
}

void ThreeQAudioProcessor::updateHighShelfFilter(const ChainSettings& chainSettings)
{
    auto coefficients = FilterCoefficients::makeHighShelfCoefficients(chainSettings, getSampleRate());
    *leftChain.get<ChainPositions::HighShelf>().coefficients = *coefficients;
    *rightChain.get<ChainPositions::HighShelf>().coefficients = *coefficients;
}

void ThreeQAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);

    updateLowShelfFilter(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighShelfFilter(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout ThreeQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("LowShelf Freq", 1),
                                                             "Low Freq",
                                                             juce::NormalisableRange<float>(20.f, 500.f, 1.f, 0.4f),
                                                             100.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("LowShelf Gain", 1),
                                                             "Low Gain",
                                                             juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f),
                                                             0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Freq", 1),
                                                             "Mid Freq",
                                                             juce::NormalisableRange<float>(200.f, 8000.f, 1.f, 0.3f),
                                                             1000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Gain", 1),
                                                             "Mid Gain",
                                                             juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f),
                                                             0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Quality", 1),
                                                             "Mid Q",
                                                             juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
                                                             0.707f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("HighShelf Freq", 1),
                                                             "High Freq",
                                                             juce::NormalisableRange<float>(2000.f, 20000.f, 1.f, 0.3f),
                                                             5000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("HighShelf Gain", 1),
                                                             "High Gain",
                                                             juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f),
                                                             0.0f));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThreeQAudioProcessor();
}
