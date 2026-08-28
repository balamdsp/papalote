#pragma once

#include <JuceHeader.h>
#include "TapeEmulation.h"

namespace AppConstants
{
    inline constexpr const char *INPUT_TRIM_ID = "input_trim";
    inline constexpr const char *DRIVE_ID = "drive";
    inline constexpr const char *DRY_WET_ID = "dry_wet";
    inline constexpr const char *TAPE_TYPE_ID = "tape_type";
    inline constexpr const char *OS_FACTOR_ID = "os_factor";
    inline constexpr const char *BYPASS_ID = "bypass";
    inline constexpr const char *TONE_ID = "tone";

    inline constexpr const char *CLIP_ID = "clip";

    inline constexpr float DB_MIN = -24.0f;
    inline constexpr float DB_MAX = 12.0f;
    inline constexpr float DB_DEFAULT = 0.0f;
    inline constexpr float DB_SKEW_CENTER = -6.0f;

    inline constexpr float DRIVE_MIN = 0.0f;
    inline constexpr float DRIVE_MAX = 100.0f;
    inline constexpr float DRIVE_DEFAULT = 0.0f;

    inline constexpr float DRY_WET_MIN = 0.0f;
    inline constexpr float DRY_WET_MAX = 1.0f;
    inline constexpr float DRY_WET_DEFAULT = 1.0f;

    inline constexpr float TONE_MIN = 0.0f;
    inline constexpr float TONE_MAX = 1.0f;
    inline constexpr float TONE_DEFAULT = 0.5f;

    inline constexpr float BUTTON_THRESHOLD = 0.5f;

    inline constexpr int TAPE_TYPE_MIN = 0;
    inline constexpr int TAPE_TYPE_MAX = 4;
    inline constexpr int TAPE_TYPE_DEFAULT = 0;

    inline constexpr int OS_FACTOR_DEFAULT_INDEX = 0;

    inline constexpr const char *UI_SCALE_ID = "ui_scale";
    inline constexpr std::array<float, 6> ZOOM_PERCENTS { 75.0f, 100.0f, 125.0f,
                                                          150.0f, 200.0f, 300.0f };
    inline constexpr int UI_SCALE_DEFAULT = 1;   // index of 100%
    inline constexpr float ZOOM_MIN = 0.75f;
    inline constexpr float ZOOM_MAX = 3.0f;
}

class PapaloteAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener,
                               public juce::AsyncUpdater
{
public:
    PapaloteAudioProcessor();
    ~PapaloteAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String &) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState &getAPVTS() { return apvts; }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::unique_ptr<TapeEmulation> ptrTapeEmulation[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplingModule;
    juce::ReadWriteLock oversamplingLock;

    std::atomic<bool> needsOversamplingRebuild{false};
    bool osToggle = false;

    int currentSamplesPerBlock = 512;

    void handleAsyncUpdate() override;
    void rebuildOversamplingModule(int numChannels, int samplesPerBlock);
    void parameterChanged(const juce::String &parameterID, float newValue) override;
    void applyModeToAll(int type);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PapaloteAudioProcessor)
};
