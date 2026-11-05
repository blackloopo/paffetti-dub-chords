#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

// ======================= voce =======================
void StabVoice::start (std::shared_ptr<SampleData> s, int n, float velocity, double hostRate, float tune,
                       const juce::ADSR::Parameters& env)
{
    sample = std::move (s);
    note = n;
    // il tasto con la stessa nota della fondamentale, nell'ottava di C3 (60-71), suona l'accordo all'altezza originale
    int home = 60 + (sample->rootMidi % 12);
    inc = std::pow (2.0, (n - home + tune) / 12.0) * sample->sourceRate / hostRate;
    pos = 0;
    gain = 0.35f + 0.65f * velocity;
    adsr.setSampleRate (hostRate);
    adsr.setParameters (env);
    adsr.reset();
    adsr.noteOn();
    active = true; keyDown = true;
}

void StabVoice::stop (bool allowTail)
{
    keyDown = false;
    if (allowTail) adsr.noteOff();
    else { adsr.reset(); active = false; sample.reset(); }
}

void StabVoice::render (float* L, float* R, int n)
{
    if (! active || sample == nullptr) return;
    auto& b = sample->buffer;
    const float* sl = b.getReadPointer (0);
    const float* sr = b.getReadPointer (1);
    const int len = b.getNumSamples() - 3;
    for (int i = 0; i < n; ++i)
    {
        int ip = (int) pos;
        if (ip >= len) { active = false; break; }
        float f = (float) (pos - ip);
        auto herm = [f] (const float* d, int k)
        {
            float y0 = d[juce::jmax (0, k - 1)], y1 = d[k], y2 = d[k + 1], y3 = d[k + 2];
            float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        };
        float e = adsr.getNextSample() * gain;
        L[i] += herm (sl, ip) * e;
        R[i] += herm (sr, ip) * e;
        pos += inc;
        if (! adsr.isActive()) { active = false; break; }
    }
    if (! active) sample.reset();
}

