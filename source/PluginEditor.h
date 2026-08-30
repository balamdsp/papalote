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
    SaveAsDialog (const juce::String& initialName, float scale = 1.0f)
    {
        uiScale = scale;
        papaloteDialogLnf().setScale (uiScale);
        setLookAndFeel (&papaloteDialogLnf());

        titleLabel.setText (">> SAVE PRESET", juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f * uiScale));
        titleLabel.setColour (juce::Label::textColourId,
                              PapaloteColors::textPrimary.withAlpha (0.50f));
        addAndMakeVisible (titleLabel);

        messageLabel.setText ("Enter a name for your preset:",
                             juce::dontSendNotification);
        messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
        messageLabel.setColour (juce::Label::textColourId,
                                PapaloteColors::textPrimary);
        addAndMakeVisible (messageLabel);

        nameEditor.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
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
        auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * uiScale));

        titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * uiScale)));
        area.removeFromTop (juce::roundToInt (5.0f * uiScale));
        messageLabel.setBounds (area.removeFromTop (juce::roundToInt (26.0f * uiScale)));
        area.removeFromTop (juce::roundToInt (8.0f * uiScale));
        nameEditor.setBounds (area.removeFromTop (juce::roundToInt (36.0f * uiScale)));
        area.removeFromTop (juce::roundToInt (12.0f * uiScale));

        auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * uiScale));
        cancelButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (150.0f * uiScale)));
        buttonsRow.removeFromRight (juce::roundToInt (10.0f * uiScale));
        confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * uiScale)));
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

    float uiScale = 1.0f;

    juce::Label titleLabel, messageLabel;
    juce::TextEditor nameEditor;
    juce::TextButton confirmButton, cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveAsDialog)
};

class PapaloteAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Button::Listener,
                                     public juce::AudioProcessorValueTreeState::Listener
{
public:
    PapaloteAudioProcessorEditor (PapaloteAudioProcessor&);
    ~PapaloteAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void buttonClicked (juce::Button* button) override;
    void parentHierarchyChanged() override;
    void parameterChanged (const juce::String &parameterID, float newValue) override;

private:
    void applyZoom (float scale);
    void updateZoomLimits();
    void applyScaledFonts();
    void layoutControls (int w, int h);
    int uiScaleIndex() const;

    CustomLookAndFeel customLookAndFeel;
    PresetManager presetManager;

    float uiScale = 1.0f;

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

    class FaderValueLabel final : public juce::Label
    {
    public:
        FaderValueLabel() : juce::Label ({}, {}) {}
        void editorShown (juce::TextEditor* te) override
        {
            te->setJustification (juce::Justification::centred);
        }
    };
    FaderValueLabel inputTrimValueLabel, clipValueLabel;

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
