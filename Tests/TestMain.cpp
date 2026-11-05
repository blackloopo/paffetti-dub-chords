// Banco di prova: suona il plugin offline e fotografa l'interfaccia (solo per verifica, non va nel plugin)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 122, ppq = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing); return p;
    }
};

static void setP (PaffettiProcessor& p, const char* id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir (argc > 1 ? argv[1] : "/tmp/pdc_test");
    outDir.createDirectory();

    auto proc = std::make_unique<PaffettiProcessor>();
    auto& lib = proc->getLibrary();
    std::cout << "Cartella libreria: " << lib.getFactoryFolder().getFullPathName() << "\n";
    int total = 0;
    for (auto& c : lib.getCategories()) { auto n = lib.getEntries (c.id).size(); total += n; std::cout << "  " << c.id << " " << c.name << ": " << n << "\n"; }
    std::cout << "Totale stab: " << total << "\n";

    // riconoscimento tonalità dai nomi dei sample utente
    for (auto s : { "Cm7 stab", "my_F#min_chord", "Eb sus", "loop 01", "Bb9", "Dark A minor" })
        std::cout << "  radice di '" << s << "' = " << Library::parseRootFromName (s, -1) << "\n";

    const double sr = 48000; const int block = 256;
    FakePlayHead ph; proc->setPlayHead (&ph);
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);

    setP (*proc, "dlyMix", 0.45f); setP (*proc, "dlyFeed", 0.6f); setP (*proc, "spMix", 0.25f); setP (*proc, "sat", 0.3f);

    juce::AudioBuffer<float> out (2, (int) (sr * 24));
    out.clear();
    juce::AudioBuffer<float> buf (2, block);
    const double spb = 60.0 / ph.bpm * sr;          // campioni per battito
    struct Ev { double t; int note; bool on; };
    std::vector<Ev> evs;
    const int prog[] = { 60, 60, 58, 63 };
    for (int bar = 0; bar < 10; ++bar)
        for (double b : { 0.5, 1.5, 2.5, 3.25 })
        {
            double t = (bar * 4 + b) * spb + sr * 0.2;
            evs.push_back ({ t, prog[bar % 4], true });
            evs.push_back ({ t + spb * 0.2, prog[bar % 4], false });
        }
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.t < b.t; });

    const char* picks[][2] = { { "BC", "" }, { "MZ", "" }, { "RS", "" }, { "CR", "" }, { "SW", "" }, { "GR", "" } };
    size_t ei = 0; float maxPeak = 0; bool bad = false;
    for (int pos = 0; pos + block <= out.getNumSamples(); pos += block)
    {
        // cambia stab ogni 4 secondi e accende phaser/flanger a metà
        int sec = pos / (int) sr;
        if (pos % ((int) sr * 4) < block)
        {
            auto cat = picks[(sec / 4) % 6][0];
            auto ents = lib.getEntries (cat);
            proc->selectSample (cat, ents[(sec * 7) % ents.size()].name);
            std::cout << "  t=" << sec << "s  " << proc->getSelectedName() << "\n";
        }
        if (sec == 8)  setP (*proc, "phMix", 0.8f);
        if (sec == 12) { setP (*proc, "phMix", 0.0f); setP (*proc, "flMix", 0.7f); }
        if (sec == 16) { setP (*proc, "flMix", 0.0f); setP (*proc, "dlySync", 0.0f); setP (*proc, "dlyMs", 280.0f); setP (*proc, "cutoff", 1500.0f); setP (*proc, "reso", 0.5f); }
        juce::MidiBuffer midi;
        while (ei < evs.size() && evs[ei].t < pos + block)
        {
            int off = juce::jmax (0, (int) evs[ei].t - pos);
            midi.addEvent (evs[ei].on ? juce::MidiMessage::noteOn (1, evs[ei].note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, evs[ei].note), off);
            ++ei;
        }
        ph.ppq = pos / spb;
        buf.clear();
        proc->processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, block);
        maxPeak = juce::jmax (maxPeak, buf.getMagnitude (0, block));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) if (! std::isfinite (buf.getSample (c, i))) bad = true;
    }
    std::cout << "Picco: " << juce::Decibels::gainToDecibels (maxPeak) << " dB, valori non validi: " << (bad ? "SI" : "no") << "\n";

    // velocità: quanto tempo di CPU per 1 secondo di audio con tutti gli effetti accesi
    setP (*proc, "phMix", 0.5f); setP (*proc, "flMix", 0.5f);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int k = 0; k < (int) (sr / block) * 4; ++k)
    {
        juce::MidiBuffer midi;
        if (k % 40 == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 60 + (k / 40) % 5, (juce::uint8) 100), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 60 + (k / 40) % 5), 100); }
        buf.clear(); proc->processBlock (buf, midi);
    }
    auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::cout << "CPU: " << ms / 4.0 << " ms per secondo di audio (" << ms / 40.0 << "% di un core)\n";

    {
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile ("render.wav"); f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    }

    // stato salvato e ricaricato
    juce::MemoryBlock mb; proc->getStateInformation (mb);
    auto proc2 = std::make_unique<PaffettiProcessor>();
    proc2->setStateInformation (mb.getData(), (int) mb.getSize());
    std::cout << "Stato ripristinato: " << proc2->getSelectedCategory() << " / " << proc2->getSelectedName()
              << "  cutoff=" << proc2->apvts.getRawParameterValue ("cutoff")->load() << "\n";

    // fotografia dell'interfaccia
    proc->selectSample ("BC", lib.getEntries ("BC")[12].name);
    proc->lastNote = 60;
    proc->keyboardState.noteOn (1, 60, 1.0f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (PaffettiEditor::W, PaffettiEditor::H);

        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("ui.png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
        // controllo "finestra nera": percentuale di pixel quasi neri
        int dark = 0, tot = 0;
        for (int y = 0; y < img.getHeight(); y += 4) for (int x = 0; x < img.getWidth(); x += 4) { auto c = img.getPixelAt (x, y); ++tot; if (c.getBrightness() < 0.06f) ++dark; }
        std::cout << "Interfaccia: " << img.getWidth() << "x" << img.getHeight() << ", pixel neri " << (100 * dark / tot) << "%\n";
        ed->setSize (PaffettiEditor::W * 3 / 4, PaffettiEditor::H * 3 / 4);
        auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream os2 (outDir.getChildFile ("ui_small.png")); png.writeImageToStream (img2, os2);
    }
    proc->keyboardState.allNotesOff (1);
    proc->setPlayHead (nullptr);
    std::cout << "OK\n";
    return 0;
}
