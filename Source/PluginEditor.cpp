#include "PluginEditor.h"
#include "Theme.h"
#include "Tooltips.h"

using namespace boba::ui;

//==============================================================================
BobaLookAndFeel::BobaLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::caramel.withAlpha (0.35f));
    setColour (juce::Label::textColourId, col::dim);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::backgroundColourId, col::raised);
    setColour (juce::TextEditor::highlightColourId, col::caramel.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, col::caramel);
    setColour (juce::TooltipWindow::backgroundColourId, col::raised);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::line2);
}

void BobaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (3.0f, radius * 0.11f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto arcCol = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (arcCol);
    g.strokePath (value, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // knob body: a soft "pearl"
    const float knobR = arcR - lineW * 1.7f;
    juce::ColourGradient grad (col::raised2.brighter (0.12f), c.x, c.y - knobR, col::panel, c.x, c.y + knobR, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (col::line2.withAlpha (0.8f));
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    const float pw = juce::jmax (2.5f, knobR * 0.09f);
    pointer.addRoundedRectangle (-pw * 0.5f, -knobR + knobR * 0.14f, pw, knobR * 0.42f, pw * 0.5f);
    g.setColour (col::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void BobaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::caramel : (highlighted ? col::raised2 : col::raised));
    g.fillRoundedRectangle (r, 8.0f);
    if (! on)
    {
        g.setColour (col::line2);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
    drawTracked (g, b.getButtonText(), r, 12.5f, 700, on ? col::pearl : col::text2, juce::Justification::centred, 0.1f);
}

juce::Label* BobaLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (13.0f, 600));
    l->setColour (juce::Label::textColourId, col::text2);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::TextLayout BobaLookAndFeel::layoutTooltip (const juce::String& text, float maxWidth)
{
    const auto title = text.upToFirstOccurrenceOf ("\n", false, false);
    const auto body = text.fromFirstOccurrenceOf ("\n", false, false);
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (title, systemFont (13.5f, true), col::caramel2);
    if (body.isNotEmpty())
        s.append ("\n" + body, systemFont (13.0f), col::text);
    s.setLineSpacing (2.0f);
    juce::TextLayout tl;
    tl.createLayout (s, maxWidth);
    return tl;
}

juce::Rectangle<int> BobaLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto tl = layoutTooltip (text, 300.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 24, h = (int) std::ceil (tl.getHeight()) + 18;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

void BobaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (col::raised);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::line2);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    layoutTooltip (text, (float) width - 24.0f).draw (g, r.reduced (12.0f, 9.0f));
}

//==============================================================================
CharacterBar::CharacterBar (juce::RangedAudioParameter& param)
    : attachment (param, [this] (float v)
                  {
                      selected = juce::jlimit (0, boba::kNumCharacters - 1, (int) std::lround (v));
                      repaint();
                      if (onChange) onChange();
                  })
{
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> CharacterBar::tile (int i) const
{
    const float gap = 8.0f;
    const float w = (getWidth() - gap * (boba::kNumCharacters - 1)) / (float) boba::kNumCharacters;
    return { i * (w + gap), 0.0f, w, (float) getHeight() };
}

int CharacterBar::tileAt (juce::Point<float> p) const
{
    for (int i = 0; i < boba::kNumCharacters; ++i)
        if (tile (i).contains (p)) return i;
    return -1;
}

void CharacterBar::mouseMove (const juce::MouseEvent& e)
{
    const int h = tileAt (e.position);
    if (h != hover)
    {
        hover = h;
        setTooltip (h >= 0 ? boba::tips::ru (boba::tips::character (h).tipRu) : juce::String());
        repaint();
    }
}

void CharacterBar::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void CharacterBar::mouseDown (const juce::MouseEvent& e)
{
    const int t = tileAt (e.position);
    if (t >= 0) attachment.setValueAsCompleteGesture ((float) t);
}

// small glyph per character, drawn as vector in the tile
static void drawGlyph (juce::Graphics& g, int i, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    const auto ctr = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    juce::Path p;
    switch (i)
    {
        case 0:   // AIR: scattered dots
        {
            const float pts[][2] = { { -0.36f, 0.1f }, { -0.2f, -0.25f }, { -0.05f, 0.22f }, { 0.08f, -0.12f }, { 0.22f, 0.18f }, { 0.36f, -0.2f }, { 0.0f, -0.36f }, { -0.3f, -0.05f } };
            for (auto& q : pts) g.fillEllipse (ctr.x + q[0] * s - 2.0f, ctr.y + q[1] * s - 2.0f, 4.0f, 4.0f);
            return;
        }
        case 1:   // PRISM: fan of lines
            for (int k = -3; k <= 3; ++k)
            {
                const float a = k * 0.22f;
                p.startNewSubPath (ctr.x - s * 0.4f, ctr.y);
                p.lineTo (ctr.x + s * 0.4f * std::cos (a), ctr.y + s * 0.4f * std::sin (a));
            }
            break;
        case 2:   // TWIN: two offset waves
            for (int k = 0; k < 2; ++k)
            {
                juce::Path w;
                for (int j = 0; j <= 24; ++j)
                {
                    const float t = j / 24.0f;
                    const float xx = ctr.x - s * 0.42f + t * s * 0.84f;
                    const float yy = ctr.y + (k == 0 ? -1.0f : 1.0f) * s * 0.08f + s * 0.16f * std::sin (t * 6.283f * 1.5f + k * 0.9f);
                    if (j == 0) w.startNewSubPath (xx, yy); else w.lineTo (xx, yy);
                }
                p.addPath (w);
            }
            break;
        case 3:   // SHINE: sparkle
            for (int k = 0; k < 4; ++k)
            {
                const float a = k * juce::MathConstants<float>::pi / 4.0f;
                const float len = (k % 2 == 0 ? 0.42f : 0.24f) * s;
                p.startNewSubPath (ctr.x - len * std::cos (a), ctr.y - len * std::sin (a));
                p.lineTo (ctr.x + len * std::cos (a), ctr.y + len * std::sin (a));
            }
            break;
        case 4:   // PHANTOM: sub circle + harmonic rings
            g.fillEllipse (ctr.x - s * 0.1f, ctr.y - s * 0.1f, s * 0.2f, s * 0.2f);
            p.addCentredArc (ctr.x, ctr.y, s * 0.24f, s * 0.24f, 0.0f, -2.2f, -0.9f, true);
            p.addCentredArc (ctr.x, ctr.y, s * 0.24f, s * 0.24f, 0.0f, 0.9f, 2.2f, true);
            p.addCentredArc (ctr.x, ctr.y, s * 0.4f, s * 0.4f, 0.0f, -2.0f, -1.1f, true);
            p.addCentredArc (ctr.x, ctr.y, s * 0.4f, s * 0.4f, 0.0f, 1.1f, 2.0f, true);
            break;
        default:  // SPACE: nested rounded squares (room)
            for (int k = 1; k <= 3; ++k)
            {
                const float hs = s * (0.13f * k);
                p.addRoundedRectangle (ctr.x - hs, ctr.y - hs, hs * 2.0f, hs * 2.0f, hs * 0.3f);
            }
            break;
    }
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CharacterBar::paint (juce::Graphics& g)
{
    for (int i = 0; i < boba::kNumCharacters; ++i)
    {
        const auto r = tile (i);
        const bool sel = i == selected, hov = i == hover;
        g.setColour (sel ? col::caramel : (hov ? col::raised2 : col::raised));
        g.fillRoundedRectangle (r, 10.0f);
        if (! sel)
        {
            g.setColour (col::line);
            g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        }
        const auto& info = boba::tips::character (i);
        auto inner = r.reduced (12.0f, 10.0f);
        auto glyph = inner.removeFromLeft (30.0f);
        drawGlyph (g, i, glyph, sel ? col::pearl : col::caramel);
        inner.removeFromLeft (8.0f);
        drawTracked (g, info.name, inner.removeFromTop (inner.getHeight() * 0.56f), 15.5f, 800,
                     sel ? col::pearl : col::text, juce::Justification::bottomLeft, 0.06f);
        drawTracked (g, info.tag, inner, 11.5f, 600, sel ? col::pearl.withAlpha (0.7f) : col::dim,
                     juce::Justification::topLeft, 0.04f);
    }
}

//==============================================================================
void MeterPanel::update()
{
    auto smooth = [] (float& v, float target) { v += 0.3f * (target - v); };
    smooth (corr, proc.meterCorrelation.load());
    smooth (lossTotal, proc.meterMonoLossTotalDb.load());
    smooth (monoChange, proc.meterMonoChangeDb.load());
    monoErr = proc.meterMonoErrorDb.load();
    keepAdded = proc.meterKeepAddedDb.load();
    for (size_t b = 0; b < loss.size(); ++b)
    {
        smooth (loss[b], proc.meterMonoLossDb[b].load());
        smooth (guard[b], proc.meterGuardGain[b].load());
    }
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    r.reduce (18.0f, 16.0f);

    // ---- correlation
    drawTracked (g, "L/R CORRELATION", r.removeFromTop (16.0f), 11.0f, 700, col::dim, juce::Justification::centredLeft);
    r.removeFromTop (8.0f);
    auto bar = r.removeFromTop (12.0f);
    g.setColour (col::raised2);
    g.fillRoundedRectangle (bar, 6.0f);
    const float mid = bar.getCentreX();
    const float c = juce::jlimit (-1.0f, 1.0f, corr);
    const float xPos = mid + c * bar.getWidth() * 0.5f;
    g.setColour (c >= 0.0f ? col::caramel : col::bad);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, xPos), bar.getY(), std::abs (xPos - mid), bar.getHeight()), 6.0f);
    g.setColour (col::faint);
    g.fillRect (mid - 0.5f, bar.getY() - 3.0f, 1.0f, bar.getHeight() + 6.0f);
    auto scale = r.removeFromTop (18.0f);
    g.setFont (font (11.0f, 600));
    g.setColour (col::faint);
    g.drawText ("-1", scale, juce::Justification::centredLeft);
    g.drawText ("+1", scale, juce::Justification::centredRight);
    g.setColour (col::text);
    g.setFont (font (12.0f, 700));
    g.drawText (juce::String (c, 2), scale, juce::Justification::centred);

    r.removeFromTop (12.0f);

    // ---- stereo louder than mono, per band
    drawTracked (g, "STEREO LOUDER THAN MONO", r.removeFromTop (16.0f), 11.0f, 700, col::dim, juce::Justification::centredLeft);
    r.removeFromTop (6.0f);
    const float limitDb = proc.apvts.getRawParameterValue ("monoSafety")->load();
    const float maxDb = 3.0f;
    const char* names[] = { "LOW-MID", "MID", "HIGH" };
    for (size_t b = 1; b < (size_t) boba::kNumBands; ++b)
    {
        auto row = r.removeFromTop (24.0f);
        auto label = row.removeFromLeft (62.0f);
        auto valueArea = row.removeFromRight (62.0f);
        auto track = row.reduced (0.0f, 8.0f);
        g.setFont (font (11.0f, 600));
        g.setColour (col::dim);
        g.drawText (names[b - 1], label, juce::Justification::centredLeft);
        g.setColour (col::raised2);
        g.fillRoundedRectangle (track, 4.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, loss[b] / maxDb);
        const bool guarding = guard[b] < 0.97f;
        g.setColour (guarding ? col::warn : col::caramel);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * frac), 4.0f);
        const float limX = track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, limitDb / maxDb);
        g.setColour (col::text.withAlpha (0.85f));
        g.fillRect (limX - 0.75f, track.getY() - 4.0f, 1.5f, track.getHeight() + 8.0f);
        g.setFont (font (12.0f, 700));
        g.setColour (guarding ? col::warn : col::text2);
        g.drawText (juce::String (loss[b], 1) + " dB", valueArea, juce::Justification::centredRight);
    }
    {
        auto row = r.removeFromTop (22.0f);
        g.setFont (font (11.0f, 700));
        g.setColour (col::dim);
        g.drawText ("TOTAL", row.removeFromLeft (62.0f), juce::Justification::centredLeft);
        g.setColour (col::text);
        g.setFont (font (12.0f, 700));
        g.drawText (juce::String (lossTotal, 1) + " dB", row.removeFromRight (62.0f), juce::Justification::centredRight);
    }

    r.removeFromTop (10.0f);
    g.setColour (col::line);
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
    r.removeFromTop (12.0f);

    // ---- proof: mono out vs mono in
    drawTracked (g, "MONO OUT vs MONO IN", r.removeFromTop (16.0f), 11.0f, 700, col::dim, juce::Justification::centredLeft);
    r.removeFromTop (6.0f);
    auto proof = r.removeFromTop (34.0f);
    const bool keepOn = proc.apvts.getRawParameterValue ("keep")->load() > 0.05f && keepAdded > -90.0f;
    const bool exact = monoErr < -120.0f;
    juce::String big, small;
    juce::Colour cc;
    if (! keepOn && exact) { big = "IDENTICAL"; small = "bit-exact, only delayed"; cc = col::good; }
    else if (keepOn)       { big = (monoChange >= 0.0f ? "+" : "") + juce::String (monoChange, 2) + " dB"; small = "Mono Keep adds, never cuts"; cc = col::taro; }
    else                   { big = juce::String (monoErr, 1) + " dB"; small = "difference"; cc = col::warn; }
    g.setColour (cc.withAlpha (0.12f));
    g.fillRoundedRectangle (proof, 8.0f);
    g.setColour (cc);
    g.drawRoundedRectangle (proof.reduced (0.5f), 8.0f, 1.0f);
    auto pin = proof.reduced (12.0f, 0.0f);
    drawTracked (g, big, pin, 15.0f, 800, cc, juce::Justification::centredLeft, 0.05f);
    g.setFont (font (11.0f, 600));
    g.setColour (col::text2);
    g.drawText (small, pin, juce::Justification::centredRight);

    // ---- latency
    r.removeFromTop (10.0f);
    const double fs = proc.currentSampleRate.load();
    const int lat = proc.latencySamples.load();
    g.setFont (font (11.0f, 400));
    g.setColour (col::faint);
    g.drawText ("Latency " + juce::String (juce::roundToInt (fs > 0 ? 1000.0 * lat / fs : 0.0)) + " ms, compensated by the DAW",
                r.removeFromTop (16.0f), juce::Justification::centredLeft);
}

