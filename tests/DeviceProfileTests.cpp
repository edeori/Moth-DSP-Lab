#include "ai-bridge/DeviceProfile.h"
#include "ai-bridge/Scenario.h"
#include "hardware/SetupTest.h"
#include "measurement/GuidedWorkflow.h"
#include "measurement/HardwareComparison.h"
#include "measurement/MeasurementReport.h"
#include <stdexcept>
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn, const char *message)
{
    bool failed = false;
    try
    {
        fn();
    }
    catch (...)
    {
        failed = true;
    }
    check(failed, message);
}
} // namespace
void runDeviceProfileTests()
{
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("device-tests-" + juce::Uuid().toString());
    root.createDirectory();
    struct Cleanup
    {
        juce::File file;
        ~Cleanup()
        {
            file.deleteRecursively();
        }
    } cleanup{root};
    auto device = lab::object();
    lab::set(device, "type", "head");
    lab::set(device, "controlsText", "CH1 Gain | 0–10\nBass | 0–10");
    lab::set(device, "controlsConfirmed", true);
    lab::set(device, "routeConfirmed", true);
    lab::set(device, "routeNotes",
             "Speaker -> compatible rated loadbox -> documented LINE OUT -> interface LINE input");
    auto c = lab::devicePlannerContext(lab::plannerContext("Generic head", "Explore controls", {}), device);
    check(c["confirmedControls"].size() == 2, "device inventory lost");
    check(!c["capabilities"].toString().contains("B7K Ultra"), "pedal-specific capabilities leaked");
    rejects([] { lab::parseControlInventory("Gain | knob\ngain | knob"); }, "duplicate labels accepted");
    auto unconfirmed = device.clone();
    lab::set(unconfirmed, "routeConfirmed", false);
    rejects([&] { lab::devicePlannerContext(c, unconfirmed); }, "unconfirmed route accepted");
    juce::Image picture(juce::Image::RGB, 2600, 400, true);
    juce::Graphics g(picture);
    g.fillAll(juce::Colours::white);
    g.setColour(juce::Colours::black);
    g.drawText("CH1 Gain         Bass", 0, 0, 2600, 400, juce::Justification::centred);
    auto raw = root.getChildFile("front.png");
    {
        auto out = raw.createOutputStream();
        juce::PNGImageFormat().writeImageToStream(picture, *out);
    }
    auto owned = lab::importPanelPhoto(root, raw);
    raw.deleteFile();
    check(owned.existsAsFile() && juce::ImageFileFormat::loadFrom(owned).getWidth() == 2400,
          "owned normalized photo not durable");
    auto entry = lab::object();
    lab::set(entry, "path", owned.getFullPathName());
    {
        juce::FileInputStream in(owned);
        lab::set(entry, "sha256", juce::SHA256(in).toHexString());
    }
    auto request = lab::object();
    lab::set(request, "task", "recognize_panel");
    lab::set(request, "panelImages", juce::var(juce::Array<juce::var>{entry}));
    auto script = root.getChildFile("fake-vision");
    script.replaceWithText(
        "#!/usr/bin/python3\nimport sys,json,os\na=sys.argv\nassert '--image' in a and '--' in a\nassert "
        "os.path.isfile(a[a.index('--image')+1])\ns=json.load(open(a[a.index('--output-schema')+1]))\nassert "
        "'controlsText' in s['required']\njson.dump({'summary':'Generic head','controlsText':'CH1 Gain | "
        "0-10\\nBass | 0-10','uncertainties':'Verify labels and "
        "route'},open(a[a.index('--output-last-message')+1],'w'))\n",
        false, false, "\n");
    script.setExecutePermission(true);
    std::atomic_bool cancelled{false};
    auto recognized = lab::runCodexPlanner(request, cancelled, script);
    check(lab::parseControlInventory(recognized["controlsText"].toString()).size() == 2,
          "vision transport lost inventory");
    auto changed = request.clone();
    auto changedPhoto = changed["panelImages"][0];
    lab::set(changedPhoto, "sha256", "different");
    rejects([&] { lab::runCodexPlanner(changed, cancelled, script); },
            "changed image accepted after consent");
    auto plan = juce::JSON::parse(
        R"({"title":"Generic head","reason":"Control response","completion":"Review evidence","limitations":"Low-level noise only","calibration":"Set the lowest-gain clean reference, retain the compatible load and verify the documented line return.","action":"request_measurement","steps":[{"id":"gain","title":"Gain change","mode":2,"settings":"CH1 Gain 2; Bass 5; output fixed","reason":"Isolate gain","expected":"No clipping","stimulus":"guarded_noise_2.6s_-36dBFS","parameters":[]}],"coverage":[{"control":"CH1 Gain","status":"planned","stepIds":["gain"],"states":["1","2"],"reason":"Vary gain"},{"control":"Bass","status":"deferred","stepIds":[],"states":[],"reason":"Later EQ pass"}]})");
    lab::validateScenario(plan);
    lab::validateDevicePlan(plan, device);
    auto bad = plan.clone();
    bad["coverage"].getArray()->removeLast();
    rejects([&] { lab::validateDevicePlan(bad, device); }, "incomplete generic coverage accepted");
    check(!lab::describeScenario(plan).contains("PLUGIN:"), "generic plan shows plugin preparation");
    // Ensure the adapter actually generates a dynamic schema for the confirmed controls.
    auto answer = juce::JSON::toString(plan, true);
    script.replaceWithText(
        "#!/usr/bin/python3\nimport "
        "sys,json\na=sys.argv\ns=json.load(open(a[a.index('--output-schema')+1]))\nassert "
        "s['properties']['coverage']['minItems']==2\nassert "
        "s['properties']['coverage']['items']['properties']['control']['enum']==['CH1 Gain','Bass']\nassert "
        "'calibration' in s['required']\nopen(a[a.index('--output-last-message')+1],'w').write(" +
            juce::JSON::toString(juce::var(answer), true) + ")\n",
        false, false, "\n");
    script.setExecutePermission(true);
    auto planned = lab::runCodexPlanner(c, cancelled, script);
    check(planned["steps"].size() == 1, "generic plan adapter failed");
    lab::GuidedWorkflow w(root);
    w.create();
    lab::set(w.state, "deviceSetup", device);
    lab::set(w.state, "scenario", plan);
    lab::set(w.state, "scenarioRevision", "generic-r1");
    lab::set(w.state, "panelImages", request["panelImages"].clone());
    w.save();
    auto probe = lab::makeSetupProbe(48000), capture = probe;
    capture.samples.applyGain(2.f);
    auto baseline = root.getChildFile("baseline");
    baseline.createDirectory();
    lab::writeWav(baseline.getChildFile("capture.wav"), probe);
    lab::writeWav(baseline.getChildFile("probe.wav"), probe);
    auto checkData = lab::object();
    lab::set(checkData, "status", "passed");
    auto analysis = lab::object();
    lab::set(analysis, "passed", true);
    lab::set(analysis, "latencySamples", 0);
    lab::set(checkData, "analysis", analysis);
    lab::set(checkData, "sampleRate", 48000.0);
    auto metadata = lab::object();
    for (auto key : {"device", "driver"})
    {
        lab::set(checkData, key, "fixture");
        lab::set(metadata, key, "fixture");
    }
    for (auto key : {"inputChannel", "outputChannel"})
    {
        lab::set(checkData, key, 1);
        lab::set(metadata, key, 1);
    }
    lab::writeJson(baseline.getChildFile("setup-check.json"), checkData);
    lab::set(metadata, "capturedSamples", capture.samples.getNumSamples());
    lab::set(metadata, "deviceError", false);
    lab::set(metadata, "interfaceAndReampUnchangedConfirmed", true);
    lab::set(metadata, "requestedPedalStateConfirmed", true);
    lab::set(metadata, "operatorPhysicalSettings", "CH1 Gain 2; Bass 5; output fixed");
    lab::set(metadata, "blockSize", 256);
    lab::set(metadata, "captureOnly", true);
    lab::set(metadata, "deviceSetup", device);
    lab::set(metadata, "workflowId", w.file.getFileNameWithoutExtension());
    lab::set(metadata, "scenarioRevision", "generic-r1");
    lab::set(metadata, "scenarioStep", plan["steps"][0]);
    lab::set(metadata, "measurementMode", 2);
    auto folder = lab::compareHardwareCapture(root.getChildFile("DO-NOT-RUN-A-PLUGIN"), root, probe, capture,
                                              {}, metadata, checkData, baseline);
    check(lab::SessionStore::load(folder)["candidate"]["kind"].toString() == "recorded_hardware_baseline",
          "invented plugin identity");
    auto summary = lab::readJson(folder.getChildFile("analysis_summary.json"));
    check(std::abs(double(summary["gainDifferenceDb"]) + 6.0206) < 0.001, "baseline gain comparison wrong");
    check(!folder.getChildFile("candidate-job.json").exists(), "generic capture launched plugin");
    lab::GuidedWorkflow resumed(root);
    resumed.load(w.file);
    check(resumed.scenarioCompleted() == 1, "generic capture cannot resume");
    auto exported = lab::exportMeasurementHandoff(root, resumed.state);
    check(exported.getChildFile("frontpanel-1.png").existsAsFile(), "photo missing from handoff");
    auto index = lab::readJson(exported.getChildFile("index.json"));
    auto record = index["measurements"][0];
    check(bool(record["captureOnly"]), "baseline type lost on export");
    check(exported.getChildFile(record["calibrationFolder"].toString())
              .getChildFile("capture.wav")
              .existsAsFile(),
          "baseline capture missing from export");
    auto archive = exported.getSiblingFile(exported.getFileName() + ".zip");
    juce::ZipFile zip(archive);
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));
        check(stream != nullptr, "ZIP entry unreadable");
        auto relative = zip.getEntry(i)->filename.fromFirstOccurrenceOf("/", false, false);
        if (relative == "index.json")
            continue;
        check(juce::SHA256(*stream).toHexString() == index["sha256"][juce::Identifier(relative)].toString(),
              "ZIP data differs from index hash");
    }
    // Cross-workflow links are durable, deduplicated and cycle-safe.
    lab::GuidedWorkflow second(root);
    second.create();
    lab::set(second.state, "collectionSources",
             juce::var(juce::Array<juce::var>{w.file.getFileNameWithoutExtension()}));
    second.save();
    lab::set(resumed.state, "collectionSources",
             juce::var(juce::Array<juce::var>{second.file.getFileNameWithoutExtension()}));
    resumed.save();
    check(lab::collectionResults(root, second.state).size() == 1, "linked collection duplicate/cycle");
    auto review = lab::object();
    lab::set(review, "status", "excluded");
    lab::writeJson(folder.getChildFile("operator-review.json"), review);
    check(lab::collectionResults(root, second.state).isEmpty(), "linked excluded result leaked");
}
