// ScfEngine.h
// JUCE-free DSP model of the SCF 04-L circuit (BBD chorus / flanger / pitch-mod).
//
// Signal path modelled (schematic page A):
//   input buffer + D1 clip (IC1) -> [mode routing] -> NE570 compressor (IC2)
//   -> clock-tracking 2nd-order LPF (IC3a) -> MN3007 BBD (IC4, 1024 stages)
//   -> clock-tracking 2nd-order LPF (IC3b) -> NE570 expander (IC2)
//   -> dry/wet summing (IC101) and output stage (IC1 second half)
// Clock + LFO (schematic page B):
//   triangle LFO (IC102, SPEED P101) -> WIDTH P102 -> voltage-controlled clock
//   (IC102 integrator + IC105 latch). BBD delay = stages / (2 * f_clk).
//
// Everything in `Calibration` marked PLACEHOLDER must be measured on a real unit
// (see README.md). Values marked "from schematic" were derived from component values.

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace scf
{
constexpr float kPi = 3.14159265358979323846f;

enum class Mode : int { Chorus = 0, Flanger = 1, PitchMod = 2 };

struct ModeCal
{
    float centerDelayMs; // mean BBD delay at LFO zero            PLACEHOLDER
    float clockDepth;    // fractional clock swing at WIDTH=100%  PLACEHOLDER
    float lfoRound;      // 0 = triangle, 1 = sine                PLACEHOLDER
    float feedbackMax;   // feedback at INTENSITY=100% (flanger)  PLACEHOLDER
};

struct Calibration
{
    int bbdStages = 1024; // MN3007 (IC4)

    ModeCal mode[3] = {
        { 6.5f, 0.35f, 0.0f, 0.00f }, // Chorus
        { 4.0f, 0.50f, 0.5f, 0.85f }, // Flanger
        { 6.5f, 0.45f, 0.5f, 0.00f }  // Pitch modulation (vibrato, 100 % wet)
    };

    // From schematic: R26=R27=24k, C21=470p, C22=220p -> fc 20.6 kHz, Q 0.73 (IC3a)
    //                 R16=R19=36k, C12=470p, C13=220p -> fc 13.75 kHz, Q 0.73 (IC3b)
    float preFcMax  = 20600.f;
    float postFcMax = 13750.f;
    float filterQ   = 0.731f;
    // The LM13600 (IC3) boxes make the filters follow the clock; the exact law is not
    // legible in the scan, so cutoff = min(fcMax, filterTrack * f_clk).   PLACEHOLDER
    float filterTrack = 0.20f;

    // NE570 (IC2): 2:1 compressor / 1:2 expander, average-responding detector.
    float compRef   = 0.10f;  // unity-gain level (linear, 1.0 = 0 dBFS)     PLACEHOLDER
    float compTauMs = 10.0f;  // detector time constant                      PLACEHOLDER
    float compMaxGain = 30.f; // gain-cell limit

    float bbdNoiseDb = -75.f; // noise at BBD output, before expander        PLACEHOLDER
    float bbdCeiling = 1.5f;  // BBD input soft-clip level                   PLACEHOLDER
    float inputClip  = 3.0f;  // IC1 / D1 input soft-clip level              PLACEHOLDER
    float outTrim    = 0.707f; // output level trim                          PLACEHOLDER
};

struct Params
{
    Mode  mode        = Mode::Chorus;
    float speedHz     = 0.8f;
    float width       = 0.5f;  // 0..1
    float intensity   = 0.5f;  // 0..1
    float inputGainDb = 0.0f;
    bool  stereo      = true;  // R output carries the inverted wet signal
    bool  bbdNoise    = true;
};

// ---------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass(float fc, float q, float fs)
    {
        fc = std::min(fc, 0.45f * fs);
        const float w = 2.f * kPi * fc / fs;
        const float cw = std::cos(w), sw = std::sin(w);
        const float alpha = sw / (2.f * q);
        const float a0 = 1.f + alpha;
        b0 = (1.f - cw) * 0.5f / a0;
        b1 = (1.f - cw) / a0;
        b2 = b0;
        a1 = -2.f * cw / a0;
        a2 = (1.f - alpha) / a0;
    }
    float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.f; }
};

// NE570-style half: use one instance for the compressor, another for the expander.
struct Compander
{
    float env = 0.f, coef = 0.01f, ref = 0.1f, maxGain = 30.f;

    void prepare(float fs, float tauMs, float r, float mg)
    {
        coef = 1.f - std::exp(-1.f / (tauMs * 0.001f * fs));
        ref = r;
        maxGain = mg;
        env = 0.f;
    }
    float compress(float x)
    {
        env += coef * (std::fabs(x) - env);
        const float g = std::min(std::sqrt(ref / std::max(env, 1.0e-6f)), maxGain);
        return x * g;
    }
    float expand(float y)
    {
        env += coef * (std::fabs(y) - env);
        return y * (env / ref);
    }
};

