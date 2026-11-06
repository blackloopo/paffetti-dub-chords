#include "PluginEditor.h"
#include "BinaryData.h"

// ======================= font =======================
static juce::Typeface::Ptr pixelFace()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize);
    return t;
}
static juce::Typeface::Ptr vtFace()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::VT323Regular_ttf, BinaryData::VT323Regular_ttfSize);
    return t;
}
juce::Font RetroFonts::pixel (float h) { return juce::Font (juce::FontOptions (pixelFace()).withHeight (h)); }
juce::Font RetroFonts::vt (float h)    { return juce::Font (juce::FontOptions (vtFace()).withHeight (h)); }

static void pixelText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c,
                       juce::Justification j = juce::Justification::centredLeft)
{
    g.setFont (RetroFonts::pixel (size)); g.setColour (c);
    g.drawText (t, r, j, false);
}

// ======================= look and feel =======================
RetroLNF::RetroLNF()
{
    setColour (juce::ResizableWindow::backgroundColourId, pal::body);
}

void RetroLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    float d = juce::jmin (r.getWidth(), r.getHeight());
    auto c = r.withSizeKeepingCentre (d, d);
    // ombra a gradino
    g.setColour (pal::edge);
    g.fillEllipse (c.translated (0, 3));
    // arco valore
    juce::Path arc;
    arc.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.5f + 1.5f, d * 0.5f + 1.5f, 0, start, start + (end - start) * pos, true);
    // corpo
    juce::ColourGradient grad (juce::Colour (0xfffff3d6), c.getX() + d * 0.35f, c.getY() + d * 0.3f,
                               juce::Colour (0xff7a4a10), c.getRight(), c.getBottom(), true);
    grad.addColour (0.3, pal::yellow);
    grad.addColour (0.75, juce::Colour (0xffc9861b));
    g.setGradientFill (grad);
    g.fillEllipse (c);
    g.setColour (pal::edge);
    g.drawEllipse (c, 3.0f);
    // tacca
    float ang = start + (end - start) * pos;
    juce::Path notch;
    notch.addRectangle (-2.0f, -d * 0.5f + 3.0f, 4.0f, d * 0.3f);
    g.fillPath (notch, juce::AffineTransform::rotation (ang).translated (c.getCentreX(), c.getCentreY()));
    // pallini della scala
    g.setColour (pal::aqua.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        float a = start + (end - start) * (float) i / 10.0f;
        if (a > ang + 0.001f) g.setColour (pal::rim.brighter (0.4f));
        float rr = d * 0.5f + 5.0f;
        g.fillRect (juce::Rectangle<float> (2.0f, 2.0f).withCentre ({ c.getCentreX() + std::sin (a) * rr, c.getCentreY() - std::cos (a) * rr }));
    }
}

// ======================= manopola =======================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l) : label (l)
{
    param = s.getParameter (id);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setVelocityBasedMode (false);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (param != nullptr) slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().removeFromTop (48).withSizeKeepingCentre (48, 48));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (50);
    pixelText (g, label, r.removeFromTop (11), 7.0f, pal::cream, juce::Justification::centred);
    g.setFont (RetroFonts::vt (18.0f)); g.setColour (pal::aqua);
    g.drawText (param != nullptr ? param->getCurrentValueAsText() : juce::String(), r.removeFromTop (16), juce::Justification::centred, false);
}

// ======================= scelte a pixel =======================
PixelChoice::PixelChoice (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::StringArray l, juce::Array<int> v)
    : labels (std::move (l)), values (std::move (v))
{
    param = s.getParameter (id);
    if (values.isEmpty()) for (int i = 0; i < labels.size(); ++i) values.add (i);
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float val) { current = juce::roundToInt (val); repaint(); });
    att->sendInitialUpdate();
}

int PixelChoice::preferredWidth() const
{
    int w = 0;
    for (auto& l : labels) w += l.length() * 7 + 14;
    return w + (labels.size() - 1) * 4;
}

