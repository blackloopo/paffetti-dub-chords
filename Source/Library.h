#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

// Un sample caricato in memoria: accordo completo, con la nota fondamentale reale
struct SampleData
{
    juce::AudioBuffer<float> buffer;   // sempre stereo
    double sourceRate = 44100.0;
    int rootMidi = 48;                 // altezza reale della fondamentale dell'accordo
    juce::String name;
};

struct LibraryEntry
{
    juce::File file;
    juce::String name;      // es. "BC-013 Csowhat"
    juce::String category;  // BC, MZ, RS, CR, SW, GR, MY
    int rootMidi = 48;
    juce::int64 packOffset = -1, packSize = 0;   // se >= 0 il sample sta dentro Samples.pdcpak
};

class Library
{
public:
    Library();

    // categorie in ordine di visualizzazione (l'ultima è "I miei sample")
    struct Category { juce::String id, name; };
    const juce::Array<Category>& getCategories() const { return categories; }

    juce::Array<LibraryEntry> getEntries (const juce::String& categoryId) const;
    const LibraryEntry* find (const juce::String& categoryId, const juce::String& name) const;

    void setUserFolder (const juce::File& folder);
    juce::File getUserFolder() const { return userFolder; }

    std::shared_ptr<SampleData> load (const LibraryEntry& e);

    juce::File getFactoryFolder() const { return factoryFolder; }
    bool factoryFound() const { return packFile.existsAsFile() || factoryFolder.isDirectory(); }

    static int parseRootFromName (const juce::String& name, int fallback);

private:
    void scanFactory();
    void scanUser();
    static juce::File locateFactoryFolder();
    static juce::File locatePack();

    juce::Array<Category> categories;
    juce::Array<LibraryEntry> factory, user;
    juce::File factoryFolder, userFolder, packFile;
    juce::AudioFormatManager formats;
};
