#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "EQDsp.h"

class WeiteAudioProcessor : public juce::AudioProcessor
{
public:
    WeiteAudioProcessor();
    ~WeiteAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Preset>& presets();
    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Anzeigen
    std::atomic<float> correlation { 1.0f }, balanceMeter { 0.0f };   // −1 … +1
    static constexpr int scopeLen = 4096;
    std::array<std::atomic<float>, scopeLen> scopeL, scopeR;           // Leuchtschirm-Punkte (L/R nach Bearbeitung)
    std::atomic<int> scopePos { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    eq::Coeffs sideHp, midLp;
    std::array<eq::BiquadState, 2> hpState, midHpState, midLpState;
    float lastMonoFreq = -1;
    juce::SmoothedValue<float> smWidth, smMonoBass, smGainL, smGainR, smOn;
    double eLL = 0, eRR = 0, eLR = 0, avgCoef = 0.001;
    int decim = 0, decimStep = 2;
    int currentProgram = 0;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeiteAudioProcessor)
};
