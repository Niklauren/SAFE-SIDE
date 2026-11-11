// SAFE-SIDE by BOBA — sparse kernels: velvet-noise decorrelators and early-reflection patterns.
#pragma once

#include "Basics.h"

namespace boba
{
struct Tap { double timeSec; double gain; };
using Kernel = std::vector<Tap>;

static constexpr int kNumSizes = 16;   // Color knob is quantised into 16 kernel sizes (cross-faded)

// Exponentially decaying velvet noise. Several candidates are drawn and the one with the flattest
// magnitude response (150 Hz – 16 kHz) is kept, so the decorrelator colours the side as little as possible.
inline Kernel makeVelvet (uint64_t seed, double durMs, double density = 1500.0, double decayDb = 60.0, int tries = 12)
{
    constexpr double designFs = 48000.0;
    const int n = (int) (durMs / 1000.0 * designFs);
    const double td = designFs / density;
    const int m = std::max (1, (int) (n / td));

    FFT fft;
    const int nfft = 8192;
    fft.prepare (nfft);
    std::vector<cplx> buf ((size_t) nfft);

    Rng master (seed);
    Kernel best;
    double bestDev = 1e300;

    for (int t = 0; t < tries; ++t)
    {
        Rng rng (master.next());
        Kernel k;
        k.reserve ((size_t) m);
        double energy = 0.0;
        for (int i = 0; i < m; ++i)
        {
            const int pos = std::min (n - 1, (int) (i * td + rng.uniform (0.0, td - 1.0)));
            const double g = rng.sign() * std::pow (10.0, -decayDb / 20.0 * pos / n);
            k.push_back ({ pos / designFs, g });
            energy += g * g;
        }
        const double norm = 1.0 / std::sqrt (energy);
        for (auto& tp : k) tp.gain *= norm;

        std::fill (buf.begin(), buf.end(), cplx {});
        for (auto& tp : k) buf[(size_t) std::lround (tp.timeSec * designFs) % nfft] += tp.gain;
        fft.forward (buf.data());
        double sum = 0, sum2 = 0;
        int cnt = 0;
        for (int b = 1; b < nfft / 2; ++b)
        {
            const double f = b * designFs / nfft;
            if (f < 150.0 || f > 16000.0) continue;
            const double db = 10.0 * std::log10 (std::norm (buf[(size_t) b]) + 1e-24);
            sum += db; sum2 += db * db; ++cnt;
        }
        const double mean = sum / cnt;
        const double dev = std::sqrt (std::max (0.0, sum2 / cnt - mean * mean));
        if (dev < bestDev) { bestDev = dev; best = k; }
    }
    return best;
}

// Sparse early-reflection pattern: taps log-spaced 3 ms .. 60 ms * size, exponential decay.
inline Kernel makeReflections (uint64_t seed, double size, int taps = 28)
{
    Rng rng (seed);
    const double tMax = 0.060 * size;
    std::vector<double> times ((size_t) taps);
    for (auto& t : times) t = std::exp (rng.uniform (std::log (0.003), std::log (tMax)));
    std::sort (times.begin(), times.end());
    Kernel k;
    double energy = 0.0;
    for (double t : times)
    {
        const double g = rng.sign() * rng.uniform (0.4, 1.0) * std::exp (-t / (0.045 * size));
        k.push_back ({ t, g });
        energy += g * g;
    }
    for (auto& tp : k) tp.gain /= std::sqrt (energy);
    return k;
}

// A bank of kernels (one per Color step) run on one delay line; switching sizes cross-fades.
class SparseBank
{
public:
    void prepare (const std::vector<Kernel>& kernels, double fs, double fadeSec = 0.08)
    {
        bank.clear();
        int maxDelay = 1;
        for (auto& k : kernels)
        {
            std::vector<std::pair<int, double>> taps;
            for (auto& t : k)
            {
                const int d = (int) std::lround (t.timeSec * fs);
                taps.emplace_back (d, t.gain);
                maxDelay = std::max (maxDelay, d);
            }
            bank.push_back (std::move (taps));
        }
        line.prepare (maxDelay + 2);
        fadeStep = 1.0 / (fadeSec * fs);
        cur = 0; next = -1; pending = -1; fade = 0.0;
    }

    void reset() noexcept { line.reset(); next = -1; pending = -1; fade = 0.0; }

    void select (int idx) noexcept
    {
        idx = std::clamp (idx, 0, (int) bank.size() - 1);
        if (next >= 0) { pending = idx; return; }
        if (idx != cur) { next = idx; fade = 0.0; }
    }

    void jumpTo (int idx) noexcept { cur = std::clamp (idx, 0, (int) bank.size() - 1); next = pending = -1; }

    inline double process (double x) noexcept
    {
        line.push (x);
        double y = conv (cur);
        if (next >= 0)
        {
            fade += fadeStep;
            const double w = fade >= 1.0 ? 1.0 : 0.5 - 0.5 * std::cos (kPi * fade);
            y += (conv (next) - y) * w;
            if (fade >= 1.0)
            {
                cur = next;
                next = -1;
                if (pending >= 0 && pending != cur) { next = pending; fade = 0.0; }
                pending = -1;
            }
        }
        return y;
    }

private:
    inline double conv (int k) const noexcept
    {
        double y = 0.0;
        for (const auto& t : bank[(size_t) k]) y += t.second * line.tap (t.first);
        return y;
    }

    std::vector<std::vector<std::pair<int, double>>> bank;
    DelayLine line;
    int cur = 0, next = -1, pending = -1;
    double fade = 0.0, fadeStep = 0.0;
};

inline int sizeIndex (double color01) noexcept
{
    return std::clamp ((int) std::lround (color01 * (kNumSizes - 1)), 0, kNumSizes - 1);
}

// Kernel banks are the same for every instance: build them once.
struct KernelLibrary
{
    std::vector<Kernel> airSide, airKeepA, airKeepB, spaceL, spaceR, spaceKeepA, spaceKeepB;

    static const KernelLibrary& get()
    {
        static const KernelLibrary lib;
        return lib;
    }

private:
    KernelLibrary()
    {
        for (int i = 0; i < kNumSizes; ++i)
        {
            const double c = (double) i / (kNumSizes - 1);
            const double airMs = 15.0 + 45.0 * c;
            airSide.push_back (makeVelvet (7, airMs));
            airKeepA.push_back (makeVelvet (21, airMs * 1.3));
            airKeepB.push_back (makeVelvet (22, airMs * 1.3));
            const double size = 0.5 + 1.5 * c;
            spaceL.push_back (makeReflections (31, size));
            spaceR.push_back (makeReflections (47, size));
            spaceKeepA.push_back (makeReflections (53, size));
            spaceKeepB.push_back (makeReflections (61, size));
        }
    }
};

} // namespace boba
