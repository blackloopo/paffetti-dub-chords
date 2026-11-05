#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>

namespace dub
{
constexpr float twoPi = 6.283185307179586f;

// linea di ritardo con lettura interpolata (Hermite)
struct DelayLine
{
    std::vector<float> buf; int w = 0, mask = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float d) const   // d in campioni, >= 1
    {
        float rp = (float) w - d;
        int i = (int) std::floor (rp); float f = rp - (float) i;
        float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)], y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
        float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
};

struct OnePole
{
    float a = 0, z = 0;
    void setLP (float fc, float sr) { a = 1.0f - std::exp (-twoPi * fc / sr); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

// LFO sincronizzabile: fase da ppq se il trasporto è in play
struct LFO
{
    double phase = 0;
    float sine() const { return std::sin (twoPi * (float) phase); }
    float tri() const { float p = (float) phase; return p < 0.5f ? 4 * p - 1 : 3 - 4 * p; }
    void advance (double hz, double sr) { phase += hz / sr; phase -= std::floor (phase); }
};

// ---------------- Phaser (4/8/12 stadi, stereo in quadratura) ----------------
struct Phaser
{
    static constexpr int maxStages = 12;
    float sr = 44100;
    std::array<std::array<float, maxStages>, 2> z {};
    std::array<float, 2> fb {};
    LFO lfo;

    void prepare (double s) { sr = (float) s; reset(); }
    void reset() { for (auto& c : z) c.fill (0); fb.fill (0); }

    void process (float* L, float* R, int n, int stages, double rateHz, float depth, float feedback, float mix)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                double ph = lfo.phase + (ch ? 0.25 : 0.0); ph -= std::floor (ph);
                float t = (float) ph; float tri = t < 0.5f ? 4 * t - 1 : 3 - 4 * t;
                float sweep = 0.5f * (tri + 1.0f);
                float fc = 180.0f * std::pow (2.0f, sweep * (1.0f + depth * 4.6f));
                float tn = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, sr * 0.45f) / sr);
                float a = (tn - 1.0f) / (tn + 1.0f);
                float* x = ch ? R : L;
                float in = x[i] + std::tanh (fb[(size_t) ch] * feedback);
                for (int s = 0; s < stages; ++s)
                {
                    float y = a * in + z[(size_t) ch][(size_t) s];
                    z[(size_t) ch][(size_t) s] = in - a * y;
                    in = y;
                }
                fb[(size_t) ch] = in;
                x[i] = x[i] * (1.0f - 0.5f * mix) + in * 0.5f * mix;
            }
            lfo.advance (rateHz, sr);
        }
    }
};

// ---------------- Flanger ----------------
struct Flanger
{
    float sr = 44100;
    DelayLine d[2]; std::array<float, 2> fb {};
    LFO lfo;
    void prepare (double s) { sr = (float) s; for (auto& x : d) x.prepare ((int) (sr * 0.03f)); fb.fill (0); }
    void reset() { for (auto& x : d) x.clear(); fb.fill (0); }
    void process (float* L, float* R, int n, double rateHz, float depth, float feedback, float mix)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                double ph = lfo.phase + (ch ? 0.25 : 0.0); ph -= std::floor (ph);
                float t = (float) ph; float tri = t < 0.5f ? 4 * t - 1 : 3 - 4 * t;
                float ms = 0.35f + depth * 7.0f * 0.5f * (tri + 1.0f);
                float* x = ch ? R : L;
                d[ch].push (x[i] + juce::jlimit (-1.5f, 1.5f, fb[(size_t) ch] * feedback));
                float y = d[ch].read (juce::jmax (1.5f, ms * 0.001f * sr));
                fb[(size_t) ch] = y;
                x[i] = x[i] * (1.0f - 0.5f * mix) + y * 0.5f * mix;
            }
            lfo.advance (rateHz, sr);
        }
    }
};

// ---------------- Tape delay (stile Space Echo) ----------------
struct TapeDelay
{
    float sr = 44100;
    DelayLine d[2];
    OnePole hp[2], lp1[2], lp2[2];
    float curDelay = 0; bool first = true;
    double t = 0;
    juce::Random rnd;
    float drift = 0, driftTarget = 0;

