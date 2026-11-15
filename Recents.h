#pragma once

#include <juce_core/juce_core.h>

// Liste des dernières vidéos ouvertes, partagée entre tous les projets :
// ~/Library/Application Support/MonStudio/VideoPlayer-recents.xml
namespace Recents
{
    constexpr int maxItems = 8;

    inline juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/MonStudio/VideoPlayer-recents.xml");
    }

    inline juce::StringArray load()
    {
        juce::StringArray list;

        if (auto xml = juce::parseXML (file()))
        {
            if (xml->hasTagName ("Recents"))
            {
                for (auto* e = xml->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                    if (e->hasTagName ("File"))
                        list.add (e->getStringAttribute ("path"));
            }
        }

        return list;
    }

    inline void save (const juce::StringArray& list)
    {
        juce::XmlElement xml ("Recents");

        for (auto& path : list)
            xml.createNewChildElement ("File")->setAttribute ("path", path);

        const auto f = file();
        f.getParentDirectory().createDirectory();
        xml.writeTo (f);
    }

    inline void add (const juce::String& path)
    {
        auto list = load();

        list.removeString (path);
        list.insert (0, path);

        while (list.size() > maxItems)
            list.remove (list.size() - 1);

        save (list);
    }

    inline void clear()
    {
        save (juce::StringArray());
    }
}