void PixelChoice::paint (juce::Graphics& g)
{
    int x = 0;
    for (int i = 0; i < labels.size(); ++i)
    {
        int w = labels[i].length() * 7 + 14;
        juce::Rectangle<int> r (x, 0, w, getHeight());
        bool on = labels.size() == 1 ? current == 1 : values[i] == current;
        g.setColour (on ? pal::aqua : pal::panel);
        g.fillRect (r);
        g.setColour (on ? pal::aqua : pal::rim.brighter (0.2f));
        g.drawRect (r, 2);
        pixelText (g, labels[i], r, 7.0f, on ? pal::bg : pal::tan, juce::Justification::centred);
        x += w + 4;
    }
}

void PixelChoice::mouseDown (const juce::MouseEvent& e)
{
    int x = 0;
    for (int i = 0; i < labels.size(); ++i)
    {
        int w = labels[i].length() * 7 + 14;
        if (e.x >= x && e.x < x + w)
        {
            att->setValueAsCompleteGesture (labels.size() == 1 ? (current == 1 ? 0.0f : 1.0f) : (float) values[i]);
            return;
        }
        x += w + 4;
    }
}

void PixelButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().withTrimmedBottom (3);
    if (down) r.translate (0, 2);
    g.setColour (pal::edge);
    g.fillRect (r.translated (0, down ? 1 : 3));
    g.setColour (over ? colour.brighter (0.15f) : colour);
    g.fillRect (r);
    g.setColour (pal::edge);
    g.drawRect (r, 3);
    pixelText (g, getButtonText(), r, 8.0f, pal::bg, juce::Justification::centred);
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (pal::panel); g.fillRect (r);
    g.setColour (pal::edge); g.drawRect (r, 3);
    g.setColour (pal::rim); g.drawRect (r.reduced (3), 3);
    g.setFont (RetroFonts::pixel (9.0f));
    int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 16;
    juce::Rectangle<int> tag (12, 12, tw, 20);
    g.setColour (colour); g.fillRect (tag);
    pixelText (g, title, tag, 9.0f, pal::bg, juce::Justification::centred);
}

void Meter::paint (juce::Graphics& g)
{
    float pk = proc.meterPeak.load();
    float dbv = juce::Decibels::gainToDecibels (pk, -60.0f);
    int lit = juce::jlimit (0, 8, (int) std::ceil ((dbv + 48.0f) / 6.0f));
    const juce::Colour cols[] = { pal::green, pal::green, pal::green, pal::yellow, pal::yellow, pal::orange, pal::orange, pal::red };
    int bw = 8, gap = 3;
    for (int i = 0; i < 8; ++i)
    {
        int h = 10 + i * 6;
        g.setColour (i < lit ? cols[i] : pal::rim);
        g.fillRect (i * (bw + gap), getHeight() - h, bw, h);
    }
}

// ======================= schermo CRT =======================
CRTScreen::CRTScreen (PaffettiProcessor& p) : proc (p)
{
    category = proc.getSelectedCategory();
    refresh();
    startTimerHz (12);
}

void CRTScreen::refresh()
{
    entries = proc.getLibrary().getEntries (category);
    int sel = -1;
    for (int i = 0; i < entries.size(); ++i) if (category == proc.getSelectedCategory() && entries[i].name == proc.getSelectedName()) sel = i;
    if (sel >= 0) scroll = juce::jlimit (0, juce::jmax (0, entries.size() - rows), sel - rows / 2);
    else scroll = 0;
    buildWave();
    repaint();
}

void CRTScreen::showCategory (const juce::String& id) { category = id; refresh(); }

void CRTScreen::buildWave()
{
    wave.clear();
    if (auto s = proc.getCurrentSample())
    {
        auto& b = s->buffer; int n = b.getNumSamples(); const int cols = 64;
        for (int c = 0; c < cols; ++c)
        {
            int a = n * c / cols, e = n * (c + 1) / cols; float m = 0;
            for (int i = a; i < e; i += 4) m = juce::jmax (m, std::abs (b.getSample (0, i)));
            wave.add (m);
        }
    }
    seenVersion = proc.sampleVersion.load();
}

void CRTScreen::timerCallback()
{
    ++blink;
    if (seenVersion != proc.sampleVersion.load()) { category = proc.getSelectedCategory(); refresh(); }
    else repaint (getLocalBounds().removeFromBottom (60));
}

