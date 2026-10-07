#include "PresetManager.h"
#include "PresetData.h"

namespace rm
{
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, SequenceData& seq, ChordData& chord)
    : apvts (state), sequence (seq), chords (chord)
{
    loadFactoryPresets();
    rescanUserPresets();
    favoritesFile = getDefaultFavoritesFile();
    loadFavorites();
}

const juce::StringArray& PresetManager::categoryOrder()
{
    static const juce::StringArray order { "Bass", "Lead", "Pad", "Keys", "Brass", "Strings", "Pluck",
                                           "Sequence", "FX", "Split", "User" };
    return order;
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        .getChildFile ("RecklessMuse")
        .getChildFile ("Presets");
}

void PresetManager::loadFactoryPresets()
{
    for (int i = 0; i < PresetData::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = PresetData::getNamedResource (PresetData::namedResourceList[i], size);
        if (data == nullptr || size <= 0)
            continue;
        juce::String text = juce::String::fromUTF8 (data, size);
        auto xml = juce::parseXML (text);
        if (xml == nullptr || ! xml->hasTagName ("RecklessMusePreset"))
            continue;
        Preset p;
        p.name = xml->getStringAttribute ("name", "Untitled");
        p.category = xml->getStringAttribute ("category", "FX");
        p.author = xml->getStringAttribute ("author", "RecklessMuse");
        p.xml = text;
        p.isFactory = true;
        presets.push_back (std::move (p));
    }
    sortPresets();
}

void PresetManager::rescanUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (const Preset& p) { return ! p.isFactory; }),
                   presets.end());

    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.xml"))
        {
            auto xml = juce::parseXML (f);
            if (xml == nullptr || ! xml->hasTagName ("RecklessMusePreset"))
                continue;
            Preset p;
            p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
            p.category = "User";
            p.author = xml->getStringAttribute ("author", "User");
            p.xml = xml->toString();
            p.isFactory = false;
            p.file = f;
            presets.push_back (std::move (p));
        }
    }
    sortPresets();
    setCurrentByName (currentName);
}

void PresetManager::sortPresets()
{
    const auto& order = categoryOrder();
    std::stable_sort (presets.begin(), presets.end(), [&order] (const Preset& a, const Preset& b)
    {
        const int ca = order.indexOf (a.category), cb = order.indexOf (b.category);
        const int ka = ca < 0 ? 99 : ca, kb = cb < 0 ? 99 : cb;
        if (ka != kb) return ka < kb;
        return a.name.compareNatural (b.name) < 0;
    });
}

juce::String PresetManager::getCurrentName() const { return currentName; }
juce::String PresetManager::getCurrentCategory() const { return currentCategory; }

void PresetManager::setCurrentByName (const juce::String& name)
{
    currentName = name;
    currentIndex = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name)
        {
            currentIndex = (int) i;
            currentCategory = presets[i].category;
            return;
        }
}

bool PresetManager::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("RecklessMusePreset"))
        return false;

    // Transport / performance / output settings belong to the session, not to a sound.
    static const juce::StringArray untouched { "seqPlay", "seqRec", "chordLearn", "kbHold", "kbOctave",
                                               "masterVol", "quality", "tempo" };

    const auto* values = xml.getChildByName ("Params");
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;
        const auto id = ranged->getParameterID();
        if (untouched.contains (id))
            continue;
        float normalised = ranged->getDefaultValue();
        if (values != nullptr && values->hasAttribute (id))
            normalised = ranged->convertTo0to1 ((float) values->getDoubleAttribute (id));
        ranged->setValueNotifyingHost (normalised);
    }

    if (const auto* seq = xml.getChildByName ("Sequence"))
    {
        juce::StringArray notes, ons;
        notes.addTokens (seq->getStringAttribute ("notes"), ",", {});
        ons.addTokens (seq->getStringAttribute ("on"), ",", {});
        for (int i = 0; i < kMaxSeqSteps; ++i)
        {
            if (i < notes.size()) sequence.note[(size_t) i] = juce::jlimit (0, 127, notes[i].getIntValue());
            sequence.on[(size_t) i] = i < ons.size() ? ons[i].getIntValue() != 0 : true;
        }
    }

    if (const auto* chord = xml.getChildByName ("Chord"))
    {
        juce::StringArray iv;
        iv.addTokens (chord->getStringAttribute ("intervals"), ",", {});
        const int count = std::min (iv.size(), ChordData::kMaxNotes);
        for (int i = 0; i < count; ++i)
            chords.intervals[(size_t) i] = iv[i].getIntValue();
        chords.count = count;
    }
    else
    {
        chords.count = 0;
    }

    sequence.recordIndex = 0;
    currentName = xml.getStringAttribute ("name", "Untitled");
    currentCategory = xml.getStringAttribute ("category", "User");
    return true;
}

