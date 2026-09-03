#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Components/AudioSettingsPanel.h"
#include "Components/AboutWindow.h"

#include <cmath>
#include <limits>

#if JucePlugin_Build_Standalone
#include "Standalone/CustomStandaloneFilterWindow.h"
#endif

// X11 standalone: _NET_FRAME_EXTENTS for outer sizing. Xlib TU is separate
// (Xlib headers clash with JUCE); queried rarely (zoom + window settle).
extern "C" int papaloteGetFrameExtents (unsigned long windowH,
                                        int* outFrameW, int* outFrameH);

static constexpr int W = 800;
static constexpr int H = 460;
static constexpr int NUM_ROWS = 3;

// Preset-menu sentinel: clear of index+1 item IDs at any list size.
static constexpr int kLoadFromFileId = 0x1001;

static bool isInStandaloneApp (const juce::Component* c);

static juce::Point<int> getNativeFrameSize (juce::Component* topLevelWindow)
{
    if (topLevelWindow != nullptr)
        if (auto* peer = topLevelWindow->getPeer())
            if (peer->getNativeHandle() != nullptr)
            {
                int frameW = 0, frameH = 0;

                if (papaloteGetFrameExtents ((unsigned long) peer->getNativeHandle(),
                                             &frameW, &frameH) != 0)
                    return { frameW, frameH };
            }

    return {};
}

// Scale-aware layout metrics. All values are the fixed logical sizes scaled
// by the zoom factor; instantiate with the current uiScale.
struct Metrics
{
    explicit Metrics (float s) : scale (s) {}

    float scale = 1.0f;
    int sc (float v) const { return juce::roundToInt (v * scale); }

    int TOP_BAR_H   = sc (77.0f);
    int BODY_TOP    = sc (87.0f);
    int BODY_H      = sc (383.0f);
    int FADER_OUTER = sc (60.0f);
    int FADER_W     = sc (56.0f);
    int FADER_GAP   = sc (12.0f);
    int BODY_X      = sc (128.0f);
    int BODY_W      = sc (544.0f);
    int CARD_PAD    = sc (14.0f);
    int TAG_Y       = sc (83.0f);
    int TAG_H       = sc (18.0f);
    int LABEL_EXTRA = sc (28.0f);
    int DB_LABEL_H  = sc (22.0f);
    int DB_GAP_ABOVE = sc (37.0f);
    int FOOTER_H    = sc (70.0f);

    int contentInset = sc (22.0f);
    int footerGap    = sc (16.0f);
    int matLabelW    = sc (80.0f);
    int matComboW    = sc (112.0f);
    int osLabelW     = sc (108.0f);
    int osComboW     = sc (74.0f);
    int bypassW      = sc (100.0f);
    int titleH       = sc (26.0f);
    int labelH       = sc (26.0f);
    int sliderH      = sc (44.0f);
    int rowGap       = sc (8.0f);
    int xButtonW     = sc (92.0f);
    int xButtonH     = sc (24.0f);

    int totalFooterW = matLabelW + matComboW + footerGap
                     + osLabelW + osComboW + footerGap + bypassW;
    int cardW      = totalFooterW;
    int cardX      = BODY_X + (BODY_W - cardW) / 2;
    int cardY      = TAG_Y + sc (15.0f);
    int cardH      = titleH + NUM_ROWS * (labelH + sliderH)
                   + (NUM_ROWS - 1) * rowGap + contentInset;
    int cardBottom = cardY + cardH;
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
    const float s = (float) w / (float) W;
    const Metrics m (s);
    const auto mf = [] (float size, float sc) { return CustomLookAndFeel::makeFont (size * sc); };

    g.fillAll (background);

    drawScanlines (g, { 0, 0, w, h }, juce::Colours::black.withAlpha (0.22f));

    g.setColour (body);
    g.fillRect (0, 0, w, m.TOP_BAR_H);

