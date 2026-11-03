#pragma once
#include "analysis/AnalysisEngine.h"
#include "core/SessionStore.h"
#include <atomic>
namespace lab
{
class MeasurementEngine
{
  public:
    explicit MeasurementEngine(juce::File executable) : worker(std::move(executable)) {}
    juce::File compare(const juce::File &project, const juce::File &input, const TargetSpec &a,
                       const TargetSpec &b, const RenderSettings &,
                       const std::atomic_bool *cancelled = nullptr);

  private:
    juce::File worker;
};
} // namespace lab