//==============================================================================
SafeSideAudioProcessorEditor::SafeSideAudioProcessorEditor (SafeSideAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), characters (*p.apvts.getParameter ("character")), meters (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (characters);
    characters.onChange = [this] { characterChanged(); };

    setupKnob (amount, "amount", "AMOUNT", boba::tips::amount(), true, col::caramel);
    setupKnob (color, "color", "COLOR", {}, true, col::caramel2);
    setupKnob (width, "width", "WIDTH", boba::tips::width(), true, col::caramel);
    setupKnob (keep, "keep", "MONO KEEP", boba::tips::keep(), true, col::taro);
    setupKnob (monoBelow, "monoBelow", "MONO BELOW", boba::tips::monoBelow(), false, col::caramel);
    setupKnob (monoSafety, "monoSafety", "MONO SAFETY", boba::tips::monoSafety(), false, col::caramel);
    setupKnob (transient, "transient", "TRANSIENT", boba::tips::transient(), false, col::caramel);

    addAndMakeVisible (monoButton);
    addAndMakeVisible (soloButton);
    monoButton.setTooltip (boba::tips::monoCheck());
    soloButton.setTooltip (boba::tips::sideSolo());
    monoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mono", monoButton);
    soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sideSolo", soloButton);

    addAndMakeVisible (meters);
    meters.setTooltip (boba::tips::meters());

    characterChanged();
    setSize (900, 580);
    startTimerHz (30);
}

