#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Tout ce qui concerne l'apparence : couleurs, style des boutons et curseurs, icônes.

namespace Theme
{
    inline const juce::Colour bg       { 0xff101116 };
    inline const juce::Colour bar      { 0xff17181f };
    inline const juce::Colour panel    { 0xff22242d };
    inline const juce::Colour panelHi  { 0xff2e313c };
    inline const juce::Colour track    { 0xff343744 };
    inline const juce::Colour text     { 0xffeceef4 };
    inline const juce::Colour textDim  { 0xff8c90a3 };
    inline const juce::Colour accent   { 0xff7c5cff };
}

// Les textes avec accents doivent être convertis explicitement depuis l'UTF-8,
// sinon "vidéo" s'affiche "vidÃ©o".
inline juce::String fr (const char* utf8)
{
    return juce::String::fromUTF8 (utf8);
}

//==============================================================================
class PluginLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PluginLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, Theme::text);
        setColour (juce::TextButton::textColourOnId,  juce::Colours::white);

        setColour (juce::TextEditor::backgroundColourId,       Theme::panel);
        setColour (juce::TextEditor::textColourId,             Theme::text);
        setColour (juce::TextEditor::outlineColourId,          juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId,   Theme::accent);
        setColour (juce::TextEditor::highlightColourId,        Theme::accent.withAlpha (0.45f));
        setColour (juce::CaretComponent::caretColourId,        Theme::accent);

        setColour (juce::Label::textColourId, Theme::textDim);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);

        auto base = b.getToggleState() ? Theme::accent : Theme::panel;
        if (down)             base = base.brighter (0.18f);
        else if (highlighted) base = base.brighter (0.10f);

        g.setColour (base);
        g.fillRoundedRectangle (r, 8.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float cy = (float) y + (float) height * 0.5f;
        const float left = (float) x;
        const float w = (float) width;

        g.setColour (Theme::track);
        g.fillRoundedRectangle (left, cy - 2.0f, w, 4.0f, 2.0f);

        g.setColour (Theme::accent);
        g.fillRoundedRectangle (left, cy - 2.0f, juce::jmax (0.0f, sliderPos - left), 4.0f, 2.0f);

        const float r = slider.isMouseOverOrDragging() ? 7.0f : 6.0f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
    }
};

//==============================================================================
// Petit bouton avec une icône dessinée (pas de texte, pas d'image à charger).
class IconButton : public juce::Button
{
public:
    enum class Icon { back, forward, reload, home, play, pause };

    explicit IconButton (Icon i) : juce::Button (""), icon (i)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setIcon (Icon i)
    {
        if (icon != i)
        {
            icon = i;
            repaint();
        }
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool isPlayIcon = (icon == Icon::play || icon == Icon::pause);

        if (isPlayIcon)
        {
            g.setColour (down ? Theme::accent.brighter (0.2f)
                              : highlighted ? Theme::accent.brighter (0.1f) : Theme::accent);
            g.fillEllipse (r);
        }
        else if (highlighted || down)
        {
            g.setColour (Theme::panelHi.withAlpha (down ? 1.0f : 0.8f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        g.setColour (isPlayIcon ? juce::Colours::white : Theme::text);

        const auto c = r.getCentre();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
        const juce::PathStrokeType stroke (2.2f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);
        juce::Path p;

        switch (icon)
        {
            case Icon::back:
                p.startNewSubPath (c.x + s * 0.5f, c.y - s);
                p.lineTo (c.x - s * 0.5f, c.y);
                p.lineTo (c.x + s * 0.5f, c.y + s);
                g.strokePath (p, stroke);
                break;

            case Icon::forward:
                p.startNewSubPath (c.x - s * 0.5f, c.y - s);
                p.lineTo (c.x + s * 0.5f, c.y);
                p.lineTo (c.x - s * 0.5f, c.y + s);
                g.strokePath (p, stroke);
                break;

            case Icon::reload:
            {
                const float endAngle = juce::MathConstants<float>::twoPi - 0.3f;
                p.addCentredArc (c.x, c.y, s, s, 0.0f, 0.6f, endAngle, true);
                g.strokePath (p, stroke);

                const juce::Point<float> tip (c.x + s * std::sin (endAngle), c.y - s * std::cos (endAngle));
                const juce::Point<float> tangent (std::cos (endAngle), std::sin (endAngle));
                const juce::Point<float> radial (std::sin (endAngle), -std::cos (endAngle));

                juce::Path head;
                head.addTriangle (tip + tangent * (s * 0.8f),
                                  tip + radial * (s * 0.55f),
                                  tip - radial * (s * 0.55f));
                g.fillPath (head);
                break;
            }

            case Icon::home:
                p.startNewSubPath (c.x - s * 1.15f, c.y - s * 0.05f);
                p.lineTo (c.x, c.y - s * 1.1f);
                p.lineTo (c.x + s * 1.15f, c.y - s * 0.05f);
                p.startNewSubPath (c.x - s * 0.75f, c.y - s * 0.2f);
                p.lineTo (c.x - s * 0.75f, c.y + s);
                p.lineTo (c.x + s * 0.75f, c.y + s);
                p.lineTo (c.x + s * 0.75f, c.y - s * 0.2f);
                g.strokePath (p, stroke);
                break;

            case Icon::play:
                p.addTriangle (c.x - s * 0.6f, c.y - s,
                               c.x - s * 0.6f, c.y + s,
                               c.x + s,        c.y);
                g.fillPath (p);
                break;

            case Icon::pause:
                g.fillRoundedRectangle (c.x - s * 0.85f, c.y - s, s * 0.6f, s * 2.0f, 1.5f);
                g.fillRoundedRectangle (c.x + s * 0.25f, c.y - s, s * 0.6f, s * 2.0f, 1.5f);
                break;
        }
    }

private:
    Icon icon;
};
