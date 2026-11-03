#include "PluginHost.h"
#include "measurement/HardwareComparison.h"
#include <cmath>
namespace lab
{
namespace
{
class OfflinePlayHead : public juce::AudioPlayHead
{
  public:
    juce::int64 sample = 0;
    double rate = 48000;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setTimeInSamples(sample);
        p.setTimeInSeconds(sample / rate);
        p.setBpm(120);
        p.setPpqPosition(sample / rate * 2);
        p.setTimeSignature(TimeSignature{4, 4});
        p.setIsPlaying(true);
        return p;
    }
};
} // namespace
PluginTarget::PluginTarget(const TargetSpec &target, const RenderSettings &settings)
{
    settings.validate();
    formats.addFormat(new juce::VST3PluginFormat());
#if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.addFormat(new juce::AudioUnitPluginFormat());
#endif
    auto xml = juce::parseXML(target.descriptionXml);
    require(xml && description.loadFromXml(*xml), "Invalid plugin description");
    if (description.pluginFormatName == "VST3")
        description.fileOrIdentifier = target.plugin.getFullPathName();
    else
    {
#if JUCE_PLUGINHOST_AU && JUCE_MAC
        require(description.pluginFormatName == "AudioUnit", "Unsupported plugin format");
        // AU identity is the registered type/subtype/manufacturer, not the bundle path.
        require(description.fileOrIdentifier.startsWith("AudioUnit:"), "Invalid Audio Unit identifier");
#else
        throw std::runtime_error("Audio Units are supported on macOS only");
#endif
    }
    juce::String error;
    plugin = formats.createPluginInstance(description, settings.sampleRate, settings.blockSize, error);
    require(plugin != nullptr, "Plugin load failed: " + error);
    if (target.state.getSize() > 0)
        plugin->setStateInformation(target.state.getData(), static_cast<int>(target.state.getSize()));
}
juce::MemoryBlock PluginTarget::state()
{
    juce::MemoryBlock b;
    plugin->getStateInformation(b);
    return b;
}
MeasurementArtifact PluginTarget::renderOffline(const AudioData &input, const RenderSettings &s)
{
    s.validate();
    require(input.sampleRate == s.sampleRate,
            "Input WAV sample rate differs; resample explicitly before measurement");
    const auto originalLayout = plugin->getBusesLayout();
    auto layout = originalLayout;
    require(layout.inputBuses.size() > 0 && layout.outputBuses.size() > 0,
            "Plugin must have audio input/output");
    for (auto &bus : layout.inputBuses)
        bus = juce::AudioChannelSet::disabled();
    for (auto &bus : layout.outputBuses)
        bus = juce::AudioChannelSet::disabled();
    auto channels = s.channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.inputBuses.set(0, channels);
    layout.outputBuses.set(0, channels);
    bool configured = plugin->setBusesLayout(layout);
    if (!configured && description.pluginFormatName == "AudioUnit")
    {
        // Some AUs require their auxiliary buses to remain enabled. Feed those buses silence.
        layout = originalLayout;
        layout.inputBuses.set(0, channels);
        layout.outputBuses.set(0, channels);
        configured = plugin->setBusesLayout(layout);
    }
    require(configured, "Plugin does not support selected mono/stereo layout");
    plugin->setNonRealtime(true);
    plugin->setRateAndBufferSizeDetails(s.sampleRate, s.blockSize);
    OfflinePlayHead head;
    head.rate = s.sampleRate;
    plugin->setPlayHead(&head);
    plugin->prepareToPlay(s.sampleRate, s.blockSize);
    plugin->reset();
    struct Cleanup
    {
        juce::AudioPluginInstance &p;
        ~Cleanup()
        {
            p.releaseResources();
            p.setPlayHead(nullptr);
        }
    } cleanup{*plugin};
    int latency = plugin->getLatencySamples();
    require(latency >= 0 && latency <= s.sampleRate * 10, "Invalid/excessive plugin latency");
    int length = input.samples.getNumSamples() + static_cast<int>(std::ceil(s.tailSeconds * s.sampleRate));
    MeasurementArtifact result;
    result.audio.sampleRate = s.sampleRate;
    result.audio.samples.setSize(s.channels, length);
    result.audio.samples.clear();
    result.rawAudio.sampleRate = s.sampleRate;
    result.rawAudio.samples.setSize(s.channels, length + latency);
    result.rawAudio.samples.clear();
    result.reportedLatency = latency;
    result.reportedTail = plugin->getTailLengthSeconds();
    const int processChannels =
        std::max(plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels());
    require(processChannels >= s.channels && processChannels <= 64, "Unsupported plugin bus channel count");
    juce::AudioBuffer<float> block(processChannels, s.blockSize);
    juce::MidiBuffer midi;
    for (int offset = 0; offset < length + latency; offset += s.blockSize)
    {
        block.clear();
        midi.clear();
        head.sample = offset;
        for (int c = 0; c < s.channels; ++c)
            for (int i = 0; i < s.blockSize && offset + i < input.samples.getNumSamples(); ++i)
            {
                float value;
                if (s.channels == 1 && input.samples.getNumChannels() == 2)
                    value = 0.5f *
                            (input.samples.getSample(0, offset + i) + input.samples.getSample(1, offset + i));
                else
                    value =
                        input.samples.getSample(std::min(c, input.samples.getNumChannels() - 1), offset + i);
                block.setSample(c, i, value);
            }
        plugin->processBlock(block, midi);
        require(plugin->getLatencySamples() == latency,
                "Plugin latency changed during render; rerun with a stable configuration");
        for (int c = 0; c < s.channels; ++c)
            for (int i = 0; i < s.blockSize; ++i)
            {
                auto value = block.getSample(c, i);
                require(std::isfinite(value), "Plugin emitted NaN/Infinity");
                if (offset + i < length + latency)
                    result.rawAudio.samples.setSample(c, offset + i, value);
                int out = offset + i - latency;
                if (out >= 0 && out < length)
                    result.audio.samples.setSample(c, out, value);
            }
    }
    auto m = object();
    set(m, "pluginName", description.name);
    set(m, "pluginFormat", description.pluginFormatName);
    set(m, "auxiliaryInputPolicy", "disabled where supported; otherwise enabled and zero-filled");
    set(m, "totalInputChannels", plugin->getTotalNumInputChannels());
    set(m, "totalOutputChannels", plugin->getTotalNumOutputChannels());
    set(m, "pluginIdentifier", description.fileOrIdentifier);
    set(m, "pluginVersion", description.version);
    set(m, "pluginId", description.createIdentifierString());
    set(m, "reportedLatencySamples", latency);
    set(m, "reportedTailSeconds", std::isfinite(result.reportedTail) ? result.reportedTail : -1);
    set(m, "latencyCompensation",
        "removed reported leading latency; uncompensated capture in separate -raw.wav");
    set(m, "tailPolicy", "explicit user duration; may truncate longer/infinite tails");
    set(m, "channelMapping", "mono->stereo duplicate; stereo->mono arithmetic mean; otherwise identity");
    result.metadata = m;
    return result;
}
juce::var scanPlugin(const juce::String &fileOrIdentifier)
{
    juce::AudioPluginFormatManager formats;
    formats.addFormat(new juce::VST3PluginFormat());
#if JUCE_PLUGINHOST_AU && JUCE_MAC
    formats.addFormat(new juce::AudioUnitPluginFormat());
#endif
    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (auto *format : formats.getFormats())
        if (format->fileMightContainThisPluginType(fileOrIdentifier))
            format->findAllTypesForFile(descriptions, fileOrIdentifier);
    require(!descriptions.isEmpty(), "No supported plugin found (VST3 or installed macOS AU component)");
    juce::Array<juce::var> types;
    for (auto *d : descriptions)
    {
        auto v = object();
        set(v, "name", d->name);
        set(v, "format", d->pluginFormatName);
        set(v, "xml", d->createXml()->toString());
        types.add(v);
    }
    auto result = object();
    set(result, "types", types);
    return result;
}
juce::var runWorker(const juce::File &executable, const juce::File &job, int timeoutMs,
                    const std::atomic_bool *cancelled)
{
    require(!cancelled || !cancelled->load(), "Measurement cancelled");
    juce::ChildProcess child;
    require(
        child.start(juce::StringArray{executable.getFullPathName(), "--worker", job.getFullPathName()}, 0),
        "Cannot start plugin worker");
    auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    while (!child.waitForProcessToFinish(100))
    {
        if ((cancelled && cancelled->load()) || juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            child.kill();
            child.waitForProcessToFinish(1000);
            throw std::runtime_error("Plugin worker cancelled or timed out");
        }
    }
    require(child.getExitCode() == 0, "Plugin worker failed/crashed; saved sessions are unchanged");
    auto result = readJson(job.getSiblingFile(job.getFileNameWithoutExtension() + "-result.json"));
    require(result["error"].isVoid(), result["error"].toString());
    return result;
}
void executeWorkerJob(const juce::File &file)
{
    auto result = object();
    try
    {
        auto job = readJson(file);
        auto action = job["action"].toString();
        if (action == "scan")
            result = scanPlugin(job["plugin"].toString());
        else if (action == "prepare_darkbass")
        {
            auto prepared =
                prepareDarkbass(juce::File::getSpecialLocation(juce::File::currentExecutableFile),
                                juce::File(job["plugin"].toString()), static_cast<bool>(job["distortion"]));
            set(result, "target", prepared.target.toJson());
            set(result, "parameters", prepared.parameters);
        }
        else if (action == "configure")
        {
            auto settings = RenderSettings::fromJson(job["settings"]);
            PluginTarget target(TargetSpec::fromJson(job["target"]), settings);
            auto *overrides = job["parameters"].getDynamicObject();
            juce::StringArray applied;
            juce::Array<juce::var> parameters;
            for (auto *parameter : target.instance().getParameters())
            {
                auto *hosted = dynamic_cast<juce::HostedAudioProcessorParameter *>(parameter);
                require(hosted != nullptr, "Plugin parameter has no stable ID");
                auto id = hosted->getParameterID();
                if (overrides && overrides->hasProperty(juce::Identifier(id)))
                {
                    auto value = overrides->getProperty(juce::Identifier(id));
                    require(value.isDouble() || value.isInt(), "Parameter must be numeric");
                    double normalized = value;
                    require(std::isfinite(normalized) && normalized >= 0 && normalized <= 1,
                            juce::String::fromUTF8("Normalized parameter outside 0–1"));
                    parameter->setValueNotifyingHost(static_cast<float>(normalized));
                    applied.add(id);
                }
                auto row = object();
                set(row, "id", id);
                set(row, "name", parameter->getName(256));
                set(row, "normalizedValue", parameter->getValue());
                parameters.add(row);
            }
            if (overrides)
                require(applied.size() == overrides->getProperties().size(), "Unknown parameter ID");
            set(result, "parameters", parameters);
            set(result, "state", encodeState(target.state()));
        }
        else if (action == "render")
        {
            auto s = RenderSettings::fromJson(job["settings"]);
            auto t = TargetSpec::fromJson(job["target"]);
            PluginTarget target(t, s);
            set(result, "initialState", encodeState(target.state()));
            auto artifact = target.renderOffline(readWav(juce::File(job["input"].toString())), s);
            auto output = juce::File(job["output"].toString());
            writeWav(output, artifact.audio);
            writeWav(output.getSiblingFile(output.getFileNameWithoutExtension() + "-raw.wav"),
                     artifact.rawAudio);
            set(result, "metadata", artifact.metadata);
            set(result, "finalState", encodeState(target.state()));
        }
        else
            throw std::runtime_error("Unknown worker action");
    }
    catch (const std::exception &e)
    {
        set(result, "error", juce::String::fromUTF8(e.what()));
    }
    writeJson(file.getSiblingFile(file.getFileNameWithoutExtension() + "-result.json"), result);
}
} // namespace lab
