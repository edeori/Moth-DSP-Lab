#include "AnalysisEngine.h"
#include <cmath>
#include <complex>
namespace lab
{
namespace
{
double db(double x)
{
    return 20 * std::log10(std::max(1.0e-8, x));
}
double rms(const juce::AudioBuffer<float> &b)
{
    double energy = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto x = b.getSample(c, i);
            require(std::isfinite(x), "Non-finite audio in analysis");
            energy += x * x;
        }
    return std::sqrt(energy / (b.getNumChannels() * static_cast<double>(b.getNumSamples())));
}
std::vector<float> spectrum(const juce::AudioBuffer<float> &b)
{
    constexpr int order = 12, n = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> power(n / 2 + 1), data(2 * n);
    int frames = 0;
    double windowSum = 0;
    for (int i = 0; i < n; ++i)
        windowSum += 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * i / (n - 1));
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int start = 0; start < b.getNumSamples(); start += n / 2)
        {
            std::fill(data.begin(), data.end(), 0);
            for (int i = 0; i < n && start + i < b.getNumSamples(); ++i)
                data[i] = b.getSample(c, start + i) *
                          static_cast<float>(
                              0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * i / (n - 1)));
            fft.performFrequencyOnlyForwardTransform(data.data());
            for (int k = 0; k <= n / 2; ++k)
            {
                auto a = data[k] / windowSum * ((k == 0 || k == n / 2) ? 1 : 2);
                power[k] += static_cast<float>(a * a);
            }
            ++frames;
            if (start + n >= b.getNumSamples())
                break;
        }
    for (auto &x : power)
        x = static_cast<float>(db(std::sqrt(x / frames)));
    return power;
}
} // namespace
AnalysisResult AnalysisEngine::compare(const AudioData &a, const AudioData &b, int maxLag) const
{
    require(a.sampleRate == b.sampleRate, "Analysis sample rates differ");
    require(a.samples.getNumChannels() == b.samples.getNumChannels(), "Analysis channel counts differ");
    require(a.samples.getNumChannels() > 0 && a.samples.getNumSamples() > 0 && b.samples.getNumSamples() > 0,
            "Empty analysis input");
    require(maxLag >= 0 && maxLag <= 65536, "Invalid alignment search range");
    AnalysisResult r;
    r.referenceRmsDb = db(rms(a.samples));
    r.candidateRmsDb = db(rms(b.samples));
    r.gainDifferenceDb = r.candidateRmsDb - r.referenceRmsDb;
    for (int c = 0; c < a.samples.getNumChannels(); ++c)
    {
        r.referencePeak = std::max(
            r.referencePeak, static_cast<double>(a.samples.getMagnitude(c, 0, a.samples.getNumSamples())));
        r.candidatePeak = std::max(
            r.candidatePeak, static_cast<double>(b.samples.getMagnitude(c, 0, b.samples.getNumSamples())));
    }
    // FFT cross-correlation across all channels, first 65536 samples. Positive lag means B is late.
    int count = std::min({a.samples.getNumSamples(), b.samples.getNumSamples(), 65536});
    int order = 1;
    while ((1 << order) < count * 2)
        ++order;
    int n = 1 << order;
    juce::dsp::FFT fft(order);
    using Complex = std::complex<float>;
    std::vector<Complex> x(n), y(n), fx(n), fy(n), sum(n), corr(n);
    double ea = 0, eb = 0;
    for (int c = 0; c < a.samples.getNumChannels(); ++c)
    {
        std::fill(x.begin(), x.end(), Complex{});
        std::fill(y.begin(), y.end(), Complex{});
        for (int i = 0; i < count; ++i)
        {
            x[i] = a.samples.getSample(c, i);
            y[i] = b.samples.getSample(c, i);
            ea += std::norm(x[i]);
            eb += std::norm(y[i]);
        }
        fft.perform(x.data(), fx.data(), false);
        fft.perform(y.data(), fy.data(), false);
        for (int i = 0; i < n; ++i)
            sum[i] += std::conj(fx[i]) * fy[i];
    }
    fft.perform(sum.data(), corr.data(), true);
    int bound = std::min(maxLag, count / 2);
    double best = std::abs(corr[0].real());
    for (int lag = -bound; lag <= bound; ++lag)
    {
        double value = std::abs(corr[lag < 0 ? n + lag : lag].real());
        if (value > best + std::max(1.0e-9, best * 1.0e-6))
        {
            best = value;
            r.candidateDelaySamples = lag;
        }
    }
    r.correlation =
        ea * eb > 1.0e-20
            ? corr[r.candidateDelaySamples < 0 ? n + r.candidateDelaySamples : r.candidateDelaySamples]
                      .real() /
                  std::sqrt(ea * eb)
            : 0;
    r.alignmentReliable =
        std::abs(r.correlation) >= 0.5 && (bound == 0 || std::abs(r.candidateDelaySamples) < bound);
    if (!r.alignmentReliable)
        r.candidateDelaySamples = 0;
    int startA = std::max(0, -r.candidateDelaySamples), startB = std::max(0, r.candidateDelaySamples);
    int length = std::min(a.samples.getNumSamples() - startA, b.samples.getNumSamples() - startB);
    r.residual.sampleRate = a.sampleRate;
    r.residual.samples.setSize(a.samples.getNumChannels(), length);
    for (int c = 0; c < a.samples.getNumChannels(); ++c)
        for (int i = 0; i < length; ++i)
            r.residual.samples.setSample(
                c, i, b.samples.getSample(c, startB + i) - a.samples.getSample(c, startA + i));
    r.residualRmsDb = db(rms(r.residual.samples));
    r.referenceSpectrumDb = spectrum(a.samples);
    r.candidateSpectrumDb = spectrum(b.samples);
    for (size_t i = 0; i < r.referenceSpectrumDb.size(); ++i)
        r.frequencies.push_back(static_cast<float>(i * a.sampleRate / 4096));
    return r;
}
juce::var AnalysisResult::toJson() const
{
    auto v = object();
    set(v, "schemaVersion", 1);
    set(v, "referenceRmsDbFS", referenceRmsDb);
    set(v, "candidateRmsDbFS", candidateRmsDb);
    set(v, "gainDifferenceDb", gainDifferenceDb);
    set(v, "referencePeak", referencePeak);
    set(v, "candidatePeak", candidatePeak);
    set(v, "referenceClipped", referencePeak >= 1);
    set(v, "candidateClipped", candidatePeak >= 1);
    set(v, "candidateDelaySamples", candidateDelaySamples);
    set(v, "alignmentCorrelation", correlation);
    set(v, "alignmentReliable", alignmentReliable);
    set(v, "residualRmsDbFS", residualRmsDb);
    set(v, "gainNormalization", "none");
    set(v, "alignmentMethod",
        "bounded integer cross-correlation; first 65536 samples; abs correlation >= 0.5; overlap residual");
    set(v, "spectrumMethod",
        "4096-point Hann-windowed RMS-averaged magnitude; output spectrum, not transfer function");
    juce::Array<juce::var> bins;
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        auto row = object();
        set(row, "hz", frequencies[i]);
        set(row, "referenceDb", referenceSpectrumDb[i]);
        set(row, "candidateDb", candidateSpectrumDb[i]);
        set(row, "differenceDb", candidateSpectrumDb[i] - referenceSpectrumDb[i]);
        bins.add(row);
    }
    set(v, "spectrum", bins);
    set(v, "caveat",
        "Residual is not a standalone quality score for nonlinear systems. Levels include configured tail. "
        "No fractional-sample/drift correction.");
    return v;
}
} // namespace lab
