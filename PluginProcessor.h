#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Le "processeur" = la partie audio du plugin.
// Ici, il ne fait RIEN au son : il laisse passer l'audio tel quel.
// Le plugin sert uniquement d'hôte pour la fenêtre vidéo / YouTube.
class VideoPlayerProcessor : public juce::AudioProcessor
{
public:
    VideoPlayerProcessor();
    ~VideoPlayerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Video Player"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // Sauvegarde / chargement dans le projet FL Studio (.flp)
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Ce qui est mémorisé dans le projet. Accès protégé par un verrou, car FL Studio
    // peut lire l'état depuis un autre "fil" que celui de la fenêtre.
    juce::String getVideoPath() const;
    void setVideoPath (const juce::String& path);

    juce::String getYouTubeUrl() const;
    void setYouTubeUrl (const juce::String& url);

    int getCurrentTab() const;       // 0 = vidéo locale, 1 = YouTube
    void setCurrentTab (int tab);

    // Réglages divers mémorisés dans le projet (taille de la fenêtre, boucle, vitesse...)
    juce::String getSetting (const juce::String& key, const juce::String& defaultValue = {}) const;
    void setSetting (const juce::String& key, const juce::String& value);

private:
    mutable juce::CriticalSection stateLock;
    juce::String videoPath;
    juce::String youtubeUrl;
    int currentTab = 0;
    juce::StringPairArray settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoPlayerProcessor)
};
