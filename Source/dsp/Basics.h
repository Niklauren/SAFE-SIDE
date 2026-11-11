// SAFE-SIDE by BOBA — DSP building blocks.
// Pure C++17 (no JUCE), so the engine can be tested offline anywhere.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace boba
{
static constexpr double kPi = 3.14159265358979323846;
using cplx = std::complex<double>;

//==============================================================================
// In-place iterative radix-2 complex FFT.
class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        logN = 0;
        while ((1 << logN) < n) ++logN;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < logN; ++b)
                if (i & (1 << b)) r |= 1 << (logN - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0, -2.0 * kPi * i / n);
    }

    int size() const noexcept { return n; }

    void forward (cplx* x) const noexcept { run (x, false); }

    // unnormalised inverse; caller scales by 1/n
    void inverse (cplx* x) const noexcept { run (x, true); }

private:
    void run (cplx* x, bool inv) const noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cplx w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const cplx t = w * x[i + j + half];
                    x[i + j + half] = x[i + j] - t;
                    x[i + j] += t;
                }
        }
    }

    int n = 0, logN = 0;
    std::vector<int> rev;
    std::vector<cplx> tw;
};

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    void setPass (bool highPass, double fc, double q, double fs) noexcept
    {
        fc = std::clamp (fc, 5.0, 0.47 * fs);
        const double w0 = 2.0 * kPi * fc / fs;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        if (highPass) { b0 = (1.0 + cosw) / 2.0 / a0; b1 = -(1.0 + cosw) / a0; }
        else          { b0 = (1.0 - cosw) / 2.0 / a0; b1 =  (1.0 - cosw) / a0; }
        b2 = b0;
        a1 = -2.0 * cosw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setButter2 (bool highPass, double fc, double fs) noexcept { setPass (highPass, fc, 0.70710678118654752, fs); }

    // group delay at DC in samples (only meaningful for low-pass sections)
    double dcGroupDelay() const noexcept
    {
        const double nb = b1 + 2.0 * b2, db = b0 + b1 + b2;
        const double na = a1 + 2.0 * a2, da = 1.0 + a1 + a2;
        return nb / db - na / da;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Linkwitz-Riley 4th order = two identical Butterworth 2nd-order sections.
struct LR4
{
    Biquad s1, s2;
    void reset() noexcept { s1.reset(); s2.reset(); }
    void set (bool highPass, double fc, double fs) noexcept { s1.setButter2 (highPass, fc, fs); s2.setButter2 (highPass, fc, fs); }
    inline double process (double x) noexcept { return s2.process (s1.process (x)); }
};

// Butterworth 4th order (two sections with Q 0.5412 / 1.3066).
struct Butter4
{
    Biquad s1, s2;
    void reset() noexcept { s1.reset(); s2.reset(); }
    void set (bool highPass, double fc, double fs) noexcept
    {
        s1.setPass (highPass, fc, 0.54119610014619698, fs);
        s2.setPass (highPass, fc, 1.30656296487637652, fs);
    }
    double dcGroupDelay() const noexcept { return s1.dcGroupDelay() + s2.dcGroupDelay(); }
    inline double process (double x) noexcept { return s2.process (s1.process (x)); }
};

struct OnePoleLP   // first-order low-pass (bilinear), for gentle tone shaping
{
    double b = 1, a = 0, z = 0;
    void reset() noexcept { z = 0; }
    void set (double fc, double fs) noexcept
    {
        const double k = std::tan (kPi * std::clamp (fc, 5.0, 0.47 * fs) / fs);
        b = k / (1.0 + k);
        a = (k - 1.0) / (k + 1.0);
    }
    inline double process (double x) noexcept
    {
        const double y = b * x + z;
        z = b * x - a * y;
        return y;
    }
};

//==============================================================================
static constexpr int kNumBands = 4;   // [0] below "Mono Below" (kept mono), [1] low-mid, [2] mid, [3] high
static constexpr int kNumEdges = kNumBands - 1;

// Cascaded LR4 split (sum of the bands = all-pass version of the input).
struct BandSplitter
{
    std::array<LR4, kNumEdges> lp, hp;

    void reset() noexcept { for (auto& f : lp) f.reset(); for (auto& f : hp) f.reset(); }

    void setEdges (const std::array<double, kNumEdges>& edges, double fs) noexcept
    {
        for (size_t e = 0; e < (size_t) kNumEdges; ++e) { lp[e].set (false, edges[e], fs); hp[e].set (true, edges[e], fs); }
    }

    inline void process (double x, std::array<double, kNumBands>& out) noexcept
    {
        double rest = x;
        for (size_t e = 0; e < (size_t) kNumEdges; ++e) { out[e] = lp[e].process (rest); rest = hp[e].process (rest); }
        out[kNumBands - 1] = rest;
    }
};

//==============================================================================
struct OnePole   // y = (1-a) x + a y
{
    double a = 0, y = 0;
    void setTime (double seconds, double fs) noexcept { a = std::exp (-1.0 / (seconds * fs)); }
    inline double process (double x) noexcept { y = (1.0 - a) * x + a * y; return y; }
};

struct GainSmoother   // fast attack when the gain drops, slow release when it rises
{
    double att = 0, rel = 0, y = 1.0;
    void setTimes (double attackSec, double releaseSec, double fs) noexcept
    {
        att = std::exp (-1.0 / (attackSec * fs));
        rel = std::exp (-1.0 / (releaseSec * fs));
    }
    inline double process (double target) noexcept
    {
        const double c = target < y ? att : rel;
        y = c * y + (1.0 - c) * target;
        return y;
    }
};

//==============================================================================
// Power-of-two ring buffer delay line.
class DelayLine
{
public:
    void prepare (int maxDelay)
    {
        int sz = 1;
        while (sz < maxDelay + 4) sz <<= 1;
        buf.assign ((size_t) sz, 0.0);
        mask = sz - 1;
        pos = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0); pos = 0; }

    inline void push (double x) noexcept { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }
    inline double tap (int d) const noexcept { return buf[(size_t) ((pos - d) & mask)]; }

    // fractional delay, 4-point cubic (Catmull-Rom); d >= 1
    inline double tapFrac (double d) const noexcept
    {
        const int i = (int) d;
        const double f = d - i;
        const double y0 = tap (i - 1), y1 = tap (i), y2 = tap (i + 1), y3 = tap (i + 2);
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    // simple fixed delay: push x, return x delayed by d
    inline double process (double x, int d) noexcept { push (x); return tap (d); }

private:
    std::vector<double> buf;
    int mask = 0, pos = 0;
};

//==============================================================================
// Analytic signal with two all-pass chains (90° ± 0.7° from ~20 Hz to ~23 kHz at 48 kHz).
// I = chain B(x),  Q = chain A(x) delayed one sample.  z = I + jQ.
class Hilbert
{
public:
    void reset() noexcept
    {
        for (auto& s : a) s = {};
        for (auto& s : b) s = {};
        qDelay = 0;
    }

    inline cplx process (double x) noexcept
    {
        double ya = x, yb = x;
        for (size_t i = 0; i < 4; ++i) ya = a[i].process (ya, kA[i] * kA[i]);
        for (size_t i = 0; i < 4; ++i) yb = b[i].process (yb, kB[i] * kB[i]);
        const double q = qDelay;
        qDelay = ya;
        return { yb, q };
    }

private:
    struct Section   // y[n] = c (x[n] + y[n-2]) - x[n-2]
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        inline double process (double x, double c) noexcept
        {
            const double y = c * (x + y2) - x2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };
    static constexpr double kA[4] { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    static constexpr double kB[4] { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
    std::array<Section, 4> a {}, b {};
    double qDelay = 0;
};

//==============================================================================
struct Rng   // xorshift64*, deterministic
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) { if (s == 0) s = 1; next(); next(); }
    uint64_t next() noexcept { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
    double uniform() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }   // [0,1)
    double uniform (double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }
    double sign() noexcept { return (next() >> 63) ? 1.0 : -1.0; }
};

// Smooth random control signal in [-1, 1]: cosine interpolation between random points.
class SmoothRandom
{
public:
    void prepare (uint64_t seed, double rateHz, double fs)
    {
        rng = Rng (seed);
        step = rateHz * 2.0 / fs;    // a new target every half period
        phase = 0.0;
        from = rng.uniform (-1.0, 1.0);
        to = rng.uniform (-1.0, 1.0);
    }
    inline double process() noexcept
    {
        phase += step;
        if (phase >= 1.0) { phase -= 1.0; from = to; to = rng.uniform (-1.0, 1.0); }
        const double w = 0.5 - 0.5 * std::cos (kPi * phase);
        return from + (to - from) * w;
    }

private:
    Rng rng { 1 };
    double step = 0, phase = 0, from = 0, to = 0;
};

inline double smoothstep (double x, double a, double b) noexcept
{
    const double t = std::clamp ((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline double erbRate (double f) noexcept { return 21.4 * std::log10 (1.0 + 0.00437 * f); }

} // namespace boba
