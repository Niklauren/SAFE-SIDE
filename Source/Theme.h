#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// BOBA palette: roasted brown ground, milk-tea text, caramel accent, taro second colour.
namespace boba::ui
{
namespace col
{
    const juce::Colour bg       { 0xff140f0d };
    const juce::Colour panel    { 0xff1f1815 };
    const juce::Colour raised   { 0xff2a211d };
    const juce::Colour raised2  { 0xff332822 };
    const juce::Colour line     { 0xff3d302a };
    const juce::Colour line2    { 0xff57453c };
    const juce::Colour text     { 0xfff6ece2 };   // milk
    const juce::Colour text2    { 0xffd9c8b8 };
    const juce::Colour dim      { 0xffa89688 };
    const juce::Colour faint    { 0xff7d6c61 };
    const juce::Colour caramel  { 0xffecb47c };   // milk tea / caramel accent
    const juce::Colour caramel2 { 0xfff6cfa3 };
    const juce::Colour taro     { 0xffb9a2f5 };   // taro
    const juce::Colour pearl    { 0xff0b0807 };   // tapioca
    const juce::Colour good     { 0xff8fdcaa };
    const juce::Colour warn     { 0xfff0a35e };
    const juce::Colour bad      { 0xffe5645c };
}

// Figtree (SIL OFL), embedded
inline juce::Typeface::Ptr typeface (int weight)
{
    static juce::Typeface::Ptr regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::FigtreeRegular_ttf,   BinaryData::FigtreeRegular_ttfSize);
    static juce::Typeface::Ptr semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::FigtreeSemiBold_ttf,  BinaryData::FigtreeSemiBold_ttfSize);
    static juce::Typeface::Ptr bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::FigtreeBold_ttf,      BinaryData::FigtreeBold_ttfSize);
    static juce::Typeface::Ptr xbold    = juce::Typeface::createSystemTypefaceFor (BinaryData::FigtreeExtraBold_ttf, BinaryData::FigtreeExtraBold_ttfSize);
    if (weight >= 800) return xbold;
    if (weight >= 700) return bold;
    if (weight >= 600) return semibold;
    return regular;
}

inline juce::Font font (float size, int weight = 400)
{
    return juce::Font (juce::FontOptions (typeface (weight)).withHeight (size));
}

// System font for Cyrillic tooltip text
inline juce::Font systemFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size,
                         int weight, juce::Colour c, juce::Justification j, float tracking = 0.12f)
{
    juce::AttributedString s;
    s.append (text, font (size, weight).withExtraKerningFactor (tracking), c);
    s.setJustification (j);
    s.setWordWrap (juce::AttributedString::none);
    s.draw (g, r);
}

// The BOBA mark: a cup outline with three tapioca pearls, drawn as vector.
inline void drawBobaMark (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    auto b = juce::Rectangle<float> (s * 0.78f, s).withCentre (r.getCentre());
    juce::Path cup;
    const float top = b.getY() + s * 0.12f, bottom = b.getBottom();
    cup.startNewSubPath (b.getX(), top);
    cup.lineTo (b.getRight(), top);
    cup.lineTo (b.getRight() - s * 0.1f, bottom);
    cup.lineTo (b.getX() + s * 0.1f, bottom);
    cup.closeSubPath();
    g.setColour (col::caramel);
    g.fillPath (cup);
    // lid line and straw
    g.setColour (col::bg);
    g.fillRect (b.getX() - 1.0f, top + s * 0.14f, b.getWidth() + 2.0f, s * 0.05f);
    g.setColour (col::caramel2);
    g.fillRoundedRectangle (b.getCentreX() + s * 0.04f, b.getY() - s * 0.08f, s * 0.08f, s * 0.36f, s * 0.03f);
    // pearls
    g.setColour (col::pearl);
    const float pr = s * 0.085f;
    const float py = bottom - s * 0.2f;
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse (b.getCentreX() + 0.18f * (float) i * s - pr, py - pr - (i == 0 ? s * 0.07f : 0.0f), pr * 2.0f, pr * 2.0f);
}
} // namespace boba::ui
