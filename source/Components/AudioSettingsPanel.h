#pragma once

#if JucePlugin_Build_Standalone

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

class AudioSettingsPanel : public Component,
                           public Button::Listener
{
public:
    AudioSettingsPanel (AudioDeviceManager& dm, juce::LookAndFeel& lf, float scale = 1.0f)
        : deviceManager (dm),
          uiScale (scale),
          selectorComp (dm, 0, 2, 0, 2, true, true, false, true)
    {
        selectorComp.setLookAndFeel (&lf);
        addAndMakeVisible (selectorComp);

        closeBtn.setButtonText ("CLOSE");
        closeBtn.setClickingTogglesState (false);
        closeBtn.setRepaintsOnMouseActivity (true);
        closeBtn.addListener (this);
        addAndMakeVisible (closeBtn);

        setSize (juce::roundToInt (760.0f * uiScale),
                 juce::roundToInt (540.0f * uiScale));
    }

    ~AudioSettingsPanel() override
    {
        selectorComp.setLookAndFeel (nullptr);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (PapaloteColors::background);

        auto titleArea = getLocalBounds().removeFromTop (juce::roundToInt (34.0f * uiScale));
        g.setColour (PapaloteColors::textPrimary.withAlpha (0.70f));
        g.setFont (CustomLookAndFeel::makeFont (18.0f * uiScale));
        g.drawText (">> AUDIO / MIDI SETTINGS",
                    titleArea.reduced (juce::roundToInt (12.0f * uiScale),
                                       juce::roundToInt (6.0f * uiScale)),
                    Justification::centredLeft, false);

        g.setColour (PapaloteColors::accent.withAlpha (0.15f));
        g.drawLine (0.0f, (float) juce::roundToInt (33.0f * uiScale), (float) getWidth(),
                    (float) juce::roundToInt (33.0f * uiScale), 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop (juce::roundToInt (34.0f * uiScale));

        auto bottomBar = bounds.removeFromBottom (juce::roundToInt (46.0f * uiScale));
        closeBtn.setBounds (bottomBar.withSizeKeepingCentre (
                                juce::roundToInt (140.0f * uiScale),
                                juce::roundToInt (26.0f * uiScale)));

        selectorComp.setBounds (bounds.reduced (juce::roundToInt (8.0f * uiScale),
                                                juce::roundToInt (4.0f * uiScale)));
    }

private:
    void buttonClicked (Button*) override
    {
        if (auto* dw = findParentComponentOfClass<DocumentWindow>())
            dw->closeButtonPressed();
    }

    AudioDeviceManager& deviceManager;
    float uiScale = 1.0f;
    AudioDeviceSelectorComponent selectorComp;
    TextButton closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsPanel)
};

class SettingsWindow : public DocumentWindow
{
public:
    SettingsWindow (AudioDeviceManager& dm, float scale = 1.0f)
        : DocumentWindow ("Audio/MIDI Settings",
                          Colour (0xff0c0c0c),
                          allButtons)
    {
        settingsLF.setScale (scale);
        setLookAndFeel (&settingsLF);
        setContentOwned (new AudioSettingsPanel (dm, settingsLF, scale), true);
        setResizable (false, false);
        centreWithSize (juce::roundToInt (760.0f * scale),
                        juce::roundToInt (540.0f * scale));
        setVisible (true);
    }

    ~SettingsWindow() override
    {
        setLookAndFeel (nullptr);
    }

    void closeButtonPressed() override { delete this; }

private:
    struct SettingsLookAndFeel : public CustomLookAndFeel
    {
        juce::Font getComboBoxFont (juce::ComboBox&) override { return getFont (19.0f); }

        juce::Font getTextButtonFont (juce::TextButton&, int height) override
        {
            return getFont (jmin (18.0f, (float) height * 0.85f));
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            const int drop = juce::roundToInt (4.0f * uiScale);
            label.setBounds (0, drop, box.getWidth() - 20, box.getHeight() - 7);
            label.setFont (getCustomFont (17.0f));
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, PapaloteColors::textPrimary);
        }

        void drawTickBox (juce::Graphics& g, juce::Component& c,
                          float x, float y, float w, float h,
                          bool ticked, bool isEnabled,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            CustomLookAndFeel::drawTickBox (g, c, x, y, w, h, ticked, isEnabled,
                                            shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool shouldDrawButtonAsHighlighted, bool) override
        {
            using namespace PapaloteColors;
            const bool isOn = button.getToggleState();
            const auto area = button.getLocalBounds().toFloat();

            g.setFont (getCustomFont (16.0f));
            g.setColour (isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

            const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                           : ("[ " + button.getButtonText() + " ]");
            g.drawText (text, area.translated (0, 0), juce::Justification::centred, true);
        }
    };

    SettingsLookAndFeel settingsLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};

#endif