    {
        const int titleX = m.sc (18.0f);

        g.setColour (textBrand);
        g.setFont (mf (38.0f, s));
        g.drawText ("PAPALOTE...", titleX, m.sc (12.0f), m.sc (260.0f), m.sc (32.0f),
                    juce::Justification::centredLeft, false);

        if (cursorVisible)
        {
            const auto titleFont = mf (38.0f, s);
            const float bannerWidth = (float) juce::GlyphArrangement::getStringWidthInt (titleFont, "PAPALOTE...");
            const float asc = titleFont.getAscent();
            const float desc = titleFont.getDescent();
            const float boxY = (float) m.sc (12.0f);
            const float boxH = (float) m.sc (32.0f);
            const float baseline = boxY + (boxH - (asc + desc)) * 0.5f + asc;
            const float curTop = baseline - asc * 0.7f - (float) m.sc (2.0f);
            const float curBottom = baseline + (float) m.sc (1.0f);
            g.setColour (textBrand.withAlpha (0.85f));
            g.fillRect ((float) titleX + bannerWidth + (float) m.sc (8.0f),
                        curTop, (float) m.sc (13.0f), curBottom - curTop);
        }

        g.setColour (textMid);
        g.setFont (mf (18.0f, s));
        g.drawText ("DIRT SATURATOR", titleX, m.sc (46.0f), m.sc (220.0f), m.sc (18.0f),
                    juce::Justification::centredLeft, false);
    }

    {
        const int brandX = w - m.sc (120.0f);

        g.setColour (textBrand);
        g.setFont (mf (24.0f, s));
        g.drawText ("BalamDSP", brandX, m.sc (16.0f), m.sc (100.0f), m.sc (26.0f),
                    juce::Justification::centredRight, false);

        g.setColour (textMid.withAlpha (0.55f));
        g.setFont (mf (22.0f, s));
        g.drawText ("v" + juce::String (ProjectInfo::versionString), brandX, m.sc (40.0f),
                    m.sc (100.0f), m.sc (22.0f),
                    juce::Justification::centredRight, false);
    }

    {
        g.setColour (body);
        g.fillRoundedRectangle ((float) m.BODY_X, (float) m.BODY_TOP,
                                (float) m.BODY_W, (float) m.BODY_H, (float) m.sc (6.0f));

        g.setColour (card);
        g.fillRoundedRectangle ((float) m.cardX, (float) m.cardY,
                                (float) m.cardW, (float) m.cardH, (float) m.sc (6.0f));

        g.setColour (accent.withAlpha (0.08f));
        g.drawRoundedRectangle ((float) m.cardX + 0.5f, (float) m.cardY + 0.5f,
                                (float) m.cardW - 1.0f, (float) m.cardH - 1.0f,
                                (float) m.sc (6.0f), 1.0f);

        const int titleY = m.cardY + m.sc (6.0f);
        g.setColour (textPrimary.withAlpha (0.70f));
        g.setFont (mf (18.0f, s));
        g.drawText (">> MAIN PARAMETERS",
                    m.cardX + m.contentInset / 2, titleY,
                    m.cardW - m.contentInset, m.sc (20.0f),
                    juce::Justification::topLeft, false);
    }

    {
        const int faderY0 = m.BODY_TOP + m.CARD_PAD;
        const int faderH  = m.cardBottom - m.sc (5.0f) - faderY0;
        const int dbTextY = faderY0 + faderH + m.DB_GAP_ABOVE;

        auto drawFaderHeader = [&] (int faderX, const juce::String& tag)
        {
            const int labelX = faderX - m.LABEL_EXTRA / 2;
            const int labelW = m.FADER_W + m.LABEL_EXTRA;

            g.setColour (textMid);
            g.setFont (mf (22.0f, s));
            g.drawText (tag, labelX, faderY0 - m.TAG_H + m.sc (4.0f), labelW, m.TAG_H,
                        juce::Justification::centred, false);

            g.setColour (textMid.withAlpha (0.7f));
            g.setFont (mf (22.0f, s));
            g.drawText ("dB", labelX, dbTextY, labelW, m.DB_LABEL_H,
                        juce::Justification::centred, false);
        };

        drawFaderHeader (m.FADER_OUTER, "IN");
        drawFaderHeader (w - m.FADER_OUTER - m.FADER_W, "OUT");
    }
}

