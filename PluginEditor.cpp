#include "PluginEditor.h"

namespace
{
    constexpr int kBarHeight   = 34;    // barre du haut
    constexpr int kStripHeight = 10;    // poignée quand la barre est masquée
    constexpr int kSeekHeight  = 10;    // ligne de progression en bas

    const char* const kYouTubeHome = "https://www.youtube.com/";

    // YouTube sert la bonne version du site à un navigateur "classique"
    const char* const kUserAgent =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/17.5 Safari/605.1.15";

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
    // Récupère les couleurs choisies (partagées entre tous les projets)
    Theme::load();
    lnf.refreshColours();
    setLookAndFeel (&lnf);

    // --- Onglets ---
    tabLocal.setButtonText (fr ("Vidéo"));
    tabYouTube.setButtonText ("YouTube");
    tabSettings.setButtonText ("Couleurs");

    for (auto* b : { &tabLocal, &tabYouTube, &tabSettings })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (*b);
    }

    tabLocal.onClick    = [this] { showTab (0); };
    tabYouTube.onClick  = [this] { showTab (1); };
    tabSettings.onClick = [this] { showTab (2); };

    collapseButton.onClick = [this] { setBarVisible (false); };
    expandButton.onClick   = [this] { setBarVisible (true); };
    addAndMakeVisible (collapseButton);
    addChildComponent (expandButton);

    // --- Onglet vidéo locale ---
    addChildComponent (video);      // caché tant qu'aucune vidéo n'est chargée

    openButton.setButtonText ("Ouvrir");
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

    seekBar.onSeek = [this] (double proportion)
    {
        const double duration = video.getVideoDuration();

        if (duration > 0.0)
            video.setPlayPosition (proportion * duration);
    };
    addChildComponent (seekBar);

    // --- Onglet YouTube ---
    backButton.onClick = [this] { if (browser != nullptr) browser->goBack(); };
    addAndMakeVisible (backButton);

    searchBox.setSelectAllWhenFocused (true);
    searchBox.onReturnKey = [this] { submitSearch(); };
    addAndMakeVisible (searchBox);

    // --- Onglet Couleurs ---
    for (auto* s : { &swatchBg, &swatchAccent, &swatchText })
        addChildComponent (*s);

    swatchBg.onClick     = [this] { selectRole (0); };
    swatchAccent.onClick = [this] { selectRole (1); };
    swatchText.onClick   = [this] { selectRole (2); };
    swatchAccent.setToggleState (true, juce::dontSendNotification);

    presetBW.setButtonText ("Noir et blanc");
    presetLight.setButtonText ("Clair");
    presetNight.setButtonText ("Minuit");

    presetBW.onClick = [this]
    {
        applyPreset (juce::Colour (0xff000000), juce::Colour (0xffffffff), juce::Colour (0xfff2f2f2));
    };
    presetLight.onClick = [this]
    {
        applyPreset (juce::Colour (0xfff4f4f5), juce::Colour (0xff111111), juce::Colour (0xff111111));
    };
    presetNight.onClick = [this]
    {
        applyPreset (juce::Colour (0xff0b1020), juce::Colour (0xff5eb8ff), juce::Colour (0xffe8eefc));
    };

    for (auto* b : { &presetBW, &presetLight, &presetNight })
    {
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addChildComponent (*b);
    }

    addChildComponent (selector);
    selector.setCurrentColour (roleColour (editingRole), juce::dontSendNotification);
    selector.addChangeListener (this);

    applyTheme();

    // --- Fenêtre redimensionnable dans FL Studio ---
    setResizable (true, true);
    setResizeLimits (440, 320, 2200, 1400);
    setSize (800, 492);             // vidéo 16:9 de 800 x 450 + barre + ligne de progression

    // Recharge la dernière vidéo si le projet en avait une (sans la lancer toute seule)
    const juce::String savedPath = processor.getVideoPath();
    if (savedPath.isNotEmpty())
    {
        juce::File f (savedPath);
        if (f.existsAsFile())
            loadVideo (f, false);
    }

    showTab (processor.getCurrentTab() == 1 ? 1 : 0);

    startTimerHz (20);
}

