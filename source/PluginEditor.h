#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "CRTScreen.h"
#include "PresetManager.h"
#include "AboutWindow.h"

class SaveAsDialog : public juce::Component
{
public:
    SaveAsDialog (const juce::String& initialName)
    {
        setLookAndFeel (&papaloteDialogLnf());

        titleLabel.setText (">> SAVE PRESET", juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        titleLabel.setColour (juce::Label::textColourId,
                              PapaloteColors::textPrimary.withAlpha (0.50f));
        addAndMakeVisible (titleLabel);

        messageLabel.setText ("Enter a name for your preset:",
                             juce::dontSendNotification);
        messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f));
        messageLabel.setColour (juce::Label::textColourId,
                                PapaloteColors::textPrimary);
        addAndMakeVisible (messageLabel);

        nameEditor.setFont (CustomLookAndFeel::makeFont (22.0f));
        nameEditor.setText (initialName);
        nameEditor.setSelectAllWhenFocused (true);
        nameEditor.setColour (juce::TextEditor::backgroundColourId, PapaloteColors::buttonOff);
        nameEditor.setColour (juce::TextEditor::textColourId, PapaloteColors::textPrimary);
        nameEditor.setColour (juce::TextEditor::outlineColourId, PapaloteColors::buttonBorder);
        addAndMakeVisible (nameEditor);

        cancelButton.setButtonText ("CANCEL");
        cancelButton.onClick = [this] { closeWindow(); };
        addAndMakeVisible (cancelButton);

        confirmButton.setButtonText ("CONFIRM");
        confirmButton.onClick = [this]
        {
            if (onConfirm)
                onConfirm (nameEditor.getText());
            closeWindow();
        };
        addAndMakeVisible (confirmButton);
    }

    ~SaveAsDialog() override { setLookAndFeel (nullptr); }

    std::function<void (const juce::String&)> onConfirm;

    void paint (juce::Graphics& g) override
    {
        paintCardLayers (g, getLocalBounds().toFloat());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (25);

        titleLabel.setBounds (area.removeFromTop (21));
        area.removeFromTop (5);
        messageLabel.setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        nameEditor.setBounds (area.removeFromTop (36));
        area.removeFromTop (12);

        auto buttonsRow = area.removeFromBottom (30);
        cancelButton.setBounds (buttonsRow.removeFromRight (150));
        buttonsRow.removeFromRight (10);
        confirmButton.setBounds (buttonsRow.removeFromRight (190));
    }

    void visibilityChanged() override
    {
        if (isVisible())
            nameEditor.grabKeyboardFocus();
    }

private:
    void closeWindow()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->closeButtonPressed();
    }

    juce::Label titleLabel, messageLabel;
    juce::TextEditor nameEditor;
    juce::TextButton confirmButton, cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveAsDialog)
};

class PapaloteAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Button::Listener
{
public:
    PapaloteAudioProcessorEditor (PapaloteAudioProcessor&);
    ~PapaloteAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void buttonClicked (juce::Button* button) override;
    void parentHierarchyChanged() override;

private:
    CustomLookAndFeel customLookAndFeel;
    PresetManager presetManager;

    struct ScreenContent : public juce::Component, private juce::Timer
    {
        ScreenContent();
        void paint (juce::Graphics& g) override;
        void timerCallback() override;
        bool cursorVisible = true;

        juce::Slider* inputTrimSlider = nullptr;
        juce::Slider* clipSlider = nullptr;
    };

    ScreenContent screenContent;
    CRTScreen crtOverlay { &screenContent, &crtEnabled };
    std::atomic<bool> crtEnabled{ true };

    class HamburgerButton : public juce::TextButton
    {
    public:
        using TextButton::TextButton;
        void paint (juce::Graphics& g) override;
    };

    juce::TextButton presetDisplay;
    HamburgerButton  menuButton;

    void showPresetMenu();
    void showHamburgerMenu();
    void handleMenuResult (int selectedId);
    void updatePresetDisplay();
    void displayInitPopup();
    void displaySaveAsPopup();
    void displayAboutPopup();

    juce::Slider inputTrimSlider, clipSlider;

    juce::Slider dirtSlider, dryWetDirtSlider, toneSlider;
    juce::Label  dirtLabel, dryWetDirtLabel, toneLabel;

    HoverableComboBox tapeTypeComboBox, osFactorComboBox;
    juce::Label    tapeTypeLabel, osFactorLabel;
    juce::ToggleButton bypassButton;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputTrimAttachment, clipAttachment,
                                       dirtAttachment, dryWetDirtAttachment,
                                       toneAttachment;
    std::unique_ptr<ComboBoxAttachment> tapeTypeAttachment, osFactorAttachment;
    std::unique_ptr<ButtonAttachment>   bypassAttachment;

    PapaloteAudioProcessor& audioProcessor;
    bool topLevelIsWindow = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PapaloteAudioProcessorEditor)
};