std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const juce::String& name,
                                                                  const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> ("RecklessMusePreset");
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("author", "User");

    auto* values = xml->createNewChildElement ("Params");
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const float plain = ranged->convertFrom0to1 (ranged->getValue());
            const float def = ranged->convertFrom0to1 (ranged->getDefaultValue());
            if (std::abs (plain - def) > 1.0e-6f)
                values->setAttribute (ranged->getParameterID(), plain);
        }

    juce::StringArray notes, ons;
    for (int i = 0; i < kMaxSeqSteps; ++i)
    {
        notes.add (juce::String (sequence.note[(size_t) i].load()));
        ons.add (sequence.on[(size_t) i].load() ? "1" : "0");
    }
    auto* seq = xml->createNewChildElement ("Sequence");
    seq->setAttribute ("notes", notes.joinIntoString (","));
    seq->setAttribute ("on", ons.joinIntoString (","));

    const int count = chords.count.load();
    if (count > 0)
    {
        juce::StringArray iv;
        for (int i = 0; i < count; ++i)
            iv.add (juce::String (chords.intervals[(size_t) i].load()));
        xml->createNewChildElement ("Chord")->setAttribute ("intervals", iv.joinIntoString (","));
    }
    return xml;
}

bool PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return false;
    auto xml = juce::parseXML (presets[(size_t) index].xml);
    if (xml == nullptr || ! applyPresetXml (*xml))
        return false;
    currentIndex = index;
    currentName = presets[(size_t) index].name;
    currentCategory = presets[(size_t) index].category;
    return true;
}

void PresetManager::loadInit()
{
    juce::XmlElement init ("RecklessMusePreset");
    init.setAttribute ("name", "Init");
    init.setAttribute ("category", "Init");
    applyPresetXml (init);
    currentIndex = -1;
}

// ---------------------------------------------------------------------------
// Favourites
// ---------------------------------------------------------------------------
juce::File PresetManager::getDefaultFavoritesFile()
{
    return getUserPresetFolder().getParentDirectory().getChildFile ("Favorites.xml");
}

void PresetManager::setFavoritesFile (const juce::File& file)
{
    favoritesFile = file;
    loadFavorites();
}

void PresetManager::loadFavorites()
{
    favorites.clear();
    favoritesLoadedTime = favoritesFile.getLastModificationTime();
    if (auto xml = juce::parseXML (favoritesFile))
        for (auto* e : xml->getChildWithTagNameIterator ("Preset"))
            favorites.addIfNotAlreadyThere (e->getStringAttribute ("key"));
}

void PresetManager::saveFavorites()
{
    juce::XmlElement xml ("RecklessMuseFavorites");
    for (auto& key : favorites)
        xml.createNewChildElement ("Preset")->setAttribute ("key", key);
    favoritesFile.getParentDirectory().createDirectory();
    xml.writeTo (favoritesFile);
    favoritesLoadedTime = favoritesFile.getLastModificationTime();
}

void PresetManager::reloadFavoritesIfChanged()
{
    // Another instance (or another project) may have liked something in the meantime.
    if (favoritesFile.getLastModificationTime() != favoritesLoadedTime)
        loadFavorites();
}

