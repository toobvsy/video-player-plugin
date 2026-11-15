#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Deux petites fonctions qui parlent directement à macOS (voir NativeTweaks.mm),
// pour ce que JUCE ne propose pas tout seul.
namespace NativeTweaks
{
    // true  = la vidéo remplit tout le cadre (elle est recadrée si le format ne correspond pas)
    // false = la vidéo est entière, avec des bandes noires si besoin
    void setVideoFillsFrame (juce::Component& videoComponent, bool fill);

    // Cache / ré-affiche le curseur de la souris
    void setCursorHidden (bool shouldHide);
}
