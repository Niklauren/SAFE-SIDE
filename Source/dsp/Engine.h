// SAFE-SIDE by BOBA — the engine: six width characters, one mono contract.
//
//     M = (L+R)/2, S = (L-R)/2
//     L' = M + Keep*G_mid' + S',   R' = M + Keep*G_mid' - S'
//
//  * Mono Keep = 0  ->  mono out == mono in (bit-exact, only delayed by the plug-in latency),
//                       whatever the character does: all widening lives in the side.
//  * Mono Keep > 0  ->  only comb-safe material is blended into mono, and it is projected so that
//                       in every time-frequency cell |M + G'| >= |M|  (it can add, never cancel),
//                       on three shifted STFT grids.
//  * Mono Guard     ->  per band (Mono Below–1 kHz, 1–5 kHz, > 5 kHz) the side is limited so that
//                       stereo is never louder than mono by more than "Mono Safety" dB and the
//                       L/R correlation stays >= 0.25.
#pragma once

#include "Analyzer.h"
#include "Characters.h"
#include "Projector.h"

namespace boba
{
#ifdef BOBA_ENGINE_DEBUG
inline int debugFlags = 0;   // 1 = no projection, 2 = projection FFT = analysis FFT
#endif

enum class Character : int { Air = 0, Prism, Twin, Shine, Phantom, Space, Count };
static constexpr int kNumCharacters = (int) Character::Count;

struct Params
{
    int character = 0;
    double amount = 0.7;         // 0..1
    double color = 0.5;          // 0..1  (meaning depends on the character)
    double keep = 0.0;           // 0..1  Mono Keep: blend comb-safe width into mono
    double width = 1.0;          // 0..2  existing side (1 = as is, 0 = mono)
    double monoBelowHz = 120.0;  // side removed below this
    double monoSafetyDb = 1.5;   // max stereo-vs-mono loudness gap per band
    double rhoMin = 0.25;        // min L/R correlation per band
    double transient = 0.6;      // 0..1 attacks stay centred
    bool monoListen = false;
    bool sideSolo = false;
};

static constexpr std::array<double, kNumBands> kWidthTilt { 0.0, 0.5, 1.0, 4.0 / 3.0 };
static constexpr double kEdgeMid = 1000.0, kEdgeHigh = 5000.0;

struct Meters
{
    std::array<double, kNumBands> monoLossDb {};   // stereo louder than mono, per band
    std::array<double, kNumBands> guardGain {};    // 1 = no limiting
    std::array<double, kNumBands> sideLevel {};    // side / mid power per band, dB
    double correlation = 1.0;
    double monoLossTotalDb = 0.0;
    double monoChangeDb = 0.0;                     // output mono vs input mono (power), dB
    double monoErrorDb = -300.0;                   // |mono out - mono in| relative to mono in, dB
    double keepAddedDb = -300.0;                   // what Mono Keep blends in, relative to mono, dB
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        analyzer.prepare (fs);
        int n2 = analyzer.fftSize() / 2;
#ifdef BOBA_ENGINE_DEBUG
        if (debugFlags & 2) n2 *= 2;
#endif
        proj1.prepare (n2, 0);
        proj2.prepare (n2, n2 / 8);    // shifted by half a hop
        proj3.prepare (n2, n2 / 16);   // shifted by a quarter hop

        dA = analyzer.latency();
        dP = proj1.latency() + proj2.latency() + proj3.latency();

        air.prepare (fs); twin.prepare (fs); shine.prepare (fs); phantom.prepare (fs); space.prepare (fs);
        dPhantom = dA - phantom.lockDelay();

        mDelay.prepare (dA); sDelay.prepare (dA);
        phSide.prepare (dPhantom); phMid.prepare (dPhantom);
        sDelayP.prepare (dP); gDelayP.prepare (dP);
#ifdef BOBA_ENGINE_DEBUG
        dbgX.prepare (dP); dbgG.prepare (dP);
#endif

        for (auto* s : { &amountS, &colorS, &keepS, &widthS }) s->setTime (0.025, fs);
        weightStep = 1.0 / (0.040 * fs);

