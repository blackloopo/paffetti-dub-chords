#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace pal
{
    const juce::Colour bg       { 0xff1a0f08 };
    const juce::Colour body     { 0xff3a2312 };
    const juce::Colour panel    { 0xff2b1a0e };
    const juce::Colour edge     { 0xff120a05 };
    const juce::Colour rim      { 0xff4a2c15 };
    const juce::Colour yellow   { 0xffF6C445 };
    const juce::Colour orange   { 0xffF08A24 };
    const juce::Colour red      { 0xffD9531E };
    const juce::Colour aqua     { 0xff7fe0d6 };
    const juce::Colour green    { 0xff8BC34A };
    const juce::Colour cream    { 0xfff3dfc0 };
    const juce::Colour tan      { 0xffc9a37a };
    const juce::Colour screen   { 0xff1f0e05 };
}

struct RetroFonts
{
    static juce::Font pixel (float h);
    static juce::Font vt (float h);
};

class RetroLNF : public juce::LookAndFeel_V4
{
public:
    RetroLNF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

// manopola con nome e valore
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& label);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String label;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// fila di pulsantini a pixel legata a un parametro (scelta o on/off)
class PixelChoice : public juce::Component
{
public:
    PixelChoice (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::StringArray labels, juce::Array<int> values = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int preferredWidth() const;
private:
    juce::StringArray labels; juce::Array<int> values; int current = 0;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::RangedAudioParameter* param;
};

class PixelButton : public juce::Button
{
public:
    PixelButton (const juce::String& t, juce::Colour c) : juce::Button (t), colour (c) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour colour;
};

class CRTScreen : public juce::Component, private juce::Timer
{
public:
    explicit CRTScreen (PaffettiProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void step (int delta);
    void stepCategory (int delta);
    void randomPick();
    void showCategory (const juce::String& id);
    void refresh();
private:
    void timerCallback() override;
    void buildWave();
    juce::Rectangle<int> tabRect (int i) const;
    PaffettiProcessor& proc;
    juce::String category;
    juce::Array<LibraryEntry> entries;
    int scroll = 0; int blink = 0; int seenVersion = -1;
    juce::Array<float> wave;
    static constexpr int rows = 13;
};

class KeyboardView : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardView (PaffettiProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    int noteAt (juce::Point<float>) const;
    PaffettiProcessor& proc;
    int lowNote = 36, numWhite = 29, mouseNote = -1;
};

class Section : public juce::Component
{
public:
    Section (const juce::String& t, juce::Colour c) : title (t), colour (c) {}
    void paint (juce::Graphics&) override;
    juce::String title; juce::Colour colour;
};

class Meter : public juce::Component, private juce::Timer
{
public:
    explicit Meter (PaffettiProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    PaffettiProcessor& proc;
};

// schermino che disegna il filtro che si muove
class FilterScope : public juce::Component, private juce::Timer
{
public:
    explicit FilterScope (PaffettiProcessor& p) : proc (p) { hist.insertMultiple (0, 0.5f, 96); startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    PaffettiProcessor& proc;
    juce::Array<float> hist;
};

// display dei preset nella testata: < NOME > , clic al centro = menu per categoria
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PaffettiProcessor& p) : proc (p) { startTimerHz (8); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void step (int delta);
    void showMenu();
private:
    void timerCallback() override;
    PaffettiProcessor& proc;
    int seenIndex = -2; bool seenMod = false;
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (PaffettiProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
    CRTScreen& getScreen() { return screen; }
private:
    void timerCallback() override;
    PaffettiProcessor& proc;
    CRTScreen screen;
    PresetBar presetBar;
    PixelButton prevB { "<", pal::yellow }, nextB { ">", pal::yellow }, randB { "RANDOM", pal::green }, mineB { "+ MIEI SAMPLE", pal::aqua };
    Section sStab { "STAB", pal::yellow }, sDelay { "TAPE DELAY", pal::orange }, sFilter { "DUB FILTER", pal::green }, sPhaser { "PHASER", pal::red },
            sFlanger { "FLANGER", pal::aqua }, sSpring { "SPRING + TAPE", pal::green }, sOut { "OUTPUT", pal::cream };
    juce::OwnedArray<Knob> knobs;
    Knob *dlyDiv, *dlyMs, *phDiv, *phHz, *flDiv, *flHz, *lfoDiv, *lfoHz;
    PixelChoice dlySync, dlyPing, dlySweepDiv, phSync, phStages, flSync, poly, fltMode, lfoShape, lfoSync;
    FilterScope scope;
    Meter meter;
    KeyboardView keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String bpmText, chordText;
};

class PaffettiEditor : public juce::AudioProcessorEditor
{
public:
    explicit PaffettiEditor (PaffettiProcessor&);
    ~PaffettiEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    MainPanel& getPanel() { return panel; }
    static constexpr int W = 1120, H = 780;
private:
    RetroLNF lnf;
    MainPanel panel;
    juce::ComponentBoundsConstrainer constrainer;
};