// ======================= parametri =======================
APVTS::ParameterLayout PaffettiProcessor::createLayout()
{
    APVTS::ParameterLayout l;
    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; };
    auto ms  = [] (float v, int) { return v >= 1000 ? juce::String (v / 1000.0f, 2) + "s" : juce::String (juce::roundToInt (v)) + "ms"; };
    auto hz  = [] (float v, int) { return v >= 1000 ? juce::String (v / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (v)); };
    auto db  = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + "dB"; };
    using P = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    using A = juce::AudioParameterFloatAttributes;
    auto add = [&] (auto p) { l.add (std::move (p)); };

    add (std::make_unique<P> (juce::ParameterID { "attack", 1 }, "Attack", juce::NormalisableRange<float> (0.1f, 300.0f, 0.0f, 0.35f), 1.0f, A().withStringFromValueFunction (ms)));
    add (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay", juce::NormalisableRange<float> (30.0f, 4000.0f, 0.0f, 0.4f), 4000.0f,
         A().withStringFromValueFunction ([] (float v, int) { return v >= 3990 ? juce::String ("FULL") : (v >= 1000 ? juce::String (v / 1000.0f, 2) + "s" : juce::String (juce::roundToInt (v)) + "ms"); })));
    add (std::make_unique<P> (juce::ParameterID { "release", 1 }, "Release", juce::NormalisableRange<float> (5.0f, 3000.0f, 0.0f, 0.4f), 250.0f, A().withStringFromValueFunction (ms)));
    add (std::make_unique<P> (juce::ParameterID { "cutoff", 1 }, "Filter", juce::NormalisableRange<float> (80.0f, 20000.0f, 0.0f, 0.25f), 20000.0f, A().withStringFromValueFunction (hz)));
    add (std::make_unique<P> (juce::ParameterID { "reso", 1 }, "Reso", juce::NormalisableRange<float> (0.0f, 0.9f), 0.0f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "tune", 1 }, "Tune", juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
         A().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String ((int) v) + "st"; })));
    add (std::make_unique<C> (juce::ParameterID { "poly", 1 }, "Poly", juce::StringArray { "1", "4", "8" }, 2));

    add (std::make_unique<B> (juce::ParameterID { "dlySync", 1 }, "Delay Sync", true));
    add (std::make_unique<C> (juce::ParameterID { "dlyDiv", 1 }, "Delay Time", delayDivisions(), 3));
    add (std::make_unique<P> (juce::ParameterID { "dlyMs", 1 }, "Delay ms", juce::NormalisableRange<float> (20.0f, 1500.0f, 0.0f, 0.5f), 375.0f, A().withStringFromValueFunction (ms)));
    add (std::make_unique<P> (juce::ParameterID { "dlyFeed", 1 }, "Delay Feedback", juce::NormalisableRange<float> (0.0f, 1.08f), 0.55f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "dlyWow", 1 }, "Wow", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "dlyTone", 1 }, "Delay Tone", juce::NormalisableRange<float> (400.0f, 9000.0f, 0.0f, 0.4f), 2600.0f, A().withStringFromValueFunction (hz)));
    add (std::make_unique<P> (juce::ParameterID { "dlyMix", 1 }, "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<B> (juce::ParameterID { "dlyPing", 1 }, "Ping Pong", false));

    add (std::make_unique<B> (juce::ParameterID { "phSync", 1 }, "Phaser Sync", true));
    add (std::make_unique<C> (juce::ParameterID { "phDiv", 1 }, "Phaser Rate", lfoDivisions(), 3));
    add (std::make_unique<P> (juce::ParameterID { "phHz", 1 }, "Phaser Hz", juce::NormalisableRange<float> (0.02f, 6.0f, 0.0f, 0.35f), 0.25f,
         A().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + "Hz"; })));
    add (std::make_unique<P> (juce::ParameterID { "phDepth", 1 }, "Phaser Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "phFeed", 1 }, "Phaser Feedback", juce::NormalisableRange<float> (0.0f, 0.9f), 0.35f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "phMix", 1 }, "Phaser Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<C> (juce::ParameterID { "phStages", 1 }, "Phaser Stages", juce::StringArray { "4 ST", "8 ST", "12 ST" }, 0));

    add (std::make_unique<B> (juce::ParameterID { "flSync", 1 }, "Flanger Sync", true));
    add (std::make_unique<C> (juce::ParameterID { "flDiv", 1 }, "Flanger Rate", lfoDivisions(), 4));
    add (std::make_unique<P> (juce::ParameterID { "flHz", 1 }, "Flanger Hz", juce::NormalisableRange<float> (0.02f, 6.0f, 0.0f, 0.35f), 0.15f,
         A().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + "Hz"; })));
    add (std::make_unique<P> (juce::ParameterID { "flDepth", 1 }, "Flanger Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.45f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "flFeed", 1 }, "Flanger Feedback", juce::NormalisableRange<float> (-0.95f, 0.95f), -0.3f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "flMix", 1 }, "Flanger Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, A().withStringFromValueFunction (pct)));

    add (std::make_unique<P> (juce::ParameterID { "spMix", 1 }, "Spring", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "spDecay", 1 }, "Spring Decay", juce::NormalisableRange<float> (0.2f, 0.85f), 0.55f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "sat", 1 }, "Saturation", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, A().withStringFromValueFunction (pct)));
    add (std::make_unique<P> (juce::ParameterID { "hiss", 1 }, "Hiss", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, A().withStringFromValueFunction (pct)));

    add (std::make_unique<P> (juce::ParameterID { "low", 1 }, "Low", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f, A().withStringFromValueFunction (db)));
    add (std::make_unique<P> (juce::ParameterID { "high", 1 }, "High", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f, A().withStringFromValueFunction (db)));
    add (std::make_unique<P> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (-36.0f, 6.0f), -3.0f, A().withStringFromValueFunction (db)));
    return l;
}

PaffettiProcessor::PaffettiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto entries = library.getEntries ("BC");
    if (! entries.isEmpty()) selectSample ("BC", entries[0].name);
}

bool PaffettiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PaffettiProcessor::prepareToPlay (double rate, int block)
{
    sr = rate;
    work.setSize (2, block);
    juce::dsp::ProcessSpec spec { rate, (juce::uint32) block, 2 };
    ladder.prepare (spec);
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.setDrive (1.2f);
    phaser.prepare (rate); flanger.prepare (rate); tape.prepare (rate); spring.prepare (rate);
    for (auto& f : lowShelf) f.reset();
    for (auto& f : highShelf) f.reset();
    lastLow = lastHigh = 999;
    volume.reset (rate, 0.05); volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("volume")->load()));
    cutoffSm.reset (rate, 0.03); cutoffSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("cutoff")->load());
    for (auto& v : voices) v.stop (false);
}

juce::ADSR::Parameters PaffettiProcessor::currentEnv() const
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    float dec = g ("decay");
    juce::ADSR::Parameters p;
    p.attack = g ("attack") / 1000.0f;
    p.decay = dec >= 3990 ? 0.01f : dec / 1000.0f;
    p.sustain = dec >= 3990 ? 1.0f : 0.0f;
    p.release = g ("release") / 1000.0f;
    return p;
}

