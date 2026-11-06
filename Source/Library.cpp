#include "Library.h"

Library::Library()
{
    formats.registerBasicFormats();
    packFile = locatePack();
    factoryFolder = packFile.existsAsFile() ? packFile.getParentDirectory() : locateFactoryFolder();
    scanFactory();
}

// I sample stanno dentro il bundle del plugin: .../Contents/Resources/Samples
juce::File Library::locateFactoryFolder()
{
    auto env = juce::SystemStats::getEnvironmentVariable ("PDC_SAMPLES", {});
    if (env.isNotEmpty() && juce::File (env).isDirectory())
        return juce::File (env);

    auto f = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (int i = 0; i < 6; ++i)
    {
        f = f.getParentDirectory();
        auto c1 = f.getChildFile ("Resources").getChildFile ("Samples");
        if (c1.getChildFile ("library.json").existsAsFile()) return c1;
        auto c2 = f.getChildFile ("Samples");
        if (c2.getChildFile ("library.json").existsAsFile()) return c2;
    }
    return {};
}

// Samples.pdcpak: "PDCPAK1\n" + lunghezza indice (8 byte) + indice JSON + dati FLAC uno dopo l'altro
juce::File Library::locatePack()
{
    auto env = juce::SystemStats::getEnvironmentVariable ("PDC_PACK", {});
    if (env.isNotEmpty() && juce::File (env).existsAsFile()) return juce::File (env);
    auto f = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (int i = 0; i < 6; ++i)
    {
        f = f.getParentDirectory();
        auto c1 = f.getChildFile ("Resources").getChildFile ("Samples.pdcpak");
        if (c1.existsAsFile()) return c1;
        auto c2 = f.getChildFile ("Samples.pdcpak");
        if (c2.existsAsFile()) return c2;
    }
    return {};
}

void Library::scanFactory()
{
    categories.clear();
    factory.clear();

    juce::var json;
    juce::int64 dataStart = 0;
    if (packFile.existsAsFile())
    {
        juce::FileInputStream in (packFile);
        char magic[8] = {};
        if (in.openedOk() && in.read (magic, 8) == 8 && juce::String (magic, 7) == "PDCPAK1")
        {
            auto len = (juce::int64) in.readInt64();
            juce::MemoryBlock mb;
            in.readIntoMemoryBlock (mb, (ssize_t) len);
            json = juce::JSON::parse (mb.toString());
            dataStart = 16 + len;
        }
    }
    else
        json = juce::JSON::parse (factoryFolder.getChildFile ("library.json"));
    if (auto* cats = json.getProperty ("categories", {}).getArray())
        for (auto& c : *cats)
            categories.add ({ c.getProperty ("id", {}).toString(), c.getProperty ("name", {}).toString() });

    if (categories.isEmpty())
    {
        const char* ids[]   = { "BC", "MZ", "RS", "CR", "SW", "GR" };
        const char* names[] = { "Basic Channel", "Maurizio", "Rhythm & Sound", "Chain Reaction", "Swayzak", "Grit 12 bit" };
        for (int i = 0; i < 6; ++i) categories.add ({ ids[i], names[i] });
    }
    categories.add ({ "MY", "I miei sample" });

    if (auto* arr = json.getProperty ("samples", {}).getArray())
        for (auto& s : *arr)
        {
            LibraryEntry e;
            e.name = juce::File::createFileWithoutCheckingPath ("/" + s.getProperty ("file", {}).toString()).getFileNameWithoutExtension();
            e.category = s.getProperty ("cat", {}).toString();
            e.rootMidi = (int) s.getProperty ("root", 48);
            if (dataStart > 0)
            {
                e.file = packFile;
                e.packOffset = dataStart + (juce::int64) s.getProperty ("offset", 0);
                e.packSize = (juce::int64) s.getProperty ("size", 0);
                factory.add (e);
            }
            else
            {
                e.file = factoryFolder.getChildFile (s.getProperty ("file", {}).toString());
                if (e.file.existsAsFile()) factory.add (e);
            }
        }
}

void Library::setUserFolder (const juce::File& folder)
{
    userFolder = folder;
    scanUser();
}

