// SAFE-SIDE by BOBA — the six width characters.
//
// Every character returns two signals at amount = 1:
//   side  goes ONLY into the side channel (mono sum untouched by construction)
//   mid   material that MAY be blended into mono by Mono Keep; it is built only from
//         comb-safe material (noise-like residual through decorrelation, or new harmonics
//         phase-locked to the partials already there) and then projected so it never subtracts.
#pragma once

#include "Kernels.h"

namespace boba
{
struct CharOut { double side = 0, mid = 0; };

//==============================================================================
// AIR — diffuse width: velvet-noise decorrelation of everything except transients.
class AirChar
{
public:
    void prepare (double fs)
    {
        const auto& lib = KernelLibrary::get();
        side.prepare (lib.airSide, fs);
        keepA.prepare (lib.airKeepA, fs);
        keepB.prepare (lib.airKeepB, fs);
        hp.set (true, 250.0, fs);
        rotStep = 2.0 * kPi * 0.23 / fs;
        reset();
    }
    void reset() { side.reset(); keepA.reset(); keepB.reset(); hp.reset(); rot = 0.0; }
    void setColor (double c, bool jump = false)
    {
        const int i = sizeIndex (c);
        if (jump) { side.jumpTo (i); keepA.jumpTo (i); keepB.jumpTo (i); }
        else { side.select (i); keepA.select (i); keepB.select (i); }
    }

    inline CharOut process (double src, double rs) noexcept
    {
        rot += rotStep;
        if (rot > 2.0 * kPi) rot -= 2.0 * kPi;
        const double k = std::cos (rot) * keepA.process (rs) + std::sin (rot) * keepB.process (rs);
        return { side.process (src), 0.6 * hp.process (k) };
    }

private:
    SparseBank side, keepA, keepB;
    LR4 hp;
    double rot = 0, rotStep = 0;
};

//==============================================================================
// TWIN — artificial double tracking: two drifting tape-like voices, their difference goes to the side.
class TwinChar
{
public:
    void prepare (double fs_)
    {
        fs = fs_;
        hpSrc.set (true, 150.0, fs);
        hpRes.set (true, 150.0, fs);
        const int maxD = (int) std::ceil (0.026 * fs) + 8;
        lineSrc.prepare (maxD);
        lineRes.prepare (maxD);
        mod1.prepare (3, 0.6, fs);
        mod2.prepare (5, 0.6, fs);
        reset();
    }
    void reset() { hpSrc.reset(); hpRes.reset(); lineSrc.reset(); lineRes.reset(); }

    inline CharOut process (double src, double rs, double color) noexcept
    {
        const double d = 0.4 + 1.6 * color;
        const double t1 = (12.0 + 1.0 * d * mod1.process()) * 0.001 * fs;
        const double t2 = (19.0 + 1.3 * d * mod2.process()) * 0.001 * fs;
        lineSrc.push (hpSrc.process (src));
        lineRes.push (hpRes.process (rs));
        const double v1 = lineSrc.tapFrac (t1), v2 = lineSrc.tapFrac (t2);
        const double r1 = lineRes.tapFrac (t1), r2 = lineRes.tapFrac (t2);
        return { 0.9 * (v1 - v2) * 0.5, 0.9 * (r1 + r2) * 0.5 };
    }

private:
    double fs = 48000.0;
    LR4 hpSrc, hpRes;
    DelayLine lineSrc, lineRes;
    SmoothRandom mod1, mod2;
};

//==============================================================================
// SHINE — stereo harmonic air: 2nd harmonics of the top end to the left, 3rd to the right.
// Harmonics of tonal material stay in the side; only harmonics of noise-like material may enter mono.
class ShineChar
{
public:
    void prepare (double fs_)
    {
        fs = fs_;
        hi2 = std::min (10500.0, 0.45 * fs / 2.0);
        hi3 = std::min (6800.0, 0.3 * fs / 2.0);
        lastColor = -1.0;
        setColor (0.5);
        reset();
    }
    void reset()
    {
        for (auto* p : { &nE, &nO, &tE, &tO }) p->reset();
        for (auto& f : post) { f.hp.reset(); f.lp.reset(); }
    }

