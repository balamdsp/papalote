#include "PresetManager.h"
#include "PluginProcessor.h"

using namespace juce;

PresetManager::PresetManager (AudioProcessor* processor)
    : mProcessor (processor)
{
    loadPresetDirectorySettings();
    ensurePresetDirectoryExists();
    storeLocalPresets();
}

PresetManager::~PresetManager()
{
    savePresetDirectorySettings();
}

String PresetManager::getPresetDirectory() const
{
    return mPresetDirectory;
}

bool PresetManager::setPresetDirectory (const String& directory)
{
    File dir (directory);
    if (dir.isDirectory())
    {
        mPresetDirectory = directory;
        savePresetDirectorySettings();
        ensurePresetDirectoryExists();
        storeLocalPresets();
        return true;
    }
    return false;
}

void PresetManager::resetPresetDirectoryToDefault()
{
    mPresetDirectory = File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Papalote")
        .getChildFile ("Presets")
        .getFullPathName();
    savePresetDirectorySettings();
    ensurePresetDirectoryExists();
    storeLocalPresets();
}

void PresetManager::ensurePresetDirectoryExists()
{
    File dir (mPresetDirectory);
    if (! dir.isDirectory())
        dir.createDirectory();
}

File PresetManager::getSettingsFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Papalote")
        .getChildFile ("settings.xml");
}

void PresetManager::loadPresetDirectorySettings()
{
    File settingsFile = getSettingsFile();
    if (settingsFile.existsAsFile())
    {
        std::unique_ptr<XmlElement> xml (XmlDocument::parse (settingsFile));
        if (xml != nullptr)
            mPresetDirectory = xml->getStringAttribute ("presetDirectory", "");
    }

    if (mPresetDirectory.isEmpty())
        resetPresetDirectoryToDefault();
}

void PresetManager::savePresetDirectorySettings()
{
    File settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    XmlElement xml ("PapaloteSettings");
    xml.setAttribute ("presetDirectory", mPresetDirectory);
    xml.writeTo (settingsFile);
}

void PresetManager::storeLocalPresets()
{
    mLocalPresets.clear();
    File dir (mPresetDirectory);
    if (dir.isDirectory())
    {
        for (auto& entry : RangedDirectoryIterator (dir, false, "*" + String (PAPALOTE_PRESET_EXTENSION)))
            mLocalPresets.add (entry.getFile());
        mLocalPresets.sort();
    }
}

int PresetManager::getNumberOfPresets()
{
    return mLocalPresets.size();
}

String PresetManager::getPresetName (int index)
{
    if (isPositiveAndBelow (index, mLocalPresets.size()))
        return mLocalPresets[index].getFileNameWithoutExtension();
    return {};
}

String PresetManager::getCurrentPresetName()
{
    return mCurrentPresetName;
}

ValueTree PresetManager::getStateTree()
{
    auto& apvts = dynamic_cast<PapaloteAudioProcessor*> (mProcessor)->getAPVTS();
    auto state = apvts.copyState();
    state.setProperty ("PresetName", mCurrentPresetName, nullptr);
    state.setProperty ("PluginVersion", String (ProjectInfo::versionString), nullptr);
    return state;
}

bool PresetManager::loadStateFromTree (const ValueTree& tree)
{
    if (! tree.isValid())
        return false;

    // The tree IS the APVTS state; replaceState wraps it back in.
    dynamic_cast<PapaloteAudioProcessor*> (mProcessor)->getAPVTS().replaceState (tree);
    return true;
}

void PresetManager::createNewPreset()
{
    mCurrentPresetName = "Untitled";
    mIsSaved = false;
    mCurrentlyLoadedPreset = File();

    for (auto* param : mProcessor->getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

void PresetManager::savePreset()
{
    if (mCurrentlyLoadedPreset.existsAsFile())
    {
        MemoryBlock block;
        mProcessor->getStateInformation (block);
        mCurrentlyLoadedPreset.replaceWithData (block.getData(), block.getSize());
        mIsSaved = true;
    }
    else
    {
        saveAsPreset (mCurrentPresetName);
    }
}

void PresetManager::saveAsPreset (const String& name)
{
    // Strip filesystem-hostile characters; reject empty/dot-only names.
    const String legalName = File::createLegalFileName (name).trim();
    if (legalName.isEmpty() || legalName == "." || legalName == "..")
        return;

    mCurrentPresetName = legalName;
    File presetFile = File (mPresetDirectory).getChildFile (legalName + PAPALOTE_PRESET_EXTENSION);

    MemoryBlock block;
    mProcessor->getStateInformation (block);
    presetFile.replaceWithData (block.getData(), block.getSize());

    mCurrentlyLoadedPreset = presetFile;
    mIsSaved = true;
    storeLocalPresets();
}

bool PresetManager::loadPreset (int index)
{
    if (! isPositiveAndBelow (index, mLocalPresets.size()))
        return false;

    const auto presetFile = mLocalPresets[index];
    MemoryBlock block;
    if (presetFile.loadFileAsData (block))
    {
        mProcessor->setStateInformation (block.getData(), (int) block.getSize());
        mCurrentPresetName = presetFile.getFileNameWithoutExtension();
        mCurrentlyLoadedPreset = presetFile;
        mIsSaved = true;
        return true;
    }
    return false;
}