void PapaloteAudioProcessorEditor::HamburgerButton::paint (juce::Graphics& g)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                           isMouseOver(), isDown());

    const auto bounds = getLocalBounds().toFloat();
    const float sc = static_cast<CustomLookAndFeel&> (getLookAndFeel()).getScale();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float barW = 18.0f * sc;
    const float barH = 2.0f * sc;
    const float gap  = 5.0f * sc;

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

    if (auto* scaleParam = audioProcessor.getAPVTS().getParameter (AppConstants::UI_SCALE_ID))
    {
        const int idx = juce::roundToInt (scaleParam->getValue() * (AppConstants::ZOOM_PERCENTS.size() - 1));
        const int clampedIdx = juce::jlimit (0, (int) AppConstants::ZOOM_PERCENTS.size() - 1, idx);
        uiScale = AppConstants::ZOOM_PERCENTS [(size_t) clampedIdx] / 100.0f;
    }
    customLookAndFeel.setScale (uiScale);

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
        s->setTextBoxStyle (juce::Slider::NoTextBox, false,
                            juce::roundToInt (40.0f * uiScale),
                            juce::roundToInt (20.0f * uiScale));
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        screenContent.addAndMakeVisible (s);
    }

    inputTrimAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::INPUT_TRIM_ID, inputTrimSlider);
    clipAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), AppConstants::CLIP_ID, clipSlider);

    auto setupValueLabel = [this] (juce::Label& l, juce::Slider& s)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, PapaloteColors::textPrimary);
        l.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l.setColour (juce::TextEditor::textColourId, PapaloteColors::textPrimary);
        l.setColour (juce::TextEditor::backgroundColourId, PapaloteColors::background.withAlpha (0.6f));
        l.setColour (juce::TextEditor::outlineColourId, PapaloteColors::accentDim.withAlpha (0.35f));
        l.setColour (juce::TextEditor::highlightColourId, PapaloteColors::accentDim);
        l.setEditable (true, true, false);
        screenContent.addAndMakeVisible (l);

        const auto snapshotText = [this, &l, &s] ()
        {
            l.setText (s.getTextFromValue (s.getValue()), juce::dontSendNotification);
        };
        l.onTextChange = [this, &l, &s, &snapshotText] ()
        {
            const auto text = l.getText();
            if (text.trim().isEmpty())
            {
                snapshotText();
                return;
            }
            const double v = s.snapValue (s.getValueFromText (text), juce::Slider::notDragging);
            if (! juce::approximatelyEqual (v, s.getValue()))
                s.setValue (v, juce::sendNotificationSync);
            l.setText (s.getTextFromValue (s.getValue()), juce::dontSendNotification);
        };
        s.onValueChange = snapshotText;
        snapshotText();
    };
    setupValueLabel (inputTrimValueLabel, inputTrimSlider);
    setupValueLabel (clipValueLabel, clipSlider);

    screenContent.inputTrimSlider = &inputTrimSlider;
    screenContent.clipSlider = &clipSlider;

    auto setupHorizontalSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false,
                           juce::roundToInt (60.0f * uiScale),
                           juce::roundToInt (24.0f * uiScale));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        screenContent.addAndMakeVisible (s);

        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
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

    xdriveButton.setButtonText ("X-DRIVE");
    xdriveButton.setRepaintsOnMouseActivity (true);
    screenContent.addAndMakeVisible (xdriveButton);
    xdriveAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), AppConstants::XDRIVE_ID, xdriveButton);

    xtoneButton.setButtonText ("X-TONE");
    xtoneButton.setRepaintsOnMouseActivity (true);
    screenContent.addAndMakeVisible (xtoneButton);
    xtoneAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), AppConstants::XTONE_ID, xtoneButton);

    dirtSlider.setNormalisableRange ({ (double) AppConstants::DRIVE_MIN,
                                       (double) AppConstants::DRIVE_MAX, 0.01 });
    dryWetDirtSlider.setNormalisableRange ({ (double) AppConstants::DRY_WET_MIN,
                                             (double) AppConstants::DRY_WET_MAX, 0.01 });
    toneSlider.setNormalisableRange ({ (double) AppConstants::TONE_MIN,
                                       (double) AppConstants::TONE_MAX, 0.01 });

    dirtSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value)) + "%";
    };
    dirtSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.getDoubleValue();
    };

    dryWetDirtSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value * 100.0)) + "%";
    };
    dryWetDirtSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.getDoubleValue() / 100.0;
    };

    toneSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value * 100.0)) + "%";
    };
    toneSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.getDoubleValue() / 100.0;
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

    osFactorComboBox.addItem ("1x", 1);
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
        l.setColour (juce::Label::textColourId, PapaloteColors::textPrimary);
        screenContent.addAndMakeVisible (l);
    };
    setupFooterLabel (tapeTypeLabel, "Material");
    setupFooterLabel (osFactorLabel, "Oversample");

    audioProcessor.getAPVTS().addParameterListener (AppConstants::UI_SCALE_ID, this);

    crtEnabled.store (presetManager.getCrtEnabled());
    crtOverlay.setCrtStrength (presetManager.getCrtStrength());

    updateZoomLimits();

    applyZoom (uiScale);

    // Standalone startup race: the wrapper clamps back to resize minimum,
    // discarding this zoom. Re-assert once settled.
    if (isInStandaloneApp (this))
        juce::Timer::callAfterDelay (250, [safeThis = juce::Component::SafePointer<PapaloteAudioProcessorEditor> (this)]
        {
            if (safeThis != nullptr)
                safeThis->applyZoom (safeThis->uiScale);
        });
}

