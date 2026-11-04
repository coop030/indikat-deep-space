#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DeepSpaceReverb.h"

class DeepSpaceProcessor : public juce::AudioProcessor
{
public:
    DeepSpaceProcessor();
    ~DeepSpaceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // for the GUI
    std::atomic<float> wetLevel { 0.0f };

private:
    double sr = 44100.0;
    int currentProgram = 0;

    DeepSpaceReverb reverb;
    juce::AudioBuffer<float> wet;
    juce::dsp::IIR::Filter<float> highCut[2], lowCut[2];
    float lastHighCut = -1.0f, lastLowCut = -1.0f;
    juce::SmoothedValue<float> dryS, wetS;

    std::atomic<float>* pMix; std::atomic<float>* pPredelay; std::atomic<float>* pDecay;
    std::atomic<float>* pHighFreq; std::atomic<float>* pHighShelf; std::atomic<float>* pBassFreq; std::atomic<float>* pBassMult;
    std::atomic<float>* pSize; std::atomic<float>* pAttack; std::atomic<float>* pEarly; std::atomic<float>* pLate;
    std::atomic<float>* pRate; std::atomic<float>* pDepth; std::atomic<float>* pHighCut; std::atomic<float>* pLowCut;
    std::atomic<float>* pMode; std::atomic<float>* pColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepSpaceProcessor)
};