void Library::scanUser()
{
    user.clear();
    if (! userFolder.isDirectory()) return;
    auto files = userFolder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.WAV;*.AIF;*.AIFF");
    files.sort();
    for (auto& f : files)
    {
        LibraryEntry e;
        e.file = f;
        e.name = f.getFileNameWithoutExtension();
        e.category = "MY";
        // senza indicazioni il sample suona alla sua altezza originale premendo il Do centrale (C3)
        e.rootMidi = parseRootFromName (e.name, 60);
        user.add (e);
    }
}

// Riconosce la tonalità dal nome ("Cm7", "F#min", "Eb sus") e la mette nell'ottava di C3
int Library::parseRootFromName (const juce::String& name, int fallback)
{
    static const char* letters = "CDEFGAB";
    static const int base[] = { 0, 2, 4, 5, 7, 9, 11 };
    auto tokens = juce::StringArray::fromTokens (name.replaceCharacters ("_-.", "   "), " ", "");
    for (auto& t : tokens)
    {
        if (t.isEmpty()) continue;
        auto first = juce::CharacterFunctions::toUpperCase (t[0]);
        auto idx = juce::String (letters).indexOfChar (first);
        if (idx < 0 || t[0] != first) continue;                 // la nota deve essere maiuscola
        int pc = base[idx];
        int i = 1;
        if (t.length() > 1 && (t[1] == '#' )) { ++pc; ++i; }
        else if (t.length() > 1 && t[1] == 'b') { --pc; ++i; }
        auto rest = t.substring (i).toLowerCase();
        static const char* ok[] = { "m", "maj", "min", "sus", "add", "aug", "dim", "q", "clus", "atone", "tritq", "drk",
                                    "5", "6", "7", "9", "11", "13", "sowhat", "o", "+" };
        bool chordish = rest.isEmpty();
        for (auto* k : ok) if (rest.startsWith (k)) chordish = true;
        // "m" deve essere seguito da qualità o numeri, non da una parola qualsiasi ("mellow")
        if (rest.startsWith ("m") && ! (rest == "m" || rest.startsWith ("maj") || rest.startsWith ("min") || rest.startsWith ("madd")
                                        || rest.startsWith ("mmaj") || juce::CharacterFunctions::isDigit (rest[1]) || rest.startsWith ("m7")))
            chordish = rest.startsWith ("maj") || rest.startsWith ("min");
        if (! chordish) continue;
        return 60 + ((pc + 12) % 12);
    }
    return fallback;
}

juce::Array<LibraryEntry> Library::getEntries (const juce::String& categoryId) const
{
    if (categoryId == "MY") return user;
    juce::Array<LibraryEntry> out;
    for (auto& e : factory) if (e.category == categoryId) out.add (e);
    return out;
}

const LibraryEntry* Library::find (const juce::String& categoryId, const juce::String& name) const
{
    auto& list = categoryId == "MY" ? user : factory;
    for (auto& e : list) if (e.name == name && (categoryId == "MY" || e.category == categoryId)) return &e;
    return nullptr;
}

std::shared_ptr<SampleData> Library::load (const LibraryEntry& e)
{
    std::unique_ptr<juce::AudioFormatReader> r;
    if (e.packOffset >= 0)
    {
        juce::FileInputStream in (e.file);
        if (! in.openedOk() || ! in.setPosition (e.packOffset)) return nullptr;
        juce::MemoryBlock mb;
        in.readIntoMemoryBlock (mb, (ssize_t) e.packSize);
        r.reset (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (std::move (mb), false)));
    }
    else
        r.reset (formats.createReaderFor (e.file));
    if (r == nullptr || r->lengthInSamples <= 0) return nullptr;

    auto len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 30.0));
    auto s = std::make_shared<SampleData>();
    s->buffer.setSize (2, len + 4);
    s->buffer.clear();
    r->read (&s->buffer, 0, len, 0, true, true);
    if (r->numChannels == 1) s->buffer.copyFrom (1, 0, s->buffer, 0, 0, len);
    s->sourceRate = r->sampleRate;
    s->rootMidi = e.rootMidi;
    s->name = e.name;
    return s;
}