PapaloteAudioProcessorEditor::~PapaloteAudioProcessorEditor()
{
    audioProcessor.getAPVTS().removeParameterListener(AppConstants::UI_SCALE_ID, this);
    setLookAndFeel(nullptr);
}

void PapaloteAudioProcessorEditor::parentHierarchyChanged()
{
    if (topLevelIsWindow)
        return;

    // Recolour host ResizableWindow wrappers too (dynamic_cast no-ops).
    //
    // NOTE: no setUsingNativeTitleBar() here: fires mid-updateContent(),
    // recreating the peer then sticks standalone at 128x128 minimum.
    if (auto* sfw = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
    {
        topLevelIsWindow = true;
        sfw->setColour (juce::ResizableWindow::backgroundColourId, PapaloteColors::background);

        // X11 size lock: peer keeps windowIsResizable or WM hides hints;
        // min==max constrainer pins do the locking. Hosts use legacy path.
        juce::Component::SafePointer<juce::ResizableWindow> safeSfw { sfw };
        juce::Component::SafePointer<PapaloteAudioProcessorEditor> safeThis { this };
        juce::MessageManager::callAsync ([safeSfw, safeThis]()
        {
            if (safeSfw == nullptr || safeThis == nullptr)
                return;
            const int w = juce::roundToInt (W * safeThis->uiScale);
            const int h = juce::roundToInt (H * safeThis->uiScale);
            if (isInStandaloneApp (safeThis.getComponent()))
            {
                if (! safeSfw->isResizable())
                    safeSfw->setResizable (true, false);
                safeSfw->setUsingNativeTitleBar (true); // no-op if already set
                // Outer accounting: live frame in.
                const auto frame = getNativeFrameSize (safeSfw.getComponent());
                const int outerW = juce::jmax (1, w + frame.x);
                const int outerH = juce::jmax (1, h + frame.y);
                if (auto* c = safeSfw->getConstrainer())
                    c->setSizeLimits (outerW, outerH, outerW, outerH);
                safeThis->setSize (w, h);
                safeSfw->setSize (outerW, outerH);
            }
            else
            {
                safeSfw->setUsingNativeTitleBar (true); // no-op if already set
                safeSfw->setResizable (false, false);
                safeSfw->setSize (w, h);
            }
        });

        // Windows: force 1:1 physical scaling (OS display scale off).
       #if JUCE_WINDOWS
        if (auto* peer = getPeer())
            peer->setCustomPlatformScaleFactor (1.0f);
       #endif
    }
}

void PapaloteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (PapaloteColors::background);
}

void PapaloteAudioProcessorEditor::resized()
{
    layoutControls (getWidth(), getHeight());
}

