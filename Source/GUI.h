#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "PluginProcessor.h"
#include "../../AppLibrary/Source/Library/Branding.h"
#include <array>
#include <vector>

struct HE2Style
{
    const juce::Colour bgColor      = juce::Colour::fromRGB(10, 12, 15);

    const juce::Colour faceTop      = juce::Colour::fromRGB(216, 219, 224);
    const juce::Colour faceMid      = juce::Colour::fromRGB(190, 194, 201);
    const juce::Colour faceBottom   = juce::Colour::fromRGB(163, 168, 177);

    const juce::Colour shellEdge    = juce::Colour::fromRGB(116, 125, 136);
    const juce::Colour shellShadow  = juce::Colour::fromRGBA(0, 0, 0, 70);

    const juce::Colour panelDark    = juce::Colour::fromRGB(21, 26, 33);
    const juce::Colour panelDark2   = juce::Colour::fromRGB(13, 17, 23);
    const juce::Colour panelStroke  = juce::Colour::fromRGBA(255, 255, 255, 28);

    const juce::Colour textMain     = juce::Colour::fromRGB(56, 63, 73);
    const juce::Colour textSoft     = juce::Colour::fromRGB(88, 97, 109);
    const juce::Colour textMuted    = juce::Colour::fromRGB(118, 127, 139);
    const juce::Colour displayText  = juce::Colour::fromRGB(246, 251, 255);

    const juce::Colour warmth       = juce::Colour::fromRGB(255, 138, 64);
    const juce::Colour presence     = juce::Colour::fromRGB(255, 187, 64);
    const juce::Colour edge         = juce::Colour::fromRGB(70, 231, 176);
    const juce::Colour air          = juce::Colour::fromRGB(88, 159, 255);
    const juce::Colour amount       = juce::Colour::fromRGB(181, 130, 255);
    const juce::Colour mix          = juce::Colour::fromRGB(116, 191, 255);
    const juce::Colour output       = juce::Colour::fromRGB(204, 214, 224);
};

class HE2GUI : public juce::Component,
               private juce::Timer
{
public:
    enum class DisplayQualityMode
    {
        autoMode = 0,
        low,
        medium,
        high
    };

    explicit HE2GUI(HE2AudioProcessor& p);
    ~HE2GUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;


    void setDisplayQualityMode(DisplayQualityMode newMode);
    DisplayQualityMode getDisplayQualityMode() const noexcept { return displayQualityMode; }

    bool isUiPowerOn() const noexcept { return uiPowerOn; }
    const HE2Style& getStyle() const noexcept { return style; }

    // Common scale for fonts, strokes, radii and component spacing. Keeping
    // a single reference aspect ensures square buttons/knobs stay square.
    float getUiScale() const noexcept
    {
        return juce::jmin((float) getWidth() / 900.0f,
                          (float) getHeight() / 700.0f);
    }
    float uiPx(float referencePixels) const noexcept { return referencePixels * getUiScale(); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    // Power uses a single explicit onClick writer (no competing ButtonAttachment).

    class HE2LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit HE2LookAndFeel(HE2GUI& ownerToUse) : owner(ownerToUse) {}

        void drawRotarySlider(juce::Graphics& g,
                              int x, int y, int width, int height,
                              float sliderPosProportional,
                              float rotaryStartAngle,
                              float rotaryEndAngle,
                              juce::Slider& slider) override;

        juce::Font getLabelFont(juce::Label& label) override;

    private:
        HE2GUI& owner;
    };

    class ModeButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit ModeButtonLookAndFeel(HE2GUI& ownerToUse) : owner(ownerToUse) {}

        void drawToggleButton(juce::Graphics& g,
                              juce::ToggleButton& button,
                              bool isMouseOverButton,
                              bool isButtonDown) override;

    private:
        HE2GUI& owner;
    };

    class IconButton : public juce::Button
    {
    public:
        explicit IconButton(const juce::String& name);
        ~IconButton() override = default;

        void setDrawAsPowerButton(bool shouldDrawAsPower)
        {
            drawAsPowerButton = shouldDrawAsPower;
            repaint();
        }

        void setPowerOn(bool shouldBeOn)
        {
            if (powerOn != shouldBeOn)
            {
                powerOn = shouldBeOn;
                repaint();
            }
        }

        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        bool drawAsPowerButton = false;
        bool powerOn = true;
    };

    struct ControlVisual
    {
        juce::Slider* slider = nullptr;
        juce::Label* nameLabel = nullptr;
        juce::Label* valueLabel = nullptr;
        juce::Colour accent;
        int iconType = 0;

        juce::Rectangle<float> columnBounds;
        juce::Rectangle<float> windowBounds;
        juce::Rectangle<float> knobBounds;
        juce::Rectangle<float> nameBounds;
        juce::Rectangle<float> valueBounds;
    };

    void timerCallback() override;

    void configureSlider(juce::Slider& slider, const juce::String& name);
    void configureNameLabel(juce::Label& label, const juce::String& text);
    void configureValueLabel(juce::Label& label);
    void bindValueLabelToSlider(juce::Label& label, juce::Slider& slider);
    void configureModeButton(juce::ToggleButton& button, const juce::String& text);
    void updateScaledFonts();

    void updateValueLabels();
    bool updateSingleValueLabel(juce::Label& label, const juce::String& newText);
    juce::String formatSliderValue(const juce::Slider& slider) const;
    bool applyTypedValueToSlider(const juce::String& text, juce::Slider& slider);

    void drawOuterShell(juce::Graphics& g);
    void drawHeader(juce::Graphics& g);
    struct TopPanelLayout
    {
        juce::Rectangle<float> left;
        juce::Rectangle<float> displayArea; // Passed to drawBrandPanel, which applies its own inset.
        juce::Rectangle<float> right;
    };
    TopPanelLayout getTopPanelLayout(juce::Rectangle<float> area) const noexcept;
    void drawTopSection(juce::Graphics& g, juce::Rectangle<float> area);
    void drawBrandPanel(juce::Graphics& g, juce::Rectangle<float> area);
    void drawVersionEtching(juce::Graphics& g);
    void drawFooterGlow(juce::Graphics& g, juce::Rectangle<float> area);

    void layoutControls(juce::Rectangle<float> area);
    void drawControlWindow(juce::Graphics& g, const ControlVisual& cv);
    void drawControlIcon(juce::Graphics& g, juce::Rectangle<float> area, int iconType, juce::Colour accent);
    void drawWindowIndicators(juce::Graphics& g, juce::Rectangle<float> area, float norm, juce::Colour accent);

    HE2AudioProcessor& processor;
    HE2Style style;
    HE2LookAndFeel knobLookAndFeel;
    ModeButtonLookAndFeel modeButtonLookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::Slider warmthSlider, presenceSlider, edgeSlider, airSlider, amountSlider, mixSlider, outputSlider;
    juce::Label warmthLabel, presenceLabel, edgeLabel, airLabel, amountLabel, mixLabel, outputLabel;
    juce::Label warmthValueLabel, presenceValueLabel, edgeValueLabel, airValueLabel, amountValueLabel, mixValueLabel, outputValueLabel;

    juce::ToggleButton classicButton, modernButton;
    std::array<juce::ToggleButton, 4> oversamplingButtons;
    juce::Label modeLabel, oversamplingLabel;
    IconButton powerButton { "Power" };
    std::unique_ptr<juce::Drawable> etchedLogo; // Cached SVG, parsed once.
    std::array<float, 4> visualBandActivity { 0.0f, 0.0f, 0.0f, 0.0f };

    std::unique_ptr<SliderAttachment> warmthAttachment;
    std::unique_ptr<SliderAttachment> presenceAttachment;
    std::unique_ptr<SliderAttachment> edgeAttachment;
    std::unique_ptr<SliderAttachment> airAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    // Power state is read via the GUI timer, not another parameter writer.

    std::array<ControlVisual, 7> controlVisuals {};

    DisplayQualityMode displayQualityMode = DisplayQualityMode::high;
    bool uiPowerOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HE2GUI)
};

// -----------------------------------------------------------------------------
// Inline GUI implementation
// -----------------------------------------------------------------------------

static double parseLooseNumber(const juce::String& text)
{
    auto cleaned = text.toLowerCase().retainCharacters("0123456789.-");
    return cleaned.getDoubleValue();
}

static juce::Colour getAccentForSliderName(const juce::String& name, const HE2Style& style)
{
    const auto n = name.toLowerCase();

    if (n == "warmth")   return style.warmth;
    if (n == "presence") return style.presence;
    if (n == "edge")     return style.edge;
    if (n == "air")      return style.air;
    if (n == "amount")   return style.amount;
    if (n == "mix")      return style.mix;
    if (n == "output")   return style.output;

    return juce::Colours::white;
}

static float relativeIconPx(juce::Rectangle<float> area, float referencePixels)
{
    return referencePixels * juce::jmin(area.getWidth() / 55.0f,
                                       area.getHeight() / 42.0f);
}

