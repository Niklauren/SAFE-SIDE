// Offline renderer for the SAFE-SIDE engine (no JUCE needed).
//
//   render in.f64 out.f64 fs character amount color keep width [monoBelow safety transient] [--blocks random|N]
//
// Files are raw interleaved stereo float64. The output is latency-compensated (same length as the input).
// Prints latency and real-time factor.
#include "../Source/dsp/Engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 9)
    {
        std::fprintf (stderr, "usage: render in.f64 out.f64 fs character amount color keep width [monoBelow safety transient] [--blocks random|N]\n");
        return 1;
    }
    const double fs = std::atof (argv[3]);
    boba::Params p;
    p.character = std::atoi (argv[4]);
    p.amount = std::atof (argv[5]);
    p.color = std::atof (argv[6]);
    p.keep = std::atof (argv[7]);
    p.width = std::atof (argv[8]);
    int ai = 9;
    if (argc > 11 && std::strncmp (argv[9], "--", 2) != 0)
    {
        p.monoBelowHz = std::atof (argv[9]);
        p.monoSafetyDb = std::atof (argv[10]);
        p.transient = std::atof (argv[11]);
        ai = 12;
    }
    int blockMode = 512;   // 0 = random
    for (; ai < argc; ++ai)
    {
#ifdef BOBA_ENGINE_DEBUG
        if (std::strcmp (argv[ai], "--debug") == 0 && ai + 1 < argc) { boba::debugFlags = std::atoi (argv[++ai]); continue; }
#endif
        if (std::strcmp (argv[ai], "--blocks") == 0 && ai + 1 < argc)
        {
            ++ai;
            blockMode = std::strcmp (argv[ai], "random") == 0 ? 0 : std::atoi (argv[ai]);
        }
    }

    FILE* f = std::fopen (argv[1], "rb");
    if (! f) { std::perror ("in"); return 1; }
    std::fseek (f, 0, SEEK_END);
    const long bytes = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);
    const long frames = bytes / 16;
    std::vector<double> inter ((size_t) frames * 2);
    if (std::fread (inter.data(), 16, (size_t) frames, f) != (size_t) frames) { std::fprintf (stderr, "read error\n"); return 1; }
    std::fclose (f);

    auto* engine = new boba::Engine();
    engine->setParams (p);
    engine->prepare (fs);
    const int lat = engine->getLatencySamples();

    const long total = frames + lat;
    std::vector<double> L ((size_t) total, 0.0), R ((size_t) total, 0.0);
    for (long i = 0; i < frames; ++i) { L[(size_t) i] = inter[(size_t) (2 * i)]; R[(size_t) i] = inter[(size_t) (2 * i + 1)]; }

    std::mt19937 rng (1234);
    const auto t0 = std::chrono::steady_clock::now();
    long pos = 0;
    while (pos < total)
    {
        int bs = blockMode > 0 ? blockMode : (int) (1 + rng() % 1024);
        bs = (int) std::min<long> (bs, total - pos);
        engine->process (L.data() + pos, R.data() + pos, bs);
        pos += bs;
    }
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    for (long i = 0; i < frames; ++i) { inter[(size_t) (2 * i)] = L[(size_t) (i + lat)]; inter[(size_t) (2 * i + 1)] = R[(size_t) (i + lat)]; }
    FILE* o = std::fopen (argv[2], "wb");
    std::fwrite (inter.data(), 16, (size_t) frames, o);
    std::fclose (o);

    std::printf ("latency %d samples (%.1f ms), realtime factor %.1fx\n", lat, 1000.0 * lat / fs, (total / fs) / sec);
    delete engine;
    return 0;
}