void PapaloteAudioProcessorEditor::applyZoom (float scale)
{
    scale = juce::jlimit (AppConstants::ZOOM_MIN, AppConstants::ZOOM_MAX, scale);
    uiScale = scale;
    customLookAndFeel.setScale (uiScale);

    applyScaledFonts();

    updateZoomLimits();

    const int pixW = juce::roundToInt (W * uiScale);
    const int pixH = juce::roundToInt (H * uiScale);

    // X11: frame lives inside the outer window, so add it back; the client
    // then lands exactly on the editor size (peer frame size stays empty).
    const auto frame = getNativeFrameSize (isInStandaloneApp (this)
                                           ? getTopLevelComponent() : nullptr);

    const int outerW = juce::jmax (1, pixW + frame.x);
    const int outerH = juce::jmax (1, pixH + frame.y);

    // Refresh the window pin BEFORE resizing (WM publishes pinned min==max).
    if (isInStandaloneApp (this))
        if (auto* tl = getTopLevelComponent())
            if (tl != this)
                if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tl))
                    if (auto* c = rw->getConstrainer())
                        c->setSizeLimits (outerW, outerH, outerW, outerH);

    setSize (pixW, pixH);

    // Follow the editor size exactly (outer == editor + frame).
    if (isInStandaloneApp (this))
        if (auto* tl = getTopLevelComponent())
            if (tl != this)
                tl->setSize (outerW, outerH);

    resized();
    repaint();
}

void PapaloteAudioProcessorEditor::updateZoomLimits()
{
    // Standalone: pin constrainer to the zoom size (edge-drag clamp + WM
    // hints). Hosts keep the zoom range for host-managed sizing.
    if (isInStandaloneApp (this))
        setResizeLimits (juce::roundToInt (W * uiScale),
                         juce::roundToInt (H * uiScale),
                         juce::roundToInt (W * uiScale),
                         juce::roundToInt (H * uiScale));
    else
        setResizeLimits (juce::roundToInt (W * AppConstants::ZOOM_MIN),
                         juce::roundToInt (H * AppConstants::ZOOM_MIN),
                         juce::roundToInt (W * AppConstants::ZOOM_MAX),
                         juce::roundToInt (H * AppConstants::ZOOM_MAX));

    setResizable (false, false);
}

void PapaloteAudioProcessorEditor::applyScaledFonts()
{
    for (auto* l : { &dirtLabel, &dryWetDirtLabel, &toneLabel })
        l->setFont (customLookAndFeel.getCustomFont (22.0f));
    for (auto* l : { &inputTrimValueLabel, &clipValueLabel })
        l->setFont (customLookAndFeel.getCustomFont (24.0f));
    for (auto* l : { &tapeTypeLabel, &osFactorLabel })
        l->setFont (customLookAndFeel.getCustomFont (18.0f));
}

void PapaloteAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID != AppConstants::UI_SCALE_ID)
        return;

    auto* scaleParam = audioProcessor.getAPVTS().getParameter (AppConstants::UI_SCALE_ID);
    if (scaleParam == nullptr)
        return;

    const int idx = juce::roundToInt (scaleParam->getValue() * (AppConstants::ZOOM_PERCENTS.size() - 1));
    const int clampedIdx = juce::jlimit (0, (int) AppConstants::ZOOM_PERCENTS.size() - 1, idx);
    applyZoom (AppConstants::ZOOM_PERCENTS [(size_t) clampedIdx] / 100.0f);
}

