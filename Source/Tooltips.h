#pragma once

#include <juce_core/juce_core.h>

// Tooltip texts (Russian). First line = title (bold), the rest = explanation.
// The source file is UTF-8 (MSVC builds with /utf-8).
namespace boba::tips
{
inline juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }

struct CharacterInfo
{
    const char* name;
    const char* tag;          // short tag under the name
    const char* line;         // one line shown under the character row (English UI)
    const char* colorName;    // what the COLOR knob means for this character
    const char* tipRu;        // tooltip
    const char* colorTipRu;   // tooltip of the COLOR knob
};

inline const CharacterInfo& character (int i)
{
    static const CharacterInfo info[] = {
        { "AIR", "diffuse",
          "Diffuse air: velvet-noise decorrelation, attacks stay centred.",
          "SIZE",
          "AIR — воздух\nМягкая широкая диффузия, как у дорогих ревёрбов, но без хвоста. Атаки остаются в центре. "
          "Подходит почти ко всему: пэды, вокал, мелодии, мастер.",
          "SIZE — размер\nНасколько крупное «облако» ширины: влево — плотнее и ближе, вправо — шире и мягче (15–60 мс)." },
        { "PRISM", "spectral",
          "Spectral spread: every frequency band gets its own place in the stereo field.",
          "MOTION",
          "PRISM — призма\nРаскладывает звук по частотам: соседние полосы уходят в разные стороны. Звук становится объёмным, "
          "но не размытым. Хорош на синтах, плаках, аккордах.",
          "MOTION — движение\n0 — раскладка стоит на месте. Вправо — полосы медленно плывут по панораме." },
        { "TWIN", "double",
          "Double tracking: two drifting tape voices around the centre.",
          "DRIFT",
          "TWIN — дабл\nКак будто партию записали ещё два раза: два голоса чуть плавают по времени, как плёнка. "
          "Для вокала, гитар, лидов.",
          "DRIFT — плавание\nНасколько сильно «плывут» дублирующие голоса. Влево — плотный дабл, вправо — живой хорус." },
        { "SHINE", "harmonic",
          "Harmonic air: new top-end harmonics, even ones left, odd ones right.",
          "TONE",
          "SHINE — блеск\nДобавляет новые верхние гармоники: чётные слева, нечётные справа. Верх звучит дороже и шире, "
          "середина не трогается.",
          "TONE — с какой частоты\nОткуда начинается блеск: влево — с 2 кГц (ярче), вправо — с 6 кГц (только воздух)." },
        { "PHANTOM", "808 width",
          "808 width: phase-locked bass harmonics in stereo, the sub itself stays mono.",
          "RANGE",
          "PHANTOM — ширина 808\nСам саб остаётся в моно, а его гармоники (2-я по центру, нечётные слева, чётные справа) "
          "делают 808 и бас широкими. Гармоники привязаны по фазе к тем, что уже есть, поэтому не гасят их.",
          "RANGE — диапазон баса\nДо какой частоты искать основной тон: влево — только суб (70 Гц), вправо — до 200 Гц." },
        { "SPACE", "depth",
          "Early reflections: depth and room without a reverb wash.",
          "ROOM",
          "SPACE — пространство\nРанние отражения комнаты, разные слева и справа: глубина и объём без ревёрберного хвоста.",
          "ROOM — комната\nРазмер комнаты: влево — маленькая и плотная, вправо — большая." },
    };
    return info[juce::jlimit (0, 5, i)];
}

inline juce::String amount()     { return ru ("AMOUNT — сколько\nСколько ширины добавляет выбранный характер. Работает и на моно-звуке."); }
inline juce::String width()      { return ru ("WIDTH — исходная ширина\nУсиливает стерео, которое уже есть в треке. 100 % — как было, 0 % — моно, 200 % — вдвое шире. "
                                              "Mono Guard не даст перестараться."); }
inline juce::String keep()       { return ru ("MONO KEEP — подмешать в моно\nЧасть ширины подмешивается и в середину, поэтому трек становится плотнее и в моно. "
                                              "Подмешивается только то, что не может погасить звук: шумовые хвосты через декорреляцию и гармоники, "
                                              "привязанные по фазе. В каждой полосе и в каждый момент добавка проверяется и не может вычесть сигнал.\n"
                                              "0 % — моно на выходе в точности равно моно на входе."); }
inline juce::String monoBelow()  { return ru ("MONO BELOW — моно ниже\nВсё ниже этой частоты остаётся в моно: саб, 808, бочка. Обычно 100–150 Гц."); }
inline juce::String monoSafety() { return ru ("MONO SAFETY — запас моно\nНасколько стерео может быть громче моно в каждой полосе. Меньше — безопаснее для клубов и телефонов. "
                                              "Mono Guard сам держит этот предел."); }
inline juce::String transient()  { return ru ("TRANSIENT — атаки\nСколько атак оставить сухими и в центре, чтобы барабаны не теряли удар."); }
inline juce::String monoCheck()  { return ru ("MONO CHECK\nПослушать трек в моно, как на телефоне или в клубе. При MONO KEEP = 0 это ровно твой исходный звук."); }
inline juce::String sideSolo()   { return ru ("SIDE SOLO\nПослушать только боковой канал (то, что звучит по бокам)."); }
inline juce::String meters()     { return ru ("Проверка моно\nКорреляция L/R и насколько стерео громче моно в каждой полосе. Белая черта — предел MONO SAFETY, "
                                              "оранжевый цвет — Mono Guard придерживает ширину. Внизу — сравнение моно на выходе с моно на входе."); }
inline juce::String logo()       { return ru ("BOBA · SAFE-SIDE\nСтереорасширитель, который не крадёт моно."); }
} // namespace boba::tips