juce::Rectangle<int> CRTScreen::tabRect (int i) const
{
    auto& cats = proc.getLibrary().getCategories();
    int x = 12;
    for (int k = 0; k < i; ++k) x += (cats[k].id == "MY" ? 44 : 32) + 5;
    return { x, 12, cats[i].id == "MY" ? 44 : 32, 22 };
}

void CRTScreen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (pal::screen);
    g.fillRoundedRectangle (r, 14.0f);

    auto& cats = proc.getLibrary().getCategories();
    for (int i = 0; i < cats.size(); ++i)
    {
        auto t = tabRect (i);
        bool on = cats[i].id == category;
        auto col = cats[i].id == "MY" ? pal::aqua : pal::orange;
        if (on) { g.setColour (col); g.fillRect (t); }
        g.setColour (col); g.drawRect (t, 2);
        pixelText (g, cats[i].id == "MY" ? "MIEI" : cats[i].id, t, 8.0f, on ? pal::bg : col, juce::Justification::centred);
    }

    juce::String catName;
    for (auto& c : cats) if (c.id == category) catName = c.name;
    pixelText (g, catName.toUpperCase() + "  " + juce::String (entries.size()) + " STAB", { 14, 42, getWidth() - 28, 14 }, 8.0f, pal::red);

    int y = 62;
    if (entries.isEmpty())
    {
        g.setFont (RetroFonts::vt (22.0f)); g.setColour (pal::yellow);
        g.drawFittedText (category == "MY" ? "Premi + MIEI SAMPLE\ne scegli la tua cartella"
                                           : "Libreria non trovata", { 14, y + 10, getWidth() - 28, 60 }, juce::Justification::topLeft, 3);
    }
    for (int i = 0; i < rows && scroll + i < entries.size(); ++i)
    {
        auto& e = entries.getReference (scroll + i);
        bool sel = category == proc.getSelectedCategory() && e.name == proc.getSelectedName();
        juce::Rectangle<int> row (10, y + i * 24, getWidth() - 20, 23);
        if (sel) { g.setColour (pal::yellow); g.fillRect (row); }
        g.setFont (RetroFonts::vt (24.0f));
        g.setColour (sel ? pal::bg : pal::yellow);
        auto nm = e.name.fromFirstOccurrenceOf ("-", false, false);
        if (nm.isEmpty()) nm = e.name;
        g.drawText ((sel ? "> " : "  ") + nm, row.withTrimmedLeft (6), juce::Justification::centredLeft, true);
    }
    // barra di scorrimento
    if (entries.size() > rows)
    {
        float frac = (float) rows / (float) entries.size();
        float top = (float) scroll / (float) entries.size();
        auto track = juce::Rectangle<float> ((float) getWidth() - 8.0f, (float) y, 3.0f, rows * 24.0f);
        g.setColour (pal::rim); g.fillRect (track);
        g.setColour (pal::orange); g.fillRect (track.withY (track.getY() + top * track.getHeight()).withHeight (frac * track.getHeight()));
    }

    // forma d'onda dello stab selezionato
    auto wr = juce::Rectangle<float> (14.0f, (float) getHeight() - 48.0f, 200.0f, 36.0f);
    g.setColour (pal::aqua);
    if (! wave.isEmpty())
    {
        float bw = wr.getWidth() / (float) wave.size();
        for (int i = 0; i < wave.size(); ++i)
        {
            float h = juce::jmax (2.0f, wave[i] * wr.getHeight());
            g.fillRect (juce::Rectangle<float> (wr.getX() + i * bw, wr.getCentreY() - h * 0.5f, juce::jmax (1.0f, bw - 1.0f), h));
        }
    }
    if ((blink / 6) % 2 == 0)
        pixelText (g, "PREMI UN TASTO", { getWidth() - 170, getHeight() - 36, 158, 14 }, 8.0f, pal::yellow, juce::Justification::centredRight);

    // scanline e bagliore
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (int yy = 0; yy < getHeight(); yy += 3) g.fillRect (0, yy, getWidth(), 1);
    juce::ColourGradient glow (pal::orange.withAlpha (0.0f), r.getCentreX(), r.getCentreY(), pal::orange.withAlpha (0.16f), 0, 0, true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colour (0xff2e1608));
    g.drawRoundedRectangle (r.reduced (1.0f), 14.0f, 2.0f);
}

