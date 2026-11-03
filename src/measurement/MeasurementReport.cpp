#include "MeasurementReport.h"
#include "GuidedWorkflow.h"
namespace lab
{
namespace
{
juce::String jsonSection(const juce::String &name, const juce::var &data)
{
    return "\n## " + name + "\n\n```json\n" + juce::JSON::toString(data) + "\n```\n";
}
} // namespace
void writeHardwareReport(const juce::File &folder)
{
    auto m = readJson(folder.getChildFile("manifest.json"));
    auto analysis = readJson(folder.getChildFile("analysis_summary.json"));
    auto hardware = m["hardware"].clone();
    auto review = GuidedWorkflow::resultReview(folder);
    auto scalars = analysis.clone();
    if (auto *obj = scalars.getDynamicObject())
        obj->removeProperty("spectrum");
    auto candidate = m["candidate"].clone();
    if (auto *obj = candidate.getDynamicObject())
        obj->removeProperty("state");
    juce::String report = "# Hardware measurement: " + hardware["scenarioStep"]["title"].toString() + "\n\n";
    report += GuidedWorkflow::resultUsable(folder)
                  ? "Status: recorded; interpretation remains subject to the checks below.\n"
                  : "**EXCLUDED: not evidence for the requested pedal settings. Do not use for fitting that "
                    "setting.**\n";
    if (bool(hardware["captureOnly"]))
    {
        report +=
            "\nHardware characterization against its recorded low-gain baseline. No plugin rendered. "
            "The baseline is NOT bypass, an absolute voltage calibration, THD or a calibrated transfer "
            "function. "
            "The photo only identified controls; the operator confirmed route, levels/load and actual "
            "settings. "
            "Fixed -36 dBFS, 2.6-second noise cannot establish high-level nonlinear response.\n\n"
            "reference-raw.wav: actual capture. reference.wav: trimmed capture. candidate.wav: recorded "
            "baseline "
            "trimmed by the same estimated delay. residual.wav: baseline minus capture, NOT plugin error. "
            "Active-path latency changes are not corrected. Retain clipping/noise/alignment uncertainty.\n";
        report += jsonSection("Confirmed device, route and physical settings", hardware);
        report += jsonSection("Analysis (baseline versus capture, not model accuracy)", scalars);
        report += jsonSection("Low-gain baseline and route verification", m["bypassCheck"]);
        report += jsonSection("Operator exclusion", review);
        require(folder.getChildFile("report.md").replaceWithText(report), "Cannot write hardware report");
        return;
    }
    report += "\nType: exploratory hardware / offline-plugin comparison using a deterministic 2.6-second "
              "guarded noise probe, output peak at most -36 dBFS. This is NOT a sweep, stepped-sine, THD, "
              "calibrated transfer-function or full nonlinear characterization measurement.\n\n";
    report += "The hardware path includes the DAC, reamp, pedal and ADC. Absolute analog level calibration "
              "and gain normalization were not performed. Physical knobs are operator-confirmed, not sensed; "
              "normalized plugin values are not calibrated physical-knob equivalents. Compare actual "
              "parameter snapshots rather than assuming title equivalence. Attack/Grunt have no verified "
              "matching plugin control. Residual alone is not a model-quality score.\n\n";
    report += "Timing: hardware latency is removed using the fresh bypass result; candidate latency is "
              "removed by the offline host. See manifest.json for alignment method, route, rate, buffer, "
              "capture completion and xrun/error fields. Treat unreliable alignment, clipping and changed "
              "routing as limitations, not DSP mismatch evidence.\n\n";
    if (!hardware.hasProperty("operatorInstructionsVersion"))
        report += "**Legacy operator UI:** the display may have shown fixed baseline settings instead of "
                  "this scenario's settings. Verify operator notes before trusting the requested state; a "
                  "saved request is not proof of an actual knob movement.\n\n";
    report += jsonSection("Operator review / exclusions", review);
    report += jsonSection(
        "Requested step, confirmed physical settings, plugin parameters and capture provenance", hardware);
    report += jsonSection("Analysis summary (spectrum bins are in analysis_summary.json)", scalars);
    report += jsonSection("Bypass calibration and route checks", m["bypassCheck"]);
    report += jsonSection("Offline render settings", m["settings"]);
    report += jsonSection("Plugin identity (full serialized state remains in manifest.json)", candidate);
    report += jsonSection("Render diagnostics", m["candidateRender"]);
    report += "\n## Artifacts and use for plugin development\n\n- stimulus.wav: exact test input.\n- "
              "reference-raw.wav: unaligned hardware capture.\n- reference.wav: latency-corrected hardware "
              "signal.\n- candidate.wav: latency-corrected plugin signal (left channel).\n- residual.wav: "
              "plugin minus hardware.\n- manifest.json: reproducibility settings, plugin serialized state, "
              "routing, physical settings and calibration.\n- analysis_summary.json: exact metrics and "
              "spectrum bins; output spectra are not a measured transfer function.\n- operator-review.json, "
              "when present: later corrections; these override original requested-state claims.\n\nNo plugin "
              "binary is bundled. Reproduction requires the matching installed binary, not just its name. "
              "Separate level/alignment errors from spectral/nonlinear hypotheses. Do not fit excluded "
              "captures or infer missing switch states. Treat historical references as potentially drifted "
              "when no new reference was captured. Specify further measurements needed before changing DSP, "
              "and validate any proposed change with independent captures.\n";
    require(folder.getChildFile("report.md").replaceWithText(report),
            "Cannot write detailed measurement report");
}
juce::Array<juce::File> collectionResults(const juce::File &project, const juce::var &workflow)
{
    juce::StringArray paths, visited;
    std::function<void(const juce::var &)> visit = [&](const juce::var &w)
    {
        auto add = [&](const juce::var &values)
        {
            if (auto *obj = values.getDynamicObject())
                for (const auto &p : obj->getProperties())
                    if (juce::File::isAbsolutePath(p.value.toString()))
                        paths.addIfNotAlreadyThere(p.value.toString());
        };
        add(w["scenarioResults"]);
        if (auto *history = w["scenarioHistory"].getArray())
            for (const auto &revision : *history)
                add(revision["results"]);
        for (auto key : {"result2", "result3"})
            if (juce::File::isAbsolutePath(w[key].toString()))
                paths.addIfNotAlreadyThere(w[key].toString());
        if (auto *sources = w["collectionSources"].getArray())
            for (const auto &source : *sources)
            {
                auto id = source.toString();
                require(
                    id.isNotEmpty() &&
                        id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"),
                    "Invalid collection source");
                if (visited.contains(id))
                    continue;
                visited.add(id);
                visit(readJson(project.getChildFile("workflows").getChildFile(id + ".json")));
            }
    };
    if (workflow["workflowId"].toString().isNotEmpty())
        visited.add(workflow["workflowId"].toString());
    visit(workflow);
    juce::Array<juce::File> results;
    for (const auto &path : paths)
    {
        juce::File file(path);
        // Only explicit exclusions may be silently omitted. Missing data is an error.
        if (!GuidedWorkflow::resultUsable(file))
            continue;
        SessionStore::load(file);
        for (auto *name : {"reference.wav", "candidate.wav", "residual.wav", "analysis_summary.json"})
            require(file.getChildFile(name).existsAsFile(),
                    "Incomplete collection measurement: " + file.getFileName());
        results.add(file);
    }
    std::sort(results.begin(), results.end(),
              [](const auto &a, const auto &b) { return a.getFileName() < b.getFileName(); });
    return results;
}
juce::File exportMeasurementHandoff(const juce::File &project, const juce::var &workflow)
{
    auto sources = collectionResults(project, workflow);
    require(!sources.isEmpty(), "Nincs exportálható, érvényes mérés");
    auto exports = project.getChildFile("ATADAS");
    require(exports.createDirectory().wasOk(), "Cannot create export folder");
    auto id = juce::Uuid().toString();
    auto staging = exports.getChildFile(".pending-" + id);
    auto zipTemp = exports.getChildFile(".pending-" + id + ".zip");
    require(staging.createDirectory().wasOk(), "Cannot create export staging folder");
    struct Cleanup
    {
        juce::File folder, zip;
        ~Cleanup()
        {
            folder.deleteRecursively();
            zip.deleteFile();
        }
    } cleanup{staging, zipTemp};
    juce::Array<juce::var> records;
    juce::StringArray calibrations;
    auto manifest = object();
    set(manifest, "schemaVersion", 2);
    set(manifest, "kind", "moth-consolidated-measurement-dataset");
    set(manifest, "createdUtc", utcNow());
    set(manifest, "name", workflow["name"]);
    set(manifest, "includedCaptures", sources.size());
    set(manifest, "excludedCaptures", 0);
    set(manifest, "ordering",
        "Chronological acquisition order; not a claim of uninterrupted acquisition or calibrated "
        "comparability");
    juce::String listing = "| ID | Mérés | Riport |\n|---|---|---|\n";
    int number = 0;
    for (const auto &source : sources)
    {
        auto m = SessionStore::load(source);
        auto hw = m["hardware"];
        auto label = hw["scenarioStep"]["title"].toString();
        if (label.isEmpty())
            label = source.getFileName();
        auto ordinal = juce::String(++number).paddedLeft('0', 2);
        auto relative = "meresek/" + ordinal + "-" + juce::File::createLegalFileName(label).substring(0, 70);
        auto dest = staging.getChildFile(relative);
        require(source.copyDirectoryTo(dest), "Cannot copy measurement");
        writeHardwareReport(dest);
        auto record = object();
        set(record, "id", ordinal);
        set(record, "title", label);
        set(record, "folder", relative);
        set(record, "availability", "included");
        set(record, "requestedSettingsEvidence", true);
        set(record, "step", hw["scenarioStep"]);
        set(record, "sourceStepId", hw["scenarioStep"]["id"]);
        set(record, "workflowId", hw["workflowId"]);
        set(record, "revision", hw["scenarioRevision"]);
        set(record, "capturedUtc", m["createdUtc"]);
        set(record, "captureOnly", hw["captureOnly"]);
        set(record, "deviceSetup", hw["deviceSetup"]);
        auto checkPath = m["bypassCheckFolder"].toString();
        if (checkPath.isNotEmpty())
        {
            require(juce::File::isAbsolutePath(checkPath) && juce::File(checkPath).isDirectory(),
                    "Missing baseline capture: " + checkPath);
            int index = calibrations.indexOf(checkPath);
            if (index < 0)
            {
                calibrations.add(checkPath);
                index = calibrations.size() - 1;
            }
            auto checkRelative = "referenciak/B" + juce::String(index + 1).paddedLeft('0', 2);
            auto check = staging.getChildFile(checkRelative);
            if (!check.exists())
                require(juce::File(checkPath).copyDirectoryTo(check), "Cannot copy baseline");
            set(record, "calibrationFolder", checkRelative);
        }
        records.add(record);
        listing += "| " + ordinal + " | " + label.replaceCharacter('|', '/') + " | [Riport](<" + relative +
                   "/report.md>) |\n";
    }
    // Preserve explicit historical comparison references in the new ID space.
    for (auto &record : records)
    {
        auto wid = record["workflowId"].toString();
        auto sourceJournal = project.getChildFile("workflows").getChildFile(wid + ".json");
        if (wid.isEmpty() || !sourceJournal.existsAsFile())
            continue;
        auto w = readJson(sourceJournal);
        auto old =
            w["profileProvenance"]["referenceSteps"][juce::Identifier(record["sourceStepId"].toString())]
                .toString();
        if (old.isEmpty())
            continue;
        auto referenceWorkflow = w["profileProvenance"]["sourceWorkflow"].toString();
        juce::String reference;
        for (const auto &candidate : records)
            if (candidate["workflowId"].toString() == referenceWorkflow &&
                candidate["sourceStepId"].toString() == old)
                reference = candidate["id"].toString();
        set(record, "referenceMeasurementId", reference);
        if (reference.isEmpty())
            set(record, "referenceWarning", "Historical reference is not included or was excluded");
    }
    set(manifest, "measurements", juce::var(records));
    auto plan = object();
    set(plan, "name", workflow["name"]);
    set(plan, "measurements", juce::var(records));
    set(plan, "note",
        "Included captures only. Source revisions are provenance, not additional tasks. Inspect each "
        "capture's quality before DSP fitting.");
    writeJson(staging.getChildFile("meresi-terv.json"), plan);
    // Device initialization evidence stays with the package; historical source paths are not needed.
    auto device = workflow["deviceSetup"].clone();
    writeJson(staging.getChildFile("device.json"), device.isObject() ? device : object());
    if (auto *photos = workflow["panelImages"].getArray())
        for (int i = 0; i < photos->size(); ++i)
        {
            auto source = juce::File((*photos)[i]["path"].toString());
            require(source.existsAsFile(), "Panel photo missing");
            auto dest = staging.getChildFile("frontpanel-" + juce::String(i + 1) + ".png");
            require(source.copyFileTo(dest), "Cannot copy front panel photo");
        }
    auto readme =
        juce::String::fromUTF8("# Egyesített mérési csomag\n\n**A mellette található, azonos nevű ZIP az "
                               "egyetlen átadandó csomag.**\n\n") +
        juce::String(sources.size()) +
        juce::String::fromUTF8(" számozott mérés. A kizárt felvételek nincsenek a csomagban.\n\n") + listing +
        juce::String::fromUTF8(
            "\nA relatív útvonalakat az index.json tartalmazza. Az eredeti manifestek abszolút útvonalai "
            "történeti adatok; ne futtasd változtatás nélkül a régi worker-job fájlokat. "
            "A külön sorozatokból származó felvételek időbélyegei és referencia-kapcsolatai megmaradtak. Az "
            "összevonás nem állít megszakítás nélküli mérést vagy driftmentességet. "
            "A 2,6 s halk zajpróba nem teljes karakterizálás, THD vagy abszolút szintkalibráció.\n\n"
            "## Codex / pluginprojekt\n\nOlvasd el az index.json-t és az egyes report.md fájlokat. Az actual "
            "physical settings, route, clipping, alignment és baseline adatok alapján értékeld a "
            "bizonyítékot. "
            "A captureOnly felvételnél a candidate.wav a rögzített alapreferencia, nem pluginrender. "
            "Ilyenkor ne értelmezd a maradékot pluginhibának. "
            "Az included státusz nem minőségi garancia. A felvételekhez kötött DSP-hipotéziseket független "
            "mérésekkel ellenőrizd. "
            "A plugin forráskódját és binárisát a célprojekt biztosítja.\n");
    require(staging.getChildFile("README.md").replaceWithText(readme), "Cannot write collection index");
    auto checksums = object();
    for (const auto &f : staging.findChildFiles(juce::File::findFiles, true))
    {
        juce::FileInputStream in(f);
        set(checksums, juce::Identifier(f.getRelativePathFrom(staging).replaceCharacter('\\', '/')),
            juce::SHA256(in).toHexString());
    }
    set(manifest, "sha256", checksums);
    writeJson(staging.getChildFile("index.json"), manifest);
    auto title = juce::File::createLegalFileName(workflow["name"].toString()).substring(0, 60);
    if (title.isEmpty())
        title = "Egyesitett-meres";
    auto final =
        exports.getChildFile(title + "-" + utcNow().removeCharacters(":-") + "-" + id.substring(0, 8));
    juce::ZipFile::Builder builder;
    for (const auto &f : staging.findChildFiles(juce::File::findFiles, true))
        builder.addFile(
            f, 6, final.getFileName() + "/" + f.getRelativePathFrom(staging).replaceCharacter('\\', '/'));
    {
        auto output = zipTemp.createOutputStream();
        require(output && builder.writeToStream(*output, nullptr), "Cannot create handoff ZIP");
    }
    juce::ZipFile verify(zipTemp);
    require(verify.getNumEntries() == staging.findChildFiles(juce::File::findFiles, true).size(),
            "Incomplete handoff ZIP");
    require(staging.moveFileTo(final), "Cannot publish handoff");
    require(zipTemp.moveFileTo(final.getSiblingFile(final.getFileName() + ".zip")), "Cannot publish ZIP");
    exports.getChildFile("LATEST.txt")
        .replaceWithText(final.getSiblingFile(final.getFileName() + ".zip").getFileName() + "\n");
    return final;
}

} // namespace lab
