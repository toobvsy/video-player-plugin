#include "PluginProcessor.h"
#include "PluginEditor.h"

VideoPlayerProcessor::VideoPlayerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void VideoPlayerProcessor::prepareToPlay (double, int) {}
void VideoPlayerProcessor::releaseResources() {}

bool VideoPlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void VideoPlayerProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Volontairement vide : l'audio entrant ressort sans modification.
}

juce::AudioProcessorEditor* VideoPlayerProcessor::createEditor()
{
    return new VideoPlayerEditor (*this);
}

//==============================================================================
juce::String VideoPlayerProcessor::getVideoPath() const
{
    const juce::ScopedLock sl (stateLock);
    return videoPath;
}

void VideoPlayerProcessor::setVideoPath (const juce::String& path)
{
    const juce::ScopedLock sl (stateLock);
    videoPath = path;
}

juce::String VideoPlayerProcessor::getYouTubeUrl() const
{
    const juce::ScopedLock sl (stateLock);
    return youtubeUrl;
}

void VideoPlayerProcessor::setYouTubeUrl (const juce::String& url)
{
    const juce::ScopedLock sl (stateLock);
    youtubeUrl = url;
}

int VideoPlayerProcessor::getCurrentTab() const
{
    const juce::ScopedLock sl (stateLock);
    return currentTab;
}

void VideoPlayerProcessor::setCurrentTab (int tab)
{
    const juce::ScopedLock sl (stateLock);
    currentTab = tab;
}

juce::String VideoPlayerProcessor::getSetting (const juce::String& key, const juce::String& defaultValue) const
{
    const juce::ScopedLock sl (stateLock);
    return settings.getValue (key, defaultValue);
}

void VideoPlayerProcessor::setSetting (const juce::String& key, const juce::String& value)
{
    const juce::ScopedLock sl (stateLock);
    settings.set (key, value);
}

//==============================================================================
void VideoPlayerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("VideoPlayerState");

    {
        const juce::ScopedLock sl (stateLock);

        for (auto& key : settings.getAllKeys())
            xml.setAttribute (key, settings[key]);

        xml.setAttribute ("videoPath", videoPath);
        xml.setAttribute ("youtubeUrl", youtubeUrl);
        xml.setAttribute ("tab", currentTab);
    }

    copyXmlToBinary (xml, destData);
}

void VideoPlayerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("VideoPlayerState"))
        {
            const juce::ScopedLock sl (stateLock);

            videoPath  = xml->getStringAttribute ("videoPath");
            youtubeUrl = xml->getStringAttribute ("youtubeUrl");
            currentTab = xml->getIntAttribute ("tab", 0);

            for (int i = 0; i < xml->getNumAttributes(); ++i)
                settings.set (xml->getAttributeName (i), xml->getAttributeValue (i));
        }
        return;
    }

    // Ancien format (version 0.1) : seulement le chemin de la vidéo
    juce::MemoryInputStream stream (data, (size_t) sizeInBytes, false);
    const juce::ScopedLock sl (stateLock);
    videoPath = stream.readString();
}

// Point d'entrée : crée une instance du plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VideoPlayerProcessor();
}