void CRTScreen::mouseDown (const juce::MouseEvent& e)
{
    auto& cats = proc.getLibrary().getCategories();
    for (int i = 0; i < cats.size(); ++i)
        if (tabRect (i).contains (e.getPosition())) { showCategory (cats[i].id); return; }
    int y = 62;
    int idx = (e.y - y) / 24;
    if (e.y >= y && idx >= 0 && idx < rows && scroll + idx < entries.size())
    {
        proc.selectSample (category, entries[scroll + idx].name);
        buildWave(); repaint();
    }
}

void CRTScreen::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    int d = w.deltaY > 0 ? -1 : (w.deltaY < 0 ? 1 : 0);
    scroll = juce::jlimit (0, juce::jmax (0, entries.size() - rows), scroll + d);
    repaint();
}

void CRTScreen::step (int delta)
{
    if (entries.isEmpty()) return;
    int sel = -1;
    for (int i = 0; i < entries.size(); ++i) if (category == proc.getSelectedCategory() && entries[i].name == proc.getSelectedName()) sel = i;
    sel = (sel < 0 ? 0 : (sel + delta + entries.size()) % entries.size());
    proc.selectSample (category, entries[sel].name);
    refresh();
}

void CRTScreen::randomPick()
{
    if (entries.isEmpty()) return;
    proc.selectSample (category, entries[juce::Random::getSystemRandom().nextInt (entries.size())].name);
    refresh();
}

// ======================= tastiera =======================
KeyboardView::KeyboardView (PaffettiProcessor& p) : proc (p) { startTimerHz (25); }

static bool isBlack (int n) { int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

void KeyboardView::paint (juce::Graphics& g)
{
    float kw = (float) getWidth() / (float) numWhite, h = (float) getHeight();
    int wi = 0;
    for (int n = lowNote; wi < numWhite; ++n)
    {
        if (isBlack (n)) continue;
        juce::Rectangle<float> r (wi * kw, 0, kw, h);
        bool on = proc.keyboardState.isNoteOnForChannels (0xffff, n);
        g.setColour (on ? pal::yellow : pal::cream); g.fillRect (r);
        g.setColour (pal::edge); g.drawRect (r, 2.0f);
        if (n % 12 == 0) { g.setFont (RetroFonts::pixel (6.0f)); g.setColour (pal::tan); g.drawText ("C" + juce::String (n / 12 - 2), r.withTrimmedBottom (4), juce::Justification::centredBottom); }
        ++wi;
    }
    wi = 0;
    for (int n = lowNote; wi < numWhite; ++n)
    {
        if (isBlack (n)) { juce::Rectangle<float> r (wi * kw - kw * 0.33f, 0, kw * 0.66f, h * 0.6f);
            bool on = proc.keyboardState.isNoteOnForChannels (0xffff, n);
            g.setColour (on ? pal::orange : pal::panel); g.fillRect (r);
            g.setColour (pal::edge); g.drawRect (r, 2.0f); }
        else ++wi;
    }
}

int KeyboardView::noteAt (juce::Point<float> p) const
{
    float kw = (float) getWidth() / (float) numWhite, h = (float) getHeight();
    int wi = 0;
    if (p.y < h * 0.6f)
        for (int n = lowNote; wi < numWhite; ++n)
        {
            if (isBlack (n)) { if (juce::Rectangle<float> (wi * kw - kw * 0.33f, 0, kw * 0.66f, h * 0.6f).contains (p)) return n; }
            else ++wi;
        }
    wi = 0;
    for (int n = lowNote; wi < numWhite; ++n)
    {
        if (isBlack (n)) continue;
        if (p.x >= wi * kw && p.x < (wi + 1) * kw) return n;
        ++wi;
    }
    return -1;
}

void KeyboardView::mouseDown (const juce::MouseEvent& e)
{
    mouseNote = noteAt (e.position);
    if (mouseNote >= 0) proc.keyboardState.noteOn (1, mouseNote, 0.85f);
}
void KeyboardView::mouseDrag (const juce::MouseEvent& e)
{
    int n = noteAt (e.position);
    if (n != mouseNote) { if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f); mouseNote = n; if (n >= 0) proc.keyboardState.noteOn (1, n, 0.85f); }
}
void KeyboardView::mouseUp (const juce::MouseEvent&)
{
    if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f);
    mouseNote = -1;
}

