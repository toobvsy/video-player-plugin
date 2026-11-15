#include "PluginEditor.h"

namespace
{
    constexpr int kTopBarHeight    = 52;
    constexpr int kBottomBarHeight = 56;

    const char* const kYouTubeHome = "https://www.youtube.com/";

    // YouTube sert la bonne version du site à un navigateur "classique"
    const char* const kUserAgent =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/17.5 Safari/605.1.15";

    juce::String formatTime (double seconds)
    {
        const int total = (int) juce::jmax (0.0, seconds);
        return juce::String::formatted ("%d:%02d", total / 60, total % 60);
    }

    bool isVideoFile (const juce::String& path)
    {
        return juce::File (path).hasFileExtension ("mp4;mov;m4v");
    }
}

//==============================================================================
YouTubeBrowser::YouTubeBrowser()
    : juce::WebBrowserComponent (juce::WebBrowserComponent::Options{}
                                     .withKeepPageLoadedWhenBrowserIsHidden()
                                     .withUserAgent (kUserAgent))
{
}

void YouTubeBrowser::pauseMedia()
{
    evaluateJavascript ("document.querySelectorAll('video,audio').forEach(function(m){ m.pause(); });");
}

bool YouTubeBrowser::pageAboutToLoad (const juce::String&)
{
    return true;
}

void YouTubeBrowser::pageFinishedLoading (const juce::String& url)
{
    if (onPageLoaded)
        onPageLoaded (url);
}

void YouTubeBrowser::newWindowAttemptingToLoad (const juce::String& newURL)
{
    // Les liens qui voudraient s'ouvrir dans une nouvelle fenêtre s'ouvrent ici
    goToURL (newURL);
}

//==============================================================================
VideoPlayerEditor::VideoPlayerEditor (VideoPlayerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    // --- Onglets ---
    tabLocal.setButtonText (fr ("Vidéo locale"));
    tabYouTube.setButtonText ("YouTube");

    for (auto* b : { &tabLocal, &tabYouTube })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (*b);
    }

    tabLocal.onClick   = [this] { showTab (0); };
    tabYouTube.onClick = [this] { showTab (1); };

    // --- Onglet vidéo locale ---
    addChildComponent (video);      // caché tant qu'aucune vidéo n'est chargée

    openButton.setButtonText (fr ("Ouvrir un fichier"));
    openButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    openButton.onClick = [this] { openFileDialog(); };
    addAndMakeVisible (openButton);

    playButton.onClick = [this]
    {
        if (! videoLoaded)
        {
            openFileDialog();
            return;
        }

        if (video.isPlaying())
            video.stop();       // "stop" = pause dans JUCE
        else
            video.play();
    };
    addAndMakeVisible (playButton);

    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    positionSlider.setRange (0.0, 1.0);
    positionSlider.onValueChange = [this]
    {
        // Uniquement quand l'utilisateur fait glisser le curseur à la souris
        if (positionSlider.isMouseButtonDown())
            video.setPlayPosition (positionSlider.getValue());
    };
    addAndMakeVisible (positionSlider);

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    volumeSlider.setRange (0.0, 1.0);
    volumeSlider.setValue (0.7, juce::dontSendNotification);
    volumeSlider.onValueChange = [this]
    {
        video.setAudioVolume ((float) volumeSlider.getValue());
    };
    addAndMakeVisible (volumeSlider);

    timeLabel.setJustificationType (juce::Justification::centredRight);
    timeLabel.setText ("0:00 / 0:00", juce::dontSendNotification);
    addAndMakeVisible (timeLabel);

    // --- Onglet YouTube ---
    backButton.onClick    = [this] { if (browser != nullptr) browser->goBack(); };
    forwardButton.onClick = [this] { if (browser != nullptr) browser->goForward(); };
    reloadButton.onClick  = [this] { if (browser != nullptr) browser->refresh(); };
    homeButton.onClick    = [this] { if (browser != nullptr) browser->goToURL (kYouTubeHome); };

    for (auto* b : { &backButton, &forwardButton, &reloadButton, &homeButton })
        addAndMakeVisible (*b);

    searchBox.setTextToShowWhenEmpty (fr ("Rechercher sur YouTube ou coller un lien…"), Theme::textDim);
    searchBox.setSelectAllWhenFocused (true);
    searchBox.onReturnKey = [this] { submitSearch(); };
    addAndMakeVisible (searchBox);

    // --- Fenêtre redimensionnable dans FL Studio ---
    setResizable (true, true);
    setResizeLimits (520, 340, 2200, 1400);
    setSize (960, 600);

    // Recharge la dernière vidéo si le projet en avait une (sans la lancer toute seule)
    const juce::String savedPath = processor.getVideoPath();
    if (savedPath.isNotEmpty())
    {
        juce::File f (savedPath);
        if (f.existsAsFile())
            loadVideo (f, false);
    }

    showTab (processor.getCurrentTab() == 1 ? 1 : 0);

    startTimerHz (10);
}