    void setColor (double c)
    {
        if (std::abs (c - lastColor) < 0.004) return;
        lastColor = c;
        fc = 2000.0 + 4000.0 * c;
        nE.set (fc, hi2, fs); tE.set (fc, hi2, fs);
        nO.set (fc, hi3, fs); tO.set (fc, hi3, fs);
        useOdd = fc < 0.9 * hi3;
        const double lpHz = std::min (19500.0, 0.45 * fs);
        for (auto& f : post) { f.hp.set (true, 1.8 * fc, fs); f.lp.set (false, lpHz, fs); }
    }

    inline CharOut process (double noiseSrc, double tonalSrc) noexcept
    {
        const double En = nE.even (noiseSrc), Et = tE.even (tonalSrc);
        const double On = useOdd ? nO.odd (noiseSrc) : 0.0;
        const double Ot = useOdd ? tO.odd (tonalSrc) : 0.0;
        const double sidePre = (En - On) * 0.5 + 0.8 * (Et - Ot) * 0.5;
        const double midPre = (En + On) * 0.5;
        const double g = 1.6;
        return { g * post[0].process (sidePre), g * post[1].process (midPre) };
    }

private:
    struct Exciter
    {
        LR4 hp, lp;
        Hilbert h;
        void reset() { hp.reset(); lp.reset(); h.reset(); }
        void set (double lo, double hi, double sampleRate) { hp.set (true, lo, sampleRate); lp.set (false, hi, sampleRate); }
        inline cplx analytic (double x) noexcept { return h.process (lp.process (hp.process (x))); }
        inline double even (double x) noexcept   // |z| cos(2 arg z)
        {
            const cplx z = analytic (x);
            const double a2 = std::norm (z);
            return (z.real() * z.real() - z.imag() * z.imag()) / std::sqrt (a2 + 1e-30);
        }
        inline double odd (double x) noexcept    // |z| cos(3 arg z)
        {
            const cplx z = analytic (x);
            const double i = z.real(), q = z.imag();
            return (i * i * i - 3.0 * i * q * q) / (i * i + q * q + 1e-30);
        }
    };
    struct Post
    {
        LR4 hp, lp;
        inline double process (double x) noexcept { return lp.process (hp.process (x)); }
    };

    double fs = 48000.0, fc = 4000.0, hi2 = 10500.0, hi3 = 6800.0, lastColor = -1.0;
    bool useOdd = true;
    Exciter nE, nO, tE, tO;
    std::array<Post, 2> post;
};

//==============================================================================
// PHANTOM — 808 width: new harmonics of the bass fundamental, phase-locked to the partials that are
// already there (2nd centre, odd to the left, even to the right). Runs on the undelayed mid with a
// causal lock filter; its output refers to time (n - lockDelay()) and is aligned by the engine.
class PhantomChar
{
public:
    static constexpr int kHarm = 5;   // harmonics 2..6

    void prepare (double fs_)
    {
        fs = fs_;
        hp25.setButter2 (true, 25.0, fs);
        aLP.setButter2 (false, 150.0, fs);
        aSlow.setTime (1.0 / (2.0 * kPi * 30.0), fs);
        for (auto& l : lockRe) l.set (false, 12.0, fs);
        for (auto& l : lockIm) l.set (false, 12.0, fs);
        tau = (int) std::lround (lockRe[0].dcGroupDelay());
        dA.prepare (tau + 4);
        dUr.prepare (tau + 4);
        dUi.prepare (tau + 4);
        lastColor = -1.0;
        setColor (0.5);
        reset();
    }
    void reset()
    {
        hp25.reset(); lp.reset(); hil.reset(); aLP.reset(); aSlow.y = 0.0;
        for (auto& l : lockRe) l.reset();
        for (auto& l : lockIm) l.reset();
        dA.reset(); dUr.reset(); dUi.reset();
        u = { 1.0, 0.0 };
    }
    void setColor (double c)
    {
        if (std::abs (c - lastColor) < 0.004) return;
        lastColor = c;
        lp.set (false, 70.0 + 130.0 * c, fs);
    }
    int lockDelay() const noexcept { return tau; }

