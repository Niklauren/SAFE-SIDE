#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::String character  { "character" };
    static const juce::String amount     { "amount" };
    static const juce::String color      { "color" };
    static const juce::String keep       { "keep" };
    static const juce::String width      { "width" };
    static const juce::String monoBelow  { "monoBelow" };
    static const juce::String monoSafety { "monoSafety" };
    static const juce::String transient  { "transient" };
    static const juce::String mono       { "mono" };
    static const juce::String sideSolo   { "sideSolo" };
}

//==============================================================================
SafeSideAudioProcessor::SafeSideAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SAFESIDE", createLayout())
{
    for (auto& m : meterMonoLossDb) m.store (0.0f);
    for (auto& g : meterGuardGain) g.store (1.0f);

    pCharacter  = apvts.getRawParameterValue (ids::character);
    pAmount     = apvts.getRawParameterValue (ids::amount);
    pColor      = apvts.getRawParameterValue (ids::color);
    pKeep       = apvts.getRawParameterValue (ids::keep);
    pWidth      = apvts.getRawParameterValue (ids::width);
    pMonoBelow  = apvts.getRawParameterValue (ids::monoBelow);
    pMonoSafety = apvts.getRawParameterValue (ids::monoSafety);
    pTransient  = apvts.getRawParameterValue (ids::transient);
    pMono       = apvts.getRawParameterValue (ids::mono);
    pSideSolo   = apvts.getRawParameterValue (ids::sideSolo);
}

juce::AudioProcessorValueTreeState::ParameterLayout SafeSideAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto pctAttr = [pct] { return AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct); };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::character, 1 }, "Character",
        StringArray { "AIR", "PRISM", "TWIN", "SHINE", "PHANTOM", "SPACE" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::amount, 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, pctAttr()));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::color, 1 }, "Color",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::keep, 1 }, "Mono Keep",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctAttr()));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::width, 1 }, "Width",
        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pctAttr()));

    NormalisableRange<float> hz (20.0f, 300.0f, 1.0f);
    hz.setSkewForCentre (100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::monoBelow, 1 }, "Mono Below", hz, 120.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::monoSafety, 1 }, "Mono Safety",
        NormalisableRange<float> (0.5f, 3.0f, 0.01f), 1.5f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::transient, 1 }, "Transient",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pctAttr()));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::mono, 1 }, "Mono Check", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::sideSolo, 1 }, "Side Solo", false));

    return layout;
}

bool SafeSideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void SafeSideAudioProcessor::pullParams()
{
    boba::Params p;
    p.character    = juce::jlimit (0, boba::kNumCharacters - 1, (int) std::lround (pCharacter->load()));
    p.amount       = pAmount->load() / 100.0;
    p.color        = pColor->load() / 100.0;
    p.keep         = pKeep->load() / 100.0;
    p.width        = pWidth->load() / 100.0;
    p.monoBelowHz  = pMonoBelow->load();
    p.monoSafetyDb = pMonoSafety->load();
    p.transient    = pTransient->load() / 100.0;
    p.monoListen   = pMono->load() > 0.5f;
    p.sideSolo     = pSideSolo->load() > 0.5f;
    engine.setParams (p);
}

void SafeSideAudioProcessor::prepareToPlay (double sampleRate, int)
{
    pullParams();
    engine.prepare (sampleRate);
    const int lat = engine.getLatencySamples();
    for (auto& d : bypassDelay) d.prepare (lat);
    latencySamples.store (lat);
    currentSampleRate.store (sampleRate);
    setLatencySamples (lat);
}

template <typename Sample>
void SafeSideAudioProcessor::processAny (juce::AudioBuffer<Sample>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
        return;
    if (getTotalNumInputChannels() == 1)                 // mono track: duplicate, then widen
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    pullParams();
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    publishMeters();
}

// Bypass keeps the same latency, so switching it never shifts the track in time.
template <typename Sample>
void SafeSideAudioProcessor::processBypassedAny (juce::AudioBuffer<Sample>& buffer)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
        return;
    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const int lat = latencySamples.load();
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = (Sample) bypassDelay[(size_t) ch].process ((double) d[i], lat);
    }
}

void SafeSideAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)          { processAny (b); }
void SafeSideAudioProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&)         { processAny (b); }
void SafeSideAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { processBypassedAny (b); }
void SafeSideAudioProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processBypassedAny (b); }

void SafeSideAudioProcessor::publishMeters()
{
    const auto m = engine.getMeters();
    for (size_t b = 0; b < (size_t) boba::kNumBands; ++b)
    {
        meterMonoLossDb[b].store ((float) m.monoLossDb[b]);
        meterGuardGain[b].store ((float) m.guardGain[b]);
    }
    meterCorrelation.store ((float) m.correlation);
    meterMonoLossTotalDb.store ((float) m.monoLossTotalDb);
    meterMonoChangeDb.store ((float) m.monoChangeDb);
    meterMonoErrorDb.store ((float) m.monoErrorDb);
    meterKeepAddedDb.store ((float) m.keepAddedDb);
}

//==============================================================================
void SafeSideAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SafeSideAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SafeSideAudioProcessor::createEditor()
{
    return new SafeSideAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SafeSideAudioProcessor();
}
