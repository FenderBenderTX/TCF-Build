#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ScfProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { "mode", 1 }, "Mode", StringArray { "Chorus", "Flanger", "Pitch Mod" }, 0));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { "speed", 1 }, "Speed", NormalisableRange<float>(0.1f, 10.f, 0.01f, 0.4f), 0.8f,
        AudioParameterFloatAttributes().withLabel("Hz")));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { "width", 1 }, "Width", NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { "intensity", 1 }, "Intensity", NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { "gain", 1 }, "Input Gain", NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { "stereo", 1 }, "Stereo Out (R = inverted wet)", true));
    p.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { "noise", 1 }, "BBD Noise", true));

    return { p.begin(), p.end() };
}

ScfProcessor::ScfProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    pMode      = apvts.getRawParameterValue("mode");
    pSpeed     = apvts.getRawParameterValue("speed");
    pWidth     = apvts.getRawParameterValue("width");
    pIntensity = apvts.getRawParameterValue("intensity");
    pGain      = apvts.getRawParameterValue("gain");
    pStereo    = apvts.getRawParameterValue("stereo");
    pNoise     = apvts.getRawParameterValue("noise");
}

void ScfProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
}

bool ScfProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    // The pedal is mono-in; accept mono or stereo in, stereo out.
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return l.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void ScfProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nIn = getTotalNumInputChannels();
    const int nOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    scf::Params p;
    p.mode        = (scf::Mode) juce::roundToInt(pMode->load());
    p.speedHz     = pSpeed->load();
    p.width       = pWidth->load();
    p.intensity   = pIntensity->load();
    p.inputGainDb = pGain->load();
    p.stereo      = pStereo->load() > 0.5f;
    p.bbdNoise    = pNoise->load() > 0.5f;
    engine.setParams(p);

    float* l = buffer.getWritePointer(0);
    float* r = nOut > 1 ? buffer.getWritePointer(1) : nullptr;
    const float* in1 = nIn > 1 ? buffer.getReadPointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        // read both inputs before writing either output
        const float mono = in1 != nullptr ? 0.5f * (l[i] + in1[i]) : l[i];
        float oL, oR;
        engine.process(mono, oL, oR);
        l[i] = oL;
        if (r != nullptr) r[i] = oR;
    }
}

juce::AudioProcessorEditor* ScfProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void ScfProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void ScfProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScfProcessor();
}