        for (int b = 0; b < kNumBands; ++b)
        {
            for (auto* e : { &envM[(size_t) b], &envS[(size_t) b], &envA[(size_t) b], &envC[(size_t) b] }) e->setTime (0.030, fs);
            guard[(size_t) b].setTimes (0.002, 0.150, fs);
            meterM[(size_t) b].setTime (0.300, fs);
            meterS[(size_t) b].setTime (0.300, fs);
        }
        for (auto* m : { &mLL, &mRR, &mLR, &mMonoIn, &mMonoOut, &mErr, &mKeep }) m->setTime (0.300, fs);

        prepared = true;
        updateEdges();
        updateK();
        reset();
    }

    void reset()
    {
        analyzer.reset(); proj1.reset(); proj2.reset(); proj3.reset();
        air.reset(); twin.reset(); shine.reset(); phantom.reset(); space.reset();
        for (auto* d : { &mDelay, &sDelay, &phSide, &phMid, &sDelayP, &gDelayP }) d->reset();
        splitM.reset(); splitS.reset(); splitG.reset(); hpSide.reset();
        for (int b = 0; b < kNumBands; ++b)
        {
            envM[(size_t) b].y = envS[(size_t) b].y = envA[(size_t) b].y = envC[(size_t) b].y = 0.0;
            meterM[(size_t) b].y = meterS[(size_t) b].y = 0.0;
            guard[(size_t) b].y = 1.0;
            guardNow[(size_t) b] = 1.0;
        }
        for (auto* m : { &mLL, &mRR, &mLR, &mMonoIn, &mMonoOut, &mErr, &mKeep }) m->y = 0.0;

        amountS.y = params.amount; colorS.y = params.color; keepS.y = params.keep; widthS.y = params.width;
        weights.fill (0.0);
        weights[(size_t) std::clamp (params.character, 0, kNumCharacters - 1)] = 1.0;
        air.setColor (params.color, true);
        space.setColor (params.color, true);
        shine.setColor (params.color);
        phantom.setColor (params.color);
    }

    void setParams (const Params& p)
    {
        const bool edgesChanged = std::abs (p.monoBelowHz - params.monoBelowHz) > 1e-9;
        const bool kChanged = std::abs (p.monoSafetyDb - params.monoSafetyDb) > 1e-9 || std::abs (p.rhoMin - params.rhoMin) > 1e-9;
        params = p;
        params.character = std::clamp (params.character, 0, kNumCharacters - 1);
        if (edgesChanged) updateEdges();
        if (kChanged) updateK();
    }

    const Params& getParams() const noexcept { return params; }
    int getLatencySamples() const noexcept { return dA + dP; }
    double getK() const noexcept { return K; }

    template <typename Sample>
    void process (Sample* left, Sample* right, int numSamples) noexcept
    {
        int done = 0;
        while (done < numSamples)
        {
            const int chunk = std::min (kControlBlock, numSamples - done);
            controlUpdate (chunk);
            for (int i = done; i < done + chunk; ++i)
            {
                double l = (double) left[i], r = (double) right[i];
                processSample (l, r);
                left[i] = (Sample) l;
                right[i] = (Sample) r;
            }
            done += chunk;
        }
    }

    Meters getMeters() const noexcept
    {
        Meters m;
        for (int b = 0; b < kNumBands; ++b)
        {
            const double pm = meterM[(size_t) b].y, ps = meterS[(size_t) b].y;
            m.monoLossDb[(size_t) b] = pm > 1e-12 ? 10.0 * std::log10 ((pm + ps) / pm) : 0.0;
            m.sideLevel[(size_t) b] = pm > 1e-12 ? 10.0 * std::log10 (ps / pm + 1e-30) : -100.0;
            m.guardGain[(size_t) b] = guardNow[(size_t) b];
        }
        const double den = std::sqrt (mLL.y * mRR.y);
        m.correlation = den > 1e-12 ? mLR.y / den : 1.0;
        m.monoLossTotalDb = mMonoOut.y > 1e-12 ? 10.0 * std::log10 (0.5 * (mLL.y + mRR.y) / mMonoOut.y) : 0.0;
        m.monoChangeDb = mMonoIn.y > 1e-12 ? 10.0 * std::log10 (mMonoOut.y / mMonoIn.y) : 0.0;
        m.monoErrorDb = mMonoIn.y > 1e-12 ? 10.0 * std::log10 (mErr.y / mMonoIn.y + 1e-30) : -300.0;
        m.keepAddedDb = mMonoIn.y > 1e-12 ? 10.0 * std::log10 (mKeep.y / mMonoIn.y + 1e-30) : -300.0;
        return m;
    }

private:
    static constexpr int kControlBlock = 32;

