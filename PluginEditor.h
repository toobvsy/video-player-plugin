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
                          private juce::ChangeListener,
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
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void showTab (int tab);                 // 0 = vidéo, 1 = YouTube, 2 = couleurs
    void setBarVisible (bool shouldShow);
    void updateControlsVisibility();
    void ensureBrowser();
    void submitSearch();
    void openFileDialog();
    void loadVideo (const juce::File& file, bool autoPlay);

    // Couleurs
    juce::Colour roleColour (int role) const;
    void selectRole (int role);
    void applyPreset (juce::Colour bg, juce::Colour accent, juce::Colour text);
    void applyTheme();

    VideoPlayerProcessor& processor;
    PluginLookAndFeel lnf;      // doit rester déclaré en premier

    int currentTab = 0;
    bool videoLoaded = false;
    bool barVisible = true;
    juce::String errorMessage;

    juce::Rectangle<int> barBounds, contentArea, presetsHeaderBounds;

    // Barre du haut
    juce::TextButton tabLocal, tabYouTube, tabSettings;
    IconButton collapseButton { IconButton::Icon::collapse };   // masque la barre
    IconButton expandButton   { IconButton::Icon::expand };     // la ré-affiche

    // Onglet "vidéo locale"
    juce::VideoComponent video { false };   // false = pas de contrôles natifs, on fait les nôtres
    juce::TextButton openButton;
    IconButton playButton { IconButton::Icon::play };
    SeekBar seekBar;

    // Onglet YouTube
    IconButton backButton { IconButton::Icon::back };
    juce::TextEditor searchBox;
    std::unique_ptr<YouTubeBrowser> browser;    // créé au premier passage sur l'onglet

    // Onglet "Couleurs"
    SwatchButton swatchBg     { fr ("Fond") };
    SwatchButton swatchAccent { "Accent" };
    SwatchButton swatchText   { "Texte" };
    juce::ColourSelector selector { juce::ColourSelector::showColourAtTop
                                    | juce::ColourSelector::showSliders
                                    | juce::ColourSelector::showColourspace };
    juce::TextButton presetBW, presetLight, presetNight;
    int editingRole = 1;        // 0 = fond, 1 = accent, 2 = texte
    int saveCountdown = 0;      // enregistre les couleurs un instant après le dernier changement

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoPlayerEditor)
};