// Fractional delay line, 4-point Hermite. Read before push; minimum delay 2 samples.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare(int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }
    void push(float x)
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }
    float read(float d) const
    {
        const int i0 = (int) d;
        const float t = d - (float) i0;
        auto s = [this](int k) { return buf[(size_t) ((w - k) & mask)]; };
        const float ym1 = s(i0 - 1), y0 = s(i0), y1 = s(i0 + 1), y2 = s(i0 + 2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }
};

// ---------------------------------------------------------------------------
class Engine
{
public:
    Calibration cal;

    void prepare(double sampleRate)
    {
        fs = (float) sampleRate;
        maxDelay = 0.06f * fs;
        line.prepare((int) (0.07f * fs));
        compC.prepare(fs, cal.compTauMs, cal.compRef, cal.compMaxGain);
        compE.prepare(fs, cal.compTauMs, cal.compRef, cal.compMaxGain);
        sParam = 1.f - std::exp(-1.f / (0.02f * fs));
        sSpeed = 1.f - std::exp(-1.f / (0.05f * fs));
        noiseAmp = std::pow(10.f, cal.bbdNoiseDb / 20.f) * 1.732f;
        reset();
        speed = target.speedHz;
        width = target.width;
        intensity = target.intensity;
        gainLin = std::pow(10.f, target.inputGainDb / 20.f);
    }

    void reset()
    {
        line.prepare((int) (0.07f * fs));
        preLP.reset();
        postLP.reset();
        compC.env = compE.env = 0.f;
        phase = 0.f;
        ctr = 0;
        rng = 0x1234567u;
    }

    void setParams(const Params& p)
    {
        target = p;
    }

    void process(float in, float& outL, float& outR)
    {
        // parameter smoothing
        speed     += sSpeed * (target.speedHz - speed);
        width     += sParam * (target.width - width);
        intensity += sParam * (target.intensity - intensity);
        gainLin   += sParam * (std::pow(10.f, target.inputGainDb / 20.f) - gainLin);

        const ModeCal& mc = cal.mode[(int) target.mode];

        // LFO (IC102 triangle, optionally rounded)
        phase += speed / fs;
        if (phase >= 1.f) phase -= 1.f;
        const float tri = phase < 0.25f ? 4.f * phase
                         : phase < 0.75f ? 2.f - 4.f * phase
                                         : 4.f * phase - 4.f;
        const float lfo = (1.f - mc.lfoRound) * tri + mc.lfoRound * std::sin(2.f * kPi * phase);

        // voltage-controlled clock -> BBD delay
        const float f0 = 0.5f * (float) cal.bbdStages / (mc.centerDelayMs * 0.001f);
        const float fclk = std::max(f0 * (1.f + mc.clockDepth * width * lfo), 0.2f * f0);
        const float d = std::clamp(0.5f * (float) cal.bbdStages * fs / fclk, 2.f, maxDelay);

        if (ctr-- <= 0)
        {
            ctr = 7;
            preLP.setLowpass(std::min(cal.preFcMax, cal.filterTrack * fclk), cal.filterQ, fs);
            postLP.setLowpass(std::min(cal.postFcMax, cal.filterTrack * fclk), cal.filterQ, fs);
        }

        // input buffer (IC1 + D1)
        const float x = cal.inputClip * std::tanh(in * gainLin / cal.inputClip);

        // BBD output (read first -> no zero-delay feedback loop)
        float wet = line.read(d);
        if (target.bbdNoise) wet += noiseAmp * nextNoise();
        wet = postLP.process(wet);
        wet = compE.expand(wet);

        // BBD input: dry + feedback -> compressor -> anti-alias filter -> BBD
        float u = x + mc.feedbackMax * intensity * wet;
        u = compC.compress(u);
        u = preLP.process(u);
        u = cal.bbdCeiling * std::tanh(u / cal.bbdCeiling);
        line.push(u);

        // mode routing / output stage
        float l, r;
        switch (target.mode)
        {
            case Mode::Chorus:
                l = x + intensity * wet;
                r = target.stereo ? x - intensity * wet : l;
                break;
            case Mode::Flanger:
                l = x + wet;
                r = target.stereo ? x - wet : l;
                break;
            default: // pitch modulation: 100 % wet
                l = wet;
                r = wet;
                break;
        }
        outL = l * cal.outTrim;
        outR = r * cal.outTrim;
    }

private:
    float nextNoise()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) (int32_t) rng * (1.f / 2147483648.f);
    }

    float fs = 48000.f, maxDelay = 2880.f;
    Params target;
    float speed = 0.8f, width = 0.5f, intensity = 0.5f, gainLin = 1.f;
    float sParam = 0.001f, sSpeed = 0.001f, noiseAmp = 0.f;
    float phase = 0.f;
    int ctr = 0;
    uint32_t rng = 0x1234567u;
    Biquad preLP, postLP;
    Compander compC, compE;
    DelayLine line;
};

} // namespace scf
