#pragma once

#include <JuceHeader.h>

#define PAPALOTE_PRESET_EXTENSION ".papalote"

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessor* processor);
    ~PresetManager();

    int getNumberOfPresets();
    juce::String getPresetName (int index);
    juce::String getCurrentPresetName();

    void createNewPreset();
    void savePreset();
    void saveAsPreset (const juce::String& name);
    bool loadPreset (int index);

    juce::String getPresetDirectory() const;
    bool setPresetDirectory (const juce::String& directory);
    void resetPresetDirectoryToDefault();

    bool getIsCurrentPresetSaved() const { return mIsSaved; }

private:
    void storeLocalPresets();
    void ensurePresetDirectoryExists();
    juce::File getSettingsFile() const;
    void loadPresetDirectorySettings();
    void savePresetDirectorySettings();

    juce::ValueTree getStateTree();
    bool loadStateFromTree (const juce::ValueTree& tree);

    juce::AudioProcessor* mProcessor;
    bool mIsSaved = false;
    juce::File mCurrentlyLoadedPreset;
    juce::Array<juce::File> mLocalPresets;
    juce::String mCurrentPresetName = "Untitled";
    juce::String mPresetDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