    void prepare (double s) { sr = (float) s; for (auto& x : d) x.prepare ((int) (sr * 4.0f)); reset(); }
    void reset() { for (auto& x : d) x.clear(); for (int c = 0; c < 2; ++c) { hp[c].reset(); lp1[c].reset(); lp2[c].reset(); } first = true; }

    void process (float* L, float* R, int n, float delaySamples, float feedback, float wow, float toneHz, float mix, bool pingPong)
    {
        delaySamples = juce::jlimit (8.0f, sr * 3.9f, delaySamples);
        if (first) { curDelay = delaySamples; first = false; }
        for (int c = 0; c < 2; ++c) { hp[c].setLP (110.0f, sr); lp1[c].setLP (toneHz, sr); lp2[c].setLP (toneHz * 1.3f, sr); }
        const float glide = 1.0f - std::exp (-1.0f / (0.25f * sr));   // cambio tempo = pitch glide da nastro
        for (int i = 0; i < n; ++i)
        {
            curDelay += glide * (delaySamples - curDelay);
            t += 1.0 / sr;
            if ((i & 255) == 0) driftTarget = (rnd.nextFloat() * 2 - 1);
            drift += 0.0005f * (driftTarget - drift);
            float mod = wow * (sr * 0.0022f * std::sin (twoPi * 0.55f * (float) t)
                             + sr * 0.00035f * std::sin (twoPi * 6.3f * (float) t)
                             + sr * 0.0006f * drift);
            float yl = d[0].read (curDelay + mod + 2.0f);
            float yr = d[1].read (curDelay * (pingPong ? 1.0f : 1.012f) + mod + 2.0f);
            auto shape = [&] (int c, float y)
            {
                y = hp[c].hp (y); y = lp2[c].lp (lp1[c].lp (y));
                return std::tanh (y * 1.4f) / 1.4f;          // saturazione del nastro nel loop
            };
            float fl = shape (0, yl), fr = shape (1, yr);
            float inL = L[i], inR = R[i];
            if (pingPong)
            {
                d[0].push (0.5f * (inL + inR) + fr * feedback);
                d[1].push (fl * feedback);
            }
            else
            {
                d[0].push (inL + fl * feedback);
                d[1].push (inR + fr * feedback);
            }
            L[i] = inL + fl * mix * 1.2f;
            R[i] = inR + fr * mix * 1.2f;
        }
    }
};

// ---------------- Riverbero a molla ----------------
struct Spring
{
    static constexpr int nAP = 48;
    float sr = 44100;
    DelayLine loop[2];
    std::array<std::array<float, nAP>, 2> apz {};
    OnePole damp[2], dc[2], outHP[2];
    LFO lfo[2];
    void prepare (double s)
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c) { loop[c].prepare ((int) (sr * 0.1f)); damp[c].setLP (4200.0f, sr); dc[c].setLP (40.0f, sr); outHP[c].setLP (220.0f, sr); }
        reset();
    }
    void reset() { for (int c = 0; c < 2; ++c) { loop[c].clear(); apz[(size_t) c].fill (0); damp[c].reset(); dc[c].reset(); outHP[c].reset(); } }
    void process (float* L, float* R, int n, float decay, float mix)
    {
        const float lens[2] = { 0.0371f * sr, 0.0433f * sr };
        const float coef[2] = { 0.62f, 0.65f };
        for (int i = 0; i < n; ++i)
        {
            float in = 0.5f * (L[i] + R[i]);
            for (int c = 0; c < 2; ++c)
            {
                lfo[c].advance (c ? 0.33 : 0.23, sr);
                float fbk = loop[c].read (lens[c] + 5.0f * lfo[c].sine());
                float x = in + fbk * decay;
                float a = coef[c];
                for (int k = 0; k < nAP; ++k) { float y = -a * x + apz[(size_t) c][(size_t) k]; apz[(size_t) c][(size_t) k] = x + a * y; x = y; }
                x = damp[c].lp (x); x = dc[c].hp (x);
                loop[c].push (std::tanh (x * 1.2f) / 1.2f);
                float wet = outHP[c].hp (x);
                (c ? R : L)[i] += wet * mix * 1.6f;
            }
        }
    }
};

// fruscio da nastro (rumore rosa)
struct Hiss
{
    juce::Random r; float b0 = 0, b1 = 0, b2 = 0;
    float next() { float w = r.nextFloat() * 2 - 1; b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f; return (b0 + b1 + b2 + w * 0.1848f) * 0.11f; }
};
} // namespace dub