// ======================= scope del filtro =======================
void FilterScope::timerCallback()
{
    float fc = proc.displayCutoff.load();
    float v = juce::jlimit (0.0f, 1.0f, (std::log2 (juce::jmax (40.0f, fc)) - std::log2 (40.0f)) / (std::log2 (16000.0f) - std::log2 (40.0f)));
    hist.remove (0); hist.add (v);
    repaint();
}

void FilterScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (pal::screen); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (pal::rim); g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 2.0f);
    auto in = r.reduced (8.0f, 8.0f);
    float px = in.getWidth() / (float) hist.size();
    for (int i = 0; i < hist.size(); ++i)
    {
        float y = in.getBottom() - hist[i] * in.getHeight();
        g.setColour ((i == hist.size() - 1 ? pal::yellow : pal::aqua).withAlpha (0.35f + 0.65f * (float) i / (float) hist.size()));
        g.fillRect (juce::Rectangle<float> (in.getX() + i * px, y - 1.5f, juce::jmax (2.0f, px), 3.0f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int yy = 0; yy < getHeight(); yy += 3) g.fillRect (0, yy, getWidth(), 1);
    float fc = proc.displayCutoff.load();
    auto txt = fc >= 19000 ? juce::String ("OPEN") : (fc >= 1000 ? juce::String (fc / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (fc)));
    g.setFont (RetroFonts::pixel (7.0f)); g.setColour (pal::yellow);
    g.drawText ("CUTOFF " + txt, getLocalBounds().reduced (8, 6), juce::Justification::topRight);
}

// ======================= pannello principale =======================
MainPanel::MainPanel (PaffettiProcessor& p)
    : proc (p), screen (p),
      dlySync (p.apvts, "dlySync", { "SYNC" }),
      dlyPing (p.apvts, "dlyPing", { "PING" }),
      dlySweepDiv (p.apvts, "dlySweepDiv", { "2", "8", "32" }),
      phSync (p.apvts, "phSync", { "SYNC" }),
      phStages (p.apvts, "phStages", { "4", "8", "12" }),
      flSync (p.apvts, "flSync", { "SYNC" }),
      poly (p.apvts, "poly", { "1", "4", "8" }),
      fltMode (p.apvts, "fltMode", { "LP", "BP", "HP" }),
      lfoShape (p.apvts, "lfoShape", { "SIN", "TRI", "RMP", "S&H" }),
      lfoSync (p.apvts, "lfoSync", { "SYNC" }),
      scope (p), meter (p), keyboard (p)
{
    for (auto* s : { &sStab, &sDelay, &sFilter, &sPhaser, &sFlanger, &sSpring, &sOut }) addAndMakeVisible (s);
    addAndMakeVisible (screen);
    for (auto* b : { &prevB, &nextB, &randB, &mineB }) addAndMakeVisible (b);
    prevB.onClick = [this] { screen.step (-1); };
    nextB.onClick = [this] { screen.step (1); };
    randB.onClick = [this] { screen.randomPick(); };
    mineB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Scegli la cartella con i tuoi sample", proc.getLibrary().getUserFolder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.isDirectory()) { proc.setUserFolder (f); screen.showCategory ("MY"); }
            });
    };

    auto mk = [&] (Section& s, const char* id, const char* label) -> Knob*
    {
        auto* k = knobs.add (new Knob (proc.apvts, id, label));
        s.addAndMakeVisible (k);
        return k;
    };
    mk (sStab, "attack", "ATTACK"); mk (sStab, "decay", "DECAY"); mk (sStab, "release", "REL");
    mk (sStab, "tune", "TUNE"); mk (sStab, "drift", "DRIFT");
    dlyDiv = mk (sDelay, "dlyDiv", "TIME"); dlyMs = mk (sDelay, "dlyMs", "TIME");
    mk (sDelay, "dlyFeed", "FEED"); mk (sDelay, "dlyWow", "WOW"); mk (sDelay, "dlyTone", "TONE"); mk (sDelay, "dlySweep", "SWEEP"); mk (sDelay, "dlyMix", "MIX");
    mk (sFilter, "cutoff", "CUTOFF"); mk (sFilter, "reso", "RESO"); mk (sFilter, "fltEnv", "ENV"); mk (sFilter, "fltDecay", "E.DEC");
    mk (sFilter, "fltRand", "RANDOM");
    lfoDiv = mk (sFilter, "lfoDiv", "LFO"); lfoHz = mk (sFilter, "lfoHz", "LFO"); mk (sFilter, "lfoDepth", "DEPTH");
    phDiv = mk (sPhaser, "phDiv", "RATE"); phHz = mk (sPhaser, "phHz", "RATE");
    mk (sPhaser, "phDepth", "DEPTH"); mk (sPhaser, "phFeed", "FEED"); mk (sPhaser, "phMix", "MIX");
    flDiv = mk (sFlanger, "flDiv", "RATE"); flHz = mk (sFlanger, "flHz", "RATE");
    mk (sFlanger, "flDepth", "DEPTH"); mk (sFlanger, "flFeed", "FEED"); mk (sFlanger, "flMix", "MIX");
    mk (sSpring, "spMix", "SPRING"); mk (sSpring, "spDecay", "DECAY"); mk (sSpring, "sat", "SATUR"); mk (sSpring, "hiss", "HISS");
    mk (sOut, "low", "LOW"); mk (sOut, "high", "HIGH"); mk (sOut, "volume", "VOLUME");

    sDelay.addAndMakeVisible (dlySync); sDelay.addAndMakeVisible (dlyPing); sDelay.addAndMakeVisible (dlySweepDiv);
    sFilter.addAndMakeVisible (fltMode); sFilter.addAndMakeVisible (lfoShape); sFilter.addAndMakeVisible (lfoSync); sFilter.addAndMakeVisible (scope);
    sPhaser.addAndMakeVisible (phSync); sPhaser.addAndMakeVisible (phStages);
    sFlanger.addAndMakeVisible (flSync);
    sOut.addAndMakeVisible (meter);
    addAndMakeVisible (poly);
    addAndMakeVisible (keyboard);
    timerCallback();
    startTimerHz (10);
}

