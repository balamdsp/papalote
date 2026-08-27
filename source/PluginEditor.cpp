#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AudioSettingsPanel.h"
#include "AboutWindow.h"

#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

static constexpr int W = 800;
static constexpr int H = 460;

static constexpr int TOP_BAR_H = 77;

static constexpr int BODY_TOP = TOP_BAR_H + 10;
static constexpr int BODY_H   = H - TOP_BAR_H;

static constexpr int FADER_OUTER = 60;
static constexpr int FADER_W     = 56;
static constexpr int FADER_GAP   = 12;
static constexpr int BODY_X      = FADER_OUTER + FADER_W + FADER_GAP;
static constexpr int BODY_W      = W - BODY_X * 2;

static constexpr int CARD_PAD = 14;
static constexpr int NUM_ROWS = 3;

static constexpr int TAG_Y       = 83;
static constexpr int TAG_H       = 18;
static constexpr int LABEL_EXTRA = 28;
static constexpr int DB_LABEL_H  = 22;
static constexpr int DB_GAP_ABOVE = 37;

static constexpr int FOOTER_H = 70;

struct CardMetrics
{
    static constexpr int contentInset = 22;
    static constexpr int footerGap    = 16;
    static constexpr int matLabelW    = 80;
    static constexpr int matComboW    = 112;
    static constexpr int osLabelW     = 108;
    static constexpr int osComboW     = 74;
    static constexpr int bypassW      = 100;

    static constexpr int titleH  = 26;
    static constexpr int labelH  = 26;
    static constexpr int sliderH = 44;
    static constexpr int rowGap  = 8;

    static constexpr int totalFooterW = matLabelW + matComboW + footerGap
                                      + osLabelW  + osComboW  + footerGap + bypassW;
    static constexpr int cardW      = totalFooterW;
    static constexpr int cardX      = BODY_X + (BODY_W - cardW) / 2;
    static constexpr int cardY      = TAG_Y + 15;
    static constexpr int cardH      = titleH + NUM_ROWS * (labelH + sliderH)
                                      + (NUM_ROWS - 1) * rowGap + contentInset;
    static constexpr int cardBottom = cardY + cardH;
};

PapaloteAudioProcessorEditor::ScreenContent::ScreenContent()
{
    startTimerHz (2);
}

void PapaloteAudioProcessorEditor::ScreenContent::timerCallback()
{
    cursorVisible = ! cursorVisible;
    repaint();
}