void PapaloteAudioProcessorEditor::layoutControls (int w, int h)
{
    screenContent.setBounds (0, 0, w, h);
    crtOverlay.setBounds (0, 0, w, h);

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

    const float s = (float) w / (float) W;
    const Metrics m (s);

    {
        const int controlH   = m.sc (28.0f);
        const int controlGap = m.sc (9.0f);
        const int menuW      = m.sc (80.0f);
        const int maxPresetW = m.sc (220.0f);

        const int totalControlsW = maxPresetW + controlGap + menuW;
        const int startX = (w - totalControlsW) / 2;
        const int controlsY = (m.TOP_BAR_H - controlH) / 2;

        presetDisplay.setBounds (startX, controlsY, maxPresetW, controlH);
        menuButton.setBounds (startX + maxPresetW + controlGap, controlsY, menuW, controlH);
    }

    const int footerY    = m.cardBottom + m.sc (26.0f);
    const int footerRowH = m.FOOTER_H - m.sc (6.0f);
    const int comboH     = m.sc (34.0f);
    const int comboY2    = footerY + (footerRowH - comboH) / 2 - m.sc (20.0f);

    int bx = m.BODY_X + m.CARD_PAD + (m.BODY_W - 2 * m.CARD_PAD - m.totalFooterW) / 2;

    tapeTypeLabel.setBounds (bx, comboY2, m.matLabelW, comboH);
    bx += m.matLabelW;
    tapeTypeComboBox.setBounds (bx, comboY2, m.matComboW, comboH);
    bx += m.matComboW + m.footerGap;

    osFactorLabel.setBounds (bx, comboY2, m.osLabelW, comboH);
    bx += m.osLabelW;
    osFactorComboBox.setBounds (bx, comboY2, m.osComboW, comboH);
    bx += m.osComboW + m.footerGap;

    bypassButton.setBounds (bx, comboY2 - m.sc (1.0f), m.bypassW, comboH + m.sc (2.0f));

    const int faderBottom = m.cardBottom - m.sc (5.0f);
    const int faderY0     = m.BODY_TOP + m.CARD_PAD;
    const int faderH      = faderBottom - faderY0;

    inputTrimSlider.setBounds (m.FADER_OUTER, faderY0, m.FADER_W, faderH);
    clipSlider.setBounds (w - m.FADER_OUTER - m.FADER_W, faderY0, m.FADER_W, faderH);

    const int valueY = faderY0 + faderH + m.sc (13.0f);
    const int valueH = m.sc (24.0f);
    inputTrimValueLabel.setBounds (m.FADER_OUTER - m.LABEL_EXTRA / 2, valueY,
                                   m.FADER_W + m.LABEL_EXTRA, valueH);
    clipValueLabel.setBounds (w - m.FADER_OUTER - m.FADER_W - m.LABEL_EXTRA / 2, valueY,
                              m.FADER_W + m.LABEL_EXTRA, valueH);

    {
        juce::Slider* sliders[NUM_ROWS] = { &dirtSlider, &dryWetDirtSlider, &toneSlider };
        juce::Label*  labels[NUM_ROWS]  = { &dirtLabel, &dryWetDirtLabel, &toneLabel };

        const int contentAreaH = m.cardH - m.titleH;
        const int rowsTotalH   = NUM_ROWS * (m.labelH + m.sliderH)
                               + (NUM_ROWS - 1) * m.rowGap;
        const int rowPaddingY  = (contentAreaH - rowsTotalH) / 2;

        for (int i = 0; i < NUM_ROWS; ++i)
        {
            const int rowY = m.cardY + m.titleH + rowPaddingY
                           + i * (m.labelH + m.sliderH + m.rowGap);

            labels[i]->setBounds (m.cardX + m.contentInset / 2, rowY,
                                  m.cardW - m.contentInset, m.labelH);
            sliders[i]->setBounds (m.cardX + m.contentInset / 2, rowY + m.labelH,
                                   m.cardW - m.contentInset, m.sliderH);
        }
    }

    {
        const int xGap       = m.sc (8.0f);
        const int xRight     = m.cardX + m.cardW - m.contentInset / 2;
        const int x1         = xRight - m.xButtonW;                       // X-TONE
        const int x0         = x1 - m.xButtonW - xGap;                    // X-DRIVE
        const int buttonY    = m.cardY + m.sc (4.0f);
        const int buttonH    = m.xButtonH;

        xdriveButton.setBounds (x0, buttonY, m.xButtonW, buttonH);
        xtoneButton.setBounds (x1, buttonY, m.xButtonW, buttonH);
    }
}

