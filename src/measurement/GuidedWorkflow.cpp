#include "GuidedWorkflow.h"
#include "ai-bridge/DeviceProfile.h"
#include "ai-bridge/Scenario.h"
#include <map>
namespace lab
{
namespace
{
bool completeScenarioResult(const juce::File &folder, const juce::var &state, const juce::var &step)
{
    if (!GuidedWorkflow::resultUsable(folder))
        return false;
    auto m = SessionStore::load(folder);
    if (m["hardware"]["scenarioRevision"] != state["scenarioRevision"] ||
        m["hardware"]["scenarioStep"]["id"] != step["id"])
        return false;
    for (auto *name : {"analysis_summary.json", "reference.wav", "candidate.wav", "residual.wav"})
        if (!folder.getChildFile(name).existsAsFile())
            return false;
    return true;
}
} // namespace

juce::File GuidedWorkflow::defaultProject()
{
    return defaultProjectForExecutable(juce::File::getSpecialLocation(juce::File::currentExecutableFile));
}
juce::File GuidedWorkflow::defaultProjectForExecutable(const juce::File &executable)
{
#if defined(LAB_DEVELOPMENT_BUILD_DIR) && defined(LAB_DEVELOPMENT_SOURCE_DIR)
    if (executable.isAChildOf(juce::File(LAB_DEVELOPMENT_BUILD_DIR)) &&
        juce::File(LAB_DEVELOPMENT_SOURCE_DIR).getChildFile("CMakeLists.txt").existsAsFile())
        return juce::File(LAB_DEVELOPMENT_SOURCE_DIR).getChildFile(".local-data/Moth DSP Lab");
#endif
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Moth DSP Lab");
}
juce::File GuidedWorkflow::profilesFolder() const
{
    return root.getChildFile("profiles");
}
juce::Array<juce::File> GuidedWorkflow::profiles() const
{
    auto files = profilesFolder().findChildFiles(juce::File::findFiles, false, "*.moth-profile.json");
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b)
              { return a.getLastModificationTime() > b.getLastModificationTime(); });
    return files;
}
juce::File GuidedWorkflow::saveProfile(const juce::var &plan, const juce::String &deviceProfile,
                                       const juce::String &goal) const
{
    validateScenario(plan);
    if (genericDevice(state))
        validateDevicePlan(plan, state["deviceSetup"]);
    auto profile = object();
    set(profile, "schemaVersion", 1);
    set(profile, "kind", "moth-measurement-profile");
    set(profile, "createdUtc", utcNow());
    set(profile, "name", plan["title"]);
    set(profile, "deviceProfile", deviceProfile);
    set(profile, "goal", goal);
    set(profile, "scenario", plan.clone());
    set(profile, "deviceSetup", state["deviceSetup"].clone());
    set(profile, "panelImages", state["panelImages"].clone());
    require(profilesFolder().createDirectory().wasOk(), "Cannot create measurement profile folder");
    auto dest = profilesFolder().getChildFile(
        juce::File::createLegalFileName(plan["title"].toString()).substring(0, 70) + "-" +
        juce::Uuid().toString() + ".moth-profile.json");
    juce::TemporaryFile temporary(dest);
    writeJson(temporary.getFile(), profile);
    require(temporary.overwriteTargetFileWithTemporary(), "Cannot save measurement profile");
    return dest;
}
juce::var GuidedWorkflow::loadProfile(const juce::File &file)
{
    auto profile = readJson(file);
    require(int(profile["schemaVersion"]) == 1 && profile["kind"].toString() == "moth-measurement-profile",
            "Unsupported measurement profile");
    require(profile["deviceProfile"].isString() && profile["goal"].isString(),
            "Invalid measurement profile fields");
    validateScenario(profile["scenario"]);
    if (genericDevice(profile))
        validateDevicePlan(profile["scenario"], profile["deviceSetup"]);
    return profile;
}
juce::String GuidedWorkflow::modeName(int mode)
{
    return juce::String::fromUTF8(mode == 1 ? "Bypass ellenőrzés" : mode == 2 ? "Tiszta / EQ" : "Drive");
}
GuidedWorkflow::GuidedWorkflow(juce::File project)
    : root(project == juce::File{} ? defaultProject() : project)
{
}
void GuidedWorkflow::create()
{
    state = object();
    set(state, "schemaVersion", 1);
    set(state, "createdUtc", utcNow());
    set(state, "phase", "idle");
    set(state, "mode", 1);
    set(state, "step", 0);
    file = root.getChildFile("workflows").getChildFile(juce::Uuid().toString() + ".json");
    save();
}
void GuidedWorkflow::duplicateSetup()
{
    auto source = state.clone();
    auto sourceId = file.getFileNameWithoutExtension();
    create();
    for (const auto *key : {"driver", "device", "input", "output", "notes2", "notes3", "deviceSetup",
                            "panelImages", "approvedDeviceSetup"})
        set(state, key, source[key]);
    set(state, "name",
        source["name"].toString().isEmpty()
            ? juce::String::fromUTF8("B7K / DarkBass — ismétlés")
            : source["name"].toString() + juce::String::fromUTF8(" — ismétlés"));
    set(state, "sourceWorkflow", sourceId);
    if (source["scenario"].isObject())
    {
        set(state, "scenario", source["scenario"].clone());
        set(state, "scenarioRevision", juce::Uuid().toString());
        set(state, "scenarioApprovedUtc", utcNow());
        set(state, "scenarioResults", object());
        set(state, "aiProfile", source["aiProfile"]);
        set(state, "aiGoal", source["aiGoal"]);
    }
    // Remembered wiring is useful; no result, confirmation or live bypass gate is copied.
    set(state, "step", 3);
    save();
}
void GuidedWorkflow::save()
{
    require(file != juce::File{}, "No workflow selected");
    require(file.getParentDirectory().createDirectory().wasOk(), "Cannot create workflow folder");
    set(state, "workflowId", file.getFileNameWithoutExtension());
    set(state, "updatedUtc", utcNow());
    juce::TemporaryFile temporary(file);
    writeJson(temporary.getFile(), state);
    require(temporary.overwriteTargetFileWithTemporary(), "Cannot save workflow atomically");
    if (state["storageRun"].toString().isNotEmpty())
    {
        auto folder = root.getChildFile(SessionStore::runPath(state, file.getFileNameWithoutExtension()));
        require(folder.createDirectory().wasOk(), "Cannot create measurement run folder");
        juce::TemporaryFile snapshot(folder.getChildFile("run.json"));
        writeJson(snapshot.getFile(), state);
        require(snapshot.overwriteTargetFileWithTemporary(), "Cannot save measurement run snapshot");
        juce::String index = "# " + state["name"].toString() + "\n\n" +
                             state["scenario"]["title"].toString() +
                             "\n\n- `results/`: analysed recordings, WAVs and reports.\n"
                             "- `captures/`: original recordings and bypass checks.\n"
                             "- `exports/`: immutable Codex handoff snapshots.\n"
                             "- `run.json`: settings, plan, progress and previous revisions.\n\n";
        if (const auto *steps = state["scenario"]["steps"].getArray())
            for (const auto &step : *steps)
            {
                auto id = step["id"].toString();
                auto path = state["scenarioResults"][juce::Identifier(id)].toString();
                index += "- " + id + " " + step["title"].toString();
                if (juce::File::isAbsolutePath(path))
                {
                    auto result = juce::File(path);
                    index += resultUsable(result) ? " — saved" : " — EXCLUDED";
                    index += " [report](<" +
                             result.getChildFile("report.md")
                                 .getRelativePathFrom(folder)
                                 .replaceCharacter('\\', '/') +
                             ">)";
                }
                else
                    index += " — pending";
                index += "\n";
            }
        require(folder.getChildFile("README.md").replaceWithText(index), "Cannot save measurement index");
    }
}
void GuidedWorkflow::load(const juce::File &source)
{
    auto loaded = readJson(source);
    require(static_cast<int>(loaded["schemaVersion"]) == 1, "Unsupported workflow version");
    state = loaded;
    file = source;
    if (state["scenario"].isObject())
        validateScenario(state["scenario"]);
    // Recover commits that reached disk just before the UI/journal was interrupted.
    for (const auto &folder : SessionStore::sessions(root))
    {
        try
        {
            auto m = SessionStore::load(folder);
            if (m["hardware"]["workflowId"].toString() != file.getFileNameWithoutExtension())
                continue;
            recordScenarioResult(m["hardware"], folder);
            int mode = static_cast<int>(m["hardware"]["measurementMode"]);
            if (mode >= 2 && mode <= 3 &&
                (!hasResult(mode) || folder.getFileName() > result(mode).getFileName()))
                set(state, juce::Identifier("result" + juce::String(mode)), folder.getFullPathName());
        }
        catch (...)
        {
        }
    }
    if (nextMeasurement() == 0)
        set(state, "phase", "complete");
    if (nextMeasurement() != 0 && state["phase"].toString() == "complete")
        set(state, "phase", "interrupted");
    auto phase = state["phase"].toString();
    if (phase == "capturing" || phase == "processing" || phase == "preparing")
    {
        set(state, "phase", "interrupted");
        save();
    }
}
juce::Array<juce::File> GuidedWorkflow::history() const
{
    auto files = root.getChildFile("workflows").findChildFiles(juce::File::findFiles, false, "*.json");
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b)
              { return a.getLastModificationTime() > b.getLastModificationTime(); });
    return files;
}
juce::File GuidedWorkflow::result(int mode) const
{
    auto path = state[juce::Identifier("result" + juce::String(mode))].toString();
    return juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File{};
}
bool GuidedWorkflow::hasResult(int mode) const
{
    try
    {
        auto folder = result(mode);
        if (folder == juce::File{})
            return false;
        auto m = readJson(folder.getChildFile(mode == 1 ? "setup-check.json" : "manifest.json"));
        if (mode == 1)
            return m["status"].toString() == "passed";
        return m["status"].toString() == "complete" && m["reference"]["kind"].toString() == "hardware" &&
               static_cast<int>(m["hardware"]["measurementMode"]) == mode &&
               folder.getChildFile("analysis_summary.json").existsAsFile() &&
               folder.getChildFile("reference.wav").existsAsFile() &&
               folder.getChildFile("candidate.wav").existsAsFile() &&
               folder.getChildFile("residual.wav").existsAsFile();
    }
    catch (...)
    {
        return false;
    }
}
bool GuidedWorkflow::hasScenario() const
{
    return state["scenario"].isObject();
}
int GuidedWorkflow::scenarioCompleted() const
{
    int n = 0;
    if (auto *steps = state["scenario"]["steps"].getArray())
        for (const auto &s : *steps)
        {
            auto path = state["scenarioResults"][juce::Identifier(s["id"].toString())].toString();
            if (!juce::File::isAbsolutePath(path))
                continue;
            try
            {
                if (completeScenarioResult(juce::File(path), state, s))
                    ++n;
            }
            catch (...)
            {
            }
        }
    return n;
}
juce::var GuidedWorkflow::nextScenarioStep() const
{
    if (auto *steps = state["scenario"]["steps"].getArray())
        for (const auto &s : *steps)
        {
            auto path = state["scenarioResults"][juce::Identifier(s["id"].toString())].toString();
            bool complete = false;
            if (juce::File::isAbsolutePath(path))
                try
                {
                    complete = completeScenarioResult(juce::File(path), state, s);
                }
                catch (...)
                {
                }
            if (!complete)
                return s;
        }
    return {};
}
void GuidedWorkflow::recordScenarioResult(const juce::var &metadata, const juce::File &folder)
{
    if (!hasScenario() || metadata["scenarioRevision"] != state["scenarioRevision"])
        return;
    auto id = metadata["scenarioStep"]["id"].toString();
    if (id.isEmpty())
        return;
    auto results = state["scenarioResults"];
    if (!results.isObject())
        results = object();
    auto existing = results[juce::Identifier(id)].toString();
    if (juce::File::isAbsolutePath(existing) && juce::File(existing) != folder)
    {
        auto old = juce::File(existing);
        if (old.getChildFile("manifest.json").existsAsFile() && resultUsable(old) &&
            (!resultUsable(folder) || old.getFileName() > folder.getFileName()))
            return;
    }
    set(results, juce::Identifier(id), folder.getFullPathName());
    set(state, "scenarioResults", results);
}
int GuidedWorkflow::nextMeasurement() const
{
    if (hasScenario())
    {
        auto next = nextScenarioStep();
        return next.isObject() ? int(next["mode"]) : 0;
    }

    if (!hasResult(2))
        return 2;
    if (!hasResult(3))
        return 3;
    return 0;
}
void GuidedWorkflow::recordResult(int mode, const juce::File &folder)
{
    set(state, juce::Identifier("result" + juce::String(mode)), folder.getFullPathName());
    set(state, "phase", nextMeasurement() == 0 ? "complete" : "idle");
    save();
}
void GuidedWorkflow::importLegacySessions()
{
    // Group existing captures by their actual bypass provenance, never by filename proximity.
    juce::StringArray known;
    for (const auto &f : history())
    {
        try
        {
            known.add(readJson(f)["legacyBypass"].toString());
        }
        catch (...)
        {
        }
    }
    auto folders = SessionStore::sessions(root);
    std::sort(folders.begin(), folders.end());
    std::map<juce::String, juce::Array<juce::File>> groups;
    for (const auto &folder : folders)
    {
        try
        {
            auto m = SessionStore::load(folder);
            if (m["reference"]["kind"].toString() != "hardware")
                continue;
            if (m["hardware"]["workflowId"].toString().isNotEmpty())
                continue;
            auto key = m["bypassCheckFolder"].toString();
            if (key.isNotEmpty() && !known.contains(key))
                groups[key].add(folder);
        }
        catch (...)
        {
        }
    }
    for (auto &[key, sessions] : groups)
    {
        create();
        set(state, "legacyBypass", key);
        set(state, "result1", key);
        for (const auto &folder : sessions)
        {
            auto m = readJson(folder.getChildFile("manifest.json"));
            int mode = static_cast<int>(m["hardware"]["measurementMode"]);
            if (mode < 2 || mode > 3)
                continue;
            set(state, "createdUtc", m["bypassCheck"]["createdUtc"]);
            set(state, "driver", m["hardware"]["driver"]);
            set(state, "device", m["hardware"]["device"]);
            set(state, "input", m["hardware"]["inputName"]);
            set(state, "output", m["hardware"]["outputName"]);
            set(state, juce::Identifier("notes" + juce::String(mode)),
                m["hardware"]["operatorPhysicalSettings"]);
            recordResult(mode, folder);
        }
        save();
    }
}
juce::var GuidedWorkflow::resultReview(const juce::File &folder)
{
    auto file = folder.getChildFile("operator-review.json");
    if (!file.existsAsFile())
        return {};
    try
    {
        auto review = readJson(file);
        if (review["status"].toString() == "excluded")
            return review;
    }
    catch (...)
    {
    }
    auto review = object();
    set(review, "status", "excluded");
    set(review, "reason", "Unreadable or unsupported operator review; inspect before using this result");
    return review;
}
bool GuidedWorkflow::resultUsable(const juce::File &folder)
{
    return resultReview(folder)["status"].toString() != "excluded";
}
juce::String GuidedWorkflow::resultDescription(const juce::File &folder)
{
    auto m = SessionStore::load(folder);
    auto s = readJson(folder.getChildFile("analysis_summary.json"));
    auto review = resultReview(folder);
    auto warning = review["status"].toString() == "excluded"
                       ? juce::String::fromUTF8("NEM IGAZOLJA A KÉRT BEÁLLÍTÁST — ÚJRAMÉRENDŐ\n") +
                             review["reason"].toString() + "\n\n"
                       : juce::String{};
    if (bool(m["hardware"]["captureOnly"]))
        return warning + m["hardware"]["scenarioStep"]["title"].toString() +
               juce::String::fromUTF8(" — ELKÉSZÜLT\nAlapreferencia − felvétel: ") +
               juce::String(double(s["gainDifferenceDb"]), 2) +
               juce::String::fromUTF8(
                   " dB\nNincs pluginrender vagy abszolút szintkalibráció. Részletek: report.md");
    return warning + modeName(static_cast<int>(m["hardware"]["measurementMode"])) +
           juce::String::fromUTF8(" — ELKÉSZÜLT\n") +
           juce::Time::fromISO8601(m["createdUtc"].toString()).formatted("%Y. %m. %d. %H:%M:%S") + "\n\n" +
           juce::String::fromUTF8("DarkBass − pedál átlagos szintkülönbség: ") +
           juce::String(static_cast<double>(s["gainDifferenceDb"]), 2) + " dB\n" +
           juce::String::fromUTF8("Eltérésjel (residual): ") +
           juce::String(static_cast<double>(s["residualRmsDbFS"]), 2) + " dBFS\n" +
           juce::String::fromUTF8("Jelkorreláció: ") +
           juce::String(static_cast<double>(s["alignmentCorrelation"]), 3) +
           juce::String::fromUTF8(static_cast<bool>(s["alignmentReliable"])
                                      ? " — megbízható\n"
                                      : " — bizonytalan; a különbség nem értékelhető önmagában\n") +
           juce::String::fromUTF8("Digitális túlvezérlés: ") +
           juce::String::fromUTF8(static_cast<bool>(s["referenceClipped"]) ||
                                          static_cast<bool>(s["candidateClipped"])
                                      ? "IGEN\n"
                                      : "nincs\n") +
           juce::String::fromUTF8(
               "\nÉrtelmezés: a pozitív szintkülönbség hangosabb plugint jelent. Az analóg meghajtás és a "
               "digitális jelszint nincs abszolút kalibrálva. Ez diagnosztikai eredmény, nem a modell "
               "pontossági pontszáma.\n\nRögzített pedálállások:\n") +
           m["hardware"]["operatorPhysicalSettings"].toString() + "\n\n" + folder.getFullPathName();
}
} // namespace lab
