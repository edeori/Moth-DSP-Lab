#pragma once
#include "core/Types.h"
#include <atomic>
namespace lab
{
class PluginTarget final : public AudioTarget
{
  public:
    PluginTarget(const TargetSpec &, const RenderSettings &);
    MeasurementArtifact renderOffline(const AudioData &, const RenderSettings &) override;
    juce::AudioPluginInstance &instance()
    {
        return *plugin;
    }
    juce::MemoryBlock state();

  private:
    juce::AudioPluginFormatManager formats;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
};
juce::var scanPlugin(const juce::String &fileOrIdentifier);
// All plugin operations are performed in an isolated invocation of this executable.
juce::var runWorker(const juce::File &executable, const juce::File &jobFile, int timeoutMs = 180000,
                    const std::atomic_bool *cancelled = nullptr);
void executeWorkerJob(const juce::File &jobFile);
} // namespace lab
