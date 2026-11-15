#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Tout ce qui concerne l'apparence : couleurs (modifiables depuis l'onglet "Couleurs"),
// style des boutons, icônes, barre de progression.

namespace Theme
{
    // Trois couleurs de base choisies par l'utilisateur : fond, accent, texte.
    // Toutes les autres (barre, boutons, survol...) en sont déduites automatiquement.
    struct Palette
    {
        juce::Colour baseBg     { 0xff000000 };     // par défaut : noir
        juce::Colour baseAccent { 0xffffffff };     // blanc
        juce::Colour baseText   { 0xfff2f2f2 };     // blanc légèrement adouci

        juce::Colour bg, bar, panel, panelHi, track, text, textDim, accent;

        Palette() { set (baseBg, baseAccent, baseText); }

        void set (juce::Colour newBg, juce::Colour newAccent, juce::Colour newText)
        {
            baseBg = newBg;
            baseAccent = newAccent;
            baseText = newText;

            bg      = newBg;
            bar     = newBg.interpolatedWith (newText, 0.06f);
            panel   = newBg.interpolatedWith (newText, 0.14f);
            panelHi = newBg.interpolatedWith (newText, 0.24f);
            track   = panelHi;
            text    = newText;
            textDim = newBg.interpolatedWith (newText, 0.55f);
            accent  = newAccent;
        }
    };

    inline Palette& palette()
    {
        static Palette instance;
        return instance;
    }

    inline juce::Colour bg()      { return palette().bg; }
    inline juce::Colour bar()     { return palette().bar; }
    inline juce::Colour panel()   { return palette().panel; }
    inline juce::Colour panelHi() { return palette().panelHi; }
    inline juce::Colour track()   { return palette().track; }
    inline juce::Colour text()    { return palette().text; }
    inline juce::Colour textDim() { return palette().textDim; }
    inline juce::Colour accent()  { return palette().accent; }

    // Couleur lisible posée sur l'accent (ex. l'icône du bouton lecture)
    inline juce::Colour onAccent()
    {
        return palette().accent.getPerceivedBrightness() > 0.6f ? juce::Colours::black
                                                                : juce::Colours::white;
    }

    inline void setColours (juce::Colour newBg, juce::Colour newAccent, juce::Colour newText)
    {
        palette().set (newBg, newAccent, newText);
    }