void PapaloteAudioProcessorEditor::ScreenContent::paint (juce::Graphics& g)
{
    using namespace PapaloteColors;

    const int w = getWidth();
    const int h = getHeight();

    g.fillAll (background);

    drawScanlines (g, { 0, 0, w, h }, juce::Colours::black.withAlpha (0.22f));

    g.setColour (body);
    g.fillRect (0, 0, w, TOP_BAR_H);

    {
        const int titleX = 18;

        g.setColour (textBrand);
        g.setFont (CustomLookAndFeel::makeFont (38.0f));
        g.drawText ("PAPALOTE...", titleX, 12, 260, 32,
                    juce::Justification::centredLeft, false);

        if (cursorVisible)
        {
            const auto titleFont = CustomLookAndFeel::makeFont (38.0f);
            const float bannerWidth = juce::GlyphArrangement::getStringWidth (titleFont, "PAPALOTE...");
            g.setColour (textBrand.withAlpha (0.85f));
            g.fillRect ((float) titleX + bannerWidth + 4.0f, 18.0f, 8.0f, 28.0f);
        }

        g.setColour (textMid);
        g.setFont (CustomLookAndFeel::makeFont (18.0f));
        g.drawText ("DIRT SATURATOR", titleX, 46, 220, 18,
                    juce::Justification::centredLeft, false);
    }

    {
        g.setColour (textBrand);
        g.setFont (CustomLookAndFeel::makeFont (24.0f));
        g.drawText ("BalamDSP", w - 120, 16, 100, 26,
                    juce::Justification::centredRight, false);

        g.setColour (textMid.withAlpha (0.55f));
        g.setFont (CustomLookAndFeel::makeFont (22.0f));
        g.drawText ("v" + juce::String (ProjectInfo::versionString), w - 120, 40, 100, 22,
                    juce::Justification::centredRight, false);
    }

    {
        using CM = CardMetrics;

        g.setColour (body);
        g.fillRoundedRectangle ((float) BODY_X, (float) BODY_TOP,
                                (float) BODY_W, (float) BODY_H, 6.0f);

        g.setColour (card);
        g.fillRoundedRectangle ((float) CM::cardX, (float) CM::cardY,
                                (float) CM::cardW, (float) CM::cardH, 6.0f);

        g.setColour (accent.withAlpha (0.08f));
        g.drawRoundedRectangle ((float) CM::cardX + 0.5f, (float) CM::cardY + 0.5f,
                                (float) CM::cardW - 1.0f, (float) CM::cardH - 1.0f, 6.0f, 1.0f);

        const int titleY = CM::cardY + 6;
        g.setColour (textPrimary.withAlpha (0.70f));
        g.setFont (CustomLookAndFeel::makeFont (18.0f));
        g.drawText (">> MAIN PARAMETERS",
                    CM::cardX + CM::contentInset / 2, titleY, CM::cardW - CM::contentInset, 20,
                    juce::Justification::topLeft, false);
    }

    {
        const int faderY0 = BODY_TOP + CARD_PAD;
        const int faderH  = CardMetrics::cardBottom - 5 - faderY0;
        const int dbTextY = faderY0 + faderH + DB_GAP_ABOVE;

        auto drawFaderHeader = [&] (int faderX, const juce::String& tag)
        {
            const int labelX = faderX - LABEL_EXTRA / 2;
            const int labelW = FADER_W + LABEL_EXTRA;

            g.setColour (textMid);
            g.setFont (CustomLookAndFeel::makeFont (22.0f));
            g.drawText (tag, labelX, faderY0 - TAG_H + 4, labelW, TAG_H,
                        juce::Justification::centred, false);

            g.setColour (textMid.withAlpha (0.7f));
            g.setFont (CustomLookAndFeel::makeFont (22.0f));
            g.drawText ("dB", labelX, dbTextY, labelW, DB_LABEL_H,
                        juce::Justification::centred, false);
        };

        drawFaderHeader (FADER_OUTER, "IN");
        drawFaderHeader (w - FADER_OUTER - FADER_W, "OUT");

        if (inputTrimSlider != nullptr && clipSlider != nullptr)
        {
            const int valueY = faderY0 + faderH + 13;
            g.setColour (textPrimary);
            g.setFont (CustomLookAndFeel::makeFont (24.0f));

            {
                const int lx = FADER_OUTER - LABEL_EXTRA / 2;
                const int lw = FADER_W + LABEL_EXTRA;
                g.drawText (inputTrimSlider->getTextFromValue (inputTrimSlider->getValue()),
                            lx, valueY, lw, 24,
                            juce::Justification::centred, false);
            }
            {
                const int lx = w - FADER_OUTER - FADER_W - LABEL_EXTRA / 2;
                const int lw = FADER_W + LABEL_EXTRA;
                g.drawText (clipSlider->getTextFromValue (clipSlider->getValue()),
                            lx, valueY, lw, 24,
                            juce::Justification::centred, false);
            }
        }
    }
}

void PapaloteAudioProcessorEditor::HamburgerButton::paint (juce::Graphics& g)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                           isMouseOver(), isDown());

    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float barW = 18.0f;
    const float barH = 2.0f;
    const float gap  = 5.0f;

    g.setColour (isMouseOver() ? PapaloteColors::textPrimary
                               : PapaloteColors::textMid);
    for (int i = -1; i <= 1; ++i)
        g.fillRect (cx - barW * 0.5f, cy + (float) i * gap - barH * 0.5f, barW, barH);
}

