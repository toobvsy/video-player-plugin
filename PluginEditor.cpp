#include "PluginEditor.h"

namespace
{
    constexpr int kBarHeight   = 34;    // barre du haut
    constexpr int kStripHeight = 10;    // poignée quand la barre est masquée
    constexpr int kSeekHeight  = 10;    // ligne de progression en bas

    constexpr int kMinWidth  = 400;
    constexpr int kMinHeight = 320;
    constexpr int kMaxWidth  = 2200;
    constexpr int kMaxHeight = 1400;

    constexpr int kNumSpeeds = 5;
    constexpr double kSpeeds[kNumSpeeds] = { 0.5, 0.75, 1.0, 1.5, 2.0 };

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

    // Préférences mémorisées dans le projet
    loopEnabled = processor.getSetting ("loop") == "1";
    fillMode    = processor.getSetting ("fill") == "1";
    cleanMode   = processor.getSetting ("clean") == "1";
    playSpeed   = juce::jlimit (0.25, 4.0, processor.getSetting ("speed", "1").getDoubleValue());

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
    moreButton.onClick     = [this] { showMoreMenu(); };
    addAndMakeVisible (collapseButton);
    addAndMakeVisible (moreButton);
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
            playVideo();
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

    // --- Fenêtre redimensionnable dans FL Studio (taille mémorisée dans le projet) ---
    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);

    setSize (juce::jlimit (kMinWidth,  kMaxWidth,  processor.getSetting ("width",  "800").getIntValue()),
             juce::jlimit (kMinHeight, kMaxHeight, processor.getSetting ("height", "494").getIntValue()));

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
    if (cursorHidden)
        NativeTweaks::setCursorHidden (false);

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

        tabLocal.setBounds (row.removeFromLeft (54));
        tabYouTube.setBounds (row.removeFromLeft (70));
        tabSettings.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (8);

        collapseButton.setBounds (row.removeFromRight (26).withSizeKeepingCentre (26, 26));
        row.removeFromRight (2);
        moreButton.setBounds (row.removeFromRight (26).withSizeKeepingCentre (26, 26));
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

    // Mémorise la taille de la fenêtre dans le projet
    if (getWidth() > 0 && getHeight() > 0)
    {
        processor.setSetting ("width",  juce::String (getWidth()));
        processor.setSetting ("height", juce::String (getHeight()));
    }
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

        wasPlaying = false;

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
    // On garde la même taille pour la vidéo : la fenêtre rétrécit / grandit de la hauteur de la barre
    const int before = chromeHeight();
    barVisible = shouldShow;
    const int after = chromeHeight();

    updateControlsVisibility();

    if (before != after)
        setSize (getWidth(), juce::jlimit (kMinHeight / 2, kMaxHeight, getHeight() + (after - before)));

    resized();
    repaint();
}

int VideoPlayerEditor::chromeHeight() const
{
    // Hauteur de tout ce qui n'est pas la vidéo (barre du haut + ligne de progression)
    if (! barVisible)
        return kStripHeight;

    return kBarHeight + ((currentTab == 0 && videoLoaded) ? kSeekHeight : 0);
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
    moreButton.setVisible (bar);
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
// Menu "..." : format de la fenêtre, cadrage, vitesse, boucle, mode propre, vidéos récentes
void VideoPlayerEditor::showMoreMenu()
{
    juce::PopupMenu formats;
    formats.addItem (1, fr ("16:9  (horizontal)"));
    formats.addItem (2, fr ("9:16  (vertical)"));
    formats.addItem (3, fr ("1:1  (carré)"));
    formats.addItem (4, fr ("4:5  (portrait)"));
    formats.addSeparator();
    formats.addItem (5, fr ("Comme la vidéo"), videoLoaded);

    juce::PopupMenu menu;
    menu.addSubMenu (fr ("Format de la fenêtre"), formats);

    juce::StringArray recentFiles;

    if (currentTab == 0)
    {
        juce::PopupMenu framing;
        framing.addItem (10, fr ("Ajuster (bandes noires si besoin)"), true, ! fillMode);
        framing.addItem (11, fr ("Remplir (recadrer)"), true, fillMode);
        menu.addSubMenu ("Cadrage", framing);

        const juce::String speedLabels[kNumSpeeds] = { fr ("0,5×"), fr ("0,75×"), fr ("1×  (normal)"),
                                                       fr ("1,5×"), fr ("2×") };
        juce::PopupMenu speeds;

        for (int i = 0; i < kNumSpeeds; ++i)
            speeds.addItem (20 + i, speedLabels[i], true, std::abs (playSpeed - kSpeeds[i]) < 0.001);

        menu.addSubMenu ("Vitesse", speeds);

        menu.addItem (30, "Boucle", true, loopEnabled);
        menu.addItem (31, fr ("Mode propre  (masquer le curseur)"), true, cleanMode);
        menu.addSeparator();

        for (auto& path : Recents::load())
            if (juce::File (path).existsAsFile())
                recentFiles.add (path);

        juce::PopupMenu recents;

        for (int i = 0; i < recentFiles.size(); ++i)
            recents.addItem (100 + i, juce::File (recentFiles[i]).getFileName());

        if (recentFiles.isEmpty())
        {
            recents.addItem (999, fr ("(aucune pour l'instant)"), false);
        }
        else
        {
            recents.addSeparator();
            recents.addItem (200, fr ("Effacer la liste"));
        }

        menu.addSubMenu (fr ("Vidéos récentes"), recents);
    }

    juce::Component::SafePointer<VideoPlayerEditor> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                        [safeThis, recentFiles] (int result)
                        {
                            if (safeThis != nullptr && result != 0)
                                safeThis->handleMenuResult (result, recentFiles);
                        });
}

