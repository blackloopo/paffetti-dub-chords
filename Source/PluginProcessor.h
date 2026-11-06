#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Library.h"
#include "DubFX.h"

// parametri del DUB FILTER condivisi dalle voci in un blocco
struct FilterFrame
{
    int mode = 0;            // 0 LP, 1 BP, 2 HP
    float cutoff = 20000, reso = 0, envOct = 0, envDecay = 0.2f, lfoOct = 0, randOct = 0, drift = 0;
    const float* lfo = nullptr;   // valore LFO per campione (-1..1)
    bool bypass = true;
};

class StabVoice
{
public:
    void start (std::shared_ptr<SampleData> s, int note, float velocity, double hostRate, float tuneSemis,
                const juce::ADSR::Parameters& env, const FilterFrame& ff);
    void stop (bool allowTail);
    void render (float* L, float* R, int n, const FilterFrame& ff, int lfoOffset);
    bool isActive() const { return active; }
    int getNote() const { return note; }
    bool isKeyDown() const { return keyDown; }
    juce::uint64 age = 0;
    void setEnv (const juce::ADSR::Parameters& p) { adsr.setParameters (p); }
    float lastCutoff = 0;

private:
    std::shared_ptr<SampleData> sample;
    double pos = 0, inc = 1, sr = 44100;
    float gain = 1, randOff = 0, fEnv = 0, fEnvCoef = 0.999f, driftPh = 0, driftRate = 0.11f, walk = 0, walkTarget = 0;
    int note = -1, ctr = 0;
    bool active = false, keyDown = false;
    juce::ADSR adsr;
    dub::SVF f1[2], f2[2];
    float g = 1, k = 1.4f;
    juce::Random rnd;
};

class PaffettiProcessor : public juce::AudioProcessor
{
public:
    PaffettiProcessor();
    ~PaffettiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Paffetti Dub Chords"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- libreria / selezione (thread dei messaggi) ----
    Library& getLibrary() { return library; }
    bool selectSample (const juce::String& categoryId, const juce::String& name);
    juce::String getSelectedCategory() const { return selCategory; }
    juce::String getSelectedName() const { return selName; }
    std::shared_ptr<SampleData> getCurrentSample() const;
    void setUserFolder (const juce::File&);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    std::atomic<float> meterPeak { 0.0f };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> lastNote { -1 };
    std::atomic<int> sampleVersion { 0 };
    std::atomic<float> displayCutoff { 20000.0f }, displayLfo { 0.0f };

    static juce::StringArray delayDivisions() { return { "1/16", "1/8T", "1/8", "3/16", "1/4T", "1/4", "3/8", "1/2" }; }
    static juce::StringArray lfoDivisions()   { return { "1/4", "1/2", "1 bar", "2 bar", "4 bar", "8 bar", "16 bar" }; }
    static juce::StringArray filterLfoDivisions() { return { "1/8", "1/4", "1/2", "1 bar", "2 bar", "4 bar", "8 bar", "16 bar", "32 bar" }; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&);
    juce::ADSR::Parameters currentEnv() const;
    static double divToBeats (const juce::String& d);

    Library library;
    juce::String selCategory { "MD" }, selName;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<SampleData> current;
    std::vector<std::shared_ptr<SampleData>> keepAlive;   // i sample vecchi non vengono distrutti nel thread audio

    static constexpr int maxVoices = 8;
    std::array<StabVoice, maxVoices> voices;
    juce::uint64 voiceCounter = 0;

    double sr = 44100.0;
    juce::AudioBuffer<float> work;
    std::vector<float> lfoBuf, sweepBuf;
    dub::LFO filterLfo, sweepLfo;
    float shValue = 0; double lastShCycle = -1;
    juce::Random lfoRnd;
    FilterFrame makeFrame();
    dub::Phaser phaser; dub::Flanger flanger; dub::TapeDelay tape; dub::Spring spring; dub::Hiss hiss[2];
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    juce::SmoothedValue<float> volume;
    float lastLow = 999, lastHigh = 999;
    double freeLfoPhase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaffettiProcessor)
};
