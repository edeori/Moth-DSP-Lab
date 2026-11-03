#include "SetupTest.h"
#include "analysis/AnalysisEngine.h"
#include <cmath>
namespace lab
{
namespace
{
double db(double value)
{
    return 20 * std::log10(std::max(1e-8, value));
}
double rms(const AudioData &data, int start, int count)
{
    return data.samples.getRMSLevel(0, start, count);
}
} // namespace
AudioData makeSetupProbe(double rate)
{
    require(std::isfinite(rate) && rate >= 8000 && rate <= 192000, "Invalid probe sample rate");
    AudioData result;
    result.sampleRate = rate;
    result.samples.setSize(1, static_cast<int>(rate * 2.6));
    result.samples.clear();
    const int length = static_cast<int>(rate * .25);
    juce::Random random(0x42b7);
    float filter = 0;
    std::vector<float> pulse(static_cast<size_t>(length));
    float peak = 0;
    for (int i = 0; i < length; ++i)
    {
        filter += .22f * ((random.nextFloat() * 2 - 1) - filter);
        const auto fade =
            static_cast<float>(std::min({1., i / (rate * .01), (length - 1 - i) / (rate * .01)}));
        pulse[static_cast<size_t>(i)] = filter * fade;
        peak = std::max(peak, std::abs(pulse[static_cast<size_t>(i)]));
    }
    const float scale = juce::Decibels::decibelsToGain(-36.f) / peak;
    for (double seconds : {.5, 1.5})
        for (int i = 0; i < length; ++i)
            result.samples.setSample(0, static_cast<int>(rate * seconds) + i,
                                     pulse[static_cast<size_t>(i)] * scale);
    return result;
}
SetupTestResult analyseSetupCapture(const AudioData &probe, const AudioData &capture)
{
    require(probe.sampleRate == capture.sampleRate && capture.samples.getNumChannels() == 1 &&
                probe.samples.getNumChannels() == 1 &&
                probe.samples.getNumSamples() == capture.samples.getNumSamples(),
            "Invalid setup capture");
    const auto rate = probe.sampleRate;
    require(capture.samples.getNumSamples() >= rate * 2.5, "Incomplete setup capture");
    SetupTestResult result;
    for (int i = 0; i < capture.samples.getNumSamples(); ++i)
        require(std::isfinite(capture.samples.getSample(0, i)), "Non-finite setup capture");
    result.noiseDb = db(rms(capture, 0, static_cast<int>(rate * .4)));
    result.peakDb = db(capture.samples.getMagnitude(0, capture.samples.getNumSamples()));
    std::array<int, 2> delays{};
    double minCorrelation = 1, signalEnergy = 0;
    bool reliable = true;
    for (int pulse = 0; pulse < 2; ++pulse)
    {
        const int start = static_cast<int>(rate * (.4 + pulse));
        const int length = static_cast<int>(rate * .65);
        AudioData reference, recorded;
        reference.sampleRate = recorded.sampleRate = rate;
        reference.samples.setSize(1, length);
        recorded.samples.setSize(1, length);
        reference.samples.copyFrom(0, 0, probe.samples, 0, start, length);
        recorded.samples.copyFrom(0, 0, capture.samples, 0, start, length);
        auto alignment = AnalysisEngine{}.compare(reference, recorded, static_cast<int>(rate * .2));
        delays[static_cast<size_t>(pulse)] = alignment.candidateDelaySamples;
        minCorrelation = std::min(minCorrelation, alignment.correlation);
        reliable &= alignment.alignmentReliable && alignment.correlation >= .7 &&
                    alignment.candidateDelaySamples >= 0;
        const int signalStart =
            static_cast<int>(rate * (.5 + pulse)) + std::max(0, alignment.candidateDelaySamples);
        const double level = rms(capture, signalStart, static_cast<int>(rate * .25));
        signalEnergy += level * level;
    }
    result.signalDb = db(std::sqrt(signalEnergy / 2));
    result.snrDb = result.signalDb - result.noiseDb;
    result.latencySamples = (delays[0] + delays[1]) * .5;
    result.repeatDriftSamples = std::abs(delays[1] - delays[0]);
    result.correlation = minCorrelation;
    if (!reliable)
        result.issues.add(juce::String::fromUTF8(
            "A tesztjel nem azonosítható megbízhatóan. Ellenőrizd a csatornákat, a bypass "
            "állást és a kábeleket."));
    if (result.signalDb < -65)
        result.issues.add(juce::String::fromUTF8(
            "Túl gyenge vagy hiányzó visszatérő jel. Ellenőrizd a bekötést; csak megállított "
            "tesztnél állíts a szinteken."));
    if (result.peakDb >= -1)
        result.issues.add(
            juce::String::fromUTF8("Túl magas bemeneti csúcs. Csökkentsd a hardveres jelszinteket."));
    if (result.noiseDb > -45 || result.snrDb < 20)
        result.issues.add(juce::String::fromUTF8(
            "Magas zaj vagy kevés jel/zaj tartalék. Ellenőrizd a földhurkot és a jelszinteket."));
    if (result.repeatDriftSamples > 2)
        result.issues.add(juce::String::fromUTF8(
            "A két próba késleltetése eltér. Ellenőrizd az eszközt és az audio-beállításokat."));
    const double tailDb = db(rms(capture, static_cast<int>(rate * 2.2), static_cast<int>(rate * .3)));
    if (tailDb > std::max(-60., result.noiseDb + 10))
        result.issues.add(juce::String::fromUTF8(
            "A tesztjel után is jel maradt. Kapcsold ki a Direct Monitort és minden szoftveres "
            "bemeneti monitorozást."));
    result.passed = result.issues.isEmpty();
    return result;
}
juce::var SetupTestResult::toJson() const
{
    auto v = object();
    set(v, "schemaVersion", 1);
    set(v, "passed", passed);
    set(v, "noiseDbFS", noiseDb);
    set(v, "signalDbFS", signalDb);
    set(v, "peakDbFS", peakDb);
    set(v, "snrDb", snrDb);
    set(v, "latencySamples", latencySamples);
    set(v, "repeatDriftSamples", repeatDriftSamples);
    set(v, "minimumCorrelation", correlation);
    juce::Array<juce::var> messages;
    for (const auto &issue : issues)
        messages.add(issue);
    set(v, "issues", messages);
    set(v, "scope",
        "Bypassed signal-path check only; not proof of physical wiring, electrical safety or absolute level "
        "calibration.");
    return v;
}
void SetupProbeRunner::start(double rate, int inputChannel, int outputChannel)
{
    require(inputChannel >= 0 && outputChannel >= 0, "Select input and output channels");
    probe = makeSetupProbe(rate);
    capture.sampleRate = rate;
    capture.samples.setSize(1, probe.samples.getNumSamples());
    capture.samples.clear();
    input = inputChannel;
    output = outputChannel;
    position = 0;
    baselineEnergy = 0;
    captured = 0;
    status = State::running;
}
void SetupProbeRunner::cancel() noexcept
{
    auto expected = State::running;
    status.compare_exchange_strong(expected, State::cancelled);
}
void SetupProbeRunner::process(const float *const *inputs, int numInputs, float *const *outputs,
                               int numOutputs, int frames) noexcept
{
    for (int c = 0; c < numOutputs; ++c)
        if (outputs[c])
            juce::FloatVectorOperations::clear(outputs[c], frames);
    if (status.load() != State::running)
        return;
    if (input >= numInputs || output >= numOutputs || !inputs[input] || !outputs[output])
    {
        status = State::missingChannel;
        return;
    }
    // Inspect the whole incoming block before emitting any signal for this callback.
    for (int i = 0; i < frames; ++i)
    {
        if (!std::isfinite(inputs[input][i]))
        {
            status = State::invalidInput;
            return;
        }
        if (std::abs(inputs[input][i]) >= .95f)
        {
            status = State::overload;
            return;
        }
    }
    for (int i = 0; i < frames && position < probe.samples.getNumSamples(); ++i, ++position)
    {
        if (status.load() != State::running)
            break;
        const auto sample = inputs[input][i];
        capture.samples.setSample(0, position, sample);
        if (position < static_cast<int>(probe.sampleRate * .4))
        {
            baselineEnergy += sample * sample;
            if (position == static_cast<int>(probe.sampleRate * .4) - 1 &&
                baselineEnergy / (position + 1) > .001)
            {
                status = State::loudBaseline;
                break;
            }
        }
        outputs[output][i] = probe.samples.getSample(0, position);
    }
    captured = position;
    if (position >= probe.samples.getNumSamples())
    {
        auto expected = State::running;
        status.compare_exchange_strong(expected, State::complete);
    }
}
} // namespace lab
