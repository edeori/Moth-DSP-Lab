#include "HardwareComparison.h"
#include "MeasurementReport.h"
#include "audio-host/PluginHost.h"
#include <cmath>
#include <map>
namespace lab
{
PreparedDarkbass prepareDarkbass(const juce::File &executable, const juce::File &bundle, bool distortion,
                                 const std::atomic_bool *cancelled, const juce::var &parameterOverrides)
{
    require(bundle.exists(), "DarkBass.component not found");
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("lab-darkbass-" + juce::Uuid().toString());
    require(dir.createDirectory().wasOk(), "Cannot create plugin preparation folder");
    struct Cleanup
    {
        juce::File dir;
        ~Cleanup()
        {
            dir.deleteRecursively();
        }
    } cleanup{dir};
    auto job = object();
    set(job, "action", "scan");
    set(job, "plugin", bundle.getFullPathName());
    auto file = dir.getChildFile("scan.json");
    writeJson(file, job);
    auto scan = runWorker(executable, file, 30000, cancelled);
    auto *types = scan["types"].getArray();
    require(types != nullptr, "Invalid scan result");
    PreparedDarkbass prepared;
    prepared.target.plugin = bundle;
    for (const auto &type : *types)
        if (type["name"].toString().equalsIgnoreCase("DarkBass"))
            prepared.target.descriptionXml = type["xml"].toString();
    require(prepared.target.descriptionXml.isNotEmpty(), "Selected bundle does not contain DarkBass");
    RenderSettings settings;
    settings.blockSize = 256;
    settings.tailSeconds = 0;
    job = object();
    set(job, "action", "configure");
    set(job, "target", prepared.target.toJson());
    set(job, "settings", settings.toJson());
    file = dir.getChildFile("configure.json");
    writeJson(file, job);
    auto inspected = runWorker(executable, file, 30000, cancelled);
    auto *parameters = inspected["parameters"].getArray();
    require(parameters != nullptr, "Plugin parameters unavailable");
    std::map<juce::String, double> desired{{"Bypass", 0},
                                           {"Distortion", distortion ? 1. : 0.},
                                           {"Bass", .5},
                                           {"Low Mid", .5},
                                           {"High Mid", .5},
                                           {"Treble", .5},
                                           {"Blend", 1},
                                           {"Drive", .25},
                                           {"Level", .5},
                                           {"Character", .4},
                                           {"Low Mid Frequency", .5},
                                           {"High Mid Frequency", .5}};
    if (auto *params = parameterOverrides.getArray())
        for (const auto &p : *params)
        {
            auto name = p["name"].toString();
            double value = p["value"];
            require(desired.count(name) && name != "Bypass" && name != "Distortion" && std::isfinite(value) &&
                        value >= 0 && value <= 1,
                    "Invalid scenario parameter");
            desired[name] = value;
        }
    auto overrides = object();
    int matched = 0;
    for (const auto &p : *parameters)
        if (auto it = desired.find(p["name"].toString()); it != desired.end())
        {
            set(overrides, juce::Identifier(p["id"].toString()), it->second);
            ++matched;
        }
    require(matched == static_cast<int>(desired.size()),
            "DarkBass parameter schema changed; review this measurement profile before use");
    set(job, "parameters", overrides);
    writeJson(file, job);
    auto configured = runWorker(executable, file, 30000, cancelled);
    require(decodeState(prepared.target.state, configured["state"].toString()) &&
                prepared.target.state.getSize() > 0,
            "Plugin did not save its state");
    prepared.parameters = configured["parameters"];
    return prepared;
}
juce::File compareHardwareCapture(const juce::File &executable, const juce::File &project,
                                  const AudioData &probe, const AudioData &capture,
                                  const TargetSpec &candidate, const juce::var &captureMetadata,
                                  const juce::var &bypassCheck, const juce::File &bypassFolder,
                                  const std::atomic_bool *cancelled)
{
    require(bypassCheck["status"].toString() == "passed" &&
                static_cast<bool>(bypassCheck["analysis"]["passed"]),
            "Successful fresh bypass check required");
    require(probe.sampleRate == capture.sampleRate &&
                probe.sampleRate == static_cast<double>(bypassCheck["sampleRate"]),
            "Capture and calibration rates differ");
    for (const auto *key : {"device", "driver", "inputChannel", "outputChannel"})
        require(captureMetadata[key] == bypassCheck[key], "Routing changed since bypass check");
    require(capture.samples.getNumChannels() == 1 && probe.samples.getNumChannels() == 1 &&
                capture.samples.getNumSamples() == probe.samples.getNumSamples(),
            "Invalid hardware capture");
    require(static_cast<int>(captureMetadata["capturedSamples"]) == capture.samples.getNumSamples() &&
                !static_cast<bool>(captureMetadata["deviceError"]),
            "Incomplete or failed hardware capture");
    require(static_cast<bool>(captureMetadata["interfaceAndReampUnchangedConfirmed"]) &&
                static_cast<bool>(captureMetadata["requestedPedalStateConfirmed"]) &&
                captureMetadata["operatorPhysicalSettings"].toString().trim().isNotEmpty(),
            "Physical settings confirmation required");
    const double measuredDelay = bypassCheck["analysis"]["latencySamples"];
    require(std::isfinite(measuredDelay) && measuredDelay >= 0 && measuredDelay <= probe.sampleRate * .2,
            "Invalid bypass latency");
    const int delay = static_cast<int>(std::lround(measuredDelay));
    const int length = capture.samples.getNumSamples() - delay;
    require(length > 0, "Capture too short");
    SessionStore store(project);
    auto folder = store.beginSession(captureMetadata["workflowId"].toString());
    auto manifest = object();
    set(manifest, "schemaVersion", 1);
    set(manifest, "createdUtc", utcNow());
    set(manifest, "processingVersion", version);
    set(manifest, "status", "running");
    auto ref = object();
    set(ref, "kind", "hardware");
    set(ref, "name",
        bool(captureMetadata["captureOnly"]) ? captureMetadata["deviceSetup"]["type"].toString()
                                             : "B7K Ultra");
    set(manifest, "reference", ref);
    set(manifest, "candidate", candidate.toJson());
    set(manifest, "hardware", captureMetadata);
    set(manifest, "bypassCheck", bypassCheck);
    set(manifest, "bypassCheckFolder", bypassFolder.getFullPathName());
    set(manifest, "absoluteLevelCalibration",
        "not performed; exploratory comparison, not a model quality score");
    set(manifest, "hardwareLatencyRemovedSamples", delay);
    set(manifest, "candidateChannelMapping", "left channel of stereo plugin render");
    RenderSettings settings;
    settings.sampleRate = probe.sampleRate;
    settings.blockSize = static_cast<int>(captureMetadata["blockSize"]);
    settings.channels = bool(captureMetadata["captureOnly"]) ? 1 : 2;
    settings.tailSeconds = 0;
    settings.validate();
    set(manifest, "settings", settings.toJson());
    try
    {
        writeJson(folder.getChildFile("manifest.json"), manifest);
        writeWav(folder.getChildFile("stimulus.wav"), probe);
        writeWav(folder.getChildFile("reference-raw.wav"), capture);
        juce::FileInputStream stream(folder.getChildFile("stimulus.wav"));
        set(manifest, "stimulusSha256", juce::SHA256(stream).toHexString());
        if (bool(captureMetadata["captureOnly"]))
        {
            // Compare to the recorded low-gain baseline, never pretend it is a plugin render.
            auto baseline = readWav(bypassFolder.getChildFile("capture.wav"));
            require(baseline.sampleRate == capture.sampleRate &&
                        baseline.samples.getNumSamples() == capture.samples.getNumSamples(),
                    "Baseline shape changed");
            AudioData reference, original;
            reference.sampleRate = original.sampleRate = capture.sampleRate;
            reference.samples.setSize(1, length);
            original.samples.setSize(1, length);
            reference.samples.copyFrom(0, 0, capture.samples, 0, delay, length);
            original.samples.copyFrom(0, 0, baseline.samples, 0, delay, length);
            auto result = AnalysisEngine{}.compare(reference, original, 0);
            auto summary = result.toJson();
            set(summary, "comparisonKind", "hardware_vs_recorded_baseline");
            set(summary, "absoluteLevelCalibration", false);
            set(summary, "qualityScore", juce::var());
            set(summary, "alignmentMethod",
                "Both captures trimmed by the same fresh low-gain baseline delay; active-path delay changes "
                "are not corrected");
            writeWav(folder.getChildFile("reference.wav"), reference);
            writeWav(folder.getChildFile("candidate.wav"), original);
            writeWav(folder.getChildFile("residual.wav"), result.residual);
            writeJson(folder.getChildFile("analysis_summary.json"), summary);
            auto baselineIdentity = object();
            set(baselineIdentity, "kind", "recorded_hardware_baseline");
            set(manifest, "candidate", baselineIdentity);
            set(manifest, "candidateChannelMapping", "mono recorded baseline, not a plugin");
            set(manifest, "status", "complete");
            writeJson(folder.getChildFile("manifest.json"), manifest);
            writeHardwareReport(folder);
            return store.commit(folder);
        }
        auto job = object();
        set(job, "action", "render");
        set(job, "settings", settings.toJson());
        set(job, "target", candidate.toJson());
        set(job, "input", folder.getChildFile("stimulus.wav").getFullPathName());
        set(job, "output", folder.getChildFile("candidate-stereo.wav").getFullPathName());
        auto jobFile = folder.getChildFile("candidate-job.json");
        writeJson(jobFile, job);
        auto rendered = runWorker(executable, jobFile, 180000, cancelled);
        set(manifest, "candidateRender", rendered);
        auto target = candidate.toJson();
        set(target, "state", rendered["initialState"]);
        set(manifest, "candidate", target);
        auto full = readWav(folder.getChildFile("candidate-stereo.wav"));
        require(full.samples.getNumSamples() >= length, "Plugin render shorter than comparison window");
        AudioData reference, plugin;
        reference.sampleRate = plugin.sampleRate = probe.sampleRate;
        reference.samples.setSize(1, length);
        plugin.samples.setSize(1, length);
        reference.samples.copyFrom(0, 0, capture.samples, 0, delay, length);
        plugin.samples.copyFrom(0, 0, full.samples, 0, 0, length);
        auto result = AnalysisEngine{}.compare(reference, plugin, 0);
        auto summary = result.toJson();
        set(summary, "alignmentMethod",
            "hardware shifted by fresh bypass delay; plugin reported latency removed by host; no "
            "active-signal auto alignment");
        set(summary, "absoluteLevelCalibration", false);
        set(summary, "qualityScore", juce::var());
        writeWav(folder.getChildFile("reference.wav"), reference);
        writeWav(folder.getChildFile("candidate.wav"), plugin);
        writeWav(folder.getChildFile("residual.wav"), result.residual);
        writeJson(folder.getChildFile("analysis_summary.json"), summary);
        set(manifest, "status", "complete");
        writeJson(folder.getChildFile("manifest.json"), manifest);
        writeHardwareReport(folder);
        return store.commit(folder);
    }
    catch (const std::exception &e)
    {
        set(manifest, "status", "failed");
        set(manifest, "error", juce::String::fromUTF8(e.what()));
        writeJson(folder.getChildFile("manifest.json"), manifest);
        throw;
    }
}
} // namespace lab
