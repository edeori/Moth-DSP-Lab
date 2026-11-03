#include "measurement/GuidedWorkflow.h"
#include <stdexcept>
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
juce::File fakeResult(const juce::File &root, int mode, const juce::String &name,
                      const juce::String &workflowId = {})
{
    auto folder = root.getChildFile("sessions").getChildFile(name);
    folder.createDirectory();
    auto m = lab::object(), ref = lab::object(), hw = lab::object();
    lab::set(ref, "kind", "hardware");
    lab::set(hw, "measurementMode", mode);
    lab::set(hw, "workflowId", workflowId);
    lab::set(m, "schemaVersion", 1);
    lab::set(m, "status", "complete");
    lab::set(m, "reference", ref);
    lab::set(m, "hardware", hw);
    lab::set(m, "bypassCheckFolder", root.getChildFile("setup-checks/bypass").getFullPathName());
    lab::writeJson(folder.getChildFile("manifest.json"), m);
    lab::writeJson(folder.getChildFile("analysis_summary.json"), lab::object());
    for (const auto *name : {"reference.wav", "candidate.wav", "residual.wav"})
        folder.getChildFile(name).replaceWithText("fixture");
    return folder;
}
} // namespace
void runWorkflowTests()
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("workflow-test-" + juce::Uuid().toString());
    struct Cleanup
    {
        juce::File f;
        ~Cleanup()
        {
            f.deleteRecursively();
        }
    } cleanup{root};
    lab::GuidedWorkflow w(root);
    w.create();
    auto first = w.file;
    check(w.nextMeasurement() == 2, "new workflow must start with clean after bypass");
    lab::set(w.state, "notes3", "Master 9h; distortion on");
    lab::set(w.state, "phase", "capturing");
    w.save();
    lab::GuidedWorkflow resumed(root);
    resumed.load(first);
    check(resumed.state["phase"].toString() == "interrupted", "capture must not auto-resume");
    check(resumed.state["notes3"].toString() == "Master 9h; distortion on", "notes lost");
    auto clean = fakeResult(root, 2, "001-clean", first.getFileNameWithoutExtension());
    resumed.recordResult(2, clean);
    check(resumed.nextMeasurement() == 3, "clean must not imply drive complete");
    resumed.create();
    check(resumed.file != first && first.existsAsFile(), "new workflow overwrote old progress");
    resumed.load(first);
    check(resumed.hasResult(2), "completed clean lost on reopen");
    lab::set(resumed.state, "phase", "processing");
    resumed.save();
    auto drive = fakeResult(root, 3, "002-drive", first.getFileNameWithoutExtension());
    // Simulate a crash after the atomic result commit but before journal update.
    resumed.load(first);
    check(resumed.hasResult(3) && resumed.nextMeasurement() == 0,
          "orphan committed result was not recovered");
    check(resumed.state["phase"].toString() == "complete", "recovered series not complete");
    drive.getChildFile("candidate.wav").deleteFile();
    check(resumed.nextMeasurement() == 3, "missing artifact must not be shown as complete");
    auto before = resumed.history().size();
    resumed.importLegacySessions();
    check(resumed.history().size() == before, "modern sessions duplicated as legacy workflows");
    auto legacyRoot = root.getChildFile("legacy");
    fakeResult(legacyRoot, 2, "clean");
    fakeResult(legacyRoot, 3, "drive");
    lab::GuidedWorkflow legacy(legacyRoot);
    legacy.importLegacySessions();
    check(legacy.history().size() == 1 && legacy.nextMeasurement() == 0,
          "legacy pair must be grouped by bypass");
    legacy.importLegacySessions();
    check(legacy.history().size() == 1, "legacy import not idempotent");
    resumed.load(first);
    lab::set(resumed.state, "name", "Bench A");
    lab::set(resumed.state, "device", "Remembered interface");
    resumed.save();
    const auto originalText = first.loadFileAsString();
    resumed.duplicateSetup();
    check(resumed.file != first && first.loadFileAsString() == originalText,
          "repeat changed source workflow");
    check(resumed.state["device"].toString() == "Remembered interface", "repeat lost interface selection");
    check(resumed.state["notes3"].toString() == "Master 9h; distortion on", "repeat lost notes");
    check(!resumed.hasResult(1) && !resumed.hasResult(2) && !resumed.hasResult(3),
          "repeat copied result or bypass authorization");
    check(resumed.state["name"].toString().startsWith("Bench A"), "repeat lost human-readable name");
    // Nested result discovery must resume committed captures, never export copies.
    lab::set(resumed.state, "storageRun",
             lab::SessionStore::runPath(resumed.state, resumed.file.getFileNameWithoutExtension()));
    resumed.save();
    auto run = lab::SessionStore::runFolder(root, resumed.file.getFileNameWithoutExtension());
    lab::SessionStore store(root);
    auto pending = store.beginSession(resumed.file.getFileNameWithoutExtension());
    auto source = fakeResult(root, 2, "nested-source", resumed.file.getFileNameWithoutExtension());
    check(source.copyDirectoryTo(pending), "cannot seed nested result");
    source.deleteRecursively();
    auto committed = store.commit(pending);
    check(committed.getParentDirectory() == run.getChildFile("results"), "wrong grouped result destination");
    check(lab::SessionStore::projectForSession(committed) == root, "nested project root lost");
    auto exportCopy = run.getChildFile("exports/snapshot/measurements/copied");
    check(committed.copyDirectoryTo(exportCopy), "cannot seed export snapshot");
    auto discovered = lab::SessionStore::sessions(root);
    check(discovered.contains(committed) && !discovered.contains(exportCopy),
          "discovery mixed exports and measurements");
    auto stable = resumed.state["storageRun"].toString();
    lab::set(resumed.state, "name", "Renamed run");
    resumed.save();
    check(lab::SessionStore::runPath(resumed.state, resumed.file.getFileNameWithoutExtension()) == stable,
          "renaming moved existing recording paths");
    lab::GuidedWorkflow recovered(root);
    recovered.load(resumed.file);
    check(recovered.result(2) == committed, "nested committed result was not recovered");
    auto corrupt = root.getChildFile("workflows/corrupt.json");
    corrupt.replaceWithText("broken");
    bool rejected = false;
    try
    {
        resumed.load(corrupt);
    }
    catch (...)
    {
        rejected = true;
    }
    check(rejected && corrupt.loadFileAsString() == "broken", "corrupt workflow silently replaced");
    auto blocked = root.getChildFile("blocked");
    blocked.replaceWithText("file, not directory");
    rejected = false;
    try
    {
        lab::GuidedWorkflow denied(blocked);
        denied.create();
    }
    catch (...)
    {
        rejected = true;
    }
    check(rejected, "save failure not surfaced");
}