    void controlUpdate (int numSamples) noexcept
    {
        // character cross-fade (40 ms)
        const int target = params.character;
        const double step = weightStep * numSamples;
        for (int c = 0; c < kNumCharacters; ++c)
        {
            auto& w = weights[(size_t) c];
            if (c == target && w <= 0.0) resetCharacter (c);   // waking up: start from a clean state
            w = c == target ? std::min (1.0, w + step) : std::max (0.0, w - step);
        }
        // colour: kernels and filters follow the smoothed value
        const double a = std::exp (-numSamples / (0.025 * fs));
        colorS.y = a * colorS.y + (1.0 - a) * params.color;
        air.setColor (colorS.y);
        space.setColor (colorS.y);
        shine.setColor (colorS.y);
        phantom.setColor (colorS.y);
        analyzer.setParams ({ 1.0, colorS.y, params.transient });
    }

    inline void processSample (double& l, double& r) noexcept
    {
        const double M = 0.5 * (l + r);
        const double S = 0.5 * (l - r);

        // ---- analysis (everything below refers to time n - dA)
        const AnalyzerOut an = analyzer.process (M);
        const double md = mDelay.process (M, dA);
        const double sd = sDelay.process (S, dA);
        const CharOut ph0 = weights[4] > 0.0 ? phantom.process (M) : CharOut {};
        const CharOut ph { phSide.process (ph0.side, dPhantom), phMid.process (ph0.mid, dPhantom) };

        const double T = params.transient;
        const double c = colorS.y;
        const double A = amountS.process (params.amount);

        double gSide = 0.0, gMid = 0.0;
        auto addChar = [&] (int idx, const CharOut& o) { const double w = weights[(size_t) idx]; gSide += w * o.side; gMid += w * o.mid; };

        // only characters that are audible (selected or fading out) are computed
        if (weights[0] > 0.0) addChar (0, air.process (md - T * an.p, an.rs));
        if (weights[1] > 0.0) addChar (1, { an.prism, 0.0 });
        if (weights[2] > 0.0) addChar (2, twin.process (md - T * an.p, an.rs, c));
        if (weights[3] > 0.0) addChar (3, shine.process (an.rs + an.ps, md - an.rs - an.ps));
        if (weights[4] > 0.0) addChar (4, ph);
        if (weights[5] > 0.0) addChar (5, space.process (md - 0.5 * T * an.p, an.rs));

        gSide = hpSide.process (A * gSide);
        const double keepG = keepS.process (params.keep) * A * gMid;

        // ---- never-subtract projection of the mono blend (three shifted grids)
        double x1, g1, x0, g0, x2, g2;
        proj1.process (md, keepG, x1, g1);
        proj2.process (x1, g1, x0, g0);
        proj3.process (x0, g0, x2, g2);
#ifdef BOBA_ENGINE_DEBUG
        if ((debugFlags & 1) && ! (debugFlags & 4)) { x2 = dbgX.process (md, dP); g2 = dbgG.process (keepG, dP); }
#endif
        const double s2 = sDelayP.process (sd, dP);
        const double gs2 = gDelayP.process (gSide, dP);
        const double mid = x2 + g2;

        // ---- Width + Mono Guard
        splitM.process (mid, mb);
        splitS.process (s2, sb);
        splitG.process (gs2, gb);
        const double W = widthS.process (params.width);

        double sOut = 0.0;
        meterM[0].process (mb[0] * mb[0]);
        meterS[0].process (0.0);
        guardNow[0] = 1.0;
        for (size_t b = 1; b < (size_t) kNumBands; ++b)
        {
            const double w = std::max (0.0, 1.0 + (W - 1.0) * kWidthTilt[b]);
            const double base = sb[b] * std::min (w, 1.0);                     // narrowing is never limited
            const double added = std::max (w - 1.0, 0.0) * sb[b] + gb[b];

            const double pM = envM[b].process (mb[b] * mb[b]);
            const double pS = envS[b].process (base * base);
            double pA = envA[b].process (added * added);
            pA += 1e-9 * (pA + pM + pS) + 1e-30;                             // relative floor
            const double cSA = envC[b].process (base * added);
            // exact: largest g in [0,1] with  pS + 2 g cSA + g^2 pA <= K pM
            const double disc = cSA * cSA - pA * (pS - K * pM);
            const double gRaw = disc > 0.0 ? (-cSA + std::sqrt (disc)) / pA : 0.0;
            const double g = guard[b].process (std::clamp (gRaw, 0.0, 1.0));

            const double sBand = base + g * added;
            sOut += sBand;
            meterM[b].process (mb[b] * mb[b]);
            meterS[b].process (sBand * sBand);
            guardNow[b] = g;
        }

        const double L = mid + sOut, R = mid - sOut;
        mLL.process (L * L); mRR.process (R * R); mLR.process (L * R);
        mMonoIn.process (x2 * x2);
        mMonoOut.process (mid * mid);
        const double e = 0.5 * (L + R) - x2;
        mErr.process (e * e);
        mKeep.process (g2 * g2);

        double outL = L, outR = R;
#ifdef BOBA_ENGINE_DEBUG
        if (debugFlags & 4) { l = dbgX.process (md, dP); r = dbgG.process (keepG, dP); return; }   // raw mid + unprojected blend
#endif
        if (params.sideSolo) { outL = sOut; outR = -sOut; }
        if (params.monoListen) { const double mono = 0.5 * (outL + outR); outL = outR = mono; }
        l = outL;
        r = outR;
    }

