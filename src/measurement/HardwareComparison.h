#pragma once
#include "analysis/AnalysisEngine.h"
#include "core/SessionStore.h"
#include <atomic>
namespace lab
{
struct PreparedDarkbass
{
    TargetSpec target;
    juce::var parameters;
};
PreparedDarkbass prepareDarkbass(const juce::File &executable, const juce::File &bundle, bool distortion,
                                 const std::atomic_bool *cancelled = nullptr,
                                 const juce::var &parameterOverrides = {});
// Captures must come from the same current routing as the successful bypass check.
juce::File compareHardwareCapture(const juce::File &executable, const juce::File &project,
                                  const AudioData &probe, const AudioData &capture,
                                  const TargetSpec &candidate, const juce::var &captureMetadata,
                                  const juce::var &bypassCheck, const juce::File &bypassFolder,
                                  const std::atomic_bool *cancelled = nullptr);
} // namespace lab
