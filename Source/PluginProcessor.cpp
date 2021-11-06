/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameter.h"

//==============================================================================
SimpleReverbAudioProcessor::SimpleReverbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters (*this)
    , paramDepth (parameters, "Depth", "", 0.0f, 1.0f, 0.5f)
    , paramFrequency (parameters, "LFO Frequency", "Hz", 0.0f, 10.0f, 2.0f)
    , paramWaveform (parameters, "LFO Waveform", waveformItemsUI, waveformSine)
{
    parameters.apvts.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

SimpleReverbAudioProcessor::~SimpleReverbAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleReverbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleReverbAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleReverbAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleReverbAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleReverbAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleReverbAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleReverbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleReverbAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleReverbAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleReverbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;

    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;

    leftReverb.prepare(spec);
    rightReverb.prepare(spec);
    const double smoothTime = 1e-3;
    paramDepth.reset (sampleRate, smoothTime);
    paramFrequency.reset (sampleRate, smoothTime);
    paramWaveform.reset (sampleRate, smoothTime);

    //======================================

    lfoPhase = 0.0f;
    inverseSampleRate = 1.0f / (float)sampleRate;
    twoPi = 2.0f * M_PI;
}

void SimpleReverbAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SimpleReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();

    params.roomSize   = *apvts.getRawParameterValue ("size");
    params.damping    = *apvts.getRawParameterValue ("damp");
    params.width      = *apvts.getRawParameterValue ("width");
    params.wetLevel   = *apvts.getRawParameterValue ("dry/wet");
    params.dryLevel   = 1.0f - *apvts.getRawParameterValue ("dry/wet");
    params.freezeMode = *apvts.getRawParameterValue ("freeze");

    leftReverb.setParameters  (params);
    rightReverb.setParameters (params);

    juce::dsp::AudioBlock<float> block (buffer);

    auto leftBlock  = block.getSingleChannelBlock (0);
    auto rightBlock = block.getSingleChannelBlock (1);

    juce::dsp::ProcessContextReplacing<float> leftContext  (leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext (rightBlock);

    leftReverb.process  (leftContext);
    rightReverb.process (rightContext);
    //======================================

    float currentDepth = paramDepth.getNextValue();
    float currentFrequency = paramFrequency.getNextValue();
    float phase;

    for (int channel = 0; channel < totalNumInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);
        phase = lfoPhase;

        for (int sample = 0; sample < numSamples; ++sample) {
            const float in = channelData[sample];
            float modulation = lfo (phase, (int)paramWaveform.getTargetValue());
            float out = in * (1 - currentDepth + currentDepth * modulation);

            channelData[sample] = out;

            phase += currentFrequency * inverseSampleRate;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }
    }

    lfoPhase = phase;

    //======================================

    for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================
//==============================================================================

float TremoloAudioProcessor::lfo (float phase, int waveform)
{
    float out = 0.0f;

    switch (waveform) {
        case waveformSine: {
            out = 0.5f + 0.5f * sinf (twoPi * phase);
            break;
        }
        case waveformTriangle: {
            if (phase < 0.25f)
                out = 0.5f + 2.0f * phase;
            else if (phase < 0.75f)
                out = 1.0f - 2.0f * (phase - 0.25f);
            else
                out = 2.0f * (phase - 0.75f);
            break;
        }
        case waveformSawtooth: {
            if (phase < 0.5f)
                out = 0.5f + phase;
            else
                out = phase - 0.5f;
            break;
        }
        case waveformInverseSawtooth: {
            if (phase < 0.5f)
                out = 0.5f - phase;
            else
                out = 1.5f - phase;
            break;
        }
        case waveformSquare: {
            if (phase < 0.5f)
                out = 0.0f;
            else
                out = 1.0f;
            break;
        }
        case waveformSquareSlopedEdges: {
            if (phase < 0.48f)
                out = 1.0f;
            else if (phase < 0.5f)
                out = 1.0f - 50.0f * (phase - 0.48f);
            else if (phase < 0.98f)
                out = 0.0f;
            else
                out = 50.0f * (phase - 0.98f);
            break;
        }
    }

    return out;
}

//==============================================================================

bool SimpleReverbAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleReverbAudioProcessor::createEditor()
{
    return new SimpleReverbAudioProcessorEditor (*this);
}

//==============================================================================
void SimpleReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    apvts.state.writeToStream (mos);
    auto state = parameters.apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, sizeInBytes);

    if (tree.isValid())
        apvts.replaceState (tree);
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.apvts.state.getType()))
            parameters.apvts.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleReverbAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("size",
                                                             "size",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f),
                                                             0.5f,
                                                             juce::String(),
                                                             juce::AudioProcessorParameter::genericParameter,
                                                             [](float value, int) {
                                                                value *= 100;
                                                                if (value < 10.0f)
                                                                    return juce::String (value, 2) + " %";
                                                                else if (value < 100.0f)
                                                                    return juce::String (value, 1) + " %";
                                                                else
                                                                    return juce::String (value, 0) + " %"; },
                                                             nullptr));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("damp",
                                                             "damp",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f),
                                                             0.5f,
                                                             juce::String(),
                                                             juce::AudioProcessorParameter::genericParameter,
                                                             [](float value, int) {
                                                                value *= 100;
                                                                if (value < 10.0f)
                                                                    return juce::String (value, 2) + " %";
                                                                else if (value < 100.0f)
                                                                    return juce::String (value, 1) + " %";
                                                                else
                                                                    return juce::String (value, 0) + " %"; },
                                                             nullptr));


    layout.add (std::make_unique<juce::AudioParameterFloat> ("width",
                                                             "width",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f),
                                                             0.5f,
                                                             juce::String(),
                                                             juce::AudioProcessorParameter::genericParameter,
                                                             [](float value, int) {
                                                                value *= 100;
                                                                if (value < 10.0f)
                                                                    return juce::String (value, 2) + " %";
                                                                else if (value < 100.0f)
                                                                    return juce::String (value, 1) + " %";
                                                                else
                                                                    return juce::String (value, 0) + " %"; },
                                                            nullptr));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("dry/wet",
                                                             "dry/wet",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f),
                                                             0.5f,
                                                             juce::String(),
                                                             juce::AudioProcessorParameter::genericParameter,
                                                             [](float value, int) {
                                                                value *= 100;
                                                                if (value < 10.0f)
                                                                    return juce::String (value, 2) + " %";
                                                                else if (value < 100.0f)
                                                                    return juce::String (value, 1) + " %";
                                                                else
                                                                    return juce::String (value, 0) + " %"; },
                                                             nullptr));

    layout.add (std::make_unique<juce::AudioParameterBool> ("freeze", "freeze", false));

    return layout;
}