SafeSideAudioProcessorEditor::~SafeSideAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SafeSideAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                                              const juce::String& tooltip, bool big, juce::Colour arc)
{
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, arc);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setTooltip (tooltip);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (font (big ? 13.0f : 11.5f, 800).withExtraKerningFactor (0.12f));
    k.label.setColour (juce::Label::textColourId, big ? col::text : col::dim);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void SafeSideAudioProcessorEditor::characterChanged()
{
    const int c = characters.getSelected();
    if (c == shownCharacter) return;
    shownCharacter = c;
    const auto& info = boba::tips::character (c);
    color.label.setText (info.colorName, juce::dontSendNotification);
    color.slider.setTooltip (boba::tips::ru (info.colorTipRu));
    repaint (descArea);
}

void SafeSideAudioProcessorEditor::timerCallback()
{
    meters.update();
    // the MONO badge in the header follows Mono Keep
    repaint (logoArea.withX (getWidth() - 300).withWidth (300));
}

void SafeSideAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // ---- header
    drawBobaMark (g, logoArea.withWidth (40).withHeight (40).withY (logoArea.getY() + 4).toFloat());
    auto head = logoArea.toFloat().withTrimmedLeft (54.0f);
    drawTracked (g, "BOBA", head.removeFromTop (18.0f), 12.0f, 800, col::caramel, juce::Justification::bottomLeft, 0.32f);
    drawTracked (g, "SAFE-SIDE", head, 25.0f, 800, col::text, juce::Justification::topLeft, 0.04f);
    g.setFont (font (13.0f, 600));
    g.setColour (col::dim);
    g.drawText ("stereo width that never steals the mono", head.withTrimmedLeft (168.0f).withHeight (27.0f),
                juce::Justification::centredLeft);

    // status badge
    const bool keepOn = proc.apvts.getRawParameterValue ("keep")->load() > 0.05f;
    auto badge = juce::Rectangle<float> ((float) getWidth() - 24.0f - 190.0f, (float) logoArea.getY() + 8.0f, 190.0f, 30.0f);
    const auto bc = keepOn ? col::taro : col::good;
    g.setColour (bc.withAlpha (0.12f));
    g.fillRoundedRectangle (badge, 15.0f);
    g.setColour (bc);
    g.drawRoundedRectangle (badge.reduced (0.5f), 15.0f, 1.0f);
    g.fillEllipse (badge.getX() + 13.0f, badge.getCentreY() - 4.0f, 8.0f, 8.0f);
    drawTracked (g, keepOn ? "MONO + KEEP, NEVER CUT" : "MONO = ORIGINAL", badge.withTrimmedLeft (28.0f), 11.0f, 800, bc,
                 juce::Justification::centredLeft, 0.08f);

    // ---- character description
    const auto& info = boba::tips::character (characters.getSelected());
    g.setFont (font (13.5f, 600));
    g.setColour (col::text2);
    g.drawText (info.line, descArea, juce::Justification::centredLeft);

    // ---- knob panel
    g.setColour (col::panel);
    g.fillRoundedRectangle (knobPanel.toFloat(), 12.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (knobPanel.toFloat().reduced (0.5f), 12.0f, 1.0f);
    const auto gh = guardHeader.toFloat();
    drawTracked (g, "MONO GUARD", gh, 10.5f, 800, col::faint, juce::Justification::centredLeft, 0.24f);
    g.fillRect (gh.getX() + 104.0f, gh.getCentreY(), gh.getWidth() - 104.0f, 1.0f);
}

void SafeSideAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 18);
    logoArea = area.removeFromTop (50);
    area.removeFromTop (14);

    characters.setBounds (area.removeFromTop (66));
    area.removeFromTop (6);
    descArea = area.removeFromTop (28);
    area.removeFromTop (8);

    auto right = area.removeFromRight (300);
    area.removeFromRight (16);
    knobPanel = area;

    auto buttons = right.removeFromBottom (40);
    right.removeFromBottom (12);
    meters.setBounds (right);
    monoButton.setBounds (buttons.removeFromLeft ((buttons.getWidth() - 10) / 2));
    buttons.removeFromLeft (10);
    soloButton.setBounds (buttons);

    auto place = [] (Knob& k, juce::Rectangle<int> cell, int labelH)
    {
        k.label.setBounds (cell.removeFromTop (labelH));
        k.slider.setBounds (cell);
    };

    auto inner = knobPanel.reduced (14, 16);
    auto bigRow = inner.removeFromTop (166);
    const int bw = bigRow.getWidth() / 4;
    place (amount, bigRow.removeFromLeft (bw).reduced (4, 0), 22);
    place (color, bigRow.removeFromLeft (bw).reduced (4, 0), 22);
    place (width, bigRow.removeFromLeft (bw).reduced (4, 0), 22);
    place (keep, bigRow.reduced (4, 0), 22);

    inner.removeFromTop (14);
    guardHeader = inner.removeFromTop (22).reduced (8, 0);
    inner.removeFromTop (8);
    auto smallRow = inner.removeFromTop (124).reduced (40, 0);
    const int sw = smallRow.getWidth() / 3;
    place (monoBelow, smallRow.removeFromLeft (sw).reduced (10, 0), 18);
    place (monoSafety, smallRow.removeFromLeft (sw).reduced (10, 0), 18);
    place (transient, smallRow.reduced (10, 0), 18);
}