std::shared_ptr<SampleData> PaffettiProcessor::getCurrentSample() const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return current;
}

void PaffettiProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        std::shared_ptr<SampleData> s;
        {
            const juce::SpinLock::ScopedTryLockType tl (sampleLock);
            if (tl.isLocked()) s = current;
        }
        if (s == nullptr) return;
        int polyIdx = (int) apvts.getRawParameterValue ("poly")->load();
        int poly = polyIdx == 0 ? 1 : (polyIdx == 1 ? 4 : 8);
        // stessa nota: chiude la precedente (con coda breve)
        for (auto& v : voices) if (v.isActive() && v.getNote() == m.getNoteNumber()) v.stop (true);
        int activeCount = 0;
        for (auto& v : voices) if (v.isActive()) ++activeCount;
        if (poly == 1) for (auto& v : voices) if (v.isActive()) v.stop (true);
        StabVoice* target = nullptr;
        for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr || (poly > 1 && activeCount >= poly))
        {
            StabVoice* oldest = &voices[0];
            for (auto& v : voices) if (v.age < oldest->age) oldest = &v;
            if (target == nullptr) { oldest->stop (false); target = oldest; }
            else oldest->stop (true);
        }
        target->age = ++voiceCounter;
        target->start (s, m.getNoteNumber(), m.getFloatVelocity(), sr, apvts.getRawParameterValue ("tune")->load(), currentEnv());
        lastNote = m.getNoteNumber();
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices) if (v.isActive() && v.isKeyDown() && v.getNote() == m.getNoteNumber()) v.stop (true);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) v.stop (m.isAllNotesOff());
    }
}

double PaffettiProcessor::divToBeats (const juce::String& d)
{
    if (d == "1/16") return 0.25;  if (d == "1/8T") return 1.0 / 3.0; if (d == "1/8") return 0.5;
    if (d == "3/16") return 0.75;  if (d == "1/4T") return 2.0 / 3.0; if (d == "1/4") return 1.0;
    if (d == "3/8") return 1.5;    if (d == "1/2") return 2.0;
    if (d == "1 bar") return 4.0;  if (d == "2 bar") return 8.0; if (d == "4 bar") return 16.0;
    if (d == "8 bar") return 32.0; if (d == "16 bar") return 64.0;
    return 1.0;
}

void PaffettiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    // tempo e posizione dal progetto
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }
    hostBpm = bpm;

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto envNow = currentEnv();
    for (auto& v : voices) if (v.isActive()) v.setEnv (envNow);

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // voci, spezzate sugli eventi MIDI
    int cursor = 0;
    for (const auto meta : midi)
    {
        int at = juce::jlimit (0, n, meta.samplePosition);
        for (auto& v : voices) v.render (L + cursor, R + cursor, at - cursor);
        cursor = at;
        handleMidi (meta.getMessage());
    }
    for (auto& v : voices) v.render (L + cursor, R + cursor, n - cursor);
    midi.clear();

    // filtro
    cutoffSm.setTargetValue (g ("cutoff"));
    float reso = g ("reso");
    if (cutoffSm.getTargetValue() < 19500.0f || reso > 0.01f || cutoffSm.isSmoothing())
    {
        ladder.setResonance (reso);
        const int sub = 32;
        for (int i = 0; i < n; i += sub)
        {
            int m = juce::jmin (sub, n - i);
            ladder.setCutoffFrequencyHz (juce::jmin (cutoffSm.skip (m), (float) (sr * 0.45)));
            float* chans[2] = { L + i, R + i };
            juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) m);
            ladder.process (juce::dsp::ProcessContextReplacing<float> (blk));
        }
    }

    auto lfoRate = [&] (const char* syncId, const char* divId, const char* hzId, juce::StringArray divs)
    {
        if (g (syncId) > 0.5f)
            return (bpm / 60.0) / divToBeats (divs[(int) g (divId)]);
        return (double) g (hzId);
    };
    auto syncPhase = [&] (dub::LFO& lfo, const char* syncId, const char* divId, juce::StringArray divs)
    {
        if (playing && g (syncId) > 0.5f)
        {
            double cyc = ppq / divToBeats (divs[(int) g (divId)]);
            lfo.phase = cyc - std::floor (cyc);
        }
    };

    if (g ("phMix") > 0.001f)
    {
        syncPhase (phaser.lfo, "phSync", "phDiv", lfoDivisions());
        int st = (int) g ("phStages"); int stages = st == 0 ? 4 : (st == 1 ? 8 : 12);
        phaser.process (L, R, n, stages, lfoRate ("phSync", "phDiv", "phHz", lfoDivisions()), g ("phDepth"), g ("phFeed"), g ("phMix"));
    }
    if (g ("flMix") > 0.001f)
    {
        syncPhase (flanger.lfo, "flSync", "flDiv", lfoDivisions());
        flanger.process (L, R, n, lfoRate ("flSync", "flDiv", "flHz", lfoDivisions()), g ("flDepth"), g ("flFeed"), g ("flMix"));
    }

    // tape delay: sempre in funzione (le code continuano anche abbassando il mix)
    {
        double secs = g ("dlySync") > 0.5f ? divToBeats (delayDivisions()[(int) g ("dlyDiv")]) * 60.0 / bpm : g ("dlyMs") / 1000.0;
        tape.process (L, R, n, (float) (secs * sr), g ("dlyFeed"), g ("dlyWow"), g ("dlyTone"), g ("dlyMix"), g ("dlyPing") > 0.5f);
    }

    if (g ("spMix") > 0.001f) spring.process (L, R, n, g ("spDecay"), g ("spMix"));

    // saturazione nastro + fruscio
    float sat = g ("sat"), hs = g ("hiss");
    if (sat > 0.001f || hs > 0.001f)
    {
        float drive = 1.0f + sat * 5.0f, norm = 1.0f / std::tanh (drive * 0.7f);
        for (int i = 0; i < n; ++i)
        {
            if (sat > 0.001f)
            {
                L[i] = juce::jmap (sat, L[i], std::tanh (drive * 0.7f * L[i] + 0.04f * sat * L[i] * L[i]) * norm * 0.7f);
                R[i] = juce::jmap (sat, R[i], std::tanh (drive * 0.7f * R[i] + 0.04f * sat * R[i] * R[i]) * norm * 0.7f);
            }
            if (hs > 0.001f) { L[i] += hiss[0].next() * hs * 0.006f; R[i] += hiss[1].next() * hs * 0.006f; }
        }
    }

    // EQ e volume
    float lo = g ("low"), hi = g ("high");
    if (lo != lastLow || hi != lastHigh)
    {
        auto lc = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0, 0.7, juce::Decibels::decibelsToGain (lo));
        auto hc = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5000.0, 0.7, juce::Decibels::decibelsToGain (hi));
        for (int c = 0; c < 2; ++c) { *lowShelf[c].coefficients = *lc; *highShelf[c].coefficients = *hc; }
        lastLow = lo; lastHigh = hi;
    }
    volume.setTargetValue (juce::Decibels::decibelsToGain (g ("volume")));
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        float gv = volume.getNextValue();
        float l = L[i], r = R[i];
        if (lo != 0.0f) { l = lowShelf[0].processSample (l); r = lowShelf[1].processSample (r); }
        if (hi != 0.0f) { l = highShelf[0].processSample (l); r = highShelf[1].processSample (r); }
        l *= gv; r *= gv;
        if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0; }
        L[i] = l; R[i] = r;
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    meterPeak = juce::jmax (peak, meterPeak.load() * 0.9f);
}

// ======================= libreria =======================
bool PaffettiProcessor::selectSample (const juce::String& cat, const juce::String& name)
{
    auto* e = library.find (cat, name);
    if (e == nullptr) return false;
    auto s = library.load (*e);
    if (s == nullptr) return false;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        if (current != nullptr) keepAlive.push_back (current);
        current = s;
    }
    // libera i sample vecchi non più usati dalle voci (rimangono solo gli ultimi)
    while (keepAlive.size() > 12) keepAlive.erase (keepAlive.begin());
    selCategory = cat; selName = name;
    ++sampleVersion;
    return true;
}

void PaffettiProcessor::setUserFolder (const juce::File& f)
{
    library.setUserFolder (f);
}

void PaffettiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("category", selCategory, nullptr);
    state.setProperty ("sample", selName, nullptr);
    state.setProperty ("userFolder", library.getUserFolder().getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PaffettiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        auto uf = state.getProperty ("userFolder").toString();
        if (uf.isNotEmpty() && juce::File::isAbsolutePath (uf)) library.setUserFolder (juce::File (uf));
        auto cat = state.getProperty ("category").toString();
        auto nm = state.getProperty ("sample").toString();
        apvts.replaceState (state);
        if (cat.isNotEmpty()) selectSample (cat, nm);
    }
}

juce::AudioProcessorEditor* PaffettiProcessor::createEditor() { return new PaffettiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PaffettiProcessor(); }
