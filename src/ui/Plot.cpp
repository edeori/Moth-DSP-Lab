#include "Plot.h"
#include "LabTheme.h"
namespace lab
{
void Plot::load(const juce::File &session)
{
    hardwareBaseline = bool(readJson(session.getChildFile("manifest.json"))["hardware"]["captureOnly"]);
    a = readWav(session.getChildFile("reference.wav"));
    b = readWav(session.getChildFile("candidate.wav"));
    residual = readWav(session.getChildFile("residual.wav"));
    summary = readJson(session.getChildFile("analysis_summary.json"));
    repaint();
}
void Plot::paint(juce::Graphics &g)
{
    g.fillAll(LabTheme::panel());
    if (a.samples.getNumSamples() == 0)
    {
        g.setColour(juce::Colours::lightgrey);
        g.drawText(juce::String::fromUTF8("Kezdés vagy visszatérés: Vezetett mérés / folytatás"),
                   getLocalBounds(), juce::Justification::centred);
        return;
    }
    auto area = getLocalBounds().reduced(16);
    auto wave = area.removeFromTop(area.getHeight() / 2).reduced(0, 20);
    auto spec = area.reduced(0, 26).withTrimmedLeft(32);
    g.setColour(juce::Colours::grey);
    g.drawRect(wave);
    g.drawRect(spec);
    g.setColour(juce::Colours::white);
    g.drawText(hardwareBaseline
                   ? (difference
                          ? juce::String::fromUTF8("Különbség: alapreferencia − felvétel")
                          : juce::String::fromUTF8("Türkiz: felvétel • narancs: rögzített alapreferencia"))
               : difference ? juce::String::fromUTF8("Különbségjel (plugin − referencia) / közös időszakasz")
                            : juce::String::fromUTF8("Hullámforma • türkiz: referencia • narancs: plugin"),
               wave.getX(), wave.getY() - 20, wave.getWidth(), 18, juce::Justification::left);
    g.drawText(hardwareBaseline
                   ? (difference
                          ? juce::String::fromUTF8("Különbség: alapreferencia − felvétel")
                          : juce::String::fromUTF8("Türkiz: felvétel • narancs: rögzített alapreferencia"))
               : difference ? juce::String::fromUTF8("Spektrumeltérés • mély → magas • −36 … +36 dB")
                            : juce::String::fromUTF8("Spektrum • mély → magas • −120 … +12 dBFS"),
               spec.getX(), spec.getY() - 20, spec.getWidth(), 18, juce::Justification::left);
    // Label the scale in the graph itself, so reading it needs no separate report.
    g.setFont(juce::FontOptions(11.f));
    for (float db :
         (difference ? std::initializer_list<float>{-36, 0, 36} : std::initializer_list<float>{-120, -60, 0}))
    {
        const float fraction = difference ? (db + 36) / 72 : (db + 120) / 132;
        const int y = spec.getBottom() - juce::roundToInt(fraction * spec.getHeight());
        g.setColour(juce::Colour(0xff334157));
        g.drawHorizontalLine(y, static_cast<float>(spec.getX()), static_cast<float>(spec.getRight()));
        g.setColour(LabTheme::muted());
        g.drawText(juce::String(db, 0), spec.getX() - 32, y - 7, 28, 14, juce::Justification::centredRight);
    }
    for (double hz : {20.0, 100.0, 1000.0, 10000.0})
    {
        if (hz >= a.sampleRate / 2)
            continue;
        const int x =
            spec.getX() + juce::roundToInt(std::log(hz / 20) / std::log(a.sampleRate / 40) * spec.getWidth());
        g.setColour(LabTheme::muted());
        g.drawText(hz >= 1000 ? juce::String(hz / 1000, 0) + "k Hz" : juce::String(hz, 0) + " Hz",
                   juce::jlimit(spec.getX(), spec.getRight() - 48, x - 24), spec.getBottom() + 3, 48, 14,
                   juce::Justification::centred);
    }
    g.drawText("0 s", wave.getX(), wave.getBottom() + 3, 45, 14, juce::Justification::left);
    g.drawText(juce::String(a.samples.getNumSamples() / a.sampleRate, 2) + " s", wave.getRight() - 65,
               wave.getBottom() + 3, 65, 14, juce::Justification::right);
    float peak = difference
                     ? std::max(0.000001f, residual.samples.getMagnitude(0, residual.samples.getNumSamples()))
                     : std::max({0.000001f, a.samples.getMagnitude(0, a.samples.getNumSamples()),
                                 b.samples.getMagnitude(0, b.samples.getNumSamples())});
    for (int side = 0; side < (difference ? 1 : 2); ++side)
    {
        auto &data = difference ? residual : (side == 0 ? a : b);
        g.setColour(side == 0 ? juce::Colour(0xff4fdfc0) : juce::Colour(0xffffae65));
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            int start = x * data.samples.getNumSamples() / wave.getWidth();
            int end = std::min(data.samples.getNumSamples(),
                               std::max(start + 1, (x + 1) * data.samples.getNumSamples() / wave.getWidth()));
            auto range = data.samples.findMinMax(0, start, end - start);
            auto y = [&](float v) { return wave.getCentreY() - v / peak * wave.getHeight() * 0.48f; };
            g.drawVerticalLine(wave.getX() + x, y(range.getEnd()), y(range.getStart()));
        }
        juce::Path p;
        bool first = true;
        if (auto *bins = summary["spectrum"].getArray())
            for (auto &bin : *bins)
            {
                double hz = bin["hz"];
                if (hz < 20)
                    continue;
                float x =
                    spec.getX() +
                    static_cast<float>(std::log(hz / 20) / std::log(data.sampleRate / 40)) * spec.getWidth();
                float value = static_cast<float>(static_cast<double>(
                    bin[difference ? "differenceDb" : (side == 0 ? "referenceDb" : "candidateDb")]));
                float y = difference ? spec.getBottom() -
                                           (juce::jlimit(-36.f, 36.f, value) + 36) / 72 * spec.getHeight()
                                     : spec.getBottom() -
                                           (juce::jlimit(-120.f, 12.f, value) + 120) / 132 * spec.getHeight();
                if (first)
                {
                    p.startNewSubPath(x, y);
                    first = false;
                }
                else
                    p.lineTo(x, y);
            }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
}
} // namespace lab