void VideoPlayerEditor::handleMenuResult (int result, const juce::StringArray& recentFiles)
{
    switch (result)
    {
        case 1:  applyWindowFormat (800, 450); return;     // 16:9
        case 2:  applyWindowFormat (405, 720); return;     // 9:16
        case 3:  applyWindowFormat (600, 600); return;     // 1:1
        case 4:  applyWindowFormat (520, 650); return;     // 4:5

        case 5:     // même proportions que la vidéo
        {
            const auto native = video.getVideoNativeSize();

            if (native.getWidth() > 0 && native.getHeight() > 0)
            {
                const double scale = juce::jmin (800.0 / native.getWidth(), 720.0 / native.getHeight());
                int w = (int) std::round (native.getWidth() * scale);
                int h = (int) std::round (native.getHeight() * scale);

                if (w < kMinWidth)
                {
                    h = (int) std::round (h * (double) kMinWidth / (double) w);
                    w = kMinWidth;
                }

                applyWindowFormat (w, h);
            }
            return;
        }

        case 10: setFillMode (false); return;
        case 11: setFillMode (true);  return;

        case 30:
            loopEnabled = ! loopEnabled;
            processor.setSetting ("loop", loopEnabled ? "1" : "0");
            return;

        case 31:
            cleanMode = ! cleanMode;
            processor.setSetting ("clean", cleanMode ? "1" : "0");
            return;

        case 200:
            Recents::clear();
            return;

        default:
            break;
    }

    if (result >= 20 && result < 20 + kNumSpeeds)
    {
        setPlaybackSpeed (kSpeeds[result - 20]);
    }
    else if (result >= 100 && result < 100 + recentFiles.size())
    {
        loadVideo (juce::File (recentFiles[result - 100]), true);
    }
}

void VideoPlayerEditor::applyWindowFormat (int contentWidth, int contentHeight)
{
    // La taille donnée est celle de la zone vidéo : on y ajoute la barre et la ligne de progression
    setSize (juce::jlimit (kMinWidth, kMaxWidth, contentWidth),
             juce::jlimit (kMinHeight, kMaxHeight, contentHeight + chromeHeight()));
}

//==============================================================================
void VideoPlayerEditor::playVideo()
{
    video.play();

    // Sur Mac, "play" remet la vitesse à 1× : on la réapplique
    if (std::abs (playSpeed - 1.0) > 0.001)
        video.setPlaySpeed (playSpeed);
}

void VideoPlayerEditor::setPlaybackSpeed (double newSpeed)
{
    playSpeed = newSpeed;
    processor.setSetting ("speed", juce::String (playSpeed));

    // Seulement si la vidéo joue : changer la vitesse d'une vidéo en pause la relancerait
    if (videoLoaded && video.isPlaying())
        video.setPlaySpeed (playSpeed);
}

void VideoPlayerEditor::setFillMode (bool shouldFill)
{
    fillMode = shouldFill;
    processor.setSetting ("fill", fillMode ? "1" : "0");
    NativeTweaks::setVideoFillsFrame (video, fillMode);
}

void VideoPlayerEditor::updateCursorHiding()
{
    // Mode propre : le curseur disparaît tant qu'il est au-dessus de la vidéo
    const bool wantHidden = cleanMode && videoLoaded && currentTab == 0 && video.isShowing()
                            && ! fileDialogOpen
                            && juce::Component::getCurrentlyModalComponent() == nullptr   // pas pendant un menu
                            && juce::Process::isForegroundProcess()
                            && video.getScreenBounds().contains (juce::Desktop::getMousePosition());

    if (wantHidden != cursorHidden)
    {
        cursorHidden = wantHidden;
        NativeTweaks::setCursorHidden (wantHidden);
    }
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

    fileDialogOpen = true;

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr)
                                  return;

                              safeThis->fileDialogOpen = false;

                              auto file = fc.getResult();
                              if (file.existsAsFile())
                                  safeThis->loadVideo (file, true);
                          });
}

void VideoPlayerEditor::loadVideo (const juce::File& file, bool userInitiated)
{
    auto result = video.load (file);
    wasPlaying = false;

    if (result.wasOk())
    {
        errorMessage = {};
        videoLoaded = true;
        processor.setVideoPath (file.getFullPathName());

        if (userInitiated)
            Recents::add (file.getFullPathName());

        NativeTweaks::setVideoFillsFrame (video, fillMode);
        video.setVisible (currentTab == 0);

        if (userInitiated)
            playVideo();
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

    updateCursorHiding();

    if (! videoLoaded || currentTab == 1)
        return;

    const bool playing = video.isPlaying();

    // Boucle : la vidéo vient de s'arrêter tout près de la fin, on repart du début
    if (loopEnabled && wasPlaying && ! playing)
    {
        const double duration = video.getVideoDuration();

        if (duration > 0.0 && video.getPlayPosition() >= duration - 0.35)
        {
            video.setPlayPosition (0.0);
            playVideo();
        }
    }

    wasPlaying = playing;

    if (currentTab != 0)
        return;

    playButton.setIcon (video.isPlaying() ? IconButton::Icon::pause : IconButton::Icon::play);

    const double duration = video.getVideoDuration();

    if (duration > 0.0)
        seekBar.setProgress (video.getPlayPosition() / duration);
}
