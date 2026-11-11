// Dev tool: renders the plug-in editor to a PNG after feeding audio through the processor,
// so the UI and meters can be checked without a DAW.
//   SafeSideSnapshot out.png [character 0-5] [keep 0-100] [hover x y]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outPath = argc > 1 ? argv[1] : "ui.png";
    const int character = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float keep = argc > 3 ? juce::String (argv[3]).getFloatValue() : 0.0f;

    SafeSideAudioProcessor proc;
    if (auto* p = proc.apvts.getParameter ("character")) p->setValueNotifyingHost (p->convertTo0to1 ((float) character));
    if (auto* p = proc.apvts.getParameter ("keep")) p->setValueNotifyingHost (p->convertTo0to1 (keep));
    if (auto* p = proc.apvts.getParameter ("width")) p->setValueNotifyingHost (p->convertTo0to1 (140.0f));
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // 3 s of a beat-like stereo signal: 808-ish bass, chord, hats
    juce::Random rnd (7);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double ph = 0.0, t = 0.0;
    for (int block = 0; block < 280; ++block)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / 48000.0)
        {
            const double tb = std::fmod (t, 0.857);
            ph += 2.0 * juce::MathConstants<double>::pi * 46.25 * (1.0 + 0.9 * std::exp (-tb / 0.025)) / 48000.0;
            const float bass = 0.35f * (float) std::tanh (2.0 * std::sin (ph) * std::exp (-tb / 0.7));
            float chord = 0.0f;
            for (double f : { 220.0, 277.2, 329.6, 440.0 })
                chord += 0.05f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * t);
            const float hat = (rnd.nextFloat() * 2.0f - 1.0f) * 0.12f * (float) std::exp (-std::fmod (t, 0.214) / 0.02);
            buf.setSample (0, i, bass + chord + hat * 1.2f);
            buf.setSample (1, i, bass + chord * 0.9f + hat * 0.8f);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);

    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (img, os);
    std::printf ("snapshot %s: %s (%dx%d)\n", ok ? "written" : "FAILED", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    editor.reset();
    return ok ? 0 : 1;
}