bool PresetManager::isFavorite (int index) const
{
    return index >= 0 && index < (int) presets.size() && favorites.contains (keyOf (presets[(size_t) index]));
}

void PresetManager::toggleFavorite (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return;
    reloadFavoritesIfChanged();
    const auto key = keyOf (presets[(size_t) index]);
    if (favorites.contains (key))
        favorites.removeString (key);
    else
        favorites.add (key);
    saveFavorites();
    if (favoritesMode && getFavoriteIndices().empty())
        favoritesMode = false;
}

std::vector<int> PresetManager::getFavoriteIndices() const
{
    // Bank order = library order (category, then name), like every other bank.
    std::vector<int> out;
    for (size_t i = 0; i < presets.size(); ++i)
        if (favorites.contains (keyOf (presets[i])))
            out.push_back ((int) i);
    return out;
}

void PresetManager::setFavoritesMode (bool shouldBrowseFavorites)
{
    reloadFavoritesIfChanged();
    favoritesMode = shouldBrowseFavorites && ! getFavoriteIndices().empty();
}

int PresetManager::getPositionInBank() const
{
    if (! favoritesMode)
        return currentIndex;
    const auto fav = getFavoriteIndices();
    const auto it = std::find (fav.begin(), fav.end(), currentIndex);
    return it == fav.end() ? -1 : (int) (it - fav.begin());
}

int PresetManager::getBankSize() const
{
    return favoritesMode ? (int) getFavoriteIndices().size() : (int) presets.size();
}

void PresetManager::next (int delta)
{
    if (presets.empty())
        return;

    if (favoritesMode)
    {
        reloadFavoritesIfChanged();
        const auto fav = getFavoriteIndices();
        if (! fav.empty())
        {
            const int n = (int) fav.size();
            const auto it = std::find (fav.begin(), fav.end(), currentIndex);
            int pos;
            if (it != fav.end())
                pos = (((int) (it - fav.begin()) + delta) % n + n) % n;
            else // current preset was un-liked: jump to the nearest liked one in that direction
            {
                pos = delta > 0 ? 0 : n - 1;
                for (int i = 0; i < n; ++i)
                    if (delta > 0 ? fav[(size_t) i] > currentIndex : fav[(size_t) (n - 1 - i)] < currentIndex)
                    {
                        pos = delta > 0 ? i : n - 1 - i;
                        break;
                    }
            }
            loadPreset (fav[(size_t) pos]);
            return;
        }
        favoritesMode = false;
    }

    const int n = (int) presets.size();
    const int start = currentIndex < 0 ? (delta > 0 ? -1 : 0) : currentIndex;
    loadPreset (((start + delta) % n + n) % n);
}

void PresetManager::nextCategory (int delta)
{
    if (presets.empty())
        return;
    reloadFavoritesIfChanged();

    // Banks: Favorites (when something is liked), then every category.
    juce::StringArray banks;
    const auto fav = getFavoriteIndices();
    if (! fav.empty())
        banks.add ("Favorites");
    for (auto& p : presets)
        banks.addIfNotAlreadyThere (p.category);

    int bi = banks.indexOf (getBankName());
    bi = bi < 0 ? 0 : ((bi + delta) % banks.size() + banks.size()) % banks.size();

    if (banks[bi] == "Favorites")
    {
        favoritesMode = true;
        loadPreset (fav.front());
        return;
    }
    favoritesMode = false;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].category == banks[bi])
        {
            loadPreset ((int) i);
            return;
        }
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto clean = name.trim().isEmpty() ? juce::String ("User Preset") : name.trim();
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;
    auto file = folder.getChildFile (juce::File::createLegalFileName (clean) + ".xml");
    auto xml = createPresetXml (clean, "User");
    if (! xml->writeTo (file))
        return false;
    currentName = clean;
    rescanUserPresets();
    return true;
}
} // namespace rm
