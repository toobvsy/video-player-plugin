#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Tout ce qui concerne l'apparence : couleurs, style des boutons, icônes, barre de progression.
// Pour changer la couleur d'accent (orange par défaut), modifie la ligne "accent" ci-dessous.

namespace Theme
{
    inline const juce::Colour bg      { 0xff000000 };   // fond (derrière la vidéo)
    inline const juce::Colour bar     { 0xff0e0e10 };   // barre du haut
    inline const juce::Colour panel   { 0xff1f1f23 };   // boutons
    inline const juce::Colour panelHi { 0xff2c2c32 };   // survol, séparateurs
    inline const juce::Colour track   { 0xff2c2c32 };   // fond de la barre de progression
    inline const juce::Colour text    { 0xfff4f4f5 };
    inline const juce::Colour textDim { 0xff85858d };
    inline const juce::Colour accent  { 0xffff5a1f };   // orange
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
        setColour (juce::TextEditor::backgroundColourId,       Theme::panel);
        setColour (juce::TextEditor::textColourId,             Theme::text);
        setColour (juce::TextEditor::outlineColourId,          juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId,   Theme::accent);
        setColour (juce::TextEditor::highlightColourId,        Theme::accent.withAlpha (0.45f));
        setColour (juce::CaretComponent::caretColourId,        Theme::accent);
    }

    // Onglets (boutons à bascule) : pas de fond, juste un trait de couleur sous l'onglet actif.
    // Autres boutons : petite pastille arrondie.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();

        if (b.getClickingTogglesState())
        {
            if (b.getToggleState())
            {
                g.setColour (Theme::accent);
                g.fillRoundedRectangle (r.getX() + 8.0f, r.getBottom() - 3.0f, r.getWidth() - 16.0f, 2.0f, 1.0f);
            }
        }
        else
        {
            auto base = Theme::panel;
            if (down)             base = base.brighter (0.25f);
            else if (highlighted) base = base.brighter (0.12f);

            g.setColour (base);
            g.fillRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool) override
    {
        const bool isTab = b.getClickingTogglesState();

        auto col = Theme::text;
        if (isTab && ! b.getToggleState() && ! highlighted)
            col = Theme::textDim;

        g.setFont (13.0f);
        g.setColour (col);
        g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0),
                    juce::Justification::centred, false);
    }
};

//==============================================================================
// Petit bouton avec une icône dessinée (pas de texte, pas d'image à charger).
class IconButton : public juce::Button
{
public:
    enum class Icon { back, play, pause, collapse, expand };

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
        const auto c = r.getCentre();

        if (icon == Icon::expand)
        {
            // Barre masquée : juste une petite poignée discrète en haut de la fenêtre
            g.setColour (highlighted ? Theme::text : Theme::textDim.withAlpha (0.6f));
            g.fillRoundedRectangle (c.x - 18.0f, c.y - 1.5f, 36.0f, 3.0f, 1.5f);
            return;
        }

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

            case Icon::collapse:        // chevron vers le haut
                p.startNewSubPath (c.x - s, c.y + s * 0.5f);
                p.lineTo (c.x, c.y - s * 0.5f);
                p.lineTo (c.x + s, c.y + s * 0.5f);
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

            case Icon::expand:
                break;
        }
    }

private:
    Icon icon;
};

//==============================================================================
// Fine ligne de progression tout en bas : on clique ou on glisse pour avancer / reculer.
class SeekBar : public juce::Component
{
public:
    SeekBar()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (double)> onSeek;     // reçoit une valeur entre 0 et 1

    void setProgress (double p)
    {
        p = juce::jlimit (0.0, 1.0, p);

        if (progress != p)
        {
            progress = p;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float h = isMouseOverOrDragging() ? 6.0f : 3.0f;
        auto track = r.withSizeKeepingCentre (r.getWidth(), h);

        g.setColour (Theme::track);
        g.fillRect (track);

        g.setColour (Theme::accent);
        g.fillRect (track.withWidth (track.getWidth() * (float) progress));
    }

    void mouseDown (const juce::MouseEvent& e) override  { seekTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override  { seekTo (e); }
    void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override   { repaint(); }

private:
    void seekTo (const juce::MouseEvent& e)
    {
        if (onSeek)
            onSeek (juce::jlimit (0.0, 1.0, (double) e.x / (double) juce::jmax (1, getWidth())));
    }

    double progress = 0.0;
};
