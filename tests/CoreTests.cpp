void runDeviceProfileTests();
void runScenarioTests();
#include "ai-bridge/AiAdvisor.h"
#include "analysis/AnalysisEngine.h"
#include "core/SessionStore.h"
#include <cmath>
#include <iostream>
void runHardwareTests();
void runWorkflowTests();
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
lab::AudioData noise(int channels = 2, int length = 8192)
{
    lab::AudioData a;
    a.samples.setSize(channels, length);
    juce::Random random(12345);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < length; ++i)
            a.samples.setSample(c, i, (random.nextFloat() - 0.5f) * 0.5f);
    return a;
}
void close(double a, double b, double tolerance, const char *message)
{
    check(std::abs(a - b) < tolerance, message);
}
} // namespace
int main()
{
    int passed = 0;
    auto test = [&](const char *name, auto fn)
    {
        try
        {
            fn();
            ++passed;
            std::cout << "PASS " << name << '\n';
        }
        catch (const std::exception &e)
        {
            std::cerr << "FAIL " << name << ": " << e.what() << '\n';
            throw;
        }
    };
    try
    {
        test("identity",
             []
             {
                 auto a = noise();
                 auto r = lab::AnalysisEngine{}.compare(a, a);
                 close(r.gainDifferenceDb, 0, 1e-6, "gain");
                 check(r.candidateDelaySamples == 0, "lag");
                 check(r.residualRmsDb <= -150, "null");
                 check(r.alignmentReliable, "confidence");
             });
        test("device photo, generic capture and clean collection", [] { runDeviceProfileTests(); });
        test("gain fixture",
             []
             {
                 auto a = noise();
                 auto b = a;
                 b.samples.applyGain(0.5f);
                 auto r = lab::AnalysisEngine{}.compare(a, b);
                 close(r.gainDifferenceDb, -6.0206, 0.001, "gain loss");
                 close(r.residualRmsDb, r.candidateRmsDb, 0.001, "no hidden normalization");
                 close(r.candidateSpectrumDb[120] - r.referenceSpectrumDb[120], -6.0206, 0.001,
                       "spectrum delta");
             });
        test("delay fixture and polarity",
             []
             {
                 auto a = noise();
                 auto b = a;
                 b.samples.clear();
                 for (int c = 0; c < 2; ++c)
                     b.samples.copyFrom(c, 137, a.samples, c, 0, a.samples.getNumSamples() - 137);
                 auto r = lab::AnalysisEngine{}.compare(a, b);
                 check(r.candidateDelaySamples == 137, "positive lag");
                 check(r.residualRmsDb <= -150, "aligned null");
                 auto reverse = lab::AnalysisEngine{}.compare(b, a);
                 check(reverse.candidateDelaySamples == -137, "negative lag");
                 b.samples.applyGain(-1);
                 r = lab::AnalysisEngine{}.compare(a, b);
                 check(r.correlation < -.9, "polarity retained");
                 check(r.residualRmsDb > -30, "polarity not silently normalized");
             });
        test("low-pass EQ fixture",
             []
             {
                 auto a = noise();
                 auto b = a;
                 for (int c = 0; c < 2; ++c)
                 {
                     float state = 0;
                     for (int i = 0; i < b.samples.getNumSamples(); ++i)
                     {
                         state += 0.1f * (a.samples.getSample(c, i) - state);
                         b.samples.setSample(c, i, state);
                     }
                 }
                 auto r = lab::AnalysisEngine{}.compare(a, b);
                 check(r.candidateSpectrumDb[1500] - r.referenceSpectrumDb[1500] < -20, "HF rolloff");
                 check(r.candidateSpectrumDb[5] - r.referenceSpectrumDb[5] > -2, "LF preserved");
             });
        test("clipping fixture",
             []
             {
                 auto a = noise();
                 auto b = a;
                 for (int c = 0; c < 2; ++c)
                     for (int i = 0; i < b.samples.getNumSamples(); ++i)
                         b.samples.setSample(c, i, juce::jlimit(-1.f, 1.f, 10 * a.samples.getSample(c, i)));
                 auto r = lab::AnalysisEngine{}.compare(a, b);
                 check(r.candidatePeak == 1, "clipping detected");
                 check(static_cast<bool>(r.toJson()["candidateClipped"]), "clipping JSON");
             });
        test("silence is ambiguous",
             []
             {
                 auto a = noise();
                 a.samples.clear();
                 auto r = lab::AnalysisEngine{}.compare(a, a);
                 check(!r.alignmentReliable, "silence must not imply confidence");
                 check(std::isfinite(r.gainDifferenceDb), "finite summary");
             });
        test("invalid audio rejected",
             []
             {
                 auto a = noise();
                 auto b = a;
                 b.sampleRate = 44100;
                 bool rejected = false;
                 try
                 {
                     lab::AnalysisEngine{}.compare(a, b);
                 }
                 catch (...)
                 {
                     rejected = true;
                 }
                 check(rejected, "sample rate mismatch");
             });
        test("WAV/session round trip",
             []
             {
                 auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("moth-tests-" + juce::Uuid().toString());
                 lab::SessionStore store(folder);
                 auto pending = store.beginSession();
                 auto a = noise();
                 lab::writeWav(pending.getChildFile("a.wav"), a);
                 auto b = lab::readWav(pending.getChildFile("a.wav"));
                 check(a.samples.getNumSamples() == b.samples.getNumSamples(), "length");
                 auto r = lab::AnalysisEngine{}.compare(a, b);
                 check(r.residualRmsDb < -140, "WAV precision");
                 auto manifest = lab::object();
                 lab::set(manifest, "schemaVersion", 1);
                 lab::set(manifest, "status", "complete");
                 lab::writeJson(pending.getChildFile("manifest.json"), manifest);
                 auto saved = store.commit(pending);
                 check(lab::SessionStore::load(saved).isObject(), "session read");
                 check(!pending.exists(), "atomic promotion");
                 folder.deleteRecursively();
             });
        test("settings bounds",
             []
             {
                 lab::RenderSettings s;
                 s.blockSize = 0;
                 bool rejected = false;
                 try
                 {
                     s.validate();
                 }
                 catch (...)
                 {
                     rejected = true;
                 }
                 check(rejected, "bad block size");
             });
        test("plugin state base64 round trip",
             []
             {
                 lab::TargetSpec t;
                 t.plugin =
                     juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fixture.vst3");
                 const unsigned char bytes[] = {0, 1, 255, 128, 0, 42};
                 t.state.append(bytes, sizeof(bytes));
                 auto copy = lab::TargetSpec::fromJson(t.toJson());
                 check(copy.state == t.state, "state bytes changed");
             });
        test("float WAV preserves overs",
             []
             {
                 auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory);
                 auto path = folder.getChildFile("moth-over-" + juce::Uuid().toString() + ".wav");
                 auto a = noise(1, 128);
                 a.samples.setSample(0, 0, 2.5f);
                 a.samples.setSample(0, 1, -3.f);
                 lab::writeWav(path, a);
                 auto b = lab::readWav(path);
                 close(b.samples.getSample(0, 0), 2.5, 1e-6, "positive over clipped");
                 close(b.samples.getSample(0, 1), -3, 1e-6, "negative over clipped");
                 path.deleteFile();
             });
        test("hardware setup probe and guards", [] { runHardwareTests(); });
        test("guided workflow recovery", [] { runWorkflowTests(); });
        test("AI scenario validation, CLI, recovery", [] { runScenarioTests(); });
        test("NoAiAdvisor",
             []
             {
                 lab::NoAiAdvisor a;
                 check(a.propose({})["action"].toString() == "no_change", "offline default");
             });
    }
    catch (...)
    {
        return 1;
    }
    std::cout << passed << " tests passed\n";
    return 0;
}
