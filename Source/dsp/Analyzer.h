// SAFE-SIDE by BOBA — streaming tonal / transient / noise analysis of the mid channel.
//
// STFT (Hann, N = 2048 @ 44.1/48 kHz, 75 % overlap) with harmonic-percussive-residual soft masks
// (median filtering: Fitzgerald 2010, soft masks with separation factor: Driedger et al. 2014).
// A "noise-likeness" mask marks bins that are safe to blend into mono (no stable partial nearby).
//
// Outputs, all delayed by latency() samples:
//   p     percussive part of the mid
//   rs    strictly noise-like residual  (the only material characters may decorrelate into mono)
//   ps    strictly noise-like percussive part
//   prism PRISM character side signal   (per-ERB-band panning of the mid, computed in the STFT domain)
#pragma once

#include "Basics.h"

namespace boba
{
struct AnalyzerParams
{
    double prismAmount = 0.0;   // 0..1
    double prismColor = 0.4;    // 0..1 (orbit speed)
    double transient = 0.6;     // 0..1 (transients excluded from PRISM)
};

struct AnalyzerOut
{
    double p = 0, rs = 0, ps = 0, prism = 0;
};

class Analyzer
{
public:
    static constexpr int kLookahead = 4;          // frames of look-ahead for the time median
    static constexpr int kRing = 2 * kLookahead + 1;
    static constexpr int kFreqMedian = 17;        // bins, percussive median
    static constexpr int kTonalMedian = 9;        // bins, neighbourhood for "tonal peak" detection

    static int fftSizeFor (double fs) noexcept
    {
        int n = 2048;
        while (fs / n > 32.0) n <<= 1;     // keep ~43 ms frames at any sample rate
        return n;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        N = fftSizeFor (fs);
        hop = N / 4;
        bins = N / 2 + 1;
        latencySamples = N - 1 + kLookahead * hop;

        fft.prepare (N);
        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / N);

        inRing.assign ((size_t) N, 0.0);
        for (auto& v : X) v.assign ((size_t) bins, cplx {});
        for (auto& v : Y) v.assign ((size_t) bins, 0.0);
        for (auto& v : tonal) v.assign ((size_t) bins, 0);
        work.assign ((size_t) N, cplx {});
        work2.assign ((size_t) N, cplx {});
        mP.assign ((size_t) bins, 0.0);
        mR.assign ((size_t) bins, 0.0);
        mH.assign ((size_t) bins, 0.0);
        nz.assign ((size_t) bins, 0.0);
        med.assign ((size_t) bins, 0.0);
        for (auto& v : dil) v.assign ((size_t) bins, 0);
        tmp.assign ((size_t) std::max (kRing, kFreqMedian) + 2, 0.0);

        erbPhase.resize ((size_t) bins);
        prismW.resize ((size_t) bins);
        for (int f = 0; f < bins; ++f)
        {
            const double hz = f * fs / N;
            erbPhase[(size_t) f] = 2.0 * kPi * erbRate (hz) / 3.0;
            prismW[(size_t) f] = smoothstep (hz, 120.0, 250.0);
        }

        int a = 1;
        while (a < N + latencySamples + hop + 8) a <<= 1;
        accMask = a - 1;
        for (auto& v : acc) v.assign ((size_t) a, 0.0);
        reset();
    }

    void reset()
    {
        std::fill (inRing.begin(), inRing.end(), 0.0);
        for (auto& v : X) std::fill (v.begin(), v.end(), cplx {});
        for (auto& v : Y) std::fill (v.begin(), v.end(), 0.0);
        for (auto& v : tonal) std::fill (v.begin(), v.end(), (uint8_t) 0);
        for (auto& v : acc) std::fill (v.begin(), v.end(), 0.0);
        n = 0;
        frame = 0;
        hopCount = 0;
        prismPhase = 0.0;
    }

    int latency() const noexcept { return latencySamples; }
    int fftSize() const noexcept { return N; }

    void setParams (const AnalyzerParams& p) noexcept { params = p; }

