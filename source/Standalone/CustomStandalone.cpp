// Custom standalone entry point for Papalote.
//
// Derived from juce_audio_plugin_client_Standalone.cpp (JUCE 9) so the
// standalone window can be created with a native OS title bar from the start
// (editor-driven title-bar switching glitches and can freeze the window at
// its 128px minimum). Compiled into Papalote_Standalone with
// JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1.

#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "CustomStandaloneFilterWindow.h"
#include "../Components/CustomLookAndFeel.h"

namespace juce
{

class PapaloteStandaloneApp final : public JUCEApplication
{
public:
    PapaloteStandaloneApp()
    {
        PropertiesFile::Options options;

        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    PapaloteFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            // No displays are available, so no window will be created!
            jassertfalse;
            return nullptr;
        }

        // Own background colour from the very first frame (no grey flash).
        return new PapaloteFilterWindow (getApplicationName(),
                                         PapaloteColors::background,
                                         createPluginHolder());
    }

    std::unique_ptr<PapalotePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices = false;

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const Array<PapalotePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<PapalotePluginHolder> (appProperties.getUserSettings(),
                                                       false,
                                                       String{},
                                                       nullptr,
                                                       channelConfig,
                                                       autoOpenMidiDevices);
    }

    void initialise (const String&) override
    {
        mainWindow = rawToUniquePtr (createWindow());

        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

protected:
    ApplicationProperties appProperties;
    std::unique_ptr<PapaloteFilterWindow> mainWindow;

private:
    std::unique_ptr<PapalotePluginHolder> pluginHolder;
};

} // namespace juce

JUCE_CREATE_APPLICATION_DEFINE (juce::PapaloteStandaloneApp)
