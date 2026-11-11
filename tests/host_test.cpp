// Dev tool: loads the built SAFE-SIDE.vst3 like a DAW does and checks the mono contract through the host API.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: host_test path/to/SAFE-SIDE.vst3\n"); return 2; }

    juce::AudioPluginFormatManager fm;
    fm.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argv[1]);
    if (found.isEmpty()) { std::printf ("FAIL: plug-in not found\n"); return 1; }
    auto& d = *found[0];
    std::printf ("found: %s by %s, %s, version %s\n", d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(), d.category.toRawUTF8(), d.version.toRawUTF8());

    bool pass = true;
    const char* names[] = { "AIR", "PRISM", "TWIN", "SHINE", "PHANTOM", "SPACE" };
    for (int c = 0; c < 6; ++c)
    {
        juce::String err;
        auto inst = fm.createPluginInstance (d, 48000.0, 512, err);
        if (! inst) { std::printf ("FAIL: %s\n", err.toRawUTF8()); return 1; }
        inst->setPlayConfigDetails (2, 2, 48000.0, 512);
        for (auto* p : inst->getParameters())
        {
            const auto n = p->getName (32);
            if (n == "Width" || n == "Amount") p->setValueNotifyingHost (1.0f);
            if (n == "Character") p->setValueNotifyingHost ((float) c / 5.0f);
        }
        inst->prepareToPlay (48000.0, 512);
        const int lat = inst->getLatencySamples();

        juce::Random rnd (3);
        const int blocks = 300, total = blocks * 512;
        std::vector<float> inM ((size_t) total), outM ((size_t) total), outS ((size_t) total);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double ph = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                ph += 2.0 * juce::MathConstants<double>::pi * 110.0 / 48000.0;
                const float s = 0.3f * (float) std::sin (ph) + 0.2f * (rnd.nextFloat() - 0.5f);
                const float l = s + 0.05f * (rnd.nextFloat() - 0.5f), r = s + 0.05f * (rnd.nextFloat() - 0.5f);
                buf.setSample (0, i, l); buf.setSample (1, i, r);
                inM[(size_t) (b * 512 + i)] = 0.5f * (l + r);
            }
            inst->processBlock (buf, midi);
            for (int i = 0; i < 512; ++i)
            {
                outM[(size_t) (b * 512 + i)] = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                outS[(size_t) (b * 512 + i)] = 0.5f * (buf.getSample (0, i) - buf.getSample (1, i));
            }
        }
        double eErr = 0, eRef = 0, eSide = 0;
        for (int i = 20000; i < total - lat; ++i)
        {
            const double e = outM[(size_t) (i + lat)] - inM[(size_t) i];
            eErr += e * e; eRef += (double) inM[(size_t) i] * inM[(size_t) i]; eSide += (double) outS[(size_t) (i + lat)] * outS[(size_t) (i + lat)];
        }
        const double nullDb = 10.0 * std::log10 (eErr / eRef + 1e-300);
        const bool ok = nullDb < -120.0 && eSide > 0.0;
        pass &= ok;
        std::printf ("%-8s latency %d: mono out vs mono in %.1f dB (float I/O), side/mid %.1f dB  %s\n", names[c], lat, nullDb,
                     10.0 * std::log10 (eSide / eRef), ok ? "ok" : "FAIL");

        if (c == 0)
        {
            juce::MemoryBlock state;
            inst->getStateInformation (state);
            auto inst2 = fm.createPluginInstance (d, 48000.0, 512, err);
            inst2->setStateInformation (state.getData(), (int) state.getSize());
            bool same = true;
            for (int i = 0; i < inst->getParameters().size(); ++i)
                same &= std::abs (inst->getParameters()[i]->getValue() - inst2->getParameters()[i]->getValue()) < 1e-6f;
            std::printf ("state save/load: %s, editor: %s\n", same ? "OK" : "MISMATCH", inst->hasEditor() ? "yes" : "no");
            pass &= same;
        }
        inst->releaseResources();
    }
    std::printf ("%s\n", pass ? "HOST TEST PASSED" : "HOST TEST FAILED");
    return pass ? 0 : 1;
}