int PapaloteAudioProcessorEditor::uiScaleIndex() const
{
    int best = AppConstants::UI_SCALE_DEFAULT;
    float bestDiff = std::numeric_limits<float>::max();
    for (size_t i = 0; i < AppConstants::ZOOM_PERCENTS.size(); ++i)
    {
        const float diff = std::fabs (AppConstants::ZOOM_PERCENTS[i] / 100.0f - uiScale);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = (int) i;
        }
    }
    return best;
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

    if (presetManager.getNumberOfPresets() == 0)
    {
        menu.addItem (1, "(no presets found)");
    }
    else
    {
        addPresetLevel (menu, juce::File (presetManager.getPresetDirectory()));
    }

    menu.addSeparator();
    menu.addItem (kLoadFromFileId, "Load From File...");

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&presetDisplay),
        [this] (int result)
        {
            if (result == kLoadFromFileId)
            {
                loadPresetFileDialog();
            }
            else if (result > 0)
            {
                const int index = result - 1;
                if (presetManager.loadPreset (index))
                    updatePresetDisplay();
            }
        });
}

// File browser for a single .papalote file (any folder, not just presets).
void PapaloteAudioProcessorEditor::loadPresetFileDialog()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Preset File",
        juce::File (presetManager.getPresetDirectory()),
        "*" + juce::String (PAPALOTE_PRESET_EXTENSION));

    juce::Component::SafePointer<PapaloteAudioProcessorEditor> safeThis { this };
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            const auto result = fc.getResult();
            if (result.existsAsFile()
                && safeThis->presetManager.loadPresetFile (result))
                safeThis->updatePresetDisplay();
        });
}

// Folders under dir become ">> name" submenus (mLocalPresets order, so
// index+1 IDs stay valid). Empty folders stay hidden.
void PapaloteAudioProcessorEditor::addPresetLevel (juce::PopupMenu& parent, const juce::File& dir)
{
    const int n = presetManager.getNumberOfPresets();
    juce::Array<juce::File> subdirs;
    for (int i = 0; i < n; ++i)
    {
        const auto f = presetManager.getPresetFile (i);
        if (! f.isAChildOf (dir))
            continue;
        if (f.getParentDirectory() == dir)
        {
            parent.addItem (i + 1, presetManager.getPresetName (i));
        }
        else
        {
            auto child = f.getParentDirectory();
            while (child.getParentDirectory() != dir)
                child = child.getParentDirectory();
            if (! subdirs.contains (child))
                subdirs.add (child);
        }
    }
    for (auto& sub : subdirs)
    {
        juce::PopupMenu sm;
        addPresetLevel (sm, sub);
        parent.addSubMenu (">> " + sub.getFileName(), sm);
    }
}

enum HamburgerMenuOption
{
    None = 0,
    Init,
    Save,
    SaveAs,
    LoadFromFile,
    SetPresetFolder,
    ResetPresetFolder,
    CrtEnabled,
    CrtStrengthLow,
    CrtStrengthMedium,
    CrtStrengthHigh,
    StandaloneAudioSettings,
    StandaloneSaveState,
    StandaloneLoadState,
    StandaloneReset,
    About,
    ZoomBase = 0x1000
};

