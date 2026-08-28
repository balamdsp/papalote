#pragma once
#include <JuceHeader.h>

namespace PapaloteColors
{
    static const juce::Colour background{0xff0a0502};
    static const juce::Colour body{0xff0c0602};
    static const juce::Colour card{0xff120904};
    static const juce::Colour cardDark{0xff0e0703};
    static const juce::Colour headerBg{0xff0a0502};

    static const juce::Colour accent{0xffffa200};
    static const juce::Colour highlight{0xffffcc44};
    static const juce::Colour accentDim{0xff331e00};
    static const juce::Colour accentSoft{0xff886200};

    static const juce::Colour textPrimary{0xffffa200};
    static const juce::Colour textMid{0xffbb7e00};
    static const juce::Colour textBrand{0xffffcc44};

    static const juce::Colour buttonOff{0xff120904};
    static const juce::Colour buttonOn{0xff261508};
    static const juce::Colour buttonBorder{0xff4a3010};

    static const juce::Colour menuBg{0xff0e0703};
    static const juce::Colour menuText{0xffffa200};
    static const juce::Colour menuTextBright{0xffffcc44};
    static const juce::Colour menuTextDim{0xff886200};
    static const juce::Colour menuHover{0xff1a0f06};
    static const juce::Colour menuBorder{0xff3a2610};
    static const juce::Colour menuInnerBorder{0xff120904};
}

inline void drawScanlines (juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           juce::Colour lineColour,
                           int phase = 0)
{
    g.setColour (lineColour);
    for (int y = area.getY() + phase; y < area.getBottom(); y += 2)
        g.fillRect (area.getX(), y, area.getWidth(), 1);
}

