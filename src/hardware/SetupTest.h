#pragma once
#include "core/Types.h"
#include <atomic>
namespace lab
{
struct SetupTestResult
{
    bool passed = false;
    double noiseDb = -160, signalDb = -160, peakDb = -160, snrDb = 0;
    double latencySamples = 0, repeatDriftSamples = 0, correlation = 0;
    juce::StringArray issues;
    juce::var toJson() const;
};
AudioData makeSetupProbe(double sampleRate);
SetupTestResult analyseSetupCapture(const AudioData &probe, const AudioData &capture);
// No allocation, locks, file I/O or UI calls in process(). start/inspection require a detached callback.
class SetupProbeRunner
{
  public:
    enum class State
    {
        idle,
        running,
        complete,
        cancelled,
        overload,
        invalidInput,
        missingChannel,
        loudBaseline
    };
    void start(double rate, int inputChannel, int outputChannel);
    void cancel() noexcept;
    void process(const float *const *inputs, int numInputs, float *const *outputs, int numOutputs,
                 int frames) noexcept;
    State state() const noexcept
    {
        return status.load();
    }
    int capturedSamples() const noexcept
    {
        return captured.load();
    }
    const AudioData &stimulus() const
    {
        return probe;
    }
    const AudioData &recording() const
    {
        return capture;
    }

  private:
    AudioData probe, capture;
    int input = 0, output = 0, position = 0;
    double baselineEnergy = 0;
    std::atomic<State> status{State::idle};
    std::atomic<int> captured{0};
};
} // namespace lab
