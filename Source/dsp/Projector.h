// SAFE-SIDE by BOBA — "never subtract" projection for material blended into mono (Mono Keep).
//
// In every time-frequency cell the added signal is written as G = (a + jb) X  (X = the mid) and forced into
// a cone around X:   a >= 0  and  |b| <= a / gamma.
// a >= 0 alone already gives |X + G'| >= |X| on this grid (the blend can only add, never notch). The cone
// leaves slack, so the property also holds on other analysis grids (and for the ear); three stages on
// grids shifted by a half and a quarter hop make it robust.
#pragma once

#include "Basics.h"

namespace boba
{
class Projector
{
public:
    void prepare (int fftSize, int gridOffset, double coneGamma = 3.0)
    {
        invGamma = 1.0 / coneGamma;
        N = fftSize;
        hop = N / 4;
        bins = N / 2 + 1;
        offset = gridOffset;
        fft.prepare (N);
        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / N);
        xRing.assign ((size_t) N, 0.0);
        gRing.assign ((size_t) N, 0.0);
        work.assign ((size_t) N, cplx {});
        tmpG.assign ((size_t) bins, cplx {});
        int a = 1;
        while (a < 2 * N + 8) a <<= 1;
        accMask = a - 1;
        acc.assign ((size_t) a, 0.0);
        xDelay.prepare (N);
        reset();
    }

    void reset()
    {
        std::fill (xRing.begin(), xRing.end(), 0.0);
        std::fill (gRing.begin(), gRing.end(), 0.0);
        std::fill (acc.begin(), acc.end(), 0.0);
        xDelay.reset();
        n = 0;
        hopCount = offset % hop;
        lastNonZero = -1;
    }

    int latency() const noexcept { return N - 1; }

    inline void process (double x, double g, double& xOut, double& gOut) noexcept
    {
        xRing[(size_t) (n & (N - 1))] = x;
        gRing[(size_t) (n & (N - 1))] = g;
        if (std::abs (g) > 0.0) lastNonZero = n;
        if (++hopCount >= hop) { hopCount = 0; doFrame(); }

        xOut = xDelay.process (x, N - 1);
        const size_t r = (size_t) ((uint64_t) (n - (N - 1)) & (uint64_t) accMask);
        gOut = acc[r];
        acc[r] = 0.0;
        ++n;
    }

private:
    void doFrame() noexcept
    {
        if (lastNonZero < 0 || n - lastNonZero >= N) return;   // nothing to blend in this frame

        for (int i = 0; i < N; ++i)
        {
            const size_t s = (size_t) ((n - N + 1 + i) & (N - 1));
            work[(size_t) i] = cplx (window[(size_t) i] * xRing[s], window[(size_t) i] * gRing[s]);
        }
        fft.forward (work.data());

        // unpack the two real spectra, project, store G' (Hermitian) back into work
        auto& w = work;
        cplx G0 {}, GN {};
        for (int f = 0; f < bins; ++f)
        {
            const cplx a = w[(size_t) f];
            const cplx b = std::conj (w[(size_t) ((N - f) & (N - 1))]);
            const cplx X = 0.5 * (a + b);
            cplx G = cplx (0.0, -0.5) * (a - b);
            const double xx = std::norm (X);
            if (xx > 1e-30)
            {
                const cplx c = G * std::conj (X) / xx;      // G = c X
                const double along = std::max (0.0, c.real());
                const double lim = along * invGamma;
                const double across = std::clamp (c.imag(), -lim, lim);
                G = cplx (along, across) * X;
            }
            else
                G = {};
            if (f == 0) G0 = G;
            else if (f == N / 2) GN = G;
            else tmpG[(size_t) f] = G;
        }
        w[0] = cplx (G0.real(), 0.0);
        w[(size_t) (N / 2)] = cplx (GN.real(), 0.0);
        for (int f = 1; f < N / 2; ++f)
        {
            w[(size_t) f] = tmpG[(size_t) f];
            w[(size_t) (N - f)] = std::conj (tmpG[(size_t) f]);
        }
        fft.inverse (w.data());

        const double scale = 1.0 / (N * 1.5);
        const int64_t start = n - N + 1;
        for (int i = 0; i < N; ++i)
            acc[(size_t) ((uint64_t) (start + i) & (uint64_t) accMask)] += window[(size_t) i] * scale * w[(size_t) i].real();
    }

    int N = 1024, hop = 256, bins = 513, offset = 0;
    double invGamma = 1.0 / 3.0;
    FFT fft;
    std::vector<double> window, xRing, gRing, acc;
    std::vector<cplx> work, tmpG;
    DelayLine xDelay;
    int accMask = 0;
    int64_t n = 0, lastNonZero = -1;
    int hopCount = 0;
};

} // namespace boba