VideoPlayerEditor::~VideoPlayerEditor()
{
    if (saveCountdown > 0)
        Theme::save();

    selector.removeChangeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void VideoPlayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg());

    if (barVisible)
    {
        g.setColour (Theme::bar());
        g.fillRect (barBounds);

        g.setColour (Theme::panelHi());
        g.fillRect (barBounds.getX(), barBounds.getBottom() - 1, barBounds.getWidth(), 1);
    }

    if (currentTab == 2)
    {
        g.setColour (Theme::textDim());
        g.setFont (12.0f);
        g.drawText (fr ("THÈMES"), presetsHeaderBounds, juce::Justification::centredLeft, false);
    }

    if (currentTab == 0 && ! videoLoaded)
    {
        auto box = contentArea.toFloat().withSizeKeepingCentre (juce::jmin (440.0f, (float) contentArea.getWidth() - 40.0f),
                                                                juce::jmin (200.0f, (float) contentArea.getHeight() - 40.0f));

        // Cadre en pointillés façon "zone de dépôt"
        juce::Path outline;
        outline.addRoundedRectangle (box, 16.0f);
        juce::Path dashed;
        const float dashes[] = { 6.0f, 6.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (Theme::panelHi());
        g.fillPath (dashed);

        auto inner = box.toNearestInt();

        g.setColour (Theme::text());
        g.setFont (18.0f);
        g.drawFittedText (fr ("Glisse une vidéo ici"), inner.withTrimmedBottom (inner.getHeight() / 2),
                          juce::Justification::centredBottom, 1);

        g.setColour (Theme::textDim());
        g.setFont (14.0f);

        const juce::String hint = errorMessage.isNotEmpty()
                                      ? errorMessage
                                      : fr ("ou clique sur « Ouvrir »\n(.mp4, .mov, .m4v)");

        g.drawFittedText (hint, inner.withTrimmedTop (inner.getHeight() / 2 + 8).reduced (16, 0),
                          juce::Justification::centredTop, 3);
    }
}

void VideoPlayerEditor::resized()
{
    auto area = getLocalBounds();

    if (! barVisible)
    {
        // Barre masquée : seulement une fine poignée, tout le reste est pour le contenu
        barBounds = {};
        expandButton.setBounds (area.removeFromTop (kStripHeight));
    }
    else
    {
        barBounds = area.removeFromTop (kBarHeight);

        auto row = barBounds.reduced (6, 0);

        tabLocal.setBounds (row.removeFromLeft (62));
        tabYouTube.setBounds (row.removeFromLeft (80));
        tabSettings.setBounds (row.removeFromLeft (78));
        row.removeFromLeft (8);

        collapseButton.setBounds (row.removeFromRight (26).withSizeKeepingCentre (26, 26));
        row.removeFromRight (6);

        auto inner = row.reduced (0, 4);

        if (currentTab == 1)
        {
            backButton.setBounds (inner.removeFromLeft (26));
            inner.removeFromLeft (6);
            searchBox.setBounds (inner);
        }
        else if (currentTab == 0)
        {
            openButton.setBounds (inner.removeFromRight (76));
            inner.removeFromRight (6);
            playButton.setBounds (inner.removeFromRight (26));

            if (videoLoaded)
                seekBar.setBounds (area.removeFromBottom (kSeekHeight));
        }
    }

    contentArea = area;
    video.setBounds (area);

    if (browser != nullptr)
        browser->setBounds (area);

    // Onglet Couleurs : à gauche les pastilles et les thèmes, à droite le sélecteur
    auto panel = area.reduced (14);
    auto left = panel.removeFromLeft (170);

    swatchBg.setBounds (left.removeFromTop (34));
    left.removeFromTop (6);
    swatchAccent.setBounds (left.removeFromTop (34));
    left.removeFromTop (6);
    swatchText.setBounds (left.removeFromTop (34));
    left.removeFromTop (14);

    presetsHeaderBounds = left.removeFromTop (18);
    presetBW.setBounds (left.removeFromTop (28));
    left.removeFromTop (6);
    presetLight.setBounds (left.removeFromTop (28));
    left.removeFromTop (6);
    presetNight.setBounds (left.removeFromTop (28));

    panel.removeFromLeft (14);
    selector.setBounds (panel);
}

