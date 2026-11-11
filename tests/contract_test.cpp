// SAFE-SIDE by BOBA — mono-contract test (runs in CI, no JUCE needed).
//
//  1. Mono Keep = 0: for every character, sample rate and block pattern, mono out == mono in
//     (delayed by the reported latency), to double precision.
//  2. Mono Keep = 1: the blend never makes the mono quieter: power of mono-out vs mono-in, measured in
//     short 1/3-octave-ish bands, never drops by more than 1 dB in any loud moment.
//  3. All outputs finite on torture input (silence, impulses, full-scale square, DC, noise).
#include "../Source/dsp/Engine.h"

#include <cstdio>
#include <memory>
#include <random>

using namespace boba;

static std::vector<double> testSignal (double fs, int n, uint64_t seed)
{
    std::vector<double> x ((size_t) n);
    Rng rng (seed);
    const double bpm = 140.0, beat = 60.0 / bpm;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double tb = std::fmod (t, 2.0 * beat);
        const double f808 = 46.25 * (1.0 + 0.9 * std::exp (-tb / 0.025));
        static double ph = 0.0;
        if (i == 0) ph = 0.0;
        ph += 2.0 * kPi * f808 / fs;
        double v = std::tanh (2.2 * std::sin (ph) * std::exp (-tb / 0.7)) * 0.35;
        for (int k = 1; k <= 6; ++k) v += 0.04 / k * std::sin (2.0 * kPi * 220.0 * k * t + k);
        const double th = std::fmod (t, beat / 2.0);
        v += 0.15 * rng.uniform (-1.0, 1.0) * std::exp (-th / 0.03);
        v += 0.02 * rng.uniform (-1.0, 1.0);
        x[(size_t) i] = v;
    }
    return x;
}

struct Result { double nullDb; double worstDropDb; bool finite; int latency; };

static Result run (double fs, int character, double keep, double width, bool randomBlocks, const std::vector<double>& inL, const std::vector<double>& inR)
{
    auto e = std::make_unique<Engine>();
    Params p;
    p.character = character;
    p.amount = 1.0;
    p.keep = keep;
    p.width = width;
    e->setParams (p);
    e->prepare (fs);
    const int lat = e->getLatencySamples();
    const int n = (int) inL.size();
    std::vector<double> L ((size_t) (n + lat), 0.0), R ((size_t) (n + lat), 0.0);
    std::copy (inL.begin(), inL.end(), L.begin());
    std::copy (inR.begin(), inR.end(), R.begin());
    std::mt19937 rng (7);
    for (int pos = 0; pos < n + lat;)
    {
        int bs = randomBlocks ? 1 + (int) (rng() % 900) : 256;
        bs = std::min (bs, n + lat - pos);
        e->process (L.data() + pos, R.data() + pos, bs);
        pos += bs;
    }

    Result r { -400.0, 0.0, true, lat };
    double err = 0.0, ref = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double mi = 0.5 * (inL[(size_t) i] + inR[(size_t) i]);
        const double mo = 0.5 * (L[(size_t) (i + lat)] + R[(size_t) (i + lat)]);
        if (! std::isfinite (mo) || ! std::isfinite (L[(size_t) (i + lat)])) r.finite = false;
        err += (mo - mi) * (mo - mi);
        ref += mi * mi;
    }
    r.nullDb = 10.0 * std::log10 (err / ref + 1e-60);

    // momentary drop: 4096-sample frames, bands split with the engine's own band splitter
    BandSplitter si, so;
    si.setEdges ({ 120.0, 1000.0, 5000.0 }, fs);
    so.setEdges ({ 120.0, 1000.0, 5000.0 }, fs);
    std::array<double, kNumBands> bi {}, bo {}, ei {}, eo {}, peak {};
    std::vector<std::array<double, 2 * kNumBands>> frames;
    for (int i = 0; i < n; ++i)
    {
        si.process (0.5 * (inL[(size_t) i] + inR[(size_t) i]), bi);
        so.process (0.5 * (L[(size_t) (i + lat)] + R[(size_t) (i + lat)]), bo);
        for (size_t b = 0; b < (size_t) kNumBands; ++b) { ei[b] += bi[b] * bi[b]; eo[b] += bo[b] * bo[b]; }
        if ((i + 1) % 4096 == 0)
        {
            std::array<double, 2 * kNumBands> f {};
            for (size_t b = 0; b < (size_t) kNumBands; ++b) { f[b] = ei[b]; f[b + kNumBands] = eo[b]; peak[b] = std::max (peak[b], ei[b]); }
            frames.push_back (f);
            ei.fill (0.0); eo.fill (0.0);
        }
    }
    for (size_t k = 2; k + 1 < frames.size(); ++k)
        for (size_t b = 0; b < (size_t) kNumBands; ++b)
            if (frames[k][b] > 0.01 * peak[b])   // loud moments (within 20 dB of the band's peak)
                r.worstDropDb = std::min (r.worstDropDb, 10.0 * std::log10 (frames[k][b + kNumBands] / frames[k][b]));
    return r;
}

int main()
{
    const char* names[] = { "AIR", "PRISM", "TWIN", "SHINE", "PHANTOM", "SPACE" };
    int failures = 0;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const int n = (int) (fs * 4.0);
        const auto m = testSignal (fs, n, 3);
        const auto s = testSignal (fs, n, 9);
        std::vector<double> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) { L[(size_t) i] = m[(size_t) i] + 0.3 * s[(size_t) i]; R[(size_t) i] = m[(size_t) i] - 0.3 * s[(size_t) i]; }

        for (int c = 0; c < kNumCharacters; ++c)
        {
            const auto a = run (fs, c, 0.0, 1.6, c % 2 == 1, L, R);
            const bool okA = a.finite && a.nullDb < -200.0;
            std::printf ("%6.0f Hz %-8s keep 0: mono null %7.1f dB  latency %d  %s\n", fs, names[c], a.nullDb, a.latency, okA ? "ok" : "FAIL");
            failures += okA ? 0 : 1;

            const auto b = run (fs, c, 1.0, 1.0, true, m, m);
            const bool okB = b.finite && b.worstDropDb > -1.0;
            std::printf ("%6.0f Hz %-8s keep 1: worst loud moment %+5.2f dB  %s\n", fs, names[c], b.worstDropDb, okB ? "ok" : "FAIL");
            failures += okB ? 0 : 1;
        }
    }

    // torture
    {
        const double fs = 48000.0;
        const int n = 48000 * 3;
        std::vector<double> t ((size_t) n, 0.0);
        for (int i = 24000; i < 24010; ++i) t[(size_t) i] = 1.0;
        for (int i = 48000; i < 96000; ++i) t[(size_t) i] = std::sin (2.0 * kPi * 55.0 * i / fs) > 0.0 ? 1.0 : -1.0;
        for (int i = 100000; i < 110000; ++i) t[(size_t) i] = 0.9;
        Rng rng (5);
        for (int i = 115000; i < n; ++i) t[(size_t) i] = rng.uniform (-1.0, 1.0);
        std::vector<double> r ((size_t) n);
        for (int i = 0; i < n; ++i) r[(size_t) i] = -0.3 * t[(size_t) i];
        for (int c = 0; c < kNumCharacters; ++c)
        {
            const auto a = run (fs, c, 1.0, 2.0, true, t, r);
            std::printf ("torture %-8s finite: %s\n", names[c], a.finite ? "ok" : "FAIL");
            failures += a.finite ? 0 : 1;
        }
    }

    std::printf (failures == 0 ? "\nALL MONO-CONTRACT TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
