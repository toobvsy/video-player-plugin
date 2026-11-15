#include "NativeTweaks.h"
#include <juce_gui_extra/juce_gui_extra.h>

#import <AppKit/AppKit.h>
#import <AVKit/AVKit.h>

namespace NativeTweaks
{
    void setVideoFillsFrame (juce::Component& videoComponent, bool fill)
    {
        // Le lecteur vidéo de JUCE contient une vue macOS native (un AVPlayerView) :
        // on la retrouve et on change la façon dont l'image est placée dans le cadre.
        for (auto* child : videoComponent.getChildren())
        {
            if (auto* nativeView = dynamic_cast<juce::NSViewComponent*> (child))
            {
                NSView* view = (NSView*) nativeView->getView();

                if (view != nil && [view isKindOfClass: [AVPlayerView class]])
                {
                    ((AVPlayerView*) view).videoGravity = fill ? AVLayerVideoGravityResizeAspectFill
                                                               : AVLayerVideoGravityResizeAspect;
                }
            }
        }
    }

    void setCursorHidden (bool shouldHide)
    {
        // "hide" et "unhide" doivent toujours être appelés par paires : on retient l'état.
        static bool hidden = false;

        if (hidden == shouldHide)
            return;

        hidden = shouldHide;

        if (shouldHide)
            [NSCursor hide];
        else
            [NSCursor unhide];
    }
}