static bool isInStandaloneApp (const juce::Component* c)
{
#if JucePlugin_Build_Standalone
    return c != nullptr
        && dynamic_cast<const juce::PapaloteFilterWindow*> (c->getTopLevelComponent()) != nullptr;
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
    menu.addItem (HamburgerMenuOption::LoadFromFile, "Load From File...");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::SetPresetFolder, "Set Preset Folder");
    menu.addItem (HamburgerMenuOption::ResetPresetFolder, "Reset Preset Folder");
    menu.addSeparator();
    {
        juce::PopupMenu crtSub;
        crtSub.addItem (HamburgerMenuOption::CrtEnabled,
                        juce::String ("CRT Enabled - ") + (crtEnabled ? "[X]" : "[ ]"));

        juce::PopupMenu strengthSub;
        const int strength = presetManager.getCrtStrength();
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthLow, "Low",
                             true, strength == 0);
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthMedium, "Medium",
                             true, strength == 1);
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthHigh, "High",
                             true, strength == 2);
        crtSub.addSubMenu (">> Strength", strengthSub);

        menu.addSubMenu (">> CRT Layout", crtSub);
    }

    {
        juce::PopupMenu zoomMenu;
        const int currentIndex = uiScaleIndex();
        for (size_t i = 0; i < AppConstants::ZOOM_PERCENTS.size(); ++i)
            zoomMenu.addItem (HamburgerMenuOption::ZoomBase + (int) i,
                              juce::String ((int) AppConstants::ZOOM_PERCENTS[i]) + "%",
                              true, i == (size_t) currentIndex);
        menu.addSubMenu (">> Zoom", zoomMenu);
    }

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
    if (selectedId >= HamburgerMenuOption::ZoomBase
        && selectedId < HamburgerMenuOption::ZoomBase + (int) AppConstants::ZOOM_PERCENTS.size())
    {
        const int idx = selectedId - HamburgerMenuOption::ZoomBase;
        auto* scaleParam = audioProcessor.getAPVTS().getParameter (AppConstants::UI_SCALE_ID);
        if (scaleParam != nullptr)
        {
            const float norm = (float) idx / (float) (AppConstants::ZOOM_PERCENTS.size() - 1);
            scaleParam->setValueNotifyingHost (norm);
        }
        const int clampedIdx = juce::jlimit (0, (int) AppConstants::ZOOM_PERCENTS.size() - 1, idx);
        applyZoom (AppConstants::ZOOM_PERCENTS [(size_t) clampedIdx] / 100.0f);
        return;
    }

    switch (selectedId)
    {
        case HamburgerMenuOption::None:   break;
        case HamburgerMenuOption::Init:   displayInitPopup();    break;
        case HamburgerMenuOption::Save:   presetManager.savePreset(); break;
        case HamburgerMenuOption::SaveAs: displaySaveAsPopup();  break;
        case HamburgerMenuOption::LoadFromFile: loadPresetFileDialog(); break;

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
        {
            const bool newState = ! crtEnabled.load();
            crtEnabled.store (newState);
            presetManager.setCrtEnabled (newState);
            break;
        }

        case HamburgerMenuOption::CrtStrengthLow:
        case HamburgerMenuOption::CrtStrengthMedium:
        case HamburgerMenuOption::CrtStrengthHigh:
        {
            const int strength = selectedId - HamburgerMenuOption::CrtStrengthLow;
            presetManager.setCrtStrength (strength);
            crtOverlay.setCrtStrength (strength);
            break;
        }

        case HamburgerMenuOption::StandaloneAudioSettings:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::PapaloteFilterWindow*> (tl))
                    new SettingsWindow (sfw->getPluginHolder()->deviceManager, uiScale);
#endif
            break;

        case HamburgerMenuOption::StandaloneSaveState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::PapaloteFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToSaveState();
#endif
            break;

        case HamburgerMenuOption::StandaloneLoadState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::PapaloteFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToLoadState();
#endif
            break;

        case HamburgerMenuOption::StandaloneReset:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::PapaloteFilterWindow*> (tl))
                {
                    // SafePointer: the window may close before the lambda runs.
                    juce::Component::SafePointer<juce::PapaloteFilterWindow> safeSfw { sfw };
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
    auto* dialog = new ConfirmDialog (">> INIT",
                                      "Are you sure you want to initialize this preset?",
                                      uiScale);
    dialog->onConfirm = [this]
    {
        presetManager.createNewPreset();
        updatePresetDisplay();
    };
    PapaloteDialogs::openWindow (dialog, "Init", this,
                                 juce::roundToInt (460.0f * uiScale),
                                 juce::roundToInt (220.0f * uiScale));
}

void PapaloteAudioProcessorEditor::displaySaveAsPopup()
{
    auto* dialog = new SaveAsDialog (presetManager.getCurrentPresetName(), uiScale);
    dialog->onConfirm = [this] (const juce::String& name)
    {
        if (name.isNotEmpty())
        {
            presetManager.saveAsPreset (name);
            updatePresetDisplay();
        }
    };
    PapaloteDialogs::openWindow (dialog, "Save Preset", this,
                                 juce::roundToInt (460.0f * uiScale),
                                 juce::roundToInt (220.0f * uiScale));
}

void PapaloteAudioProcessorEditor::displayAboutPopup()
{
    new AboutWindow (uiScale);
}

void PapaloteAudioProcessorEditor::updatePresetDisplay()
{
    presetDisplay.setButtonText (presetManager.getCurrentPresetName());
}
