// Build: g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test
#include "ScfEngine.h"
#include <cstdio>

using namespace scf;

static double measure(Mode mode, float ampDb, float freq, float& gainOut, float& delayMsOut,
                      float width = 0.f, bool noise = false, int calFrom = -1)
{
    const double fs = 48000.0;
    Engine e;
    Params p;
    p.mode = mode;
    p.width = width;
    p.intensity = 1.f;
    p.bbdNoise = noise;
    p.stereo = false;
    if (calFrom >= 0) { e.cal.mode[2] = e.cal.mode[calFrom]; e.cal.mode[2].feedbackMax = 0.f; }
    e.setParams(p);
    e.prepare(fs);

    const int N = (int) (fs * 2.0), skip = (int) (fs * 1.0);
    const float A = std::pow(10.f, ampDb / 20.f);
    double sI = 0, cI = 0, sO = 0, cO = 0, pIn = 0, pOut = 0;
    bool bad = false;
    for (int n = 0; n < N; ++n)
    {
        const float x = A * std::sin(2.f * kPi * freq * (float) n / (float) fs);
        float l, r;
        e.process(x, l, r);
        if (!std::isfinite(l)) bad = true;
        if (n >= skip)
        {
            const double w = 2.0 * kPi * freq * n / fs;
            sI += x * std::sin(w); cI += x * std::cos(w);
            sO += l * std::sin(w); cO += l * std::cos(w);
            pIn += (double) x * x; pOut += (double) l * l;
        }
    }
    gainOut = (float) std::sqrt(pOut / pIn);
    double dphi = std::atan2(cO, sO) - std::atan2(cI, sI); // output lags -> negative
    while (dphi > 0) dphi -= 2 * kPi;
    delayMsOut = (float) (-dphi / (2.0 * kPi * freq) * 1000.0);
    return bad ? -1 : 0;
}

int main()
{
    int fails = 0;
    float g, d;

    std::printf("== Pitch-mod (100%% wet, width 0): level linearity through compander ==\n");
    for (float db : { -60.f, -40.f, -20.f, -6.f, 0.f })
    {
        measure(Mode::PitchMod, db, 440.f, g, d);
        const float expect = 0.707f;
        std::printf("in %6.1f dBFS  gain %.3f (expect ~%.3f)  phase-delay %.2f ms\n", db, g, expect, d);
        if (db >= -40.f && db <= -6.f && std::fabs(g - expect) > 0.03f) { ++fails; std::printf("  ^ check failed at line %d\n", __LINE__); }
    }

    std::printf("\n== Delay at centre per mode (width 0) ==\n");
    for (int m = 0; m < 3; ++m)
    {
        Engine tmp;
        measure(Mode::PitchMod, -20.f, 40.f, g, d, 0.f, false, m); // wet-only path, this mode's delay settings
        std::printf("mode %d: measured %.2f ms, configured %.2f ms\n", m, d, tmp.cal.mode[m].centerDelayMs);
        if (std::fabs(d - tmp.cal.mode[m].centerDelayMs) > 0.6f) { ++fails; std::printf("  ^ check failed at line %d\n", __LINE__); }
    }

    std::printf("\n== Stability: flanger, max intensity, loud + quiet, modulated ==\n");
    for (float db : { -50.f, -10.f, 6.f })
    {
        measure(Mode::Flanger, db, 330.f, g, d, 1.f, true);
        std::printf("in %6.1f dBFS  rms gain %.2f\n", db, g);
        if (!(g < 20.f) || !std::isfinite(g)) { ++fails; std::printf("  ^ check failed at line %d\n", __LINE__); }
    }

    std::printf("\n%s\n", fails ? "FAILED" : "ALL CHECKS PASSED");
    return fails ? 1 : 0;
}