static void fillInnerShadow(juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.drawRoundedRectangle(area.reduced(radius * 0.04f), radius, radius * 0.08f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawRoundedRectangle(area.reduced(radius * 0.09f), radius * 0.95f, radius * 0.05f);
}

static void drawSoftGlow(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, float alphaScale)
{
    auto expanded = area.expanded(area.getWidth() * 0.20f, area.getHeight() * 0.20f);
    juce::ColourGradient grad(c.withAlpha(alphaScale), expanded.getCentreX(), expanded.getCentreY(),
                              c.withAlpha(0.0f), expanded.getRight(), expanded.getBottom(), true);
    g.setGradientFill(grad);
    g.fillEllipse(expanded);
}

// Layered radial bloom, rather than a hard-edged translucent ellipse.
// All measurements are proportional to the dot diameter and therefore resize
// with the component.  Avoid drawing outside the surrounding control area.
static void drawIlluminatedDot(juce::Graphics& g,
                               juce::Rectangle<float> dot,
                               juce::Colour accent,
                               float intensity = 1.0f)
{
    const float d = juce::jmin(dot.getWidth(), dot.getHeight());
    const float cx = dot.getCentreX();
    const float cy = dot.getCentreY();
    const auto core = juce::Rectangle<float>(cx - d * 0.5f, cy - d * 0.5f, d, d);

    auto drawHalo = [&] (float radiusFactor, float alpha)
    {
        auto halo = core.expanded(d * radiusFactor);
        juce::ColourGradient glow(accent.withAlpha(alpha * intensity), cx, cy,
                                  accent.withAlpha(0.0f), halo.getRight(), cy, true);
        g.setGradientFill(glow);
        g.fillEllipse(halo);
    };

    drawHalo(1.30f, 0.25f);
    drawHalo(0.68f, 0.35f);

    g.setColour(accent.withAlpha(0.93f * intensity));
    g.fillEllipse(core);

    // Small centre reflection makes it read as a lit lens, not a flat dot.
    const auto hotSpot = core.withSizeKeepingCentre(d * 0.47f, d * 0.47f);
    g.setColour(juce::Colours::white.withAlpha(0.38f * intensity));
    g.fillEllipse(hotSpot.translated(-d * 0.08f, -d * 0.10f));
}

static void drawWarmthIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    auto r = area;

    const float x = r.getX();
    const float y = r.getY();
    const float w = r.getWidth();
    const float h = r.getHeight();

    auto bulb = juce::Rectangle<float>(x + w * 0.34f,
                                       y + h * 0.02f,
                                       w * 0.37f,
                                       h * 0.85f);

    auto base = juce::Rectangle<float>(x + w * 0.34f,
                                       y + h * 0.85f,
                                       w * 0.37f,
                                       h * 0.10f);

    auto pin1 = juce::Rectangle<float>(x + w * 0.40f,
                                       y + h * 0.95f,
                                       w * 0.03f,
                                       h * 0.10f);

    auto pin2 = juce::Rectangle<float>(x + w * 0.59f,
                                       y + h * 0.95f,
                                       w * 0.03f,
                                       h * 0.10f);

    juce::Path bulbPath;
    bulbPath.addRoundedRectangle(bulb, bulb.getWidth() * 0.18f);

    juce::Path filament;
    filament.startNewSubPath(x + w * 0.40f, y + h * 0.46f);
    filament.lineTo(x + w * 0.46f, y + h * 0.38f);
    filament.lineTo(x + w * 0.52f, y + h * 0.46f);
    filament.lineTo(x + w * 0.58f, y + h * 0.38f);
    filament.lineTo(x + w * 0.64f, y + h * 0.46f);

    juce::Path support;
    support.startNewSubPath(x + w * 0.52f, y + h * 0.24f);
    support.lineTo(x + w * 0.52f, y + h * 0.36f);

    g.setColour(colour.withAlpha(0.18f));
    g.strokePath(bulbPath, juce::PathStrokeType(relativeIconPx(area, 6.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(filament, juce::PathStrokeType(relativeIconPx(area, 4.8f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRoundedRectangle(base.expanded(relativeIconPx(area, 1.5f)), base.getHeight() * 0.35f);

    g.setColour(colour.withAlpha(0.25f));
    g.drawLine(bulb.getX() + bulb.getWidth() * 0.24f,
               bulb.getY() + bulb.getHeight() * 0.16f,
               bulb.getX() + bulb.getWidth() * 0.24f,
               bulb.getBottom() - bulb.getHeight() * 0.22f,
               relativeIconPx(area, 1.0f));

    g.setColour(colour.withAlpha(0.55f));
    g.strokePath(support, juce::PathStrokeType(relativeIconPx(area, 1.4f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(colour);
    g.strokePath(bulbPath, juce::PathStrokeType(relativeIconPx(area, 2.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRoundedRectangle(base, base.getHeight() * 0.35f);
    g.fillRoundedRectangle(pin1, pin1.getWidth() * 0.4f);
    g.fillRoundedRectangle(pin2, pin2.getWidth() * 0.4f);

    g.setColour(colour.brighter(0.30f));
    g.strokePath(filament, juce::PathStrokeType(relativeIconPx(area, 2.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void drawPresenceIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float x = area.getX();
    const float y = area.getBottom() - area.getHeight() * 0.20f;
    const float w = area.getWidth();
    const float h = area.getHeight();

    p.startNewSubPath(x + w * 0.10f, y);
    p.lineTo(x + w * 0.28f, y - h * 0.16f);
    p.lineTo(x + w * 0.44f, y - h * 0.02f);
    p.lineTo(x + w * 0.58f, y - h * 0.38f);
    p.lineTo(x + w * 0.74f, y - h * 0.14f);
    p.lineTo(x + w * 0.90f, y - h * 0.48f);

    g.setColour(colour.withAlpha(0.22f));
    g.strokePath(p, juce::PathStrokeType(relativeIconPx(area, 7.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(relativeIconPx(area, 2.3f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void drawEdgeIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    // Keep the bolt vertical and proportional, but a touch fuller than the
    // previous slim silhouette. Aspect ratio is still independent of GUI size.
    const float boltHeight = area.getHeight() * 0.96f;
    const float boltWidth = juce::jmin(area.getWidth() * 0.62f, boltHeight * 0.53f);
    const float x = area.getCentreX() - boltWidth * 0.5f;
    const float y = area.getCentreY() - boltHeight * 0.5f;

    juce::Path bolt;
    bolt.startNewSubPath(x + boltWidth * 0.38f, y + boltHeight * 0.02f);
    bolt.lineTo(x + boltWidth * 0.86f, y + boltHeight * 0.02f);
    bolt.lineTo(x + boltWidth * 0.56f, y + boltHeight * 0.43f);
    bolt.lineTo(x + boltWidth * 0.92f, y + boltHeight * 0.43f);
    bolt.lineTo(x + boltWidth * 0.12f, y + boltHeight * 0.98f);
    bolt.lineTo(x + boltWidth * 0.39f, y + boltHeight * 0.56f);
    bolt.lineTo(x + boltWidth * 0.08f, y + boltHeight * 0.56f);
    bolt.closeSubPath();

    g.setColour(colour.withAlpha(0.14f));
    g.fillPath(bolt, juce::AffineTransform::translation(0.0f, relativeIconPx(area, 2.0f)));
    g.setColour(colour);
    g.fillPath(bolt);
}

static void drawAirIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p1, p2;
    const float x = area.getX();
    const float y = area.getBottom();
    const float w = area.getWidth();
    const float h = area.getHeight();

    const float p1OffsetX = area.getWidth() * 0.2f;
    const float p1OffsetY = area.getHeight() * -0.25f;
    const float p2OffsetY = 0.0f;

    p1.startNewSubPath(x + w * 0.12f + p1OffsetX, y - h * 0.22f + p1OffsetY);
    p1.cubicTo(x + w * 0.26f + p1OffsetX, y - h * 0.58f + p1OffsetY,
               x + w * 0.40f + p1OffsetX, y - h * 0.02f + p1OffsetY,
               x + w * 0.54f + p1OffsetX, y - h * 0.36f + p1OffsetY);

    p2.startNewSubPath(x + w * 0.12f + p1OffsetX, y - h * 0.22f + p2OffsetY);
    p2.cubicTo(x + w * 0.26f + p1OffsetX, y - h * 0.58f + p2OffsetY,
               x + w * 0.40f + p1OffsetX, y - h * 0.02f + p2OffsetY,
               x + w * 0.54f + p1OffsetX, y - h * 0.36f + p2OffsetY);

    g.setColour(colour.withAlpha(0.22f));
    g.strokePath(p1, juce::PathStrokeType(relativeIconPx(area, 6.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(p2, juce::PathStrokeType(relativeIconPx(area, 6.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(colour);
    g.strokePath(p1, juce::PathStrokeType(relativeIconPx(area, 2.1f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(p2, juce::PathStrokeType(relativeIconPx(area, 2.1f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void drawAmountIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    auto bar1 = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.18f, area.getBottom() - area.getHeight() * 0.24f,
                                       area.getWidth() * 0.10f, area.getHeight() * 0.20f);
    auto bar2 = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.36f, area.getBottom() - area.getHeight() * 0.44f,
                                       area.getWidth() * 0.10f, area.getHeight() * 0.40f);
    auto bar3 = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.54f, area.getBottom() - area.getHeight() * 0.66f,
                                       area.getWidth() * 0.10f, area.getHeight() * 0.62f);
    auto bar4 = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.72f, area.getBottom() - area.getHeight() * 0.84f,
                                       area.getWidth() * 0.10f, area.getHeight() * 0.80f);

    for (auto r : { bar1, bar2, bar3, bar4 })
    {
        g.setColour(colour.withAlpha(0.18f));
        g.fillRoundedRectangle(r.expanded(relativeIconPx(area, 2.0f)), relativeIconPx(area, 3.0f));
        g.setColour(colour);
        g.fillRoundedRectangle(r, relativeIconPx(area, 2.2f));
    }
}

static void drawMixIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    auto a = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.20f, area.getY() + area.getHeight() * 0.24f,
                                    area.getWidth() * 0.30f, area.getWidth() * 0.30f);
    auto b = juce::Rectangle<float>(area.getX() + area.getWidth() * 0.48f, area.getY() + area.getHeight() * 0.24f,
                                    area.getWidth() * 0.30f, area.getWidth() * 0.30f);

    g.setColour(colour.withAlpha(0.16f));
    g.fillEllipse(a.expanded(relativeIconPx(area, 2.0f)));
    g.fillEllipse(b.expanded(relativeIconPx(area, 2.0f)));

    g.setColour(colour);
    g.drawEllipse(a, relativeIconPx(area, 2.0f));
    g.drawEllipse(b, relativeIconPx(area, 2.0f));
}

static void drawOutputIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float x = area.getX();
    const float y = area.getCentreY();
    const float w = area.getWidth();
    const float h = area.getHeight();

    p.startNewSubPath(x + w * 0.12f, y);
    p.lineTo(x + w * 0.72f, y);
    p.lineTo(x + w * 0.58f, y - h * 0.18f);
    p.startNewSubPath(x + w * 0.72f, y);
    p.lineTo(x + w * 0.58f, y + h * 0.18f);

    g.setColour(colour.withAlpha(0.20f));
    g.strokePath(p, juce::PathStrokeType(relativeIconPx(area, 6.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(relativeIconPx(area, 2.2f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline HE2GUI::IconButton::IconButton(const juce::String& name)
    : juce::Button(name)
{
}

inline juce::Font HE2GUI::HE2LookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(owner.uiPx(14.0f));
}

inline void HE2GUI::IconButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const float d = (float) juce::jmin(getWidth(), getHeight());
    auto circle = getLocalBounds().toFloat().withSizeKeepingCentre(d * 0.94f, d * 0.94f);
    const auto centre = circle.getCentre();

    // A recessed, circular housing with a sloped silver/plastic lip.
    g.setColour(juce::Colours::black.withAlpha(0.17f));
    g.fillEllipse(circle.translated(0.0f, d * 0.025f));

    juce::ColourGradient lip(juce::Colour::fromRGB(252, 253, 255), circle.getX(), circle.getY(),
                              juce::Colour::fromRGB(133, 143, 156), circle.getRight(), circle.getBottom(), false);
    g.setGradientFill(lip);
    g.fillEllipse(circle);
    g.setColour(juce::Colour::fromRGB(94, 104, 116));
    g.drawEllipse(circle, d * 0.014f);
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.drawEllipse(circle.reduced(d * 0.024f), d * 0.010f);

    auto recess = circle.reduced(d * 0.110f);
    juce::ColourGradient socket(juce::Colour::fromRGB(15, 19, 25), recess.getX(), recess.getY(),
                                 juce::Colour::fromRGB(57, 66, 77), recess.getRight(), recess.getBottom(), false);
    g.setGradientFill(socket);
    g.fillEllipse(recess);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawEllipse(recess, d * 0.014f);

    auto cap = recess.reduced(d * 0.045f);
    auto capTop = juce::Colour::fromRGB(57, 66, 79);
    if (isMouseOverButton) capTop = capTop.brighter(0.09f);
    if (isButtonDown) capTop = capTop.darker(0.13f);
    juce::ColourGradient capGradient(capTop, cap.getCentreX(), cap.getY(),
                                      juce::Colour::fromRGB(23, 29, 37), cap.getCentreX(), cap.getBottom(), false);
    g.setGradientFill(capGradient);
    g.fillEllipse(cap);
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.drawEllipse(cap.reduced(d * 0.014f), d * 0.010f);

    if (! drawAsPowerButton)
        return;

    const float radius = d * 0.205f;
    const float cx = centre.x, cy = centre.y + d * 0.012f;
    juce::Path ring;
    ring.addCentredArc(cx, cy, radius, radius, 0.0f,
                        juce::MathConstants<float>::pi * 0.22f,
                        juce::MathConstants<float>::pi * 1.78f, true);
    juce::Path stem;
    stem.startNewSubPath(cx, cy - radius * 1.17f);
    stem.lineTo(cx, cy - radius * 0.16f);

    const auto colour = powerOn ? juce::Colour::fromRGB(62, 196, 255)
                                : juce::Colour::fromRGB(118, 127, 136);
    if (powerOn)
    {
        // Faint ambient light on the *socket*, not a bulky blob around glyph.
        auto halo = cap.reduced(d * 0.04f);
        juce::ColourGradient spill(juce::Colour::fromRGBA(59, 192, 255, 28), cx, cy,
                                    juce::Colour::fromRGBA(59, 192, 255, 0), halo.getRight(), cy, true);
        g.setGradientFill(spill);
        g.fillEllipse(halo);
        g.setColour(colour.withAlpha(0.30f));
        const juce::PathStrokeType soft(d * 0.070f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
        g.strokePath(ring, soft);
        g.strokePath(stem, soft);
    }

    const juce::PathStrokeType mainStroke(d * 0.036f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);
    g.setColour(colour);
    g.strokePath(ring, mainStroke);
    g.strokePath(stem, mainStroke);
    if (powerOn)
    {
        g.setColour(juce::Colour::fromRGB(230, 247, 255).withAlpha(0.85f));
        const juce::PathStrokeType core(d * 0.010f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
        g.strokePath(ring, core);
        g.strokePath(stem, core);
    }
}

inline void HE2GUI::HE2LookAndFeel::drawRotarySlider(juce::Graphics& g,
                                              int x, int y, int width, int height,
                                              float sliderPosProportional,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const auto& style = owner.getStyle();

    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto knob = juce::Rectangle<float>(bounds.getCentreX() - diameter * 0.5f,
                                       bounds.getCentreY() - diameter * 0.5f,
                                       diameter, diameter);

    auto outer = knob.reduced(diameter * 0.05f);
    auto inner = knob.reduced(diameter * 0.18f);
    auto arcBounds = outer.reduced(diameter * 0.06f);

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = getAccentForSliderName(slider.getName(), style);

    juce::ColourGradient shellGrad(juce::Colour::fromRGB(244, 247, 250),
                                   outer.getX(), outer.getY(),
                                   juce::Colour::fromRGB(158, 164, 174),
                                   outer.getRight(), outer.getBottom(), false);
    g.setGradientFill(shellGrad);
    g.fillEllipse(outer);

    g.setColour(juce::Colour::fromRGB(100, 108, 118));
    g.drawEllipse(outer, diameter * 0.010f);

    // Etched precision index: five main marks at 0/25/50/75/100, and three
    // finer divisions between each. The ticks are drawn later, above the arc,
    // so they stay readable rather than becoming part of the colored glow.
    constexpr int tickCount = 17;
    const float tickCenterX = outer.getCentreX();
    const float tickCenterY = outer.getCentreY();

    juce::ColourGradient capGrad(juce::Colour::fromRGB(60, 67, 77),
                                 inner.getX(), inner.getY(),
                                 juce::Colour::fromRGB(20, 24, 30),
                                 inner.getRight(), inner.getBottom(), false);
    g.setGradientFill(capGrad);
    g.fillEllipse(inner);

    auto gloss = inner.withTrimmedBottom(inner.getHeight() * 0.40f).reduced(inner.getWidth() * 0.14f, inner.getHeight() * 0.10f);
    juce::ColourGradient glossFill(juce::Colours::white.withAlpha(0.12f),
                                   gloss.getCentreX(), gloss.getY(),
                                   juce::Colours::white.withAlpha(0.0f),
                                   gloss.getCentreX(), gloss.getBottom(), false);
    g.setGradientFill(glossFill);
    g.fillEllipse(gloss);

    juce::Path bgArc, valueArc;
    bgArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                        arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                        0.0f, rotaryStartAngle, rotaryEndAngle, true);

    valueArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                           arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                           0.0f, rotaryStartAngle, angle, true);

    g.setColour(juce::Colour::fromRGB(157, 165, 175));
    g.strokePath(bgArc, juce::PathStrokeType(diameter * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (owner.isUiPowerOn())
    {
        g.setColour(accent.withAlpha(0.17f));
        g.strokePath(valueArc, juce::PathStrokeType(diameter * 0.10f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(accent);
        g.strokePath(valueArc, juce::PathStrokeType(diameter * 0.031f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // With power off, show only the physical dial, pointer, and engraved
    // index marks; the value arc is an electronic indicator and goes dark.

    // Keep the reference marks neutral rather than colorizing them with the active arc.
    // Drawing them after the glow also keeps them readable against the illumination.
    for (int i = 0; i < tickCount; ++i)
    {
        const float t = (float) i / (float) (tickCount - 1);
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const bool majorTick = (i % 4 == 0 || i == tickCount - 1);
        const float fromRadius = diameter * (majorTick ? 0.390f : 0.409f);
        const float toRadius = diameter * 0.452f;
        const float sx = std::sin(tickAngle);
        const float sy = -std::cos(tickAngle);
        g.setColour(juce::Colour::fromRGB(86, 95, 106)
                        .withAlpha(majorTick ? 0.92f : 0.70f));
        g.drawLine(tickCenterX + sx * fromRadius, tickCenterY + sy * fromRadius,
                   tickCenterX + sx * toRadius, tickCenterY + sy * toRadius,
                   diameter * (majorTick ? 0.0130f : 0.0095f));
    }

    juce::Path pointer;
    const float pointerLength = diameter * 0.28f;
    const float pointerThickness = diameter * 0.028f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength,
                                pointerThickness * 0.5f);

    g.setColour(juce::Colours::white.withAlpha(0.96f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(outer.getCentreX(), outer.getCentreY()));

    const float capSize = diameter * 0.08f;
    g.setColour(juce::Colour::fromRGB(10, 13, 17));
    g.fillEllipse(outer.getCentreX() - capSize * 0.5f,
                  outer.getCentreY() - capSize * 0.5f,
                  capSize, capSize);
}

inline void HE2GUI::ModeButtonLookAndFeel::drawToggleButton(juce::Graphics& g,
                                                     juce::ToggleButton& button,
                                                     bool isMouseOverButton,
                                                     bool isButtonDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced(button.getHeight() * 0.016f);
    const bool selected = button.getToggleState();
    const float round = r.getHeight() * 0.19f;
    const float unit = r.getHeight();

    g.setColour(juce::Colours::black.withAlpha(selected ? 0.26f : 0.16f));
    g.fillRoundedRectangle(r.translated(0.0f, unit * 0.060f), round);

    auto upper = selected ? juce::Colour::fromRGB(76, 85, 97)
                          : juce::Colour::fromRGB(238, 242, 247);
    auto lower = selected ? juce::Colour::fromRGB(34, 41, 50)
                          : juce::Colour::fromRGB(192, 198, 208);
    if (isMouseOverButton)
    {
        upper = upper.brighter(0.06f);
        lower = lower.brighter(selected ? 0.02f : 0.00f);
    }
    if (isButtonDown)
    {
        upper = upper.darker(0.10f);
        lower = lower.darker(0.06f);
    }

    juce::ColourGradient fill(upper, r.getCentreX(), r.getY(),
                              lower, r.getCentreX(), r.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(r, round);

    g.setColour(juce::Colours::white.withAlpha(selected ? 0.12f : 0.46f));
    g.drawRoundedRectangle(r.reduced(unit * 0.012f), round * 0.90f, unit * 0.015f);
    g.setColour(selected ? juce::Colour::fromRGB(94, 108, 122)
                         : juce::Colour::fromRGB(141, 151, 164));
    g.drawRoundedRectangle(r, round, unit * 0.018f);

    // Thin top bevel and a slightly inset lower line give the buttons some of the
    // same machined depth as the power button without making them circular.
    g.setColour(juce::Colours::white.withAlpha(selected ? 0.10f : 0.35f));
    g.drawLine(r.getX() + unit * 0.16f, r.getY() + unit * 0.080f,
               r.getRight() - unit * 0.16f, r.getY() + unit * 0.080f,
               unit * 0.016f);
    g.setColour(juce::Colours::black.withAlpha(selected ? 0.14f : 0.09f));
    g.drawLine(r.getX() + unit * 0.18f, r.getBottom() - unit * 0.11f,
               r.getRight() - unit * 0.18f, r.getBottom() - unit * 0.11f,
               unit * 0.014f);

    // The physical button remains pressed when off; its blue status light does not.
    if (selected && owner.isUiPowerOn())
    {
        const float stripW = r.getWidth() * 0.28f;
        const float stripH = unit * 0.034f;
        const float stripY = r.getBottom() - unit * 0.17f;
        g.setColour(juce::Colour::fromRGB(164, 216, 244).withAlpha(0.58f));
        g.fillRoundedRectangle(r.getCentreX() - stripW * 0.5f,
                               stripY,
                               stripW, stripH, stripH * 0.5f);
    }

    g.setColour(selected ? juce::Colour::fromRGB(246, 249, 252)
                         : juce::Colour::fromRGB(57, 65, 76));
    // Compact 1x-8x buttons benefit from larger numerals. The larger mode
    // buttons keep their existing type scale and selected-state indicator.
    const bool smallQualityButton = button.getButtonText().endsWithIgnoreCase("x");
    const float fontProportion = smallQualityButton ? 0.45f : 0.34f;
    g.setFont(juce::Font(button.getHeight() * fontProportion, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().translated(0, (int) std::round(-unit * 0.01f)),
                     juce::Justification::centred, 1);
}

inline HE2GUI::HE2GUI(HE2AudioProcessor& p)
    : processor(p),
      knobLookAndFeel(*this),
      modeButtonLookAndFeel(*this)
{
    // Parse the official Parish Audio SVG once. Its original dark navy fill
    // disappears against the black display, so make a brighter, cool-silver
    // copy for this display only (without changing Branding.h elsewhere).
    const auto displayLogoSVG = juce::String(brandLogoSVGText)
                                    .replace("#27374D", "#A4B8C9", true);
    if (auto xml = juce::XmlDocument::parse(displayLogoSVG))
        etchedLogo = juce::Drawable::createFromSVG(*xml);

    titleLabel.setText("Parish Audio", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, style.displayText);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("HE2 Harmonic Enhancer", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGBA(222, 230, 241, 190));
    addAndMakeVisible(subtitleLabel);

    configureSlider(warmthSlider, "Warmth");
    configureSlider(presenceSlider, "Presence");
    configureSlider(edgeSlider, "Edge");
    configureSlider(airSlider, "Air");
    configureSlider(amountSlider, "Amount");
    configureSlider(mixSlider, "Mix");
    configureSlider(outputSlider, "Output");

    configureNameLabel(warmthLabel, "WARMTH");
    configureNameLabel(presenceLabel, "PRESENCE");
    configureNameLabel(edgeLabel, "EDGE");
    configureNameLabel(airLabel, "AIR");
    configureNameLabel(amountLabel, "AMOUNT");
    configureNameLabel(mixLabel, "MIX");
    configureNameLabel(outputLabel, "OUTPUT");

    configureValueLabel(warmthValueLabel);
    configureValueLabel(presenceValueLabel);
    configureValueLabel(edgeValueLabel);
    configureValueLabel(airValueLabel);
    configureValueLabel(amountValueLabel);
    configureValueLabel(mixValueLabel);
    configureValueLabel(outputValueLabel);

    bindValueLabelToSlider(warmthValueLabel, warmthSlider);
    bindValueLabelToSlider(presenceValueLabel, presenceSlider);
    bindValueLabelToSlider(edgeValueLabel, edgeSlider);
    bindValueLabelToSlider(airValueLabel, airSlider);
    bindValueLabelToSlider(amountValueLabel, amountSlider);
    bindValueLabelToSlider(mixValueLabel, mixSlider);
    bindValueLabelToSlider(outputValueLabel, outputSlider);

    configureModeButton(classicButton, "SMOOTH");
    configureModeButton(modernButton, "FOCUSED");
    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setColour(juce::Label::textColourId, style.textSoft);
    modeLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(modeLabel);
    constexpr const char* osNames[] = { "1x", "2x", "4x", "8x" };
    for (size_t i = 0; i < oversamplingButtons.size(); ++i)
    {
        configureModeButton(oversamplingButtons[i], osNames[i]);
        addAndMakeVisible(oversamplingButtons[i]);
    }
    oversamplingLabel.setText("OVERSAMPLING", juce::dontSendNotification);
    oversamplingLabel.setJustificationType(juce::Justification::centred);
    oversamplingLabel.setColour(juce::Label::textColourId, style.textSoft);
    oversamplingLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(oversamplingLabel);

    addAndMakeVisible(warmthSlider);
    addAndMakeVisible(presenceSlider);
    addAndMakeVisible(edgeSlider);
    addAndMakeVisible(airSlider);
    addAndMakeVisible(amountSlider);
    addAndMakeVisible(mixSlider);
    addAndMakeVisible(outputSlider);

    addAndMakeVisible(warmthLabel);
    addAndMakeVisible(presenceLabel);
    addAndMakeVisible(edgeLabel);
    addAndMakeVisible(airLabel);
    addAndMakeVisible(amountLabel);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(outputLabel);

    addAndMakeVisible(warmthValueLabel);
    addAndMakeVisible(presenceValueLabel);
    addAndMakeVisible(edgeValueLabel);
    addAndMakeVisible(airValueLabel);
    addAndMakeVisible(amountValueLabel);
    addAndMakeVisible(mixValueLabel);
    addAndMakeVisible(outputValueLabel);

    addAndMakeVisible(classicButton);
    addAndMakeVisible(modernButton);

    powerButton.setDrawAsPowerButton(true);
    addAndMakeVisible(powerButton);

    warmthAttachment   = std::make_unique<SliderAttachment>(processor.apvts, "warmth", warmthSlider);
    presenceAttachment = std::make_unique<SliderAttachment>(processor.apvts, "presence", presenceSlider);
    edgeAttachment     = std::make_unique<SliderAttachment>(processor.apvts, "edge", edgeSlider);
    airAttachment      = std::make_unique<SliderAttachment>(processor.apvts, "air", airSlider);
    amountAttachment   = std::make_unique<SliderAttachment>(processor.apvts, "amount", amountSlider);
    mixAttachment      = std::make_unique<SliderAttachment>(processor.apvts, "mix", mixSlider);
    outputAttachment   = std::make_unique<SliderAttachment>(processor.apvts, "output", outputSlider);
    powerButton.setClickingTogglesState(false);

    auto setChoiceUndoable = [this] (const juce::String& paramID,
                                      float value,
                                      const juce::String& actionName)
    {
        if (std::abs(processor.getParamValue(paramID) - value) <= 1.0e-6f)
            return;

        processor.beginUndoTransaction(actionName);
        processor.setParamValue(paramID, value);
        processor.endUndoTransaction();
    };

    classicButton.onClick = [setChoiceUndoable]
    {
        setChoiceUndoable("mode", 0.0f, "Change HE2 Mode");
    };

    modernButton.onClick = [setChoiceUndoable]
    {
        setChoiceUndoable("mode", 1.0f, "Change HE2 Mode");
    };

    for (size_t i = 0; i < oversamplingButtons.size(); ++i)
    {
        oversamplingButtons[i].onClick = [setChoiceUndoable, i]
        {
            setChoiceUndoable("oversampling", (float) i, "Change HE2 Oversampling");
        };
    }

    powerButton.onClick = [this]
    {
        processor.beginUndoTransaction("Toggle HE2");
        processor.setParamValue("power", processor.getPowerState() ? 0.0f : 1.0f);
        processor.endUndoTransaction();
    };

    controlVisuals[0] = { &warmthSlider,   &warmthLabel,   &warmthValueLabel,   style.warmth,   0 };
    controlVisuals[1] = { &presenceSlider, &presenceLabel, &presenceValueLabel, style.presence, 1 };
    controlVisuals[2] = { &edgeSlider,     &edgeLabel,     &edgeValueLabel,     style.edge,     2 };
    controlVisuals[3] = { &airSlider,      &airLabel,      &airValueLabel,      style.air,      3 };
    controlVisuals[4] = { &amountSlider,   &amountLabel,   &amountValueLabel,   style.amount,   4 };
    controlVisuals[5] = { &mixSlider,      &mixLabel,      &mixValueLabel,      style.mix,      5 };
    controlVisuals[6] = { &outputSlider,   &outputLabel,   &outputValueLabel,   style.output,   6 };

    uiPowerOn = processor.getPowerState();
    powerButton.setPowerOn(uiPowerOn);

    startTimerHz(20); // Only labels/buttons are polled; no unconditional full redraw.
}

inline HE2GUI::~HE2GUI()
{
    warmthSlider.setLookAndFeel(nullptr);
    presenceSlider.setLookAndFeel(nullptr);
    edgeSlider.setLookAndFeel(nullptr);
    airSlider.setLookAndFeel(nullptr);
    amountSlider.setLookAndFeel(nullptr);
    mixSlider.setLookAndFeel(nullptr);
    outputSlider.setLookAndFeel(nullptr);

    classicButton.setLookAndFeel(nullptr);
    modernButton.setLookAndFeel(nullptr);
    for (auto& b : oversamplingButtons) b.setLookAndFeel(nullptr);
}

inline void HE2GUI::setDisplayQualityMode(DisplayQualityMode newMode)
{
    displayQualityMode = newMode;
    repaint();
}

inline void HE2GUI::configureSlider(juce::Slider& slider, const juce::String& name)
{
    slider.setName(name);
    slider.setLookAndFeel(&knobLookAndFeel);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setNumDecimalPlacesToDisplay(1);

    // Only the decorative dot/display portion is drawn by the parent. The
    // JUCE slider repaints its own knob on value changes.
    slider.onValueChange = [this, control = &slider]
    {
        for (const auto& cv : controlVisuals)
            if (cv.slider == control)
            {
                repaint(cv.windowBounds.expanded(uiPx(3.0f)).getSmallestIntegerContainer());
                break;
            }
    };

    slider.onDragStart = [this]
    {
        processor.beginUndoTransaction("Edit HE2");
    };

    slider.onDragEnd = [this]
    {
        processor.endUndoTransaction();
    };
}

inline void HE2GUI::configureNameLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, style.textSoft);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setInterceptsMouseClicks(false, false);
}

inline void HE2GUI::configureValueLabel(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, style.textMain);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    label.setEditable(true, true, false);
    label.setColour(juce::Label::textWhenEditingColourId, style.textMain);
    label.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour::fromRGBA(235, 240, 246, 240));
    label.setColour(juce::Label::outlineWhenEditingColourId, juce::Colour::fromRGB(130, 140, 150));
}

inline void HE2GUI::bindValueLabelToSlider(juce::Label& label, juce::Slider& slider)
{
    label.onEditorShow = [this, &label]
    {
        if (auto* editor = label.getCurrentTextEditor())
        {
            editor->setJustification(juce::Justification::centred);
            editor->setColour(juce::TextEditor::textColourId, style.textMain);
            editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGBA(235, 240, 246, 240));
            editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor->setColour(juce::CaretComponent::caretColourId, juce::Colour::fromRGB(90, 100, 110));
            editor->setFont(label.getFont());
            editor->selectAll();
        }
    };

    label.onTextChange = [this, &label, &slider]
    {
        applyTypedValueToSlider(label.getText(), slider);
        label.setText(formatSliderValue(slider), juce::dontSendNotification);
        slider.repaint();
    };
}

inline void HE2GUI::configureModeButton(juce::ToggleButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setLookAndFeel(&modeButtonLookAndFeel);
    button.setClickingTogglesState(false);
}

inline void HE2GUI::updateValueLabels()
{
    updateSingleValueLabel(warmthValueLabel, formatSliderValue(warmthSlider));
    updateSingleValueLabel(presenceValueLabel, formatSliderValue(presenceSlider));
    updateSingleValueLabel(edgeValueLabel, formatSliderValue(edgeSlider));
    updateSingleValueLabel(airValueLabel, formatSliderValue(airSlider));
    updateSingleValueLabel(amountValueLabel, formatSliderValue(amountSlider));
    updateSingleValueLabel(mixValueLabel, formatSliderValue(mixSlider));
    updateSingleValueLabel(outputValueLabel, formatSliderValue(outputSlider));
}

inline bool HE2GUI::updateSingleValueLabel(juce::Label& label, const juce::String& newText)
{
    if (label.isBeingEdited())
        return false;

    if (label.getText() == newText)
        return false;

    label.setText(newText, juce::dontSendNotification);
    return true;
}

inline juce::String HE2GUI::formatSliderValue(const juce::Slider& slider) const
{
    const auto name = slider.getName().toLowerCase();
    const double value = slider.getValue();

    if (name == "output")
    {
        juce::String s = juce::String(value, 1);
        if (value > 0.0)
            s = "+" + s;
        return s + " dB";
    }

    return juce::String(value, 1) + " %";
}

inline bool HE2GUI::applyTypedValueToSlider(const juce::String& text, juce::Slider& slider)
{
    double value = parseLooseNumber(text);
    // JUCE Slider/AudioParameter enforces its declared valid parameter range.
    // Don't impose an additional GUI-specific clamp.

    if (std::abs(slider.getValue() - value) <= 1.0e-9)
        return false;

    processor.beginUndoTransaction("Edit HE2 Value");
    slider.setValue(value, juce::sendNotificationSync);
    processor.endUndoTransaction();
    return true;
}

inline void HE2GUI::timerCallback()
{
    processor.serviceHostStateChanges(); // message thread only
    if (processor.isLocked())
        return;

    const bool newPower = processor.getPowerState();
    if (uiPowerOn != newPower)
    {
        uiPowerOn = newPower;
        powerButton.setPowerOn(uiPowerOn);
        repaint(); // Centre screen and seven icon windows change power state.
        classicButton.repaint();
        modernButton.repaint();
        for (auto& button : oversamplingButtons)
            button.repaint(); // Turn selected-state accent lights on/off.
        for (const auto& cv : controlVisuals)
            if (cv.slider != nullptr)
                cv.slider->repaint();
    }

    const bool focusMode = (processor.getModeIndex() == 1);
    classicButton.setToggleState(!focusMode, juce::dontSendNotification);
    modernButton.setToggleState(focusMode, juce::dontSendNotification);
    const int os = juce::roundToInt(processor.getParamValue("oversampling"));
    for (size_t i = 0; i < oversamplingButtons.size(); ++i)
        oversamplingButtons[i].setToggleState(os == (int) i, juce::dontSendNotification);

    // The processor publishes four thread-safe level snapshots. A slow visual
    // attack/release smooths the LEDs on the message thread; no audio callback
    // ever touches graphics or the JUCE Component hierarchy.
    bool ledsChanged = false;
    for (int band = 0; band < 4; ++band)
    {
        const float rawTarget = uiPowerOn ? processor.getBandActivity(band) : 0.0f;
        const float target = std::pow(juce::jlimit(0.0f, 1.0f, rawTarget), 0.78f);
        auto& visual = visualBandActivity[(size_t) band];
        const float response = target > visual ? 0.56f : 0.16f;
        const float next = visual + (target - visual) * response;
        if (std::abs(next - visual) > 0.0020f)
            ledsChanged = true;
        visual = next;
    }
    if (ledsChanged && uiPowerOn)
        repaint(juce::Rectangle<int>(juce::roundToInt(getWidth() * 0.40f),
                                    juce::roundToInt(getHeight() * 0.275f),
                                    juce::roundToInt(getWidth() * 0.22f),
                                    juce::roundToInt(getHeight() * 0.065f)));

    updateValueLabels(); // Each JUCE Label repaints only when its text changes.
}

inline void HE2GUI::paint(juce::Graphics& g)
{
    g.setColour(style.bgColor);
    g.fillAll();

    drawOuterShell(g);
    //drawHeader(g);

    auto topArea = juce::Rectangle<float>(getWidth() * 0.05f,
                                          getHeight() * 0.12f,
                                          getWidth() * 0.90f,
                                          getHeight() * 0.228f);
    drawTopSection(g, topArea);

    auto footerGlowArea = juce::Rectangle<float>(getWidth() * 0.06f,
                                                 getHeight() * 0.35f,
                                                 getWidth() * 0.88f,
                                                 getHeight() * 0.58f);
    //drawFooterGlow(g, footerGlowArea);

    for (const auto& cv : controlVisuals)
        drawControlWindow(g, cv);
    drawVersionEtching(g);
}

inline void HE2GUI::drawOuterShell(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(getWidth() * 0.03f);

    g.setColour(style.shellShadow);
    g.fillRoundedRectangle(r.translated(0.0f, uiPx(6.0f)), uiPx(28.0f));

    juce::ColourGradient shellGrad(style.faceTop, r.getX(), r.getY(),
                                   style.faceBottom, r.getX(), r.getBottom(), false);
    shellGrad.addColour(0.48, style.faceMid);
    g.setGradientFill(shellGrad);
    g.fillRoundedRectangle(r, uiPx(28.0f));

    g.setColour(style.shellEdge);
    g.drawRoundedRectangle(r, uiPx(28.0f), uiPx(1.2f));

    g.setColour(juce::Colours::white.withAlpha(0.40f));
    g.drawRoundedRectangle(r.reduced(uiPx(2.0f)), uiPx(26.0f), uiPx(1.0f));

    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.drawRoundedRectangle(r.reduced(uiPx(4.0f)), uiPx(24.0f), uiPx(1.0f));

    auto shine = r.removeFromTop(r.getHeight() * 0.22f);
    juce::ColourGradient shineGrad(juce::Colours::white.withAlpha(0.15f), shine.getCentreX(), shine.getY(),
                                   juce::Colours::white.withAlpha(0.0f), shine.getCentreX(), shine.getBottom(), false);
    g.setGradientFill(shineGrad);
    g.fillRoundedRectangle(shine, uiPx(24.0f));

    // Extremely fine satin brushing confined to the faceplate. Kept below
    // the visual threshold of an obvious striped texture.
    const int pitch = juce::jmax(1, juce::roundToInt(uiPx(3.5f)));
    for (int x = juce::roundToInt(getWidth() * 0.035f);
         x < juce::roundToInt(getWidth() * 0.965f); x += pitch)
    {
        const float alpha = ((x / pitch) % 7 == 0) ? 0.018f : 0.006f;
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawVerticalLine(x, getHeight() * 0.045f, getHeight() * 0.952f);
    }
}

inline void HE2GUI::drawHeader(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().removeFromTop(getHeight() * 0.10f);

    g.setColour(juce::Colours::red.withAlpha(0.18f));
    g.drawLine(area.getX() + area.getWidth() * 0.07f, area.getBottom(),
               area.getRight() - area.getWidth() * 0.07f, area.getBottom(), uiPx(1.0f));
}

inline HE2GUI::TopPanelLayout HE2GUI::getTopPanelLayout(juce::Rectangle<float> area) const noexcept
{
    // One geometry source for painting AND resized(): equal-width side housings,
    // a fixed symmetrical gap, and a centred display.  The display itself
    // applies its historical 8x2-reference-pixel inner inset.
    const float sideWidth = area.getWidth() * 0.148f;
    const float gap = area.getWidth() * 0.012f;
    const float verticalInset = uiPx(2.0f);
    const float sideHeight = area.getHeight() - 2.0f * verticalInset;

    TopPanelLayout layout;
    layout.left = { area.getX(), area.getY() + verticalInset,
                    sideWidth, sideHeight };
    layout.displayArea = { area.getX() + sideWidth + gap, area.getY(),
                           area.getWidth() - 2.0f * (sideWidth + gap),
                           area.getHeight() };
    layout.right = { area.getRight() - sideWidth, area.getY() + verticalInset,
                     sideWidth, sideHeight };
    return layout;
}

inline void HE2GUI::drawTopSection(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto layout = getTopPanelLayout(area);

    // Matching satin silver-gray side panels: a narrower, darker vertical
    // gradient than the faceplate, with a quiet inset rim rather than a domed
    // plastic highlight. Panel and control geometry are completely unchanged.
    auto drawHousing = [this, &g] (juce::Rectangle<float> r)
    {
        const float radius = uiPx(20.0f);
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillRoundedRectangle(r.translated(0.0f, uiPx(2.0f)), radius);

        juce::ColourGradient satin(juce::Colour::fromRGB(204, 210, 217), r.getX(), r.getY(),
                                   juce::Colour::fromRGB(185, 191, 200), r.getX(), r.getBottom(), false);
        g.setGradientFill(satin);
        g.fillRoundedRectangle(r, radius);

        // Defined perimeter without the pronounced glossy/raised rim.
        g.setColour(juce::Colour::fromRGB(124, 134, 146).withAlpha(0.86f));
        g.drawRoundedRectangle(r, radius, uiPx(1.0f));
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(r.reduced(uiPx(1.4f)), radius - uiPx(1.4f), uiPx(0.8f));

        // Very light inset detail to communicate a separate material panel.
        auto inner = r.reduced(uiPx(6.0f));
        g.setColour(juce::Colours::white.withAlpha(0.055f));
        g.drawRoundedRectangle(inner, radius - uiPx(6.0f), uiPx(0.75f));
        g.setColour(juce::Colours::black.withAlpha(0.045f));
        g.drawLine(inner.getX() + uiPx(6.0f), inner.getBottom() - uiPx(1.5f),
                   inner.getRight() - uiPx(6.0f), inner.getBottom() - uiPx(1.5f), uiPx(0.8f));
    };

    drawHousing(layout.left);
    drawHousing(layout.right);

    // Light contact shadows just below and around the display; these make the
    // centre look slightly lifted off the faceplate rather than sunk in.
    const auto display = layout.displayArea.reduced(uiPx(8.0f), uiPx(2.0f));
    const auto shadowRadius = uiPx(20.0f);
    g.setColour(juce::Colours::black.withAlpha(0.055f));
    g.fillRoundedRectangle(display.expanded(uiPx(2.8f), uiPx(2.4f))
                                  .translated(0.0f, uiPx(4.5f)), shadowRadius);
    g.setColour(juce::Colours::black.withAlpha(0.11f));
    g.fillRoundedRectangle(display.expanded(uiPx(1.0f), uiPx(0.8f))
                                  .translated(0.0f, uiPx(2.0f)), shadowRadius);

    drawBrandPanel(g, layout.displayArea);
}

inline void HE2GUI::drawBrandPanel(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto panel = area.reduced(uiPx(8.0f), uiPx(2.0f));

    const auto displayTop = uiPowerOn ? style.panelDark
                                      : juce::Colour::fromRGB(6, 8, 11);
    const auto displayBottom = uiPowerOn ? style.panelDark2
                                         : juce::Colour::fromRGB(8, 10, 14);
    juce::ColourGradient panelGrad(displayTop, panel.getX(), panel.getY(),
                                   displayBottom, panel.getX(), panel.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(panel, uiPx(20.0f));

    g.setColour(style.panelStroke);
    g.drawRoundedRectangle(panel, uiPx(20.0f), uiPx(1.0f));
    fillInnerShadow(g, panel, uiPx(20.0f));

    // The display is actual powered-off glass: no logo, text, bars, or LEDs.
    // Its etched rim and faint glass gradient stay visible as physical hardware.
    if (! uiPowerOn)
        return;

    if (etchedLogo != nullptr)
    {
        auto logoArea = panel.withSizeKeepingCentre(panel.getHeight() * 0.54f,
                                                    panel.getHeight() * 0.54f)
                            .translated(0.0f, +panel.getHeight() * 0.02f);
        etchedLogo->drawWithin(g, logoArea.translated(uiPx(0.8f), uiPx(1.0f)),
                               juce::RectanglePlacement::centred, 0.025f);
        etchedLogo->drawWithin(g, logoArea,
                               juce::RectanglePlacement::centred, 0.23f);
    }

    auto inner = panel.reduced(panel.getWidth() * 0.05f, panel.getHeight() * 0.12f);

    g.setColour(juce::Colours::white.withAlpha(0.040f));
    for (int i = 0; i < 9; ++i)
    {
        auto x = juce::jmap((float)i, 0.0f, 8.0f, inner.getX(), inner.getRight());
        g.drawVerticalLine((int)std::round(x), inner.getY(), inner.getBottom());
    }

    auto top = inner.removeFromTop(inner.getHeight() * 0.52f);
    auto bottom = inner;

    g.setColour(style.displayText.withAlpha(0.95f));
    g.setFont(juce::Font(top.getHeight() * 0.7f, juce::Font::bold));
    g.drawFittedText("Parish Audio", top.toNearestInt(), juce::Justification::centredTop, 1);

    g.setColour(style.displayText.withAlpha(0.80f));
    g.setFont(juce::Font(bottom.getHeight() * 0.28f, juce::Font::bold));
    juce::Rectangle<float> a(bottom.getX(), bottom.getY() - getHeight() * 0.025f, bottom.getWidth(), bottom.getHeight());
    g.drawFittedText("HE2 Harmonic Enhancer", a.toNearestInt(), juce::Justification::centredTop, 1);

    auto accentStrip = juce::Rectangle<float>(panel.getCentreX() - panel.getWidth() * 0.16f,
                                              panel.getBottom() - panel.getHeight() * 0.16f,
                                              panel.getWidth() * 0.32f,
                                              panel.getHeight() * 0.05f);

    const std::array<juce::Colour, 4> bandColors { style.warmth, style.presence, style.edge, style.air };

    for (int i = 0; i < 4; ++i)
    {
        auto seg = accentStrip.removeFromLeft(accentStrip.getWidth() / (float)(4 - i));
        auto dot = seg.withSizeKeepingCentre(seg.getHeight() * 0.85f, seg.getHeight() * 0.85f);

        // Physical dark lens socket, followed by an audio-reactive colored LED.
        // The band activity is derived from the existing DSP envelope followers.
        g.setColour(juce::Colours::black.withAlpha(0.53f));
        g.fillEllipse(dot.expanded(dot.getWidth() * 0.65f));
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.drawEllipse(dot.expanded(dot.getWidth() * 0.68f), uiPx(0.8f));
        const float visual = visualBandActivity[(size_t) i];
        const float intensity = 0.46f + 0.94f * std::pow(juce::jlimit(0.0f, 1.0f, visual), 0.72f);
        drawIlluminatedDot(g, dot, bandColors[(size_t) i], intensity);
    }
}

inline void HE2GUI::drawVersionEtching(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

inline void HE2GUI::drawFooterGlow(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::ColourGradient wash(juce::Colours::white.withAlpha(0.10f), area.getCentreX(), area.getY(),
                              juce::Colours::white.withAlpha(0.0f), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill(wash);
    g.fillRoundedRectangle(area, uiPx(26.0f));
}

inline void HE2GUI::drawControlWindow(juce::Graphics& g, const ControlVisual& cv)
{
    if (cv.slider == nullptr || cv.nameLabel == nullptr || cv.valueLabel == nullptr)
        return;

    auto panel = cv.windowBounds;

    juce::ColourGradient panelGrad(style.panelDark, panel.getX(), panel.getY(),
                                   style.panelDark2, panel.getX(), panel.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(panel, uiPx(20.0f));

    g.setColour(style.panelStroke);
    g.drawRoundedRectangle(panel, uiPx(20.0f), uiPx(1.0f));
    fillInnerShadow(g, panel, uiPx(20.0f));
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.drawRoundedRectangle(panel.reduced(uiPx(1.8f)), uiPx(18.5f), uiPx(2.0f));
    g.setColour(juce::Colours::white.withAlpha(0.075f));
    g.drawLine(panel.getX() + uiPx(15.0f), panel.getY() + uiPx(2.2f),
               panel.getRight() - uiPx(15.0f), panel.getY() + uiPx(2.2f), uiPx(1.2f));

    auto gloss = panel.removeFromTop(panel.getHeight() * 0.16f);
    juce::ColourGradient glossGrad(juce::Colours::white.withAlpha(0.10f), gloss.getCentreX(), gloss.getY(),
                                   juce::Colours::white.withAlpha(0.0f), gloss.getCentreX(), gloss.getBottom(), false);
    g.setGradientFill(glossGrad);
    g.fillRoundedRectangle(gloss, uiPx(18.0f));

    auto displayArea = cv.windowBounds;
    displayArea = displayArea.removeFromTop(displayArea.getHeight() * 1.0f);
    displayArea = displayArea.reduced(displayArea.getWidth() * 0.06f,
                                      displayArea.getHeight() * 0.10f);

    const auto glassTop = uiPowerOn ? juce::Colour::fromRGB(14, 18, 24)
                                    : juce::Colour::fromRGB(5, 7, 10);
    const auto glassBottom = uiPowerOn ? juce::Colour::fromRGB(24, 30, 38)
                                       : juce::Colour::fromRGB(9, 12, 16);
    juce::ColourGradient displayGrad(glassTop, displayArea.getX(), displayArea.getY(),
                                     glassBottom, displayArea.getX(), displayArea.getBottom(), false);
    g.setGradientFill(displayGrad);
    g.fillRoundedRectangle(displayArea, uiPx(14.0f));

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawRoundedRectangle(displayArea, uiPx(14.0f), uiPx(1.4f));

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(displayArea.reduced(uiPx(1.2f)), uiPx(12.8f), uiPx(1.0f));

    // Keep the recessed glass and silver bezel visible when powered off,
    // but eliminate all seven colored icons and their tiny indicator LEDs.
    if (! uiPowerOn)
        return;

    auto innerDisplay = displayArea.reduced(displayArea.getWidth() * 0.08f,
                                            displayArea.getHeight() * 0.10f);

    // Preserve the rounded, recessed glass corners (no rectangular overfill).

    auto iconArea = innerDisplay.removeFromTop(innerDisplay.getHeight() * 0.70f)
                                .reduced(innerDisplay.getWidth() * 0.10f,
                                         innerDisplay.getHeight() * 0.15f);

    drawControlIcon(g, iconArea, cv.iconType, cv.accent);

    auto indicatorArea = innerDisplay.removeFromBottom(innerDisplay.getHeight() * 0.42f)
                                     .reduced(innerDisplay.getWidth() * 0.10f, 0.0f);

    const auto sliderMin = cv.slider->getMinimum();
    const auto sliderMax = cv.slider->getMaximum();
    const auto sliderVal = cv.slider->getValue();
    const float norm = (float)juce::jlimit(0.0, 1.0,
                                           (sliderVal - sliderMin) / (sliderMax - sliderMin));

    drawWindowIndicators(g, indicatorArea, norm, cv.accent);
}

inline void HE2GUI::drawControlIcon(juce::Graphics& g, juce::Rectangle<float> area, int iconType, juce::Colour accent)
{
    switch (iconType)
    {
        case 0: drawWarmthIcon(g, area, accent);   break;
        case 1: drawPresenceIcon(g, area, accent); break;
        case 2: drawEdgeIcon(g, area, accent);     break;
        case 3: drawAirIcon(g, area, accent);      break;
        case 4: drawAmountIcon(g, area, accent);   break;
        case 5: drawMixIcon(g, area, accent);      break;
        case 6: drawOutputIcon(g, area, accent);   break;
        default: break;
    }
}

inline void HE2GUI::drawWindowIndicators(juce::Graphics& g, juce::Rectangle<float> area, float norm, juce::Colour accent)
{
    constexpr int numDots = 5;
    auto row = area.withTrimmedTop(area.getHeight() * 0.10f)
                   .withTrimmedBottom(area.getHeight() * 0.12f);

    const float dotSize = juce::jmin(row.getHeight(), row.getWidth() / (float) (numDots * 2.2f));
    const float totalWidth = (float) numDots * dotSize + (float) (numDots - 1) * dotSize * 0.80f;
    const float startX = row.getCentreX() - totalWidth * 0.5f;
    const float y = row.getCentreY() - dotSize * 0.5f;

    const int litDots = juce::jlimit(0, numDots, (int) std::round(norm * (float) numDots));

    for (int i = 0; i < numDots; ++i)
    {
        const float x = startX + (float) i * dotSize * 1.80f;
        auto dot = juce::Rectangle<float>(x, y, dotSize, dotSize);
        const bool lit = (i < litDots) && uiPowerOn;

        if (lit)
            drawIlluminatedDot(g, dot, accent);
        else
        {
            // Unlit lenses have depth but no colored bloom.
            g.setColour(juce::Colours::black.withAlpha(0.16f));
            g.fillEllipse(dot.expanded(dotSize * 0.12f));
            g.setColour(juce::Colours::white.withAlpha(0.17f));
            g.fillEllipse(dot);
            g.setColour(juce::Colours::white.withAlpha(0.075f));
            g.fillEllipse(dot.withSizeKeepingCentre(dotSize * 0.40f, dotSize * 0.40f)
                              .translated(-dotSize * 0.10f, -dotSize * 0.10f));
        }
    }
}

inline void HE2GUI::layoutControls(juce::Rectangle<float> area)
{
    area = area.reduced(area.getWidth() * 0.01f, area.getHeight() * 0.02f);

    const float colGap = area.getWidth() * 0.012f;
    const float colWidth = (area.getWidth() - colGap * (float)(controlVisuals.size() - 1))
                           / (float) controlVisuals.size();

    for (size_t i = 0; i < controlVisuals.size(); ++i)
    {
        auto& cv = controlVisuals[i];
        juce::Rectangle<float> col(area.getX() + (float) i * (colWidth + colGap),
                                   area.getY(), colWidth, area.getHeight());

        cv.columnBounds = col;
        cv.windowBounds = juce::Rectangle<float>(col.getX(),
                                                 col.getY() + col.getHeight() * 0.02f,
                                                 col.getWidth(),
                                                 col.getHeight() * 0.30f);

        // Each control is a deliberate vertical stack:
        // icon display / parameter name / knob / editable value.
        cv.nameBounds = juce::Rectangle<float>(col.getX() + col.getWidth() * 0.06f,
                                               col.getY() + col.getHeight() * 0.350f,
                                               col.getWidth() * 0.88f,
                                               col.getHeight() * 0.064f);

        const float knobSize = juce::jmin(col.getWidth() * 0.92f,
                                          col.getHeight() * 0.36f);
        cv.knobBounds = juce::Rectangle<float>(col.getCentreX() - knobSize * 0.5f,
                                               col.getY() + col.getHeight() * 0.423f,
                                               knobSize, knobSize);

        cv.valueBounds = juce::Rectangle<float>(col.getX() + col.getWidth() * 0.06f,
                                                cv.knobBounds.getBottom() + col.getHeight() * 0.018f,
                                                col.getWidth() * 0.88f,
                                                col.getHeight() * 0.065f);

        if (cv.slider != nullptr)     cv.slider->setBounds(cv.knobBounds.toNearestInt());
        if (cv.nameLabel != nullptr)  cv.nameLabel->setBounds(cv.nameBounds.toNearestInt());
        if (cv.valueLabel != nullptr) cv.valueLabel->setBounds(cv.valueBounds.toNearestInt());
    }
}

inline void HE2GUI::updateScaledFonts()
{
    for (auto* label : { &warmthLabel, &presenceLabel, &edgeLabel, &airLabel,
                         &amountLabel, &mixLabel, &outputLabel })
        label->setFont(juce::Font(uiPx(19.0f), juce::Font::bold));

    for (auto* label : { &warmthValueLabel, &presenceValueLabel, &edgeValueLabel,
                         &airValueLabel, &amountValueLabel, &mixValueLabel, &outputValueLabel })
        label->setFont(juce::Font(uiPx(18.5f)));

    modeLabel.setFont(juce::Font(uiPx(13.5f), juce::Font::bold));
    oversamplingLabel.setFont(juce::Font(uiPx(10.5f), juce::Font::bold));
    titleLabel.setFont(juce::Font(uiPx(20.0f)));
    subtitleLabel.setFont(juce::Font(uiPx(12.0f)));
}

inline void HE2GUI::resized()
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    updateScaledFonts();

    const auto topArea = juce::Rectangle<float>(w * 0.05f, h * 0.12f,
                                                 w * 0.90f, h * 0.228f);
    const auto layout = getTopPanelLayout(topArea);

    // LEFT — MODE above Smooth and Focused. Centre the three-item cluster
    // inside the housing with the same reference-scaled spacing at every size.
    const int modeWidth = juce::roundToInt(layout.left.getWidth() - 2.0f * uiPx(10.0f));
    const int modeHeight = juce::roundToInt(uiPx(38.0f));
    const int modeGap = juce::roundToInt(uiPx(13.0f));
    const int modeLabelHeight = juce::roundToInt(uiPx(18.0f));
    const int modeLabelGap = juce::roundToInt(uiPx(6.0f));
    const int modeX = juce::roundToInt(layout.left.getCentreX() - modeWidth * 0.5f);
    const int modeClusterHeight = modeLabelHeight + modeLabelGap + 2 * modeHeight + modeGap;
    const int modeTop = juce::roundToInt(layout.left.getCentreY() - modeClusterHeight * 0.5f);
    modeLabel.setBounds(modeX, modeTop, modeWidth, modeLabelHeight);
    const int modeY = modeTop + modeLabelHeight + modeLabelGap;
    classicButton.setBounds(modeX, modeY, modeWidth, modeHeight);
    modernButton.setBounds(modeX, modeY + modeHeight + modeGap, modeWidth, modeHeight);

    // RIGHT — lower the power button slightly, then put the four quality
    // buttons under it and the OVERSAMPLING caption under the buttons.
    // The entire column is vertically centred within the matching housing.
    const int powerSize = juce::roundToInt(uiPx(75.0f));
    const int powerX = juce::roundToInt(layout.right.getCentreX() - powerSize * 0.5f);

    const int osGap = juce::roundToInt(uiPx(4.0f));
    const int availableOsWidth = juce::roundToInt(layout.right.getWidth() - 2.0f * uiPx(9.0f));
    const int osSide = juce::jmin(juce::roundToInt(uiPx(28.0f)),
                                  (availableOsWidth - osGap * 3) / 4);
    const int osTotalWidth = osSide * 4 + osGap * 3;
    const int osStartX = juce::roundToInt(layout.right.getCentreX() - osTotalWidth * 0.5f);

    const int powerToButtonsGap = juce::roundToInt(uiPx(16.0f));
    const int buttonsToLabelGap = juce::roundToInt(uiPx(4.0f));
    const int osLabelHeight = juce::roundToInt(uiPx(15.0f));
    const int rightClusterHeight = powerSize + powerToButtonsGap + osSide
                                 + buttonsToLabelGap + osLabelHeight;
    const int rightTop = juce::roundToInt(layout.right.getCentreY() - rightClusterHeight * 0.5f);
    powerButton.setBounds(powerX, rightTop, powerSize, powerSize);

    const int osY = rightTop + powerSize + powerToButtonsGap;
    for (size_t i = 0; i < oversamplingButtons.size(); ++i)
        oversamplingButtons[i].setBounds(osStartX + (int) i * (osSide + osGap),
                                        osY, osSide, osSide);

    oversamplingLabel.setBounds(juce::roundToInt(layout.right.getX() + uiPx(4.0f)),
                                osY + osSide + buttonsToLabelGap,
                                juce::roundToInt(layout.right.getWidth() - 2.0f * uiPx(4.0f)),
                                osLabelHeight);

    // Branding is painted directly; these legacy labels stay invisible.
    titleLabel.setBounds(0, 0, 0, 0);
    subtitleLabel.setBounds(0, 0, 0, 0);

    auto controlsArea = juce::Rectangle<float>(w * 0.045f,
                                               h * 0.38f,
                                               w * 0.91f,
                                               h * 0.58f);
    layoutControls(controlsArea);
}
