#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

#include "dsp/Engine.h"

class SafeSideAudioProcessor final : public juce::AudioProcessor
{
public:
    SafeSideAudioProcessor();
    ~SafeSideAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Meters for the editor (written on the audio thread, read on the message thread)
    std::array<std::atomic<float>, boba::kNumBands> meterMonoLossDb;
    std::array<std::atomic<float>, boba::kNumBands> meterGuardGain;
    std::atomic<float> meterCorrelation { 1.0f };
    std::atomic<float> meterMonoLossTotalDb { 0.0f };
    std::atomic<float> meterMonoChangeDb { 0.0f };
    std::atomic<float> meterMonoErrorDb { -300.0f };
    std::atomic<float> meterKeepAddedDb { -300.0f };
    std::atomic<int> latencySamples { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };

private:
    template <typename Sample> void processAny (juce::AudioBuffer<Sample>& buffer);
    template <typename Sample> void processBypassedAny (juce::AudioBuffer<Sample>& buffer);
    void pullParams();
    void publishMeters();

    boba::Engine engine;
    std::array<boba::DelayLine, 2> bypassDelay;

    std::atomic<float>* pCharacter = nullptr;
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pKeep = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMonoBelow = nullptr;
    std::atomic<float>* pMonoSafety = nullptr;
    std::atomic<float>* pTransient = nullptr;
    std::atomic<float>* pMono = nullptr;
    std::atomic<float>* pSideSolo = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SafeSideAudioProcessor)
};
