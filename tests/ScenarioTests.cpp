#include "ai-bridge/Scenario.h"
#include "measurement/GuidedWorkflow.h"
#include "measurement/MeasurementReport.h"
#include <iostream>
#include <stdexcept>
namespace
{
void check(bool ok, const char *text)
{
    if (!ok)
        throw std::runtime_error(text);
}
juce::var fixture()
{
    auto plan = juce::JSON::parse(
        R"({"action":"request_measurement","title":"EQ exploration","reason":"Isolate bass response","completion":"Compare repeatability and bass changes","limitations":"No absolute calibration or THD","steps":[{"id":"flat","title":"Flat baseline","mode":2,"settings":"All EQ noon, Blend max, Drive 9h, Level noon, Master 9h, Attack/Grunt flat, mid 500/1500","reason":"Reference","expected":"No clipping","stimulus":"guarded_noise_2.6s_-36dBFS","parameters":[]},{"id":"bass","title":"Bass boost","mode":2,"settings":"Same with Bass 3h","reason":"Change one control","expected":"More low end","stimulus":"guarded_noise_2.6s_-36dBFS","parameters":[{"name":"Bass","value":0.75}]}]})");
    juce::Array<juce::var> coverage;
    for (auto name : {"Bass", "Low Mid", "High Mid", "Treble", "Drive", "Blend", "Level", "Master", "Attack",
                      "Grunt", "Low Mid Frequency", "High Mid Frequency", "Distortion"})
    {
        auto item = lab::object();
        lab::set(item, "control", name);
        lab::set(item, "status", "deferred");
        lab::set(item, "reason", "Isolated test scope");
        lab::set(item, "stepIds", juce::var(juce::Array<juce::var>{}));
        lab::set(item, "states", juce::var(juce::Array<juce::var>{}));
        coverage.add(item);
    }
    lab::set(plan, "coverage", juce::var(coverage));
    return plan;
}
} // namespace
void runScenarioTests()
{
    {
        auto record = lab::object(), analysis = lab::object();
        juce::Array<juce::var> bins;
        for (int i = 0; i < 2049; ++i)
        {
            auto b = lab::object();
            lab::set(b, "frequencyHz", double(i));
            lab::set(b, "referenceDb", -51.23456789);
            lab::set(b, "candidateDb", -42.123456789);
            bins.add(b);
        }
        lab::set(analysis, "spectrum", juce::var(bins));
        lab::set(analysis, "gainDifferenceDb", 13.994);
        lab::set(analysis, "alignmentReliable", false);
        lab::set(record, "analysis", analysis);
        lab::set(record, "settings", "EQ noon");
        auto original = lab::plannerContext("B7K", "Explore EQ", juce::var(juce::Array<juce::var>{record}));
        auto compact = lab::compactPlannerContext(original);
        check(juce::JSON::toString(original).length() > 100000, "regression fixture not large enough");
        check(juce::JSON::toString(compact).length() < 10000, "spectrum still inflates context");
        check(compact["evidence"][0]["analysis"]["gainDifferenceDb"] == analysis["gainDifferenceDb"],
              "metric lost");
        check(compact["evidence"][0]["analysis"]["alignmentReliable"].isBool(), "uncertainty lost");
        check(original["evidence"][0]["analysis"]["spectrum"].size() == 2049, "stored data modified");
        auto excluded = original.clone();
        auto review = lab::object();
        lab::set(review, "status", "excluded");
        lab::set(review, "reason", "Bass unchanged");
        auto excludedRecord = excluded["evidence"][0];
        lab::set(excludedRecord, "review", review);
        auto excludedContext = lab::compactPlannerContext(excluded);
        check(excludedContext["evidence"][0]["review"]["status"].toString() == "excluded" &&
                  excludedContext["evidence"][0]["analysis"]["gainDifferenceDb"].isVoid(),
              "invalid settings evidence sent as usable metrics");

        juce::Array<juce::var> many;
        for (int i = 0; i < 100; ++i)
        {
            auto e = record.clone();
            lab::set(e, "settings", juce::String::repeatedString(juce::String::fromUTF8("ő"), 2000));
            many.add(e);
        }
        lab::set(original, "evidence", juce::var(many));
        compact = lab::compactPlannerContext(original);
        check(juce::JSON::toString(compact, true).getNumBytesAsUTF8() <= 80000, "UTF-8 byte budget exceeded");
        check(int(compact["contextCoverage"]["omittedMeasurements"]) > 0, "omitted evidence hidden");
        auto request = juce::SystemStats::getEnvironmentVariable("MOTH_TEST_SAVED_CONTEXT", "");
        if (request.isNotEmpty())
        {
            auto saved = lab::readJson(juce::File(request))["aiLastRequest"];
            auto bounded = lab::compactPlannerContext(saved);
            std::cout << "Saved request: " << juce::JSON::toString(saved, true).getNumBytesAsUTF8() << " -> "
                      << juce::JSON::toString(bounded, true).getNumBytesAsUTF8()
                      << " bytes (local validation only)\n";
            check(juce::JSON::toString(bounded, true).getNumBytesAsUTF8() < 80000,
                  "real request still too large");
        }
    }
    auto profilePath = juce::SystemStats::getEnvironmentVariable("MOTH_TEST_PROFILE_FILE", "");
    if (profilePath.isNotEmpty())
    {
        auto profile = lab::GuidedWorkflow::loadProfile(juce::File(profilePath));
        check(profile["scenario"]["coverage"].size() == 13, "profile coverage missing");
        std::cout << "Saved correction profile validated: " << profile["scenario"]["steps"].size()
                  << " steps\n";
    }
    auto exportPath = juce::SystemStats::getEnvironmentVariable("MOTH_EXPORT_WORKFLOW", "");
    if (exportPath.isNotEmpty())
    {
        auto source = juce::File(exportPath);
        auto package = lab::exportMeasurementHandoff(source.getParentDirectory().getParentDirectory(),
                                                     lab::readJson(source));
        std::cout << "HANDOFF=" << package.getFullPathName() << "\n";
    }
    auto plan = fixture();
    lab::validateScenario(plan);
    {
        auto natural = plan.clone();
        auto step = natural["steps"][0];
        lab::set(step, "settings", juce::String::fromUTF8("Attack a lejtő jelhez; Grunt FLAT."));
        lab::validateScenario(natural); // Hungarian article 'a' is not an anonymous A switch label.
    }

    {
        auto broken = plan.clone();
        broken["coverage"].getArray()->removeLast();
        bool rejected = false;
        try
        {
            lab::validateScenario(broken);
        }
        catch (...)
        {
            rejected = true;
        }
        check(rejected, "missing control coverage accepted");
        broken = plan.clone();
        auto item = broken["coverage"][0];
        lab::set(item, "status", "planned");
        lab::set(item, "stepIds", juce::var(juce::Array<juce::var>{"missing"}));
        lab::set(item, "states", juce::var(juce::Array<juce::var>{"2h"}));
        rejected = false;
        try
        {
            lab::validateScenario(broken);
        }
        catch (...)
        {
            rejected = true;
        }
        check(rejected, "coverage claims nonexistent measurement");
    }

    {
        auto first = plan["steps"][0];
        auto repeated = first.clone();
        lab::set(repeated, "title", "Repeat reference");
        auto repeatText = lab::scenarioStepInstructions(repeated, first);
        check(repeatText.contains(juce::String::fromUTF8("ISMÉTLÉS:")), "identical capture not identified");
        lab::set(repeated, "parameters", plan["steps"][1]["parameters"].clone());
        auto pluginOnly = lab::scenarioStepInstructions(repeated, first);
        check(pluginOnly.contains(juce::String::fromUTF8("Csak a pluginprofil változik")),
              "plugin-only change requests physical changes");
        auto reset = lab::scenarioStepInstructions(first, repeated);
        check(reset.contains("0.75") && reset.contains("0.50"), "default-reset plugin changes hidden");
        auto changed = lab::scenarioStepInstructions(plan["steps"][1], first);
        check(changed.contains("Same with Bass 3h") &&
                  !changed.contains(juce::String::fromUTF8("PEDÁL: nincs átállítás")),
              "actual step settings replaced");
        check(changed.contains(juce::String::fromUTF8("PRÓBAJEL: változatlan")),
              "identical stimulus not disclosed");
    }

    for (int bad = 0; bad < 6; ++bad)
    {
        auto p = plan.clone();
        auto s = p["steps"][0];
        if (bad == 0)
            lab::set(s, "stimulus", "sweep");
        if (bad == 1)
            lab::set(s, "mode", 1);
        if (bad == 2)
            lab::set(s, "id", "bass");
        if (bad == 3)
            lab::set(s, "settings", "");
        if (bad == 4)
            lab::set(s, "parameters", juce::JSON::parse(R"([{"name":"Bypass","value":1}])"));
        if (bad == 5)
            lab::set(s, "settings", "Attack A; Grunt F");
        bool rejected = false;
        try
        {
            lab::validateScenario(p);
        }
        catch (...)
        {
            rejected = true;
        }
        check(rejected, "invalid plan accepted");
    }
    auto done = plan.clone();
    lab::set(done, "action", "no_change");
    lab::set(done, "steps", juce::var(juce::Array<juce::var>{}));
    lab::validateScenario(done);
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("scenario-test-" + juce::Uuid().toString());
    root.createDirectory();
    struct Cleanup
    {
        juce::File d;
        ~Cleanup()
        {
            d.deleteRecursively();
        }
    } cleanup{root};
    lab::GuidedWorkflow w(root);
    w.create();
    {
        auto profile = w.saveProfile(plan, "B7K Ultra", "EQ comparison");
        auto loaded = lab::GuidedWorkflow::loadProfile(profile);
        check(loaded["scenario"]["steps"].size() == 2, "profile plan lost");
        check(loaded["deviceProfile"].toString() == "B7K Ultra", "profile device lost");
        check(loaded["scenarioResults"].isVoid() && loaded["result2"].isVoid(),
              "profile contains completed measurements");
        auto another = w.saveProfile(plan, "B7K Ultra", "EQ comparison");
        check(another != profile && w.profiles().size() == 2, "profile save overwrites previous version");
        check(lab::GuidedWorkflow::defaultProjectForExecutable(
                  root.getChildFile("Installed.app/Contents/MacOS/Lab")) ==
                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getChildFile("Moth DSP Lab"),
              "installed app does not use Documents");
    }

    lab::set(w.state, "scenario", plan);
    lab::set(w.state, "scenarioRevision", "r1");
    lab::set(w.state, "scenarioResults", lab::object());
    w.save();
    check(w.nextScenarioStep()["id"].toString() == "flat", "first step missing");
    auto folder = root.getChildFile("sessions/first");
    folder.createDirectory();
    auto hw = lab::object();
    lab::set(hw, "workflowId", w.file.getFileNameWithoutExtension());
    lab::set(hw, "measurementMode", 2);
    lab::set(hw, "scenarioRevision", "r1");
    lab::set(hw, "scenarioStep", plan["steps"][0]);
    auto m = lab::object();
    lab::set(m, "schemaVersion", 1);
    lab::set(m, "status", "complete");
    lab::set(m, "hardware", hw);
    lab::writeJson(folder.getChildFile("manifest.json"), m);
    for (auto *name : {"analysis_summary.json", "reference.wav", "candidate.wav", "residual.wav"})
        folder.getChildFile(name).replaceWithText("fixture");
    lab::writeJson(folder.getChildFile("analysis_summary.json"), lab::object());
    lab::GuidedWorkflow recovered(root);
    recovered.load(w.file);
    check(recovered.nextScenarioStep()["id"].toString() == "bass",
          "crash after commit did not recover exact step");
    check(recovered.scenarioCompleted() == 1 && recovered.nextMeasurement() == 2,
          "same-mode steps collapsed");
    auto review = lab::object();
    lab::set(review, "status", "excluded");
    lab::set(review, "reason", "Operator did not change Bass");
    lab::writeJson(folder.getChildFile("operator-review.json"), review);
    check(recovered.scenarioCompleted() == 0 && recovered.nextScenarioStep()["id"].toString() == "flat",
          "excluded capture still satisfies plan");
    auto replacement = root.getChildFile("sessions/second");
    folder.copyDirectoryTo(replacement);
    replacement.getChildFile("operator-review.json").deleteFile();
    recovered.recordScenarioResult(hw, replacement);
    recovered.recordScenarioResult(hw, folder);
    check(recovered.scenarioCompleted() == 1,
          "recovery resurrected excluded capture over its valid replacement");
    {
        auto snapshot = recovered.state.clone(), history = lab::object(), oldResults = lab::object();
        lab::set(oldResults, "old-flat", folder.getFullPathName());
        lab::set(history, "results", oldResults);
        lab::set(snapshot, "scenarioHistory", juce::var(juce::Array<juce::var>{history}));
        auto exported = lab::exportMeasurementHandoff(root, snapshot);
        auto index = lab::readJson(exported.getChildFile("index.json"));
        check(int(index["includedCaptures"]) == 1 && int(index["excludedCaptures"]) == 0,
              "clean handoff must omit excluded audio");
        check(exported.getSiblingFile(exported.getFileName() + ".zip").existsAsFile(), "handoff ZIP missing");
        juce::ZipFile zip(exported.getSiblingFile(exported.getFileName() + ".zip"));
        check(zip.getNumEntries() > 0, "handoff ZIP empty");
        for (const auto &record : *index["measurements"].getArray())
        {
            auto relative = record["folder"].toString();
            check(!juce::File::isAbsolutePath(relative), "handoff artifact path not portable");
            auto copied = exported.getChildFile(relative);
            check(copied.getChildFile("reference.wav").existsAsFile(), "handoff audio missing");
            if (!bool(record["requestedSettingsEvidence"]))
                check(copied.getChildFile("report.md").loadFileAsString().contains("EXCLUDED"),
                      "report lost exclusion");
            juce::FileInputStream input(copied.getChildFile("reference.wav"));
            check(index["sha256"][juce::Identifier(relative + "/reference.wav")].toString() ==
                      juce::SHA256(input).toHexString(),
                  "handoff checksum mismatch");
        }
    }
    folder.getChildFile("operator-review.json").deleteFile();

    lab::set(recovered.state, "scenarioRevision", "r2");
    check(recovered.nextScenarioStep()["id"].toString() == "flat", "old revision skipped new step");
    recovered.duplicateSetup();
    check(recovered.scenarioCompleted() == 0 && recovered.hasScenario(),
          "duplicate lost plan or reused result");
    // Fake executable validates transport/schema plumbing without network or physical audio.
    auto script = root.getChildFile("fake-codex");
    auto answer = juce::JSON::toString(plan, true);
    script.replaceWithText(
        "#!/usr/bin/python3\nimport sys,json\nsys.stdout.write('x'*200000)\na=sys.argv\nassert "
        "'--output-schema' in a and 'read-only' in a\nassert '--ignore-user-config' in "
        "a\njson.load(open(a[a.index('--output-schema')+1]))\nopen(a[a.index('--output-last-message')+1],'w')"
        ".write(" +
            juce::JSON::toString(juce::var(answer), true) + ")\n",
        false, false, "\n");
    script.setExecutePermission(true);
    std::atomic_bool cancelled{false};
    auto actual = lab::runCodexPlanner(lab::plannerContext("test", "test", {}), cancelled, script);
    check(actual["steps"].size() == 2, "CLI adapter failed");
    cancelled = true;
    bool stopped = false;
    try
    {
        lab::runCodexPlanner({}, cancelled, script);
    }
    catch (...)
    {
        stopped = true;
    }
    check(stopped, "cancel ignored");
    script.replaceWithText("#!/usr/bin/python3\nimport time\ntime.sleep(30)\n", false, false, "\n");
    script.setExecutePermission(true);
    cancelled = false;
    bool timeout = false;
    auto start = juce::Time::getMillisecondCounterHiRes();
    try
    {
        lab::runCodexPlanner({}, cancelled, script, 150);
    }
    catch (...)
    {
        timeout = true;
    }
    check(timeout && juce::Time::getMillisecondCounterHiRes() - start < 5000,
          "CLI timeout did not stop process");
    if (juce::SystemStats::getEnvironmentVariable("MOTH_TEST_LIVE_CODEX", "") == "1")
    {
        cancelled = false;
        auto inputPath = juce::SystemStats::getEnvironmentVariable("MOTH_TEST_PLANNER_INPUT", "");
        auto input = inputPath.isNotEmpty() ? lab::readJson(juce::File(inputPath)) : lab::object();
        auto context = lab::compactPlannerContext(lab::plannerContext(
            inputPath.isNotEmpty()
                ? input["profile"].toString()
                : "B7K Ultra baseline. EQ noon, mid 500/1500, Attack Grunt flat, Master 9h.",
            inputPath.isNotEmpty()
                ? input["goal"].toString()
                : "Create two exploratory EQ measurements. No measured evidence available.",
            input["evidence"]));
        auto result = lab::runCodexPlanner(context, cancelled);
        lab::validateScenario(result);
        auto saved = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("moth-live-planner-result.json");
        lab::writeJson(saved, result);
        std::cout << "Live CLI plan validated: " << result["steps"].size() << " steps\n";
    }
}
