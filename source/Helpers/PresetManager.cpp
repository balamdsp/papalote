#include "PresetManager.h"
#include "../PluginProcessor.h"

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

void PresetManager::setCrtEnabled (bool enabled)
{
    mCrtEnabled = enabled;
    savePresetDirectorySettings();
}

void PresetManager::setCrtStrength (int strength)
{
    mCrtStrength = jlimit (0, 2, strength);
    savePresetDirectorySettings();
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
    return File (mPresetDirectory).getParentDirectory().getChildFile ("settings.xml");
}

File PresetManager::getBookmarkFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile ("Papalote")
        .getChildFile ("settings.xml");
}

void PresetManager::loadPresetDirectorySettings()
{
    String storedDir;
    {
        File bookmarkFile = getBookmarkFile();
        if (bookmarkFile.existsAsFile())
        {
            std::unique_ptr<XmlElement> xml (XmlDocument::parse (bookmarkFile));
            if (xml != nullptr)
            {
                storedDir = xml->getStringAttribute ("presetDirectory", "");
                mCrtEnabled = xml->getBoolAttribute ("crtEnabled", mCrtEnabled);
                mCrtStrength = jlimit (0, 2, xml->getIntAttribute ("crtStrength", mCrtStrength));
            }
        }
    }

    if (storedDir.isEmpty())
    {
        resetPresetDirectoryToDefault();
        return;
    }

    mPresetDirectory = storedDir;

    // If the whole folder was relocated, the settings file that travels with
    // the presets is authoritative over the fixed bookmark.
    mLastSettingsFile = getSettingsFile();
    if (mLastSettingsFile.existsAsFile())
    {
        std::unique_ptr<XmlElement> xml (XmlDocument::parse (mLastSettingsFile));
        if (xml != nullptr)
        {
            const String dir = xml->getStringAttribute ("presetDirectory", "");
            if (! dir.isEmpty())
            {
                mPresetDirectory = dir;
                mLastSettingsFile = getSettingsFile();
            }
            mCrtEnabled = xml->getBoolAttribute ("crtEnabled", mCrtEnabled);
            mCrtStrength = jlimit (0, 2, xml->getIntAttribute ("crtStrength", mCrtStrength));
        }
    }
}

void PresetManager::savePresetDirectorySettings()
{
    File settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    XmlElement xml ("PapaloteSettings");
    xml.setAttribute ("presetDirectory", mPresetDirectory);
    xml.setAttribute ("crtEnabled", mCrtEnabled);
    xml.setAttribute ("crtStrength", mCrtStrength);

    xml.writeTo (settingsFile);

    File bookmarkFile = getBookmarkFile();
    if (bookmarkFile != settingsFile)
    {
        bookmarkFile.getParentDirectory().createDirectory();
        xml.writeTo (bookmarkFile);
    }

    // Drop a stale portable copy left behind by a previous preset folder.
    if (mLastSettingsFile.existsAsFile()
        && mLastSettingsFile != settingsFile
        && mLastSettingsFile != bookmarkFile)
        mLastSettingsFile.deleteFile();

    mLastSettingsFile = settingsFile;
}

void PresetManager::storeLocalPresets()
{
    mLocalPresets.clear();
    File dir (mPresetDirectory);
    if (dir.isDirectory())
    {
        // Recursive; full-path sort == relative-path order under one root.
        for (auto& entry : RangedDirectoryIterator (dir, true, "*" + String (PAPALOTE_PRESET_EXTENSION)))
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

File PresetManager::getPresetFile (int index) const
{
    if (isPositiveAndBelow (index, mLocalPresets.size()))
        return mLocalPresets[index];
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

    return loadPresetFile (mLocalPresets[index]);
}

// Any .papalote file, inside or outside the preset folder. A later Save
// writes back to the picked file via mCurrentlyLoadedPreset.
bool PresetManager::loadPresetFile (const File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;

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