void MainPanel::timerCallback()
{
    auto on = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    dlyDiv->setVisible (on ("dlySync")); dlyMs->setVisible (! on ("dlySync"));
    phDiv->setVisible (on ("phSync"));   phHz->setVisible (! on ("phSync"));
    flDiv->setVisible (on ("flSync"));   flHz->setVisible (! on ("flSync"));
    lfoDiv->setVisible (on ("lfoSync")); lfoHz->setVisible (! on ("lfoSync"));

    auto bt = "SYNC " + juce::String (proc.hostBpm.load(), 1);
    juce::String ct;
    int ln = proc.lastNote.load();
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    if (auto s = proc.getCurrentSample())
    {
        auto chord = s->name.fromFirstOccurrenceOf (" ", false, false);
        if (chord.isEmpty()) chord = s->name;
        if (ln >= 0)
        {
            int home = 60 + s->rootMidi % 12;
            int shift = ln - home + (int) proc.apvts.getRawParameterValue ("tune")->load();
            int rootPc = ((s->rootMidi % 12) + shift % 12 + 24) % 12;
            // sostituisce la nota iniziale del nome con quella suonata
            int skip = (chord.length() > 1 && (chord[1] == 'b' || chord[1] == '#')) ? 2 : 1;
            auto qual = chord.substring (skip);
            ct = juce::String (names[ln % 12]) + juce::String (ln / 12 - 2) + " > " + names[rootPc] + qual;
        }
        else ct = "C3 > " + chord;
    }
    if (bt != bpmText || ct != chordText) { bpmText = bt; chordText = ct; repaint(); }
}

void MainPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.fillAll (pal::body);
    // testata
    auto head = r.removeFromTop (70);
    g.setColour (pal::bg); g.fillRect (head);
    g.setColour (pal::edge); g.fillRect (head.removeFromBottom (6));
    const juce::Colour stripes[] = { pal::yellow, pal::orange, pal::red, pal::aqua, pal::green };
    for (int i = 0; i < 5; ++i) { g.setColour (stripes[i]); g.fillRect (22 + i * 26, 13, 26, 6); }
    g.setFont (RetroFonts::pixel (20.0f));
    const juce::String title = "PAFFETTI DUB CHORDS";
    g.setColour (juce::Colour (0xff6b2a0c)); g.drawText (title, 28, 30, 600, 26, juce::Justification::centredLeft);
    g.setColour (pal::red);                  g.drawText (title, 25, 27, 600, 26, juce::Justification::centredLeft);
    g.setColour (pal::yellow);               g.drawText (title, 22, 24, 600, 26, juce::Justification::centredLeft);

    pixelText (g, "POWERED BY", { 760, 20, 130, 12 }, 8.0f, pal::tan, juce::Justification::centredRight);
    pixelText (g, "BLACK LOOPS", { 760, 36, 130, 12 }, 8.0f, pal::cream, juce::Justification::centredRight);
    g.setColour (pal::green.withAlpha (0.35f)); g.fillEllipse (910.0f, 23.0f, 22.0f, 22.0f);
    g.setColour (pal::green); g.fillRect (915, 28, 12, 12);
    g.setColour (pal::edge); g.drawRect (915, 28, 12, 12, 2);
    pixelText (g, bpmText, { 936, 26, 170, 16 }, 8.0f, pal::green);

    // cornice dello schermo
    auto frame = juce::Rectangle<int> (18, 84, 430, 526);
    g.setColour (pal::red); g.fillRect (frame.expanded (3));
    g.setColour (pal::edge); g.fillRect (frame);
    g.setColour (juce::Colour (0xff6b3d18)); g.drawRect (frame.reduced (2), 3);

    // area tastiera
    pixelText (g, "TASTO = ACCORDO", { 18, 652, 140, 12 }, 7.0f, pal::tan);
    g.setFont (RetroFonts::vt (22.0f)); g.setColour (pal::yellow);
    g.drawFittedText (chordText, { 18, 668, 136, 44 }, juce::Justification::topLeft, 2);
    pixelText (g, "POLY", { 1040, 696, 60, 12 }, 7.0f, pal::tan, juce::Justification::centredRight);
    pixelText (g, "v0.2", { 1040, 752, 60, 12 }, 6.0f, pal::rim.brighter (0.5f), juce::Justification::centredRight);
}

