// Banco di prova: suona il plugin offline e fotografa l'interfaccia (solo per verifica, non va nel plugin)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 126, ppq = 0; bool playing = true;
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

    // ogni stab della libreria deve caricarsi pieno
    {
        int ok = 0, ko = 0;
        for (auto& c : lib.getCategories())
            for (auto& e : lib.getEntries (c.id))
            {
                auto s = lib.load (e);
                if (s != nullptr && s->buffer.getMagnitude (0, 0, s->buffer.getNumSamples()) > 0.3f) ++ok;
                else { ++ko; std::cout << "  NON VALIDO: " << e.name << "\n"; }
            }
        std::cout << "Stab caricati correttamente: " << ok << ", non validi: " << ko << "\n";
    }

    const double sr = 48000; const int block = 256;
    FakePlayHead ph; proc->setPlayHead (&ph);
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);

    // impostazione "Mango": passa-banda, LFO 4 battute, inviluppo, random, deriva, delay 3/16 con sweep
    setP (*proc, "fltMode", 1); setP (*proc, "cutoff", 750.0f); setP (*proc, "reso", 0.4f); setP (*proc, "fltEnv", 0.45f);
    setP (*proc, "fltDecay", 280.0f); setP (*proc, "lfoDiv", 5); setP (*proc, "lfoDepth", 0.45f); setP (*proc, "fltRand", 0.3f);
    setP (*proc, "drift", 0.35f); setP (*proc, "dlyDiv", 3); setP (*proc, "dlyFeed", 0.58f); setP (*proc, "dlyMix", 0.38f);
    setP (*proc, "dlySweep", 0.5f); setP (*proc, "dlySweepDiv", 0); setP (*proc, "spMix", 0.22f); setP (*proc, "hiss", 0.25f);
    setP (*proc, "release", 400.0f);

    const double spb = 60.0 / ph.bpm * sr;          // campioni per battito
    const int bars = 32;
    juce::AudioBuffer<float> out (2, (int) ((bars * 4 + 8) * spb));
    out.clear();
    juce::AudioBuffer<float> buf (2, block);
    struct Ev { double t; int note; bool on; };
    std::vector<Ev> evs;
    const int prog[] = { 66, 66, 64, 69 };          // F#m – F#m – E – A (registro di C3)
    for (int bar = 0; bar < bars; ++bar)
        for (double b : { 0.5, 1.5, 2.5, 3.5 })
        {
            double t = (bar * 4 + b) * spb;
            int nt = prog[(bar / 2) % 4];
            evs.push_back ({ t, nt, true });
            evs.push_back ({ t + spb * 0.25, nt, false });
        }
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.t < b.t; });

    const char* cats[] = { "MD", "BC", "BW", "MV" };
    size_t ei = 0; float maxPeak = 0; bool bad = false; int lastSection = -1;
    for (int pos = 0; pos + block <= out.getNumSamples(); pos += block)
    {
        int section = juce::jmin (3, (int) (pos / (spb * 4 * 8)));     // 8 battute per categoria
        if (section != lastSection)
        {
            auto ents = lib.getEntries (cats[section]);
            juce::String pick = ents[0].name;
            int seen = 0;
            for (auto& e : ents) if (e.name.contains ("m7") || e.name.contains ("m9")) { if (seen++ == section + 1) { pick = e.name; break; } }
            proc->selectSample (cats[section], pick);
            std::cout << "  battuta " << section * 8 + 1 << ": " << proc->getSelectedName() << "\n";
            lastSection = section;
        }
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

    // calibrazione dei volumi: livello medio (RMS) di ogni preset con un giro di stab
    auto cal = juce::SystemStats::getEnvironmentVariable ("PDC_CALIBRATE", {});
    if (cal.isNotEmpty())
    {
        juce::DynamicObject::Ptr res = new juce::DynamicObject();
        auto& ps = proc->getPresets();
        for (int i = 0; i < ps.size(); ++i)
        {
            proc->loadPreset (i);
            proc->prepareToPlay (sr, block);          // azzera code di delay e molla
            proc->loadPreset (i);
            double sum = 0; long cnt = 0;
            const int total = (int) (sr * 6 / block);
            for (int b = 0; b < total; ++b)
            {
                juce::MidiBuffer midi;
                double t0 = b * (double) block, t1 = t0 + block;
                for (int h = 0; h < 8; ++h)
                {
                    double on = (h + 0.5) * spb, off = on + spb * 0.25;
                    if (on >= t0 && on < t1) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), (int) (on - t0));
                    if (off >= t0 && off < t1) midi.addEvent (juce::MidiMessage::noteOff (1, 60), (int) (off - t0));
                }
                buf.clear(); proc->processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) for (int s = 0; s < block; ++s) { double v = buf.getSample (c, s); sum += v * v; ++cnt; }
            }
            double rmsDb = 10.0 * std::log10 (sum / juce::jmax (1L, cnt) + 1e-12);
            res->setProperty (ps[i].name, rmsDb);
        }
        juce::File (cal).replaceWithText (juce::JSON::toString (juce::var (res.get())));
        std::cout << "Calibrazione scritta: " << cal << "\n";
    }

    // preset: tutti caricabili, sample presente, parametri applicati
    {
        int ok = 0, ko = 0;
        auto& ps = proc->getPresets();
        for (int i = 0; i < ps.size(); ++i)
        {
            proc->loadPreset (i);
            bool good = proc->getSelectedName() == ps[i].sample && proc->getSelectedCategory() == ps[i].category;
            if (ps[i].params.contains ("cutoff"))
                good = good && std::abs (proc->apvts.getRawParameterValue ("cutoff")->load() - (float) (double) ps[i].params["cutoff"]) < 2.0f;
            if (good) ++ok; else { ++ko; std::cout << "  PRESET NON VALIDO: " << ps[i].name << "\n"; }
        }
        std::cout << "Preset caricati correttamente: " << ok << " su " << ps.size() << ", non validi: " << ko << "\n";
        // un preset fa rumore? render breve di ognuno dei primi di categoria
        int silent = 0;
        for (int i = 0; i < ps.size(); i += 10)
        {
            proc->loadPreset (i);
            float pk = 0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 2) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                if (b == 60) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                buf.clear(); proc->processBlock (buf, midi); pk = juce::jmax (pk, buf.getMagnitude (0, block));
            }
            std::cout << "  " << ps[i].name << ": picco " << juce::String (juce::Decibels::gainToDecibels (pk), 1) << " dB\n";
            if (pk < 0.01f) ++silent;
        }
        std::cout << "Preset muti: " << silent << "\n";
    }

    // fotografia dell'interfaccia
    proc->selectSample ("MD", lib.getEntries ("MD")[0].name);
    if (auto s = proc->getCurrentSample())
        std::cout << "Sample in memoria: " << s->name << " " << s->buffer.getNumSamples() << " campioni, picco "
                  << s->buffer.getMagnitude (0, 0, s->buffer.getNumSamples()) << " a " << s->sourceRate << " Hz\n";
    proc->lastNote = 60;
    proc->keyboardState.noteOn (1, 60, 1.0f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (PaffettiEditor::W, PaffettiEditor::H);
        // frecce: giù tre volte, poi destra (categoria successiva) e giù
        if (auto* pe = dynamic_cast<PaffettiEditor*> (ed.get()))
        {
            auto& panel = pe->getPanel();
            auto before = proc->getSelectedName();
            panel.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            panel.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            panel.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            std::cout << "Freccia giù x3: " << before << " -> " << proc->getSelectedName() << "\n";
            panel.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
            std::cout << "Freccia su: -> " << proc->getSelectedName() << "\n";
            panel.keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
            panel.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            std::cout << "Destra + giù: -> " << proc->getSelectedCategory() << " / " << proc->getSelectedName() << "\n";
            panel.keyPressed (juce::KeyPress (juce::KeyPress::pageDownKey));
            std::cout << "Pagina giù (preset successivo): " << proc->getPresetIndex() << " " << proc->getSelectedName() << "\n";
            proc->loadPreset (11);   // per la foto: un preset Basic Channel
            panel.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
            std::cout << "Preset + giù: " << proc->getSelectedName() << " modificato=" << (int) proc->isPresetModified() << "\n";
        }

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
