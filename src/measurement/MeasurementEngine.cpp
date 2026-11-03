#include "MeasurementEngine.h"
#include "ai-bridge/AiAdvisor.h"
#include "audio-host/PluginHost.h"
namespace lab
{
juce::File MeasurementEngine::compare(const juce::File &project, const juce::File &input, const TargetSpec &a,
                                      const TargetSpec &b, const RenderSettings &settings,
                                      const std::atomic_bool *cancelled)
{
    settings.validate();
    auto source = readWav(input);
    require(source.sampleRate == settings.sampleRate,
            "WAV sample rate must match settings; automatic resampling is disabled");
    SessionStore store(project);
    auto folder = store.beginSession();
    auto manifest = object();
    set(manifest, "schemaVersion", 1);
    set(manifest, "status", "running");
    set(manifest, "createdUtc", utcNow());
    set(manifest, "processingVersion", version);
    set(manifest, "os", juce::SystemStats::getOperatingSystemName());
    set(manifest, "settings", settings.toJson());
    set(manifest, "sourceGitCommit", juce::var());
    set(manifest, "reference", a.toJson());
    set(manifest, "candidate", b.toJson());
    set(manifest, "originalInput", input.getFullPathName());
    try
    {
        require(input.copyFileTo(folder.getChildFile("stimulus.wav")), "Cannot preserve input WAV");
        juce::FileInputStream stream(folder.getChildFile("stimulus.wav"));
        set(manifest, "stimulusSha256", juce::SHA256(stream).toHexString());
        writeJson(folder.getChildFile("manifest.json"), manifest);
        for (auto side : {juce::String("reference"), juce::String("candidate")})
        {
            auto job = object();
            set(job, "action", "render");
            set(job, "settings", settings.toJson());
            set(job, "target", side == "reference" ? a.toJson() : b.toJson());
            set(job, "input", folder.getChildFile("stimulus.wav").getFullPathName());
            set(job, "output", folder.getChildFile(side + ".wav").getFullPathName());
            auto jobFile = folder.getChildFile(side + "-job.json");
            writeJson(jobFile, job);
            auto result = runWorker(worker, jobFile, 180000, cancelled);
            set(manifest, juce::Identifier(side + "Render"), result);
            // Materialise the exact initial state for reruns, including the plugin's default preset.
            auto target = side == "reference" ? a.toJson() : b.toJson();
            set(target, "state", result["initialState"]);
            set(manifest, juce::Identifier(side), target);
        }
        auto ref = readWav(folder.getChildFile("reference.wav"));
        auto candidate = readWav(folder.getChildFile("candidate.wav"));
        auto analysis = AnalysisEngine{}.compare(ref, candidate);
        writeWav(folder.getChildFile("residual.wav"), analysis.residual);
        writeJson(folder.getChildFile("analysis_summary.json"), analysis.toJson());
        NoAiAdvisor advisor;
        writeJson(folder.getChildFile("recommendations.json"), advisor.propose(analysis.toJson()));
        auto report =
            juce::String("# Moth DSP Lab A/B report\n\n") +
            "Reference RMS: " + juce::String(analysis.referenceRmsDb, 3) + " dBFS\n\n" +
            "Candidate RMS: " + juce::String(analysis.candidateRmsDb, 3) + " dBFS\n\n" +
            "B minus A level: " + juce::String(analysis.gainDifferenceDb, 3) + " dB (no normalization)\n\n" +
            "Additional candidate delay: " + juce::String(analysis.candidateDelaySamples) + " samples\n\n" +
            "Alignment reliable: " + (analysis.alignmentReliable ? "yes" : "no") + "\n\n" +
            "Overlap residual RMS: " + juce::String(analysis.residualRmsDb, 3) + " dBFS\n\n" +
            "Output spectra are not transfer functions. A nonlinear residual is not a standalone quality "
            "score. "
            "Reported plugin latency is removed before analysis. Tail is user-bounded. "
            "Third-party plugins may be nondeterministic; repeat the measurement to establish "
            "repeatability.\n";
        require(folder.getChildFile("report.md").replaceWithText(report), "Cannot write report");
        set(manifest, "status", "complete");
        writeJson(folder.getChildFile("manifest.json"), manifest);
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
