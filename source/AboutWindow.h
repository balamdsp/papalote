#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

// Function-local static: one L&F instance shared by every dialog lifetime.
// Component::setLookAndFeel does NOT take ownership -- never `new` one here.
inline CustomLookAndFeel& papaloteDialogLnf()
{
    static CustomLookAndFeel lnf;
    return lnf;
}

inline void paintCardLayers (juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    g.setColour (PapaloteColors::cardDark);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (PapaloteColors::accent.withAlpha (0.40f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto inner = bounds.reduced (12.0f);
    g.setColour (PapaloteColors::card);
    g.fillRoundedRectangle (inner, 4.0f);
    g.setColour (PapaloteColors::accent.withAlpha (0.10f));
    g.drawRoundedRectangle (inner, 4.0f, 1.0f);
}

class PapaloteCardDialog : public juce::Component
{
public:
    PapaloteCardDialog (const juce::String& title,
                        const juce::String& message,
                        const juce::String& confirmText,
                        const juce::String& cancelText = {},
                        const juce::File& pathToShow = {})
    {
        setLookAndFeel (&papaloteDialogLnf());

        titleLabel.setText (title, juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        titleLabel.setColour (juce::Label::textColourId,
                              PapaloteColors::textPrimary.withAlpha (0.50f));
        addAndMakeVisible (titleLabel);

        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f));
        messageLabel.setColour (juce::Label::textColourId,
                                PapaloteColors::textPrimary);
        messageLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (messageLabel);

        if (! pathToShow.getFullPathName().isEmpty())
        {
            pathEditor.setReadOnly (true);
            pathEditor.setMultiLine (false, false);
            pathEditor.setReturnKeyStartsNewLine (false);
            pathEditor.setFont (CustomLookAndFeel::makeFont (20.0f));
            pathEditor.setText (pathToShow.getFullPathName());
            addAndMakeVisible (pathEditor);
        }

        if (cancelText.isNotEmpty())
        {
            cancelButton.setButtonText (cancelText);
            cancelButton.onClick = [this] { closeWindow(); };
            addAndMakeVisible (cancelButton);
        }

        confirmButton.setButtonText (confirmText);
        confirmButton.onClick = [this]
        {
            if (onConfirm)
                onConfirm();
            closeWindow();
        };
        addAndMakeVisible (confirmButton);
    }

    ~PapaloteCardDialog() override
    {
        setLookAndFeel (nullptr);
    }

    std::function<void()> onConfirm;

    void paint (juce::Graphics& g) override
    {
        paintCardLayers (g, getLocalBounds().toFloat());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (25);

        titleLabel.setBounds (area.removeFromTop (21));
        area.removeFromTop (5);

        constexpr int rowGap = 10;
        auto buttonsRow = area.removeFromBottom (30);
        area.removeFromBottom (rowGap);

        const int cancelW = cancelButton.isVisible() ? 150 : 0;
        if (cancelButton.isVisible())
        {
            cancelButton.setBounds (buttonsRow.removeFromRight (cancelW));
            buttonsRow.removeFromRight (rowGap);
        }
        confirmButton.setBounds (buttonsRow.removeFromRight (190));

        if (pathEditor.isVisible())
        {
            pathEditor.setBounds (area.removeFromBottom (32));
            area.removeFromBottom (rowGap);
        }

        messageLabel.setBounds (area);
    }

private:
    void closeWindow()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->closeButtonPressed();
    }

    static constexpr int kTitleInsetPx = 5;
    static constexpr int kTitleHeightPx = 16;

    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextEditor pathEditor;
    juce::TextButton confirmButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PapaloteCardDialog)
};

class PapaloteDialogWindow : public juce::DocumentWindow
{
public:
    PapaloteDialogWindow (const juce::String& title, juce::Component* content)
        : juce::DocumentWindow (title, juce::Colour (0xff0c0c0c), allButtons)
    {
        setContentOwned (content, true);
        setResizable (false, false);
    }

    // Empty body in JUCE 9 -- without this override nothing closes.
    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PapaloteDialogWindow)
};

namespace PapaloteDialogs
{
    inline void openWindow (juce::Component* ownedContent,
                            const juce::String& title,
                            juce::Component* centreOn,
                            int w, int h)
    {
        auto* win = new PapaloteDialogWindow (title, ownedContent);

        if (centreOn != nullptr && centreOn->getPeer() != nullptr)
        {
            const auto centre = centreOn->getScreenBounds().getCentre();
            win->setTopLeftPosition (centre.getX() - w / 2, centre.getY() - h / 2);
            win->setSize (w, h);
        }
        else
        {
            win->centreWithSize (w, h);
        }

        win->setVisible (true);
    }
}

class AboutWindow : public juce::DocumentWindow
{
public:
    AboutWindow()
        : juce::DocumentWindow ("About Papalote",
                                juce::Colour (0xff0c0c0c),
                                allButtons)
    {
        const juce::String message =
            "Version " + juce::String (ProjectInfo::versionString)
            + "\n\nA dirt saturator plugin"
            + "\nBy BalamDSP"
            + "\n\nBased on:"
            + "\n  JUCE framework    -- JUCE Ltd (AGPL)"
            + "\n  JClones Phoenix   -- JClones (MIT)"
            + "\n  cool-retro-term   -- Swordfish90 (GPL)"
            + "\n  VT323 typeface    -- Peter Hull (OFL)";

        setContentOwned (
            new PapaloteCardDialog (">> ABOUT PAPALOTE", message, "CLOSE"),
            true);
        setResizable (false, false);

        centreWithSize (460, 380);
        setVisible (true);
    }

    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWindow)
};
