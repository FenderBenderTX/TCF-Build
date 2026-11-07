#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ScfEngine.h"

class ScfProcessor : public juce::AudioProcessor
{
public:
    ScfProcessor();
    ~ScfProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BBD SCF04 Model"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    scf::Engine engine;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pIntensity = nullptr;
    std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pNoise = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScfProcessor)
};