//==============================================================================
void VideoPlayerEditor::showTab (int tab)
{
    currentTab = tab;

    if (tab != 2)
        processor.setCurrentTab (tab);

    tabLocal.setToggleState (tab == 0, juce::dontSendNotification);
    tabYouTube.setToggleState (tab == 1, juce::dontSendNotification);
    tabSettings.setToggleState (tab == 2, juce::dontSendNotification);

    if (tab == 1)
    {
        if (videoLoaded)
            video.stop();           // on met la vidéo locale en pause

        ensureBrowser();
        browser->setVisible (true);
    }
    else if (tab == 0)
    {
        if (browser != nullptr)
        {
            browser->pauseMedia();  // et YouTube aussi
            browser->setVisible (false);
        }
    }
    else if (browser != nullptr)
    {
        browser->setVisible (false);    // l'onglet Couleurs ne coupe pas le son
    }

    video.setVisible (tab == 0 && videoLoaded);

    updateControlsVisibility();
    resized();
    repaint();
}

void VideoPlayerEditor::setBarVisible (bool shouldShow)
{
    barVisible = shouldShow;

    updateControlsVisibility();
    resized();
    repaint();
}

void VideoPlayerEditor::updateControlsVisibility()
{
    const bool bar   = barVisible;
    const bool local = (currentTab == 0);
    const bool yt    = (currentTab == 1);
    const bool st    = (currentTab == 2);

    tabLocal.setVisible (bar);
    tabYouTube.setVisible (bar);
    tabSettings.setVisible (bar);
    collapseButton.setVisible (bar);
    expandButton.setVisible (! bar);

    backButton.setVisible (bar && yt);
    searchBox.setVisible (bar && yt);

    openButton.setVisible (bar && local);
    playButton.setVisible (bar && local);
    seekBar.setVisible (bar && local && videoLoaded);

    swatchBg.setVisible (st);
    swatchAccent.setVisible (st);
    swatchText.setVisible (st);
    selector.setVisible (st);
    presetBW.setVisible (st);
    presetLight.setVisible (st);
    presetNight.setVisible (st);
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
juce::Colour VideoPlayerEditor::roleColour (int role) const
{
    auto& pal = Theme::palette();

    if (role == 0) return pal.baseBg;
    if (role == 1) return pal.baseAccent;
    return pal.baseText;
}

void VideoPlayerEditor::selectRole (int role)
{
    editingRole = role;
    selector.setCurrentColour (roleColour (role), juce::dontSendNotification);
}

void VideoPlayerEditor::applyPreset (juce::Colour newBg, juce::Colour newAccent, juce::Colour newText)
{
    Theme::setColours (newBg, newAccent, newText);
    Theme::save();

    selector.setCurrentColour (roleColour (editingRole), juce::dontSendNotification);
    applyTheme();
}

void VideoPlayerEditor::applyTheme()
{
    lnf.refreshColours();

    searchBox.setTextToShowWhenEmpty (fr ("Rechercher sur YouTube ou coller un lien…"), Theme::textDim());

    swatchBg.setSwatchColour (Theme::palette().baseBg);
    swatchAccent.setSwatchColour (Theme::palette().baseAccent);
    swatchText.setSwatchColour (Theme::palette().baseText);

    sendLookAndFeelChange();
    repaint();
}

void VideoPlayerEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Le sélecteur de couleur a bougé : on applique la couleur à l'élément en cours d'édition
    const auto c = selector.getCurrentColour();
    auto& pal = Theme::palette();

    Theme::setColours (editingRole == 0 ? c : pal.baseBg,
                       editingRole == 1 ? c : pal.baseAccent,
                       editingRole == 2 ? c : pal.baseText);

    saveCountdown = 20;     // enregistré 1 seconde après le dernier mouvement
    applyTheme();
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

    updateControlsVisibility();
    resized();
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
    if (saveCountdown > 0 && --saveCountdown == 0)
        Theme::save();

    if (currentTab != 0 || ! videoLoaded)
        return;

    playButton.setIcon (video.isPlaying() ? IconButton::Icon::pause : IconButton::Icon::play);

    const double duration = video.getVideoDuration();

    if (duration > 0.0)
        seekBar.setProgress (video.getPlayPosition() / duration);
}