class HoverableComboBox : public juce::ComboBox
{
public:
    using ComboBox::ComboBox;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }
};

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        uiScale = 1.0f;
        using namespace PapaloteColors;

        vt323 = juce::Typeface::createSystemTypefaceFor(
            BinaryData::VT323Regular_ttf,
            BinaryData::VT323Regular_ttfSize);

        setColour(juce::ResizableWindow::backgroundColourId, background);

        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accentDim);
        setColour(juce::Slider::rotarySliderFillColourId, highlight);
        setColour(juce::Slider::rotarySliderOutlineColourId, accentDim);
        setColour(juce::Slider::textBoxTextColourId, textPrimary);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, accentDim);

        setColour(juce::ComboBox::backgroundColourId, buttonOff);
        setColour(juce::ComboBox::outlineColourId, buttonBorder);
        setColour(juce::ComboBox::textColourId, textPrimary);
        setColour(juce::ComboBox::arrowColourId, textMid);

        setColour(juce::TextButton::buttonColourId, buttonOff);
        setColour(juce::TextButton::buttonOnColourId, buttonOn);
        setColour(juce::TextButton::textColourOffId, textMid);
        setColour(juce::TextButton::textColourOnId, accent);

        setColour(juce::Label::textColourId, textMid);

        setColour(juce::PopupMenu::backgroundColourId, menuBg);
        setColour(juce::PopupMenu::textColourId, menuText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, menuHover);
        setColour(juce::PopupMenu::highlightedTextColourId, menuTextBright);

        setColour(juce::TextEditor::backgroundColourId, menuBg);
        setColour(juce::TextEditor::textColourId, menuTextBright);
        setColour(juce::TextEditor::highlightColourId, menuHover);
        setColour(juce::TextEditor::highlightedTextColourId, menuTextBright);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, menuBorder);
        setColour(juce::Label::textWhenEditingColourId, menuTextBright);
        setColour(juce::Label::backgroundWhenEditingColourId, menuBg);
        setColour(juce::Label::outlineWhenEditingColourId, menuBorder);
        setColour(juce::CaretComponent::caretColourId, menuTextBright);

        setColour(juce::DocumentWindow::textColourId, textBrand);
        setColour(juce::ListBox::backgroundColourId, body);
        setColour(juce::ListBox::textColourId, textPrimary);
        setColour(juce::ListBox::outlineColourId, menuBorder);
        setColour(juce::GroupComponent::outlineColourId, accentDim);
        setColour(juce::GroupComponent::textColourId, textMid);
        setColour(juce::ToggleButton::textColourId, textPrimary);
        setColour(juce::ScrollBar::thumbColourId, accentDim);
    }

    static const juce::Typeface::Ptr& getTypeface()
    {
        static const juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::VT323Regular_ttf,
            BinaryData::VT323Regular_ttfSize);
        return tf;
    }

    static juce::Font makeFont(float height)
    {
        return juce::Font(juce::FontOptions(getTypeface())).withHeight(height);
    }

    void setScale (float s) noexcept { uiScale = s; }
    float getScale() const noexcept { return uiScale; }

    juce::Font getFont(float height, bool /*bold*/ = true) const
    {
        const float h = height * uiScale;
        if (vt323 != nullptr)
            return juce::Font(juce::FontOptions(vt323)).withHeight(h);
        return juce::Font(juce::FontOptions(
            juce::Font::getDefaultSansSerifFontName(), h, juce::Font::plain));
    }

    juce::Font getCustomFont(float height, bool bold = true) const
    {
        return getFont(height, bold);
    }

    float uiScale = 1.0f;

    juce::Font getComboBoxFont(juce::ComboBox&) override { return getFont(18.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return getFont(18.0f); }
    juce::Font getPopupMenuFont() override { return getCustomFont(18.0f); }
    int getPopupMenuBorderSize() override { return juce::roundToInt (3.0f * uiScale); }
    juce::Font getAlertWindowTitleFont() override { return getCustomFont(36.0f); }
    juce::Font getAlertWindowMessageFont() override { return getCustomFont(26.0f); }
    int getAlertWindowButtonHeight() override { return juce::roundToInt (34.0f * uiScale); }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override
    {
        LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator,
                                                   standardMenuItemHeight,
                                                   idealWidth, idealHeight);
        idealWidth = juce::jmax (idealWidth, roundToInt (220.0f * uiScale));
        idealHeight = roundToInt ((float) idealHeight * uiScale);
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        juce::Slider::SliderLayout layout;
        auto bounds = slider.getLocalBounds();

        if (slider.isRotary())
        {
            layout.textBoxBounds = bounds.removeFromBottom(30)
                                       .removeFromRight(60)
                                       .translated(-8, -4);
            layout.sliderBounds = bounds.expanded(4, 4)
                                      .translated(0, 3);
        }
        else if (slider.isHorizontal())
        {
            layout.textBoxBounds = bounds.removeFromRight (juce::roundToInt (64.0f * uiScale)).translated (0, -1);
            layout.sliderBounds = bounds.reduced (juce::roundToInt (4.0f * uiScale), 0);
        }
        else
        {
            layout.textBoxBounds = {};
            layout.sliderBounds = bounds.reduced (0, juce::roundToInt (8.0f * uiScale)).translated (0, juce::roundToInt (8.0f * uiScale));
        }
        return layout;
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        using namespace PapaloteColors;

        const float cx = (float)x + (float)width * 0.5f;
        const float cy = (float)y + (float)height * 0.54f;
        const float r = juce::jmin((float)width, (float)height) * 0.5f - 26.0f;
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float arcR = r + 6.0f;

        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(accentDim);
        g.strokePath(bgArc, juce::PathStrokeType(3.0f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::square));

        juce::Path valArc;
        valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                             rotaryStartAngle, toAngle, true);
        g.setColour(highlight);
        g.strokePath(valArc, juce::PathStrokeType(3.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::square));

        g.setColour(cardDark);
        g.fillRoundedRectangle(cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);

        const float outerR = r * 0.85f;
        g.setColour(accent);
        const float pLen = 6.0f;
        const float pWid = 3.0f;
        juce::Path pointer;
        pointer.addRectangle(cx - pWid * 0.5f, cy - outerR - pLen,
                             pWid, pLen);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle, cx, cy));
        g.fillPath(pointer);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider&) override
    {
        using namespace PapaloteColors;

        if (style == juce::Slider::LinearHorizontal)
        {
            const float th = 12.0f * uiScale;
            juce::Rectangle<float> track{
                (float)x, (float)y + (float)height * 0.5f - th * 0.5f,
                (float)width, th};

            g.setColour(accentDim);
            g.fillRect(track);

            const float fillW = sliderPos - track.getX();
            if (fillW > 0.0f)
            {
                g.setColour(highlight);
                g.fillRect(juce::Rectangle<float>(track.getX(), track.getY(), fillW, th));
            }
            return;
        }

        const float tw = 12.0f * uiScale;

        juce::Rectangle<float> track{
            (float)x + (float)width * 0.5f - tw * 0.5f,
            (float)y, tw, (float)height};

        g.setColour(accentDim);
        g.fillRect(track);

        float fillH = track.getBottom() - sliderPos;
        float fillY = track.getBottom() - fillH;

        g.setColour(highlight);
        g.fillRect(juce::Rectangle<float>(track.getX(), fillY, tw, fillH));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool) override
    {
        using namespace PapaloteColors;

        const bool isOn = button.getToggleState();
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        if (isOn)
        {
            g.setColour(buttonOn);
            g.fillRoundedRectangle(bounds, 2.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(buttonOn.withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 2.0f);
        }
        else
        {
            g.setColour(buttonOff);
            g.fillRoundedRectangle(bounds, 2.0f);
        }

        g.setColour(buttonBorder);
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool) override
    {
        using namespace PapaloteColors;

        const bool isOn = button.getToggleState();
        const auto area = button.getLocalBounds().toFloat();

        if (button.getButtonText() == "X")
        {
            g.setFont(getCustomFont(18.0f));
            g.setColour(shouldDrawButtonAsHighlighted ? textBrand : textMid);
            g.drawText("X", area, juce::Justification::centred, false);
            return;
        }

        g.setFont(getCustomFont(18.0f));
        g.setColour(isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

        const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                       : ("[ " + button.getButtonText() + " ]");

        g.drawText(text, area, juce::Justification::centred, true);
    }

    void drawTickBox (juce::Graphics& g, juce::Component&,
                      float x, float y, float w, float h,
                      bool ticked, bool isEnabled,
                      bool shouldDrawButtonAsHighlighted, bool) override
    {
        using namespace PapaloteColors;

        const juce::Rectangle<float> box (x, y + (h - w) * 0.5f, w, w);

        g.setColour (shouldDrawButtonAsHighlighted ? buttonOn : buttonOff);
        g.fillRoundedRectangle (box, 2.0f);

        g.setColour (buttonBorder);
        g.drawRoundedRectangle (box, 2.0f, 1.0f);

        if (ticked)
        {
            g.setFont (getCustomFont (18.0f));
            g.setColour (isEnabled ? textPrimary : textMid);
            g.drawText ("X", box.translated (0, -1), juce::Justification::centred, false);
        }
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool) override
    {
        using namespace PapaloteColors;

        const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isOn = button.getToggleState();
        const bool hover = shouldDrawButtonAsHighlighted;

        g.setColour(isOn ? buttonOn : hover ? buttonOff.brighter(0.06f) : buttonOff);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isOn ? accent : buttonBorder);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        g.setFont(getCustomFont(18.0f));
        g.setColour(isOn ? textPrimary : (hover ? textPrimary : textMid));

        const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                       : ("[ " + button.getButtonText() + " ]");

        g.drawText(text, button.getLocalBounds().translated(0, 0),
                   juce::Justification::centred, true);
    }

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool /*isButtonDown*/, int, int, int, int,
                      juce::ComboBox& comboBox) override
    {
        using namespace PapaloteColors;

        g.setColour(comboBox.isMouseOver(true) ? buttonOn : buttonOff);
        g.fillRoundedRectangle(0, 0, (float)width, (float)height, 2.0f);
        g.setColour(buttonBorder);
        g.drawRoundedRectangle(0.5f, 0.5f,
                               (float)width - 1.0f, (float)height - 1.0f, 2.0f, 1.0f);

        juce::Path p;
        p.addTriangle((float)width - 14.0f, (float)height * 0.5f - 2.0f,
                      (float)width - 6.0f, (float)height * 0.5f - 2.0f,
                      (float)width - 10.0f, (float)height * 0.5f + 3.0f);
        g.setColour(textMid);
        g.fillPath(p);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(6, 0, box.getWidth() - 22, box.getHeight());
        label.setFont(getCustomFont(18.0f));
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, PapaloteColors::textPrimary);
    }

    void drawPopupMenuBackground(juce::Graphics&g, int width, int height) override
    {
        using namespace PapaloteColors;

        g.fillAll(menuBg);

        drawScanlines (g, { 0, 0, width, height }, juce::Colours::black.withAlpha (0.22f));

        g.setColour(menuBorder.withAlpha(0.7f));
        g.drawRect(0, 0, width, height, 1);
        g.setColour(menuInnerBorder);
        g.drawRect(1, 1, width - 2, height - 2, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool, bool,
                           const juce::String& text,
                           const juce::String&,
                           const juce::Drawable*,
                           const juce::Colour*) override
    {
        using namespace PapaloteColors;

        if (isSeparator)
        {
            g.setColour(menuBorder.withAlpha(0.6f));
            g.fillRect(area.getX(), area.getCentreY(), area.getWidth(), 1);
            return;
        }

        if (isHighlighted)
        {
            g.setColour(menuHover);
            g.fillRect(area.getX(), area.getY() + 1, area.getWidth(), area.getHeight() - 1);
            g.setColour(menuTextBright.withAlpha(0.8f));
            g.fillRect(area.getX(), area.getY() + 1, 2, area.getHeight() - 1);
        }

        drawScanlines (g, area, juce::Colours::black.withAlpha (0.22f), 1);

        g.setColour(!isActive       ? menuTextDim
                    : isHighlighted ? menuTextBright
                                    : menuText);
        g.setFont(getPopupMenuFont());
        g.drawText(text, area.reduced(10, 0), juce::Justification::centredLeft, true);
    }

    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert,
                       const juce::Rectangle<int>& textArea,
                       juce::TextLayout& textLayout) override
    {
        using namespace PapaloteColors;
        const auto bounds = alert.getLocalBounds();

        g.setColour (card);
        g.fillRect (bounds);

        drawScanlines (g, bounds, juce::Colours::black.withAlpha (0.28f));

        g.setColour (accent.withAlpha (0.22f));
        g.drawRect (0, 0, bounds.getWidth(), bounds.getHeight(), 1);
        g.setColour (accentDim);
        g.drawRect (1, 1, bounds.getWidth() - 2, bounds.getHeight() - 2, 1);

        const auto padded = textArea.reduced (24, 6);
        if (padded.isEmpty()) return;
        const float layoutWidth = textLayout.getWidth();
        const float layoutHeight = textLayout.getHeight();
        const int drawY = padded.getY();
        const float drawX = (float) bounds.getCentreX() - layoutWidth * 0.5f;
        textLayout.draw (g, juce::Rectangle<float> (drawX, (float) drawY,
                                                    layoutWidth,
                                                    juce::jmin ((float) padded.getHeight(),
                                                                layoutHeight)));
    }

    class PapaloteSliderTextLabel final : public juce::Label
    {
    public:
        PapaloteSliderTextLabel() : juce::Label({}, {}) {}

        void lookAndFeelChanged() override
        {
            juce::Label::lookAndFeelChanged();
            setFont (static_cast<CustomLookAndFeel&> (getLookAndFeel()).getFont (22.0f));
        }

        void editorShown(juce::TextEditor* te) override
        {
            te->setColour(juce::TextEditor::outlineColourId, PapaloteColors::accentDim.withAlpha(0.35f));
            te->setColour(juce::TextEditor::backgroundColourId, PapaloteColors::background.withAlpha(0.6f));
            te->setJustification(juce::Justification::centred);
        }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PapaloteSliderTextLabel)
    };

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        using namespace PapaloteColors;

        auto* l = new PapaloteSliderTextLabel();
        l->setJustificationType(juce::Justification::centred);
        l->setKeyboardType(juce::TextInputTarget::decimalKeyboard);
        l->setFont(getFont(22.0f));

        l->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
        l->setColour(juce::Label::backgroundColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
        l->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
        l->setColour(juce::TextEditor::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
        l->setColour(juce::TextEditor::backgroundColourId, slider.findColour(juce::Slider::textBoxBackgroundColourId));
        l->setColour(juce::TextEditor::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
        l->setColour(juce::TextEditor::highlightColourId, slider.findColour(juce::Slider::textBoxHighlightColourId));
        return l;
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (dynamic_cast<PapaloteSliderTextLabel*> (&label) != nullptr)
            return getFont (22.0f);
        if (label.getFont().getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
            return getFont (22.0f);
        return label.getFont();
    }

    class PapaloteDocumentWindowButton final : public juce::Button
    {
    public:
        PapaloteDocumentWindowButton (const juce::String& name, juce::Colour c,
                                       const juce::Path& normal, const juce::Path& toggled)
            : juce::Button (name), colour (c), normalShape (normal), toggledShape (toggled) {}

        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            g.fillAll (PapaloteColors::background);

            g.setColour ((! isEnabled() || shouldDrawButtonAsDown) ? colour.withAlpha (0.6f) : colour);

            if (shouldDrawButtonAsHighlighted)
            {
                g.fillAll();
                g.setColour (PapaloteColors::background);
            }

            auto& p = getToggleState() ? toggledShape : normalShape;

            auto reducedRect = juce::Justification (juce::Justification::centred)
                                  .appliedToRectangle (juce::Rectangle<int> (getHeight(), getHeight()), getLocalBounds())
                                  .toFloat()
                                  .reduced ((float) getHeight() * 0.3f);

            g.fillPath (p, p.getTransformToScaleToFit (reducedRect, true));
        }

    private:
        juce::Colour colour;
        juce::Path normalShape, toggledShape;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PapaloteDocumentWindowButton)
    };

    juce::Button* createDocumentWindowButton (int buttonType) override
    {
        juce::Path shape;
        const auto crossThickness = 0.15f;

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            shape.addLineSegment ({ 0.0f, 0.0f, 1.0f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 1.0f, 0.0f, 0.0f, 1.0f }, crossThickness);
            return new PapaloteDocumentWindowButton ("close", PapaloteColors::textBrand, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new PapaloteDocumentWindowButton ("minimise", PapaloteColors::textBrand, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            shape.addLineSegment ({ 0.5f, 0.0f, 0.5f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new PapaloteDocumentWindowButton ("maximise", PapaloteColors::textBrand, shape, shape);
        }

        jassertfalse;
        return nullptr;
    }

    void drawDocumentWindowTitleBar (juce::DocumentWindow& window, juce::Graphics& g,
                                     int w, int h, int titleSpaceX, int titleSpaceW,
                                     const juce::Image* icon, bool drawTitleTextOnLeft) override
    {
        if (w * h == 0)
            return;

        g.fillAll (PapaloteColors::background);

        juce::Font font = makeFont ((float) h * 0.65f);
        g.setFont (font);

        auto textW = juce::GlyphArrangement::getStringWidthInt (font, window.getName());
        auto iconW = 0;
        auto iconH = 0;

        if (icon != nullptr)
        {
            iconH = static_cast<int> (font.getHeight());
            iconW = icon->getWidth() * iconH / icon->getHeight() + 4;
        }

        textW = juce::jmin (titleSpaceW, textW + iconW);
        auto textX = drawTitleTextOnLeft ? titleSpaceX
                                         : juce::jmax (titleSpaceX, (w - textW) / 2);

        if (textX + textW > titleSpaceX + titleSpaceW)
            textX = titleSpaceX + titleSpaceW - textW;

        if (icon != nullptr)
        {
            g.setOpacity (window.isActiveWindow() ? 1.0f : 0.6f);
            g.drawImageWithin (*icon, textX, (h - iconH) / 2, iconW, iconH,
                               juce::RectanglePlacement::centred, false);
            textX += iconW;
            textW -= iconW;
        }

        g.setColour (PapaloteColors::textBrand);
        g.drawText (window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
    }

private:
    juce::Typeface::Ptr vt323{nullptr};
};