    inline AnalyzerOut process (double m) noexcept
    {
        inRing[(size_t) (n & (N - 1))] = m;
        if (++hopCount == hop) { hopCount = 0; doFrame(); }

        const size_t r = (size_t) ((uint64_t) (n - latencySamples) & (uint64_t) accMask);
        AnalyzerOut o { acc[0][r], acc[1][r], acc[2][r], acc[3][r] };
        acc[0][r] = acc[1][r] = acc[2][r] = acc[3][r] = 0.0;
        ++n;
        return o;
    }

private:
    // median of 9 values (sorting network, 19 compare-swaps)
    static inline double median9 (double* p) noexcept
    {
        auto sw = [p] (int a, int b) { if (p[a] > p[b]) std::swap (p[a], p[b]); };
        sw (1, 2); sw (4, 5); sw (7, 8); sw (0, 1); sw (3, 4); sw (6, 7);
        sw (1, 2); sw (4, 5); sw (7, 8); sw (0, 3); sw (5, 8); sw (4, 7);
        sw (3, 6); sw (1, 4); sw (2, 5); sw (4, 7); sw (4, 2); sw (6, 4); sw (4, 2);
        return p[4];
    }

    // running median over a window of `len` bins (edges clamped), one pass over a spectrum
    void slidingMedian (const std::vector<double>& in, std::vector<double>& out, int len) noexcept
    {
        const int half = len / 2;
        double* w = tmp.data();
        for (int j = 0; j < len; ++j) w[j] = in[(size_t) std::clamp (j - half, 0, bins - 1)];
        std::sort (w, w + len);
        for (int f = 0; f < bins; ++f)
        {
            out[(size_t) f] = w[half];
            if (f + 1 == bins) break;
            const double gone = in[(size_t) std::clamp (f - half, 0, bins - 1)];
            const double come = in[(size_t) std::clamp (f + 1 + half, 0, bins - 1)];
            int i = (int) (std::lower_bound (w, w + len, gone) - w);
            // remove `gone`, insert `come`, keeping the window sorted
            if (come >= gone)
            {
                while (i + 1 < len && w[i + 1] < come) { w[i] = w[i + 1]; ++i; }
                w[i] = come;
            }
            else
            {
                while (i > 0 && w[i - 1] > come) { w[i] = w[i - 1]; --i; }
                w[i] = come;
            }
        }
    }