VideoPlayerEditor::~VideoPlayerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void VideoPlayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    g.setColour (Theme::bar);
    g.fillRect (0, 0, getWidth(), kTopBarHeight);

    if (currentTab == 0 && ! videoLoaded)
    {
        auto area = getLocalBounds().withTrimmedTop (kTopBarHeight).withTrimmedBottom (kBottomBarHeight);

        // Cadre en pointillés façon "zone de dépôt"
        auto box = area.toFloat().withSizeKeepingCentre (juce::jmin (480.0f, area.getWidth() - 40.0f),
                                                         juce::jmin (220.0f, area.getHeight() - 40.0f));
        g.setColour (Theme::panelHi);
        juce::Path outline;
        outline.addRoundedRectangle (box, 16.0f);
        juce::Path dashed;
        const float dashes[] = { 6.0f, 6.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
        g.fillPath (dashed);

        g.setColour (Theme::text);
        g.setFont (18.0f);
        g.drawFittedText (fr ("Glisse une vidéo ici"), box.toNearestInt().withTrimmedBottom ((int) box.getHeight() / 2),
                          juce::Justification::centredBottom, 1);

        g.setColour (Theme::textDim);
        g.setFont (14.0f);

        const juce::String hint = errorMessage.isNotEmpty()
                                      ? errorMessage
                                      : fr ("ou clique sur « Ouvrir un fichier »\n(.mp4, .mov, .m4v)");

        g.drawFittedText (hint, box.toNearestInt().withTrimmedTop ((int) box.getHeight() / 2 + 8).reduced (16, 0),
                          juce::Justification::centredTop, 3);
    }
}

void VideoPlayerEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (kTopBarHeight).reduced (12, 10);

    tabLocal.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (6);
    tabYouTube.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (14);

    if (currentTab == 1)
    {
        backButton.setBounds    (top.removeFromLeft (32));
        top.removeFromLeft (2);
        forwardButton.setBounds (top.removeFromLeft (32));
        top.removeFromLeft (2);
        reloadButton.setBounds  (top.removeFromLeft (32));
        top.removeFromLeft (2);
        homeButton.setBounds    (top.removeFromLeft (32));
        top.removeFromLeft (10);
        searchBox.setBounds (top);

        if (browser != nullptr)
            browser->setBounds (area);
    }
    else
    {
        openButton.setBounds (top.removeFromRight (150));

        auto bottom = area.removeFromBottom (kBottomBarHeight).reduced (14, 10);
        playButton.setBounds (bottom.removeFromLeft (36));
        bottom.removeFromLeft (12);
        volumeSlider.setBounds (bottom.removeFromRight (110));
        bottom.removeFromRight (12);
        timeLabel.setBounds (bottom.removeFromRight (100));
        bottom.removeFromRight (8);
        positionSlider.setBounds (bottom);

        video.setBounds (area);
    }
}

//==============================================================================
void VideoPlayerEditor::showTab (int tab)
{
    currentTab = tab;
    processor.setCurrentTab (tab);

    tabLocal.setToggleState (tab == 0, juce::dontSendNotification);
    tabYouTube.setToggleState (tab == 1, juce::dontSendNotification);

    if (tab == 1)
    {
        if (videoLoaded)
            video.stop();           // on met la vidéo locale en pause

        ensureBrowser();
        browser->setVisible (true);
    }
    else if (browser != nullptr)
    {
        browser->pauseMedia();      // et YouTube aussi
        browser->setVisible (false);
    }

    video.setVisible (tab == 0 && videoLoaded);

    updateControlsVisibility();
    resized();
    repaint();
}

