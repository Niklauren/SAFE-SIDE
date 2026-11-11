#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class BobaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BobaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    static juce::TextLayout layoutTooltip (const juce::String& text, float maxWidth);
};

//==============================================================================
class CharacterBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CharacterBar (juce::RangedAudioParameter& param);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getSelected() const noexcept { return selected; }
    std::function<void()> onChange;

private:
    int tileAt (juce::Point<float>) const;
    juce::Rectangle<float> tile (int i) const;
    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};

//==============================================================================
class MeterPanel final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MeterPanel (SafeSideAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void update();

private:
    SafeSideAudioProcessor& proc;
    float corr = 1.0f, lossTotal = 0.0f, monoErr = -300.0f, monoChange = 0.0f, keepAdded = -300.0f;
    std::array<float, boba::kNumBands> loss {}, guard { 1.0f, 1.0f, 1.0f, 1.0f };
};

//==============================================================================
class SafeSideAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SafeSideAudioProcessorEditor (SafeSideAudioProcessor&);
    ~SafeSideAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void characterChanged();

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& tooltip, bool big, juce::Colour arc);

    SafeSideAudioProcessor& proc;
    BobaLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 1200 };

    CharacterBar characters;
    Knob amount, color, width, keep, monoBelow, monoSafety, transient;
    juce::ToggleButton monoButton { "MONO CHECK" }, soloButton { "SIDE SOLO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAtt, soloAtt;
    MeterPanel meters;
    juce::Rectangle<int> logoArea, knobPanel, descArea, guardHeader;
    int shownCharacter = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SafeSideAudioProcessorEditor)
};