void MainPanel::resized()
{
    screen.setBounds (30, 96, 406, 452);
    prevB.setBounds (30, 560, 40, 36); nextB.setBounds (76, 560, 40, 36); randB.setBounds (122, 560, 84, 36);
    mineB.setBounds (300, 560, 136, 36);

    const int gx = 462, gy = 84, cw = 315, gap = 10, rh = 124, rs = 134;
    sStab.setBounds    (gx,            gy,          cw, rh);
    sDelay.setBounds   (gx + cw + gap, gy,          cw, rh);
    sFilter.setBounds  (gx,            gy + rs,     cw * 2 + gap, rh);
    sPhaser.setBounds  (gx,            gy + rs * 2, cw, rh);
    sFlanger.setBounds (gx + cw + gap, gy + rs * 2, cw, rh);
    sSpring.setBounds  (gx,            gy + rs * 3, cw, rh);
    sOut.setBounds     (gx + cw + gap, gy + rs * 3, cw, rh);

    auto knobsOf = [this] (Section& s) { juce::Array<Knob*> a; for (auto* k : knobs) if (k->getParentComponent() == &s) a.add (k); return a; };
    auto row = [] (juce::Array<Knob*> ks, int x0, int step, int y)
    {
        int x = x0;
        for (auto* k : ks) { k->setBounds (x, y, 48, 78); x += step; }
    };
    row (knobsOf (sStab), 12, 58, 38);

    // delay: TIME (div e ms sovrapposti) + 5 manopole
    dlyDiv->setBounds (12, 38, 48, 78); dlyMs->setBounds (12, 38, 48, 78);
    { auto ks = knobsOf (sDelay); int x = 61; for (int i = 2; i < ks.size(); ++i) { ks[i]->setBounds (x, 38, 48, 78); x += 49; } }
    {
        int w3 = dlySweepDiv.preferredWidth(), w2 = dlyPing.preferredWidth(), w1 = dlySync.preferredWidth();
        int x = sDelay.getWidth() - 12 - w3; dlySweepDiv.setBounds (x, 12, w3, 20);
        x -= 6 + w2; dlyPing.setBounds (x, 12, w2, 20);
        x -= 6 + w1; dlySync.setBounds (x, 12, w1, 20);
    }

    // dub filter: 6 manopole + LFO (div/hz sovrapposti) + scope
    {
        auto ks = knobsOf (sFilter);   // cutoff, reso, env, e.dec, random, lfoDiv, lfoHz, depth
        int x = 12;
        for (int i = 0; i < 5; ++i) { ks[i]->setBounds (x, 38, 48, 78); x += 56; }
        lfoDiv->setBounds (x, 38, 48, 78); lfoHz->setBounds (x, 38, 48, 78); x += 56;
        ks[7]->setBounds (x, 38, 48, 78); x += 64;
        scope.setBounds (x, 40, sFilter.getWidth() - x - 14, 72);
        int w3 = lfoSync.preferredWidth(), w2 = lfoShape.preferredWidth(), w1 = fltMode.preferredWidth();
        int cx = sFilter.getWidth() - 12 - w3; lfoSync.setBounds (cx, 12, w3, 20);
        cx -= 8 + w2; lfoShape.setBounds (cx, 12, w2, 20);
        cx -= 14 + w1; fltMode.setBounds (cx, 12, w1, 20);
    }

    phDiv->setBounds (12, 38, 48, 78); phHz->setBounds (12, 38, 48, 78);
    { auto ks = knobsOf (sPhaser); int x = 61; for (int i = 2; i < ks.size(); ++i) { ks[i]->setBounds (x, 38, 48, 78); x += 49; } }
    phSync.setBounds (sPhaser.getWidth() - 12 - phSync.preferredWidth() - phStages.preferredWidth() - 8, 12, phSync.preferredWidth(), 20);
    phStages.setBounds (sPhaser.getWidth() - 12 - phStages.preferredWidth(), 12, phStages.preferredWidth(), 20);

    flDiv->setBounds (12, 38, 48, 78); flHz->setBounds (12, 38, 48, 78);
    { auto ks = knobsOf (sFlanger); int x = 61; for (int i = 2; i < ks.size(); ++i) { ks[i]->setBounds (x, 38, 48, 78); x += 49; } }
    flSync.setBounds (sFlanger.getWidth() - 12 - flSync.preferredWidth(), 12, flSync.preferredWidth(), 20);

    row (knobsOf (sSpring), 12, 49, 38);
    row (knobsOf (sOut), 12, 49, 38);
    meter.setBounds (200, 48, 90, 62);

    keyboard.setBounds (166, 632, 856, 134);
    poly.setBounds (1100 - poly.preferredWidth(), 714, poly.preferredWidth(), 20);
}

// ======================= editor =======================
PaffettiEditor::PaffettiEditor (PaffettiProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, W, H);
    constrainer.setFixedAspectRatio ((double) W / H);
    constrainer.setSizeLimits (W * 3 / 5, H * 3 / 5, W * 3 / 2, H * 3 / 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (W, H);
}

PaffettiEditor::~PaffettiEditor() { setLookAndFeel (nullptr); }

void PaffettiEditor::paint (juce::Graphics& g) { g.fillAll (pal::body); }

void PaffettiEditor::resized()
{
    float s = (float) getWidth() / (float) W;
    panel.setTransform (juce::AffineTransform::scale (s));
}