void VideoPlayerEditor::updateControlsVisibility()
{
    const bool yt = (currentTab == 1);

    backButton.setVisible (yt);
    forwardButton.setVisible (yt);
    reloadButton.setVisible (yt);
    homeButton.setVisible (yt);
    searchBox.setVisible (yt);

    openButton.setVisible (! yt);
    playButton.setVisible (! yt);
    positionSlider.setVisible (! yt);
    volumeSlider.setVisible (! yt);
    timeLabel.setVisible (! yt);
}

void VideoPlayerEditor::ensureBrowser()
{
    if (browser != nullptr)
        return;

    browser = std::make_unique<YouTubeBrowser>();

    browser->onPageLoaded = [this] (const juce::String& url)
    {
        if (url.startsWith ("http"))
            processor.setYouTubeUrl (url);
    };

    addChildComponent (*browser);
    resized();

    const juce::String saved = processor.getYouTubeUrl();
    browser->goToURL (saved.startsWith ("http") ? saved : juce::String (kYouTubeHome));
}

void VideoPlayerEditor::submitSearch()
{
    const auto text = searchBox.getText().trim();

    if (text.isEmpty() || browser == nullptr)
        return;

    juce::String url;

    if (text.startsWithIgnoreCase ("http://") || text.startsWithIgnoreCase ("https://"))
        url = text;
    else if (text.startsWithIgnoreCase ("youtube.com") || text.startsWithIgnoreCase ("www.youtube.com")
             || text.startsWithIgnoreCase ("youtu.be"))
        url = "https://" + text;
    else
        url = "https://www.youtube.com/results?search_query=" + juce::URL::addEscapeChars (text, true);

    browser->goToURL (url);
}

//==============================================================================
void VideoPlayerEditor::openFileDialog()
{
    chooser = std::make_unique<juce::FileChooser> (
        fr ("Choisir une vidéo"),
        juce::File::getSpecialLocation (juce::File::userMoviesDirectory),
        "*.mp4;*.mov;*.m4v");

    // SafePointer : évite un plantage si la fenêtre est fermée pendant que le sélecteur est ouvert
    juce::Component::SafePointer<VideoPlayerEditor> safeThis (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (safeThis != nullptr && file.existsAsFile())
                                  safeThis->loadVideo (file, true);
                          });
}

void VideoPlayerEditor::loadVideo (const juce::File& file, bool autoPlay)
{
    auto result = video.load (file);

    if (result.wasOk())
    {
        errorMessage = {};
        videoLoaded = true;
        processor.setVideoPath (file.getFullPathName());

        video.setAudioVolume ((float) volumeSlider.getValue());
        video.setVisible (currentTab == 0);

        if (autoPlay)
            video.play();
    }
    else
    {
        videoLoaded = false;
        video.setVisible (false);
        errorMessage = fr ("Impossible de lire cette vidéo : ") + result.getErrorMessage();
    }

    repaint();
}

bool VideoPlayerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isVideoFile (f))
            return true;

    return false;
}

void VideoPlayerEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        if (isVideoFile (f))
        {
            showTab (0);
            loadVideo (juce::File (f), true);
            return;
        }
    }
}

void VideoPlayerEditor::timerCallback()
{
    if (currentTab != 0 || ! videoLoaded)
        return;

    playButton.setIcon (video.isPlaying() ? IconButton::Icon::pause : IconButton::Icon::play);

    const double duration = video.getVideoDuration();
    if (duration > 0.0)
    {
        if (positionSlider.getMaximum() != duration)
            positionSlider.setRange (0.0, duration);

        if (! positionSlider.isMouseButtonDown())
            positionSlider.setValue (video.getPlayPosition(), juce::dontSendNotification);

        timeLabel.setText (formatTime (video.getPlayPosition()) + " / " + formatTime (duration),
                           juce::dontSendNotification);
    }
}
