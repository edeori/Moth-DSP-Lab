#include "hardware/SetupTest.h"
#include <cmath>
#include <limits>
namespace
{
lab::AudioData returned(const lab::AudioData &probe, int delay = 137)
{
    lab::AudioData result;
    result.sampleRate = probe.sampleRate;
    result.samples.setSize(1, probe.samples.getNumSamples());
    result.samples.clear();
    result.samples.copyFrom(0, delay, probe.samples, 0, 0, probe.samples.getNumSamples() - delay);
    result.samples.applyGain(.8f);
    return result;
}
void check(bool condition, const char *message)
{
    lab::require(condition, message);
}
} // namespace
void runHardwareTests()
{
    using Runner = lab::SetupProbeRunner;
    using State = Runner::State;
    auto probe = lab::makeSetupProbe(48000);
    auto capture = returned(probe);
    check(probe.samples.getMagnitude(0, probe.samples.getNumSamples()) <=
              juce::Decibels::decibelsToGain(-36.f) + 1e-7f,
          "probe exceeds output cap");
    check(probe.samples.getMagnitude(0, 20000) == 0, "probe must begin with silence");
    auto good = lab::analyseSetupCapture(probe, capture);
    check(good.passed, "known stable loopback should pass");
    check(std::abs(good.latencySamples - 137) < .1, "round-trip delay");
    check(good.repeatDriftSamples == 0, "repeat latency stability");
    auto silence = capture;
    silence.samples.clear();
    check(!lab::analyseSetupCapture(probe, silence).passed, "disconnected path must fail");
    auto inverted = capture;
    inverted.samples.applyGain(-1);
    check(!lab::analyseSetupCapture(probe, inverted).passed, "inverted polarity must not pass");
    auto clipped = capture;
    clipped.samples.setSample(0, 10000, 1);
    check(!lab::analyseSetupCapture(probe, clipped).passed, "clipped capture must fail");
    auto drift = capture;
    int second = 72000;
    drift.samples.clear(0, second, 20000);
    drift.samples.copyFrom(0, second + 150, probe.samples, 0, second, 12000);
    drift.samples.applyGain(.8f);
    check(!lab::analyseSetupCapture(probe, drift).passed, "unstable repeated latency must fail");
    auto feedback = capture;
    for (int i = 105600; i < 120000; ++i)
        feedback.samples.setSample(0, i, .03f);
    check(!lab::analyseSetupCapture(probe, feedback).passed, "persistent signal after probe must fail");
    Runner runner;
    runner.start(48000, 0, 0);
    float input[256]{}, selected[256]{}, other[256]{};
    const float *ins[]{input};
    float *outs[]{selected, other};
    for (int offset = 0; offset < probe.samples.getNumSamples(); offset += 256)
    {
        for (int i = 0; i < 256; ++i)
            input[i] =
                offset + i < capture.samples.getNumSamples() ? capture.samples.getSample(0, offset + i) : 0;
        std::fill_n(other, 256, 1.f);
        runner.process(ins, 1, outs, 2, 256);
        for (int i = 0; i < 256; ++i)
        {
            check(other[i] == 0, "unselected output must remain silent");
            check(std::abs(selected[i]) <= .01585f, "real-time output cap");
        }
    }
    check(runner.state() == State::complete, "runner did not finish");
    check(lab::analyseSetupCapture(runner.stimulus(), runner.recording()).passed, "captured callback data");
    runner.start(48000, 0, 0);
    runner.cancel();
    std::fill_n(selected, 256, 1.f);
    runner.process(ins, 1, outs, 2, 256);
    check(runner.state() == State::cancelled && selected[0] == 0 && selected[255] == 0, "cancel must mute");
    runner.start(48000, 0, 0);
    std::fill_n(input, 256, 0.f);
    input[42] = .96f;
    runner.process(ins, 1, outs, 2, 256);
    check(runner.state() == State::overload && selected[0] == 0, "overload must abort before output");
    runner.start(48000, 0, 0);
    input[42] = std::numeric_limits<float>::quiet_NaN();
    runner.process(ins, 1, outs, 2, 256);
    check(runner.state() == State::invalidInput, "non-finite input must abort");
    runner.start(48000, 1, 0);
    runner.process(ins, 1, outs, 2, 256);
    check(runner.state() == State::missingChannel, "wrong input channel must abort");
    runner.start(48000, 0, 0);
    std::fill_n(input, 256, .05f);
    for (int i = 0; i < 100 && runner.state() == State::running; ++i)
        runner.process(ins, 1, outs, 2, 256);
    check(runner.state() == State::loudBaseline, "high baseline must prevent probe playback");
    // Fresh runs must not inherit cancellation, old captures or the previous analysis outcome.
    runner.start(48000, 0, 0);
    check(runner.state() == State::running && runner.capturedSamples() == 0, "restart state reset");
    runner.cancel();
}
