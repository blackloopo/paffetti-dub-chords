#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Library.h"
#include "DubFX.h"

class StabVoice
{
public:
    void start (std::shared_ptr<SampleData> s, int note, float velocity, double hostRate, float tuneSemis,
                const juce::ADSR::Parameters& env);
    void stop (bool allowTail);
    void render (float* L, float* R, int n);
    bool isActive() const { return active; }
    int getNote() const { return note; }
    bool isKeyDown() const { return keyDown; }
    juce::uint64 age = 0;
    void setEnv (const juce::ADSR::Parameters& p) { adsr.setParameters (p); }

private:
    std::shared_ptr<SampleData> sample;
    double pos = 0, inc = 1;
    float gain = 1;
    int note = -1;
    bool active = false, keyDown = false;
    juce::ADSR adsr;
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

    static juce::StringArray delayDivisions() { return { "1/16", "1/8T", "1/8", "3/16", "1/4T", "1/4", "3/8", "1/2" }; }
    static juce::StringArray lfoDivisions()   { return { "1/4", "1/2", "1 bar", "2 bar", "4 bar", "8 bar", "16 bar" }; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&);
    juce::ADSR::Parameters currentEnv() const;
    static double divToBeats (const juce::String& d);

    Library library;
    juce::String selCategory { "BC" }, selName;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<SampleData> current;
    std::vector<std::shared_ptr<SampleData>> keepAlive;   // i sample vecchi non vengono distrutti nel thread audio

    static constexpr int maxVoices = 8;
    std::array<StabVoice, maxVoices> voices;
    juce::uint64 voiceCounter = 0;

    double sr = 44100.0;
    juce::AudioBuffer<float> work;
    juce::dsp::LadderFilter<float> ladder;
    dub::Phaser phaser; dub::Flanger flanger; dub::TapeDelay tape; dub::Spring spring; dub::Hiss hiss[2];
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    juce::SmoothedValue<float> volume, cutoffSm;
    float lastLow = 999, lastHigh = 999;
    double freeLfoPhase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaffettiProcessor)
};
