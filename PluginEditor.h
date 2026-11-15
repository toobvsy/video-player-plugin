#pragma once

#include "PluginProcessor.h"
#include "Theme.h"
#include <juce_video/juce_video.h>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
// Le navigateur intégré qui affiche YouTube.
class YouTubeBrowser : public juce::WebBrowserComponent
{
public:
    YouTubeBrowser();

    // Appelé quand une page a fini de charger (sert à mémoriser l'adresse dans le projet)
    std::function<void (const juce::String&)> onPageLoaded;

    // Met en pause toute vidéo en cours (quand on change d'onglet)
    void pauseMedia();

    bool pageAboutToLoad (const juce::String& newURL) override;
    void pageFinishedLoading (const juce::String& url) override;
    void newWindowAttemptingToLoad (const juce::String& newURL) override;
};

//==============================================================================
// L'"éditeur" = la fenêtre du plugin (ce que tu vois dans FL Studio).
class VideoPlayerEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit VideoPlayerEditor (VideoPlayerProcessor&);
    ~VideoPlayerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;

    void showTab (int tab);
    void updateControlsVisibility();
    void ensureBrowser();
    void submitSearch();
    void openFileDialog();
    void loadVideo (const juce::File& file, bool autoPlay);

    VideoPlayerProcessor& processor;
    PluginLookAndFeel lnf;      // doit rester déclaré en premier

    int currentTab = 0;
    bool videoLoaded = false;
    juce::String errorMessage;

    // Barre du haut
    juce::TextButton tabLocal, tabYouTube;

    // Onglet "vidéo locale"
    juce::VideoComponent video { false };   // false = pas de contrôles natifs, on fait les nôtres
    juce::TextButton openButton;
    IconButton playButton { IconButton::Icon::play };
    juce::Slider positionSlider, volumeSlider;
    juce::Label timeLabel;

    // Onglet YouTube
    IconButton backButton    { IconButton::Icon::back };
    IconButton forwardButton { IconButton::Icon::forward };
    IconButton reloadButton  { IconButton::Icon::reload };
    IconButton homeButton    { IconButton::Icon::home };
    juce::TextEditor searchBox;
    std::unique_ptr<YouTubeBrowser> browser;    // créé au premier passage sur l'onglet

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoPlayerEditor)
};
