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
    juce::File getPresetFile (int index) const;
    juce::String getCurrentPresetName();

    void createNewPreset();
    void savePreset();
    void saveAsPreset (const juce::String& name);
    bool loadPreset (int index);
    bool loadPresetFile (const juce::File& file);

    juce::String getPresetDirectory() const;
    bool setPresetDirectory (const juce::String& directory);
    void resetPresetDirectoryToDefault();

    bool getCrtEnabled() const noexcept { return mCrtEnabled; }
    void setCrtEnabled (bool enabled);

    int getCrtStrength() const noexcept { return mCrtStrength; }
    void setCrtStrength (int strength);

    bool getIsCurrentPresetSaved() const { return mIsSaved; }

private:
    void storeLocalPresets();
    void ensurePresetDirectoryExists();
    juce::File getSettingsFile() const;
    juce::File getBookmarkFile() const;
    void loadPresetDirectorySettings();
    void savePresetDirectorySettings();

    juce::ValueTree getStateTree();
    bool loadStateFromTree (const juce::ValueTree& tree);

    juce::AudioProcessor* mProcessor;
    bool mIsSaved = false;
    bool mCrtEnabled = true;
    int mCrtStrength = 2;
    juce::File mCurrentlyLoadedPreset;
    juce::Array<juce::File> mLocalPresets;
    juce::String mCurrentPresetName = "Untitled";
    juce::String mPresetDirectory;
    juce::File mLastSettingsFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