    inline CharOut process (double m) noexcept
    {
        const cplx z = hil.process (lp.process (hp25.process (m)));
        const double a = std::abs (z);
        if (a > 1e-9) u = z / a;
        const double aSmooth = std::max (0.0, aLP.process (a));
        const double aS = aSlow.process (aSmooth);

        // measure the phase of the partials already present (complex demodulation of the real mid)
        std::array<double, kHarm> theta {};
        const cplx uc = std::conj (u);
        cplx dem = m * uc;                             // m * conj(u)^1
        for (int k = 0; k < kHarm; ++k)
        {
            dem *= uc;                                 // m * conj(u)^(k+2)
            const double re = lockRe[(size_t) k].process (dem.real());
            const double im = lockIm[(size_t) k].process (dem.imag());
            theta[(size_t) k] = std::atan2 (im, re + 0.01 * aS);
        }

        // generate on the fundamental delayed by the lock filter's group delay
        dA.push (aSmooth); dUr.push (u.real()); dUi.push (u.imag());
        const double ad = dA.tap (tau);
        const cplx ud (dUr.tap (tau), dUi.tap (tau));
        std::array<double, kHarm> h {};
        cplx p = ud;
        for (int k = 0; k < kHarm; ++k)
        {
            p *= ud;                                   // ud^(k+2)
            const cplx lock = std::polar (1.0, theta[(size_t) k]);
            h[(size_t) k] = kGain[(size_t) k] * ad * (p * lock).real();
        }
        const double h2 = h[0], h3 = h[1], h4 = h[2], h5 = h[3], h6 = h[4];
        const double odd = h3 + h5, even = h4 + h6;
        return { (odd - even) * 0.5, h2 + (odd + even) * 0.5 };
    }

private:
    static constexpr std::array<double, kHarm> kGain { 0.22, 0.42, 0.34, 0.22, 0.14 };
    double fs = 48000.0, lastColor = -1.0;
    Biquad hp25, aLP;
    Butter4 lp;
    Hilbert hil;
    OnePole aSlow;
    std::array<Butter4, kHarm> lockRe, lockIm;
    DelayLine dA, dUr, dUi;
    int tau = 0;
    cplx u { 1.0, 0.0 };
};

//==============================================================================
// SPACE — a stereo field of early reflections for depth; only noise-like reflections may enter mono.
class SpaceChar
{
public:
    void prepare (double fs)
    {
        const auto& lib = KernelLibrary::get();
        L.prepare (lib.spaceL, fs);
        R.prepare (lib.spaceR, fs);
        KA.prepare (lib.spaceKeepA, fs);
        KB.prepare (lib.spaceKeepB, fs);
        for (auto& r : room) { r.lp.set (7000.0, fs); r.hp.set (true, 150.0, fs); }
        rotStep = 2.0 * kPi * 0.19 / fs;
        reset();
    }
    void reset()
    {
        L.reset(); R.reset(); KA.reset(); KB.reset();
        for (auto& r : room) { r.lp.reset(); r.hp.reset(); }
        rot = 0.0;
    }
    void setColor (double c, bool jump = false)
    {
        const int i = sizeIndex (c);
        if (jump) { L.jumpTo (i); R.jumpTo (i); KA.jumpTo (i); KB.jumpTo (i); }
        else { L.select (i); R.select (i); KA.select (i); KB.select (i); }
    }

    inline CharOut process (double src, double rs) noexcept
    {
        rot += rotStep;
        if (rot > 2.0 * kPi) rot -= 2.0 * kPi;
        const double side = 1.1 * room[0].process ((L.process (src) - R.process (src)) * 0.5);
        const double mid = 0.5 * room[1].process (std::cos (rot) * KA.process (rs) + std::sin (rot) * KB.process (rs));
        return { side, mid };
    }

private:
    struct Room
    {
        OnePoleLP lp;
        LR4 hp;
        inline double process (double x) noexcept { return hp.process (lp.process (x)); }
    };
    SparseBank L, R, KA, KB;
    std::array<Room, 2> room;
    double rot = 0, rotStep = 0;
};

} // namespace boba