    void resetCharacter (int c) noexcept
    {
        switch (c)
        {
            case 0: air.reset(); air.setColor (colorS.y, true); break;
            case 2: twin.reset(); break;
            case 3: shine.reset(); break;
            case 4: phantom.reset(); phSide.reset(); phMid.reset(); break;
            case 5: space.reset(); space.setColor (colorS.y, true); break;
            default: break;
        }
    }

    void updateEdges()
    {
        if (! prepared) return;
        const double low = std::clamp (params.monoBelowHz, 20.0, 400.0);
        const std::array<double, kNumEdges> edges { low, std::max (kEdgeMid, low * 2.0), kEdgeHigh };
        splitM.setEdges (edges, fs);
        splitS.setEdges (edges, fs);
        splitG.setEdges (edges, fs);
        hpSide.set (true, low, fs);
    }

    void updateK()
    {
        const double kRho = (1.0 - params.rhoMin) / (1.0 + params.rhoMin);
        const double kLoss = std::pow (10.0, params.monoSafetyDb / 10.0) - 1.0;
        K = std::max (0.0, std::min (kRho, kLoss));
    }

    double fs = 48000.0;
    bool prepared = false;
    Params params;
    double K = 0.41;

    Analyzer analyzer;
    Projector proj1, proj2, proj3;
    AirChar air;
    TwinChar twin;
    ShineChar shine;
    PhantomChar phantom;
    SpaceChar space;

    int dA = 0, dP = 0, dPhantom = 0;
    DelayLine mDelay, sDelay, phSide, phMid, sDelayP, gDelayP;
#ifdef BOBA_ENGINE_DEBUG
    DelayLine dbgX, dbgG;
#endif

    OnePole amountS, colorS, keepS, widthS;
    std::array<double, kNumCharacters> weights {};
    double weightStep = 0.0;

    LR4 hpSide;
    BandSplitter splitM, splitS, splitG;
    std::array<double, kNumBands> mb {}, sb {}, gb {};
    std::array<OnePole, kNumBands> envM, envS, envA, envC, meterM, meterS;
    std::array<GainSmoother, kNumBands> guard;
    std::array<double, kNumBands> guardNow {};
    OnePole mLL, mRR, mLR, mMonoIn, mMonoOut, mErr, mKeep;
};

} // namespace boba