    void doFrame() noexcept
    {
        const int k = frame++;
        const size_t sk = (size_t) (k % kRing);

        // ---- analysis of the newest frame k
        for (int i = 0; i < N; ++i)
            work[(size_t) i] = cplx (window[(size_t) i] * inRing[(size_t) ((n - N + 1 + i) & (N - 1))], 0.0);
        fft.forward (work.data());
        for (int f = 0; f < bins; ++f)
        {
            X[sk][(size_t) f] = work[(size_t) f];
            Y[sk][(size_t) f] = std::norm (work[(size_t) f]);
        }
        // tonal peaks: bin > 4 x median of its 9-bin neighbourhood
        slidingMedian (Y[sk], med, kTonalMedian);
        for (int f = 0; f < bins; ++f)
            tonal[sk][(size_t) f] = Y[sk][(size_t) f] > 4.0 * med[(size_t) f] ? 1 : 0;

        // ---- masks for the centre frame c = k - lookahead
        const int c = k - kLookahead;
        const size_t sc = (size_t) (((c % kRing) + kRing) % kRing);
        const auto& Yc = Y[sc];
        slidingMedian (Yc, med, kFreqMedian);
        static_assert (kRing == 9, "time median uses a 9-value sorting network");

        for (int f = 0; f < bins; ++f)
        {
            double t9[9];
            for (int j = 0; j < kRing; ++j) t9[j] = Y[(size_t) j][(size_t) f];
            const double H = median9 (t9);
            const double P = med[(size_t) f];

            // soft masks, separation factor beta = 2, steepness k = 3, r = ln(H/P)/2:
            //   mH = 1 / (1 + exp(-k (r - ln beta))) = 1 / (1 + 8 q^1.5),  q = P/H
            const double q = (P + 1e-20) / (H + 1e-20);
            const double q15 = q * std::sqrt (q);
            double h = 1.0 / (1.0 + 8.0 * q15);
            double p = 1.0 / (1.0 + 8.0 / q15);
            double res = std::max (0.0, 1.0 - h - p);
            const double s = h + p + res;
            mH[(size_t) f] = h / s;
            mP[(size_t) f] = p / s;
            mR[(size_t) f] = res / s;
        }

        // noise-likeness: tonal bins dilated (+-1 frame, +-2 bins) are excluded, 3x3 smoothing
        for (int d = 0; d < 3; ++d)
        {
            const int j = c - 1 + d;
            auto slot = [&] (int fr) { return (size_t) (((fr % kRing) + kRing) % kRing); };
            const auto& t0 = tonal[slot (j - 1)];
            const auto& t1 = tonal[slot (j)];
            const auto& t2 = tonal[slot (j + 1)];
            auto& out = dil[(size_t) d];
            for (int f = 0; f < bins; ++f)
            {
                uint8_t any = 0;
                for (int q = std::max (0, f - 2); q <= std::min (bins - 1, f + 2); ++q)
                    any |= (uint8_t) (t0[(size_t) q] | t1[(size_t) q] | t2[(size_t) q]);
                out[(size_t) f] = any;
            }
        }
        for (int f = 0; f < bins; ++f)
        {
            double sum = 0.0;
            for (int d = 0; d < 3; ++d)
                for (int q = -1; q <= 1; ++q)
                    sum += 1.0 - dil[(size_t) d][(size_t) std::clamp (f + q, 0, bins - 1)];
            nz[(size_t) f] = sum / 9.0 * (1.0 - mH[(size_t) f]);
        }

        // ---- output spectra (two real signals per complex inverse FFT)
        const auto& Xc = X[sc];
        const double pa = params.prismAmount, T = params.transient;
        prismPhase += 2.0 * kPi * 0.6 * params.prismColor * params.prismColor * hop / fs;
        if (prismPhase > 2.0 * kPi) prismPhase -= 2.0 * kPi;

        for (int f = 0; f < bins; ++f)
        {
            const cplx x = Xc[(size_t) f];
            const cplx A1 = mP[(size_t) f] * x;
            const cplx B1 = mR[(size_t) f] * nz[(size_t) f] * x;
            const cplx A2 = mP[(size_t) f] * nz[(size_t) f] * x;
            const double g = pa * std::sin (erbPhase[(size_t) f] + prismPhase) * prismW[(size_t) f] * (1.0 - T * mP[(size_t) f]);
            const cplx B2 = g * x;
            const cplx j (0.0, 1.0);
            work[(size_t) f] = A1 + j * B1;
            work2[(size_t) f] = A2 + j * B2;
            if (f > 0 && f < N / 2)
            {
                work[(size_t) (N - f)] = std::conj (A1) + j * std::conj (B1);
                work2[(size_t) (N - f)] = std::conj (A2) + j * std::conj (B2);
            }
        }
        fft.inverse (work.data());
        fft.inverse (work2.data());

        const double scale = 1.0 / (N * 1.5);   // Hann analysis x Hann synthesis at 75 % overlap sums to 1.5
        const int64_t start = (n - kLookahead * hop) - N + 1;
        for (int i = 0; i < N; ++i)
        {
            const size_t a = (size_t) ((uint64_t) (start + i) & (uint64_t) accMask);
            const double w = window[(size_t) i] * scale;
            acc[0][a] += w * work[(size_t) i].real();
            acc[1][a] += w * work[(size_t) i].imag();
            acc[2][a] += w * work2[(size_t) i].real();
            acc[3][a] += w * work2[(size_t) i].imag();
        }
    }

    double fs = 48000.0;
    int N = 2048, hop = 512, bins = 1025, latencySamples = 0;
    FFT fft;
    std::vector<double> window, inRing;
    std::array<std::vector<cplx>, kRing> X;
    std::array<std::vector<double>, kRing> Y;
    std::array<std::vector<uint8_t>, kRing> tonal;
    std::array<std::vector<uint8_t>, 3> dil;
    std::vector<cplx> work, work2;
    std::vector<double> mH, mP, mR, nz, med, tmp, erbPhase, prismW;
    std::array<std::vector<double>, 4> acc;
    int accMask = 0;
    int64_t n = 0;
    int frame = 0, hopCount = 0;
    double prismPhase = 0.0;
    AnalyzerParams params;
};

} // namespace boba