PapaloteAudioProcessorEditor::PapaloteAudioProcessorEditor (PapaloteAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetManager (&p)
{
    setLookAndFeel (&customLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&customLookAndFeel);

    addAndMakeVisible (screenContent);

    addAndMakeVisible (crtOverlay);
    crtOverlay.toFront (false);

    presetDisplay.setButtonText (presetManager.getCurrentPresetName());
    presetDisplay.setClickingTogglesState (false);
    presetDisplay.addListener (this);
    screenContent.addAndMakeVisible (presetDisplay);

    menuButton.setClickingTogglesState (false);
    menuButton.setRepaintsOnMouseActivity (false);
    menuButton.addListener (this);
    screenContent.addAndMakeVisible (menuButton);

    inputTrimSlider.setSliderStyle (juce::Slider::LinearVertical);
    clipSlider.setSliderStyle (juce::Slider::LinearVertical);

    for (auto* s : { &inputTrimSlider, &clipSlider })
    {
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 40, 20);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        screenContent.addAndMakeVisible (s);
    }

    inputTrimAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::INPUT_TRIM_ID, inputTrimSlider);
    clipAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::CLIP_ID, clipSlider);

    screenContent.inputTrimSlider = &inputTrimSlider;
    screenContent.clipSlider = &clipSlider;

    auto setupHorizontalSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        screenContent.addAndMakeVisible (s);

        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (customLookAndFeel.getCustomFont (22.0f));
        l.setColour (juce::Label::textColourId, PapaloteColors::textPrimary);
        screenContent.addAndMakeVisible (l);
    };

    setupHorizontalSlider (dirtSlider, dirtLabel, "Drive");
    setupHorizontalSlider (dryWetDirtSlider, dryWetDirtLabel, "Dry/Wet");
    setupHorizontalSlider (toneSlider, toneLabel, "Tone");

    dirtAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::DRIVE_ID, dirtSlider);
    dryWetDirtAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::DRY_WET_ID, dryWetDirtSlider);

    toneAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::TONE_ID, toneSlider);

    dirtSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (static_cast<int> (value)) + "%";
    };

    dryWetDirtSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (static_cast<int> (value * 100.0)) + "%";
    };

    toneSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (static_cast<int> (value * 100.0)) + "%";
    };

    dirtSlider.updateText();
    dryWetDirtSlider.updateText();
    toneSlider.updateText();

    tapeTypeComboBox.addItem ("Ambar", 1);
    tapeTypeComboBox.addItem ("Obsidiana", 2);
    tapeTypeComboBox.addItem ("Aguamarina", 3);
    tapeTypeComboBox.addItem ("Ceniza", 4);
    tapeTypeComboBox.addItem ("Niebla", 5);
    screenContent.addAndMakeVisible (tapeTypeComboBox);
    tapeTypeAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), AppConstants::TAPE_TYPE_ID, tapeTypeComboBox);

    osFactorComboBox.addItem ("x1", 1);
    osFactorComboBox.addItem ("2x", 2);
    osFactorComboBox.addItem ("4x", 3);
    osFactorComboBox.addItem ("8x", 4);
    osFactorComboBox.addItem ("16x", 5);
    screenContent.addAndMakeVisible (osFactorComboBox);
    osFactorAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), AppConstants::OS_FACTOR_ID, osFactorComboBox);

    for (auto* box : { &tapeTypeComboBox, &osFactorComboBox })
        box->setRepaintsOnMouseActivity (true);

    bypassButton.setButtonText ("BYPASS");
    screenContent.addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), AppConstants::BYPASS_ID, bypassButton);

    auto setupFooterLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (customLookAndFeel.getCustomFont (18.0f));
        l.setColour (juce::Label::textColourId, PapaloteColors::textPrimary);
        screenContent.addAndMakeVisible (l);
    };
    setupFooterLabel (tapeTypeLabel, "Material");
    setupFooterLabel (osFactorLabel, "Oversample");

    setSize (W, H);
}

PapaloteAudioProcessorEditor::~PapaloteAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PapaloteAudioProcessorEditor::parentHierarchyChanged()
{
    if (topLevelIsWindow)
        return;

    if (auto* sfw = dynamic_cast<juce::StandaloneFilterWindow*> (getTopLevelComponent()))
    {
        topLevelIsWindow = true;

        // SafePointer: the editor or the standalone window may be destroyed
        // before this async lambda fires.
        juce::Component::SafePointer<juce::StandaloneFilterWindow> safeSfw { sfw };
        juce::Component::SafePointer<PapaloteAudioProcessorEditor> safeThis { this };
        juce::MessageManager::callAsync ([safeSfw, safeThis]()
        {
            if (safeSfw == nullptr || safeThis == nullptr)
                return;
            safeSfw->setUsingNativeTitleBar (true);
            safeThis->setSize (W, H);
        });
    }
}

void PapaloteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (PapaloteColors::background);
}

void PapaloteAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    screenContent.setBounds (getLocalBounds());
    crtOverlay.setBounds (getLocalBounds());

    if (crtEnabled)
    {
        const float kFrameSize = CRTScreen::getFrameSize();
        const float scale = 1.0f / (1.0f + 2.0f * kFrameSize);
        const float tx = (float) w * 0.5f * (1.0f - scale);
        const float ty = (float) h * 0.5f * (1.0f - scale);
        screenContent.setTransform (
            juce::AffineTransform::scale (scale, scale).translated (tx, ty));
    }
    else
    {
        screenContent.setTransform (juce::AffineTransform());
    }

    {
        const int controlH   = 28;
        const int controlGap = 9;
        const int menuW      = 80;
        const int maxPresetW = 220;

        const int totalControlsW = maxPresetW + controlGap + menuW;
        const int startX = (w - totalControlsW) / 2;
        const int controlsY = (TOP_BAR_H - controlH) / 2;

        presetDisplay.setBounds (startX, controlsY, maxPresetW, controlH);
        menuButton.setBounds (startX + maxPresetW + controlGap, controlsY, menuW, controlH);
    }

    using CM = CardMetrics;

    const int footerY    = CM::cardBottom + 26;
    const int footerRowH = FOOTER_H - 6;
    const int comboH     = 34;
    const int comboY2    = footerY + (footerRowH - comboH) / 2 - 20;

    int bx = BODY_X + CARD_PAD + (BODY_W - 2 * CARD_PAD - CM::totalFooterW) / 2;

    tapeTypeLabel.setBounds (bx, comboY2, CM::matLabelW, comboH);
    bx += CM::matLabelW;
    tapeTypeComboBox.setBounds (bx, comboY2, CM::matComboW, comboH);
    bx += CM::matComboW + CM::footerGap;

    osFactorLabel.setBounds (bx, comboY2, CM::osLabelW, comboH);
    bx += CM::osLabelW;
    osFactorComboBox.setBounds (bx, comboY2, CM::osComboW, comboH);
    bx += CM::osComboW + CM::footerGap;

    bypassButton.setBounds (bx, comboY2 - 1, CM::bypassW, comboH + 2);

    const int faderBottom = CM::cardBottom - 5;
    const int faderY0     = BODY_TOP + CARD_PAD;
    const int faderH      = faderBottom - faderY0;

    inputTrimSlider.setBounds (FADER_OUTER, faderY0, FADER_W, faderH);
    clipSlider.setBounds (w - FADER_OUTER - FADER_W, faderY0, FADER_W, faderH);

    {
        juce::Slider* sliders[NUM_ROWS] = { &dirtSlider, &dryWetDirtSlider, &toneSlider };
        juce::Label*  labels[NUM_ROWS]  = { &dirtLabel, &dryWetDirtLabel, &toneLabel };

        const int contentAreaH = CM::cardH - CM::titleH;
        const int rowsTotalH   = NUM_ROWS * (CM::labelH + CM::sliderH)
                               + (NUM_ROWS - 1) * CM::rowGap;
        const int rowPaddingY  = (contentAreaH - rowsTotalH) / 2;

        for (int i = 0; i < NUM_ROWS; ++i)
        {
            const int rowY = CM::cardY + CM::titleH + rowPaddingY
                           + i * (CM::labelH + CM::sliderH + CM::rowGap);
            labels[i]->setBounds (CM::cardX + CM::contentInset / 2, rowY,
                                  CM::cardW - CM::contentInset, CM::labelH);
            sliders[i]->setBounds (CM::cardX + CM::contentInset / 2, rowY + CM::labelH,
                                   CM::cardW - CM::contentInset, CM::sliderH);
        }
    }
}

void PapaloteAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &menuButton)
        showHamburgerMenu();
    else if (button == &presetDisplay)
        showPresetMenu();
}

void PapaloteAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;

    const int numPresets = presetManager.getNumberOfPresets();
    for (int i = 0; i < numPresets; ++i)
        menu.addItem (i + 1, presetManager.getPresetName (i));

    if (numPresets == 0)
        menu.addItem (1, "(no presets found)");

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&presetDisplay),
        [this] (int result)
        {
            if (result > 0)
            {
                const int index = result - 1;
                if (presetManager.loadPreset (index))
                    updatePresetDisplay();
            }
        });
}

enum HamburgerMenuOption
{
    None = 0,
    Init,
    Save,
    SaveAs,
    SetPresetFolder,
    ResetPresetFolder,
    CrtEnabled,
    StandaloneAudioSettings,
    StandaloneSaveState,
    StandaloneLoadState,
    StandaloneReset,
    About
};

static bool isInStandaloneApp (const juce::Component* c)
{
#if JucePlugin_Build_Standalone
    return c != nullptr
        && dynamic_cast<const juce::StandaloneFilterWindow*> (c->getTopLevelComponent()) != nullptr;
#else
    return false;
#endif
}

void PapaloteAudioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;

    menu.addItem (HamburgerMenuOption::Init, "Init");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::Save, "Save");
    menu.addItem (HamburgerMenuOption::SaveAs, "Save As...");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::SetPresetFolder, "Set Preset Folder");
    menu.addItem (HamburgerMenuOption::ResetPresetFolder, "Reset Preset Folder");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::CrtEnabled,
                  juce::String ("CRT Enabled - ") + (crtEnabled ? "[X]" : "[ ]"));
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::About, "About");

    if (isInStandaloneApp (this))
    {
        menu.addSeparator();
        juce::PopupMenu standaloneSub;
        standaloneSub.addItem (HamburgerMenuOption::StandaloneAudioSettings, "Audio/MIDI Settings...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneSaveState, "Save State...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneLoadState, "Load State...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneReset, "Reset to Default");
        menu.addSubMenu (">> Standalone", standaloneSub);
    }

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&menuButton),
        [this] (int result) { handleMenuResult (result); });
}

void PapaloteAudioProcessorEditor::handleMenuResult (int selectedId)
{
    switch (selectedId)
    {
        case HamburgerMenuOption::None:   break;
        case HamburgerMenuOption::Init:   displayInitPopup();    break;
        case HamburgerMenuOption::Save:   presetManager.savePreset(); break;
        case HamburgerMenuOption::SaveAs: displaySaveAsPopup();  break;

        case HamburgerMenuOption::SetPresetFolder:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Select Preset Folder",
                juce::File (presetManager.getPresetDirectory()),
                "*", true, false, this);

            const auto openFlags = juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectDirectories;

            chooser->launchAsync (openFlags, [this, chooser] (const juce::FileChooser& c)
            {
                const auto result = c.getResult();
                if (result != juce::File{})
                    presetManager.setPresetDirectory (result.getFullPathName());
            });
            break;
        }

        case HamburgerMenuOption::ResetPresetFolder:
            presetManager.resetPresetDirectoryToDefault();
            updatePresetDisplay();
            break;

        case HamburgerMenuOption::CrtEnabled:
            crtEnabled = ! crtEnabled;
            break;

        case HamburgerMenuOption::StandaloneAudioSettings:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::StandaloneFilterWindow*> (tl))
                    new SettingsWindow (sfw->getPluginHolder()->deviceManager);
#endif
            break;

        case HamburgerMenuOption::StandaloneSaveState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::StandaloneFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToSaveState();
#endif
            break;

        case HamburgerMenuOption::StandaloneLoadState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::StandaloneFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToLoadState();
#endif
            break;

        case HamburgerMenuOption::StandaloneReset:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::StandaloneFilterWindow*> (tl))
                {
                    // SafePointer: the window may close before the lambda runs.
                    juce::Component::SafePointer<juce::StandaloneFilterWindow> safeSfw { sfw };
                    juce::MessageManager::callAsync ([safeSfw]
                    {
                        if (safeSfw != nullptr)
                            safeSfw->resetToDefaultState();
                    });
                }
#endif
            break;

        case HamburgerMenuOption::About:
            displayAboutPopup();
            break;

        default:
            break;
    }
}

void PapaloteAudioProcessorEditor::displayInitPopup()
{
    auto options = juce::MessageBoxOptions()
        .withIconType (juce::AlertWindow::NoIcon)
        .withTitle ("Init")
        .withMessage ("Are you sure you want to initialize this preset?")
        .withButton ("Confirm")
        .withButton ("Cancel")
        .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [this] (int result)
    {
        if (result == 1)
        {
            presetManager.createNewPreset();
            updatePresetDisplay();
        }
    });
}

void PapaloteAudioProcessorEditor::displaySaveAsPopup()
{
    auto* dialog = new SaveAsDialog (presetManager.getCurrentPresetName());
    dialog->onConfirm = [this] (const juce::String& name)
    {
        if (name.isNotEmpty())
        {
            presetManager.saveAsPreset (name);
            updatePresetDisplay();
        }
    };
    PapaloteDialogs::openWindow (dialog, "Save Preset", this, 460, 220);
}

void PapaloteAudioProcessorEditor::displayAboutPopup()
{
    new AboutWindow();
}

void PapaloteAudioProcessorEditor::updatePresetDisplay()
{
    presetDisplay.setButtonText (presetManager.getCurrentPresetName());
}