    // Les réglages sont gardés dans un petit fichier, pour tous les projets :
    // ~/Library/Application Support/MonStudio/VideoPlayer-couleurs.xml
    inline juce::File settingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/MonStudio/VideoPlayer-couleurs.xml");
    }

    inline void load()
    {
        if (auto xml = juce::parseXML (settingsFile()))
        {
            if (xml->hasTagName ("Couleurs"))
            {
                setColours (juce::Colour::fromString (xml->getStringAttribute ("fond",   "ff000000")),
                            juce::Colour::fromString (xml->getStringAttribute ("accent", "ffffffff")),
                            juce::Colour::fromString (xml->getStringAttribute ("texte",  "fff2f2f2")));
            }
        }
    }

    inline void save()
    {
        juce::XmlElement xml ("Couleurs");
        xml.setAttribute ("fond",   palette().baseBg.toString());
        xml.setAttribute ("accent", palette().baseAccent.toString());
        xml.setAttribute ("texte",  palette().baseText.toString());

        const auto file = settingsFile();
        file.getParentDirectory().createDirectory();
        xml.writeTo (file);
    }
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
        refreshColours();
    }

    // À rappeler chaque fois que les couleurs changent
    void refreshColours()
    {
        setColour (juce::TextEditor::backgroundColourId,       Theme::panel());
        setColour (juce::TextEditor::textColourId,             Theme::text());
        setColour (juce::TextEditor::outlineColourId,          juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId,   Theme::accent());
        setColour (juce::TextEditor::highlightColourId,        Theme::accent().withAlpha (0.35f));
        setColour (juce::TextEditor::highlightedTextColourId,  Theme::text());
        setColour (juce::CaretComponent::caretColourId,        Theme::accent());

        // Sélecteur de couleur (onglet "Couleurs")
        setColour (juce::ColourSelector::backgroundColourId,   Theme::bg());
        setColour (juce::ColourSelector::labelTextColourId,    Theme::text());
        setColour (juce::Slider::thumbColourId,                Theme::accent());
        setColour (juce::Slider::trackColourId,                Theme::panelHi());
        setColour (juce::Slider::backgroundColourId,           Theme::panel());
        setColour (juce::Slider::textBoxTextColourId,          Theme::text());
        setColour (juce::Slider::textBoxBackgroundColourId,    Theme::panel());
        setColour (juce::Slider::textBoxOutlineColourId,       juce::Colours::transparentBlack);

        // Menu déroulant (bouton "...")
        setColour (juce::PopupMenu::backgroundColourId,             Theme::panel());
        setColour (juce::PopupMenu::textColourId,                   Theme::text());
        setColour (juce::PopupMenu::headerTextColourId,             Theme::textDim());
        setColour (juce::PopupMenu::highlightedBackgroundColourId,  Theme::panelHi());
        setColour (juce::PopupMenu::highlightedTextColourId,        Theme::text());
    }

    // Onglets (boutons à bascule) : pas de fond, juste un trait sous l'onglet actif.
    // Autres boutons : petite pastille arrondie.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();

        if (b.getClickingTogglesState())
        {
            if (b.getToggleState())
            {
                g.setColour (Theme::accent());
                g.fillRoundedRectangle (r.getX() + 8.0f, r.getBottom() - 3.0f, r.getWidth() - 16.0f, 2.0f, 1.0f);
            }
        }
        else
        {
            auto base = Theme::panel();
            if (down)             base = base.interpolatedWith (Theme::text(), 0.25f);
            else if (highlighted) base = base.interpolatedWith (Theme::text(), 0.12f);

            g.setColour (base);
            g.fillRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool) override
    {
        const bool isTab = b.getClickingTogglesState();

        auto col = Theme::text();
        if (isTab && ! b.getToggleState() && ! highlighted)
            col = Theme::textDim();

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
    enum class Icon { back, play, pause, collapse, expand, more };

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
            g.setColour (highlighted ? Theme::text() : Theme::textDim().withAlpha (0.8f));
            g.fillRoundedRectangle (c.x - 18.0f, c.y - 1.5f, 36.0f, 3.0f, 1.5f);
            return;
        }

        const bool isPlayIcon = (icon == Icon::play || icon == Icon::pause);

        if (isPlayIcon)
        {
            auto fill = Theme::accent();
            if (down)             fill = fill.interpolatedWith (Theme::bg(), 0.3f);
            else if (highlighted) fill = fill.interpolatedWith (Theme::bg(), 0.15f);

            g.setColour (fill);
            g.fillEllipse (r);
        }
        else if (highlighted || down)
        {
            g.setColour (Theme::panelHi().withAlpha (down ? 1.0f : 0.8f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        g.setColour (isPlayIcon ? Theme::onAccent() : Theme::text());

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

            case Icon::more:            // trois points
                for (int i = -1; i <= 1; ++i)
                    g.fillEllipse (c.x + (float) i * s * 0.9f - 1.7f, c.y - 1.7f, 3.4f, 3.4f);
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
        auto trackArea = r.withSizeKeepingCentre (r.getWidth(), h);

        g.setColour (Theme::track());
        g.fillRect (trackArea);

        g.setColour (Theme::accent());
        g.fillRect (trackArea.withWidth (trackArea.getWidth() * (float) progress));
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

//==============================================================================
// Pastille de couleur cliquable (onglet "Couleurs") : montre la couleur actuelle d'un élément.
class SwatchButton : public juce::Button
{
public:
    explicit SwatchButton (const juce::String& label) : juce::Button (label)
    {
        setClickingTogglesState (true);
        setRadioGroupId (1002);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSwatchColour (juce::Colour c)
    {
        colour = c;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);

        g.setColour (colour);
        g.fillRoundedRectangle (r, 8.0f);

        // Contour : visible quelle que soit la couleur choisie
        g.setColour (getToggleState() ? Theme::text()
                                      : (highlighted ? Theme::textDim() : Theme::panelHi()));
        g.drawRoundedRectangle (r, 8.0f, getToggleState() ? 2.0f : 1.0f);

        g.setColour (colour.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
        g.setFont (13.0f);
        g.drawText (getButtonText(), r.toNearestInt().reduced (12, 0),
                    juce::Justification::centredLeft, false);
    }

private:
    juce::Colour colour { juce::Colours::black };
};
