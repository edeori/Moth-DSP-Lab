#include "ui/HardwareSetupComponent.h"
#include <iostream>
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
template <typename T> T &control(juce::Component &parent, const char *id)
{
    for (auto *child : parent.getChildren())
        if (child->getComponentID() == id)
            if (auto *typed = dynamic_cast<T *>(child))
                return *typed;
    throw std::runtime_error(std::string("Missing control: ") + id);
}
juce::File result(const juce::File &root, int mode)
{
    auto folder = root.getChildFile("sessions").getChildFile(juce::String(mode));
    folder.createDirectory();
    auto m = lab::object(), hw = lab::object(), ref = lab::object();
    lab::set(m, "schemaVersion", 1);
    lab::set(m, "status", "complete");
    lab::set(m, "createdUtc", lab::utcNow());
    lab::set(hw, "measurementMode", mode);
    lab::set(ref, "kind", "hardware");
    lab::set(m, "hardware", hw);
    lab::set(m, "reference", ref);
    lab::writeJson(folder.getChildFile("manifest.json"), m);
    lab::writeJson(folder.getChildFile("analysis_summary.json"), lab::object());
    for (auto *name : {"reference.wav", "candidate.wav", "residual.wav"})
    {
        lab::AudioData audio;
        audio.samples.setSize(1, 512);
        audio.samples.clear();
        lab::writeWav(folder.getChildFile(name), audio);
    }
    return folder;
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("workflow-ui-" + juce::Uuid().toString());
    struct Cleanup
    {
        juce::File f;
        ~Cleanup()
        {
            f.deleteRecursively();
        }
    } cleanup{root};
    try
    {
        lab::GuidedWorkflow w(root);
        w.create();
        auto original = w.file;
        w.recordResult(2, result(root, 2));
        lab::set(w.state, "phase", "capturing");
        lab::set(w.state, "mode", 3);
        lab::set(w.state, "notes3", "Retained drive notes");
        w.save();
        {
            lab::HardwareSetupComponent panel(root, [](juce::File) {});
            auto &start = control<juce::TextButton>(panel, "capture-start");
            auto &mode = control<juce::ComboBox>(panel, "measurement-mode");
            check(!start.isEnabled(), "restoring must never authorize capture");
            check(mode.getSelectedId() == 1 && !mode.isItemEnabled(2) && !mode.isItemEnabled(3),
                  "fresh bypass required on resume");
            check(control<juce::ComboBox>(panel, "result-history").getNumItems() == 1,
                  "clean result not available on resume");
            check(!control<juce::TextButton>(panel, "advance-measurement").isVisible(),
                  "clean incorrectly marked as fully complete");
            {
                auto snapshot = panel.createComponentSnapshot(panel.getLocalBounds());
                auto out = juce::File("/tmp/moth-workflow-resume.png").createOutputStream();
                out->setPosition(0);
                out->truncate();
                juce::PNGImageFormat{}.writeImageToStream(snapshot, *out);
            }
            // Programmatic selection tests note restoration; disabled UI cannot make this transition.
            mode.setSelectedId(3, juce::sendNotificationSync);
            panel.setSize(1048, 746); // Content area at the main window's minimum supported size.
            for (auto *child : panel.getChildren())
                if (child->isVisible())
                    check(panel.getLocalBounds().contains(child->getBounds()),
                          "visible control clipped at minimum window size");
            {
                auto snapshot = panel.createComponentSnapshot(panel.getLocalBounds());
                auto out = juce::File("/tmp/moth-workflow-active.png").createOutputStream();
                out->setPosition(0);
                out->truncate();
                juce::PNGImageFormat{}.writeImageToStream(snapshot, *out);
            }
            auto &notes = control<juce::TextEditor>(panel, "physical-notes");
            check(notes.getText() == "Retained drive notes", "saved notes not restored per measurement");
            notes.setText("Edited notes", false);
            notes.onTextChange();
            check(!start.isEnabled(), "notes must not bypass live routing authorization");
        }
        w.load(original);
        check(w.state["notes3"].toString() == "Edited notes", "notes lost on panel close");
        w.recordResult(3, result(root, 3));
        {
            lab::HardwareSetupComponent panel(root, [](juce::File) {});
            check(!control<juce::TextButton>(panel, "capture-start").isVisible(),
                  "completed series still asks to capture");
            check(control<lab::Plot>(panel, "inline-result-plot").isVisible(),
                  "completed graph must appear inline automatically");
            check(!control<lab::Plot>(panel, "inline-result-plot").isOnDesktop(),
                  "result plot opened another window");
            auto &selectedResult = control<juce::ComboBox>(panel, "result-history");
            selectedResult.setSelectedId(2, juce::sendNotificationSync);
            check(control<juce::Label>(panel, "result-summary").getText().contains("EQ"),
                  "inline graph selector did not update result summary");
            check(control<juce::ComboBox>(panel, "result-history").getNumItems() == 2,
                  "both results must be accessible");
            check(control<juce::Label>(panel, "next-action")
                      .getText()
                      .contains(juce::String::fromUTF8("MINDKÉT MÉRÉS KÉSZ")),
                  "completion has no clear next action");
            control<juce::TextButton>(panel, "new-workflow").onClick();
            check(!control<juce::TextButton>(panel, "capture-start").isVisible(),
                  "capture controls must stay hidden during wiring guide");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(), "new workflow auto-armed");
            {
                auto snapshot = panel.createComponentSnapshot(panel.getLocalBounds());
                auto out = juce::File("/tmp/moth-workflow-new.png").createOutputStream();
                out->setPosition(0);
                out->truncate();
                juce::PNGImageFormat{}.writeImageToStream(snapshot, *out);
            }
            check(control<juce::ComboBox>(panel, "result-history").getNumItems() == 0,
                  "new workflow inherited results");
        }
        check(w.history().size() == 2, "new workflow did not preserve original");
        w.load(original);
        lab::set(w.state, "name", "Reusable bench");
        w.save();
        {
            lab::HardwareSetupComponent panel(root, [](juce::File) {});
            check(control<juce::TextEditor>(panel, "workflow-name").getText() == "Reusable bench",
                  "workflow name not restored");
            control<juce::TextButton>(panel, "duplicate-setup").onClick();
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(), "repeat armed audio");
            check(control<juce::ComboBox>(panel, "result-history").getNumItems() == 0,
                  "repeat inherited completed results");
            check(control<juce::TextEditor>(panel, "workflow-name").getText().contains("Reusable bench"),
                  "repeat name not visible");
        }
        w.create();
        auto captured = root.getChildFile("setup-checks/interrupted-render");
        captured.createDirectory();
        captured.getChildFile("probe.wav").replaceWithText("invalid WAV fixture");
        captured.getChildFile("capture.wav").replaceWithText("preserved capture fixture");
        auto job = lab::object(), metadata = lab::object();
        lab::set(metadata, "measurementMode", 3);
        lab::set(job, "metadata", metadata);
        lab::writeJson(captured.getChildFile("recovery-job.json"), job);
        lab::set(w.state, "phase", "processing");
        lab::set(w.state, "captureFolder", captured.getFullPathName());
        w.save();
        {
            lab::HardwareSetupComponent panel(root, [](juce::File) {});
            auto &retry = control<juce::TextButton>(panel, "retry-render");
            check(retry.isVisible() && retry.isEnabled(), "preserved capture retry not offered");
            retry.onClick();
            for (int i = 0; i < 100 && !retry.isEnabled(); ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
            check(retry.isEnabled(), "failed offline retry did not return to actionable state");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(),
                  "offline retry armed hardware");
            check(captured.getChildFile("capture.wav").loadFileAsString() == "preserved capture fixture",
                  "retry destroyed raw capture");
        }
        {
            w.create();
            auto plan = juce::JSON::parse(
                R"({"action":"request_measurement","title":"Test EQ plan","reason":"Isolate EQ","completion":"Repeatable results","limitations":"No THD","steps":[{"id":"eq1","title":"Bass boost","mode":2,"settings":"Bass 3h, other EQ noon, mid 500/1500, Blend max, Drive 9h, Level noon, Master 9h, Attack/Grunt flat","reason":"EQ response","expected":"More bass","stimulus":"guarded_noise_2.6s_-36dBFS","parameters":[{"name":"Bass","value":0.75}]}]})");
            lab::set(w.state, "aiDraft", plan);
            w.save();
            // Select this fixture deterministically, even when filesystem millisecond timestamps tie.
            w.file.setLastModificationTime(juce::Time::getCurrentTime() + juce::RelativeTime::seconds(1));
            lab::HardwareSetupComponent panel(root);
            panel.setSize(1048, 746);
            control<juce::TextButton>(panel, "ai-planner").onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            auto &planner = control<lab::PlannerPanel>(panel, "planner-panel");
            check(!control<juce::TextButton>(planner, "generate-plan").isEnabled(), "AI sent without opt-in");
            check(control<juce::TextButton>(planner, "approve-plan").isEnabled(),
                  "valid draft cannot be reviewed");
            control<juce::TextButton>(planner, "save-profile").onClick();
            check(w.profiles().size() == 1, "UI profile save failed");
            auto &profiles = control<juce::ComboBox>(planner, "saved-profiles");
            profiles.setSelectedId(1, juce::dontSendNotification);
            profiles.onChange();
            lab::GuidedWorkflow beforeApproval(root);
            beforeApproval.load(w.file);
            check(!beforeApproval.hasScenario(), "loading a profile approved it automatically");
            check(control<juce::TextEditor>(planner, "plan-preview").getText().contains("Bass boost"),
                  "plan not visible");
            for (auto *c : planner.getChildren())
                check(planner.getLocalBounds().contains(c->getBounds()), "planner control outside window");
            auto image = panel.createComponentSnapshot(panel.getLocalBounds());
            auto output = juce::File("/tmp/moth-ai-planner-ui.png").createOutputStream();
            output->setPosition(0);
            output->truncate();
            juce::PNGImageFormat{}.writeImageToStream(image, *output);
            control<juce::TextButton>(planner, "approve-plan").onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            lab::GuidedWorkflow saved(root);
            saved.load(w.file);
            check(saved.hasScenario() && saved.nextScenarioStep()["id"].toString() == "eq1",
                  "approved plan not saved");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(), "approval armed hardware");
            check(control<juce::ComboBox>(panel, "measurement-mode").getSelectedId() == 1,
                  "approval skipped fresh bypass");
            // Exercise step selection without arming audio; plugin preparation remains gated by bypass.
            control<juce::TextButton>(panel, "advance-measurement").onClick();
            const auto instruction = control<juce::TextEditor>(panel, "measurement-instructions").getText();
            check(instruction.contains("Bass 3h"), "scenario instructions replaced by fixed baseline");
            check(instruction.contains(juce::String::fromUTF8("PRÓBAJEL:")), "stimulus explanation missing");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(),
                  "instruction preview armed capture");

            control<juce::TextButton>(panel, "ai-planner").onClick();
            auto &reviewed = control<lab::PlannerPanel>(panel, "planner-panel");
            check(!control<juce::TextButton>(reviewed, "approve-plan").isEnabled(),
                  "reopening active plan can erase progress");
            control<juce::TextButton>(reviewed, "close-planner").onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        }
        {
            w.create();
            lab::set(w.state, "initializationRequired", true);
            w.save();
            w.file.setLastModificationTime(juce::Time::getCurrentTime() + juce::RelativeTime::seconds(2));
            lab::HardwareSetupComponent panel(root);
            panel.setSize(1048, 746);
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(),
                  "new device emitted audio before initialization");
            control<juce::TextButton>(panel, "ai-planner").onClick();
            auto &planner = control<lab::PlannerPanel>(panel, "planner-panel");
            auto &type = control<juce::ComboBox>(planner, "device-type");
            type.setSelectedId(2, juce::sendNotificationSync);
            auto &controls = control<juce::TextEditor>(planner, "device-controls");
            controls.setText("Gain | 0-10\nBass | 0-10", false);
            controls.onTextChange();
            check(!control<juce::ToggleButton>(planner, "controls-confirmed").getToggleState(),
                  "edited inventory stayed confirmed");
            check(control<juce::TextEditor>(planner, "device-route").getText().contains("loadbox"),
                  "head default route missing loadbox");
            check(!control<juce::TextButton>(planner, "generate-plan").isEnabled(),
                  "unreviewed device can generate plan");
            check(!control<juce::TextEditor>(planner, "plan-preview").getText().contains("\"capabilities\""),
                  "technical payload shown as default instructions");
            control<juce::TextButton>(planner, "planner-raw-data").onClick();
            check(control<juce::TextEditor>(planner, "plan-preview").getText().contains("\"capabilities\""),
                  "full payload no longer reviewable");
            control<juce::TextButton>(planner, "planner-summary").onClick();

            for (auto *child : planner.getChildren())
                check(planner.getLocalBounds().contains(child->getBounds()), "device planner outside window");
            auto image = panel.createComponentSnapshot(panel.getLocalBounds());
            {
                auto output = juce::File("/tmp/moth-device-init-ui.png").createOutputStream();
                output->setPosition(0);
                output->truncate();
                juce::PNGImageFormat{}.writeImageToStream(image, *output);
            }
            control<juce::TextButton>(planner, "close-planner").onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            lab::GuidedWorkflow restored(root);
            restored.load(w.file);
            check(restored.state["deviceSetup"]["type"].toString() == "head", "device type not durable");
            check(restored.state["deviceSetup"]["controlsText"].toString().contains("Gain"),
                  "inventory not durable");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(),
                  "unapproved head auto-armed");
            control<juce::TextButton>(panel, "merge-runs").onClick();
            auto &collection = control<lab::CollectionPanel>(panel, "collection-panel");
            for (auto *child : collection.getChildren())
                check(collection.getLocalBounds().contains(child->getBounds()), "collection outside window");
        }
        {
            w.load(w.file);
            lab::set(w.state, "collectionSources",
                     juce::var(juce::Array<juce::var>{original.getFileNameWithoutExtension()}));
            w.save();
            lab::CollectionPanel panel(w, [] {});
            panel.setSize(1048, 746);
            check(control<juce::ListBox>(panel, "collection-sources").getNumSelectedRows() == 1,
                  "saved collection selection lost on reopen");
            control<juce::TextButton>(panel, "save-collection").onClick();
            check(w.state["collectionSources"].size() == 1, "saving unchanged collection removed sources");
        }
        {
            w.create();
            auto device = juce::JSON::parse(
                R"({"type":"head","controlsText":"Gain | 0-10\nBass | 0-10","controlsConfirmed":true,"routeNotes":"Head speaker -> compatible loadbox -> LINE OUT -> interface LINE IN","routeConfirmed":true})");
            auto plan = juce::JSON::parse(
                R"({"title":"Head plan","reason":"Control test","completion":"Review recorded evidence","limitations":"Low-level noise only","calibration":"Keep the rated load connected; set Gain 1 and Bass 5; use the loadbox LINE OUT with fixed level.","action":"request_measurement","steps":[{"id":"gain","title":"Gain 2","mode":2,"settings":"Gain 2; Bass 5; output fixed","reason":"Gain response","expected":"No clipping","stimulus":"guarded_noise_2.6s_-36dBFS","parameters":[]}],"coverage":[{"control":"Gain","status":"planned","stepIds":["gain"],"states":["1","2"],"reason":"Vary gain"},{"control":"Bass","status":"deferred","stepIds":[],"states":[],"reason":"Later EQ test"}]})");
            lab::set(w.state, "initializationRequired", true);
            lab::set(w.state, "deviceSetup", device);
            lab::set(w.state, "aiDraft", plan);
            w.save();
            w.file.setLastModificationTime(juce::Time::getCurrentTime() + juce::RelativeTime::seconds(3));
            lab::HardwareSetupComponent panel(root);
            panel.setSize(1048, 746);
            control<juce::TextButton>(panel, "ai-planner").onClick();
            auto &planner = control<lab::PlannerPanel>(panel, "planner-panel");
            check(control<juce::TextButton>(planner, "approve-plan").isEnabled(),
                  "reviewed generic draft cannot be approved");
            check(control<juce::TextEditor>(planner, "plan-preview").getText().contains("loadbox LINE OUT"),
                  "baseline plan hidden from review");
            control<juce::TextButton>(planner, "approve-plan").onClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            lab::GuidedWorkflow approved(root);
            approved.load(w.file);
            check(approved.hasScenario() && !bool(approved.state["initializationRequired"]),
                  "generic approval did not persist");
            check(!control<juce::TextButton>(panel, "capture-start").isEnabled(),
                  "approval bypassed physical confirmations");
            auto text = control<juce::TextEditor>(panel, "measurement-instructions").getText();
            check(text.contains("loadbox LINE OUT") && !text.contains("DarkBass"),
                  "head baseline uses pedal instructions");
        }
        std::cout
            << "PASS workflow UI resume, notes, gates, results, new series and failed offline recovery\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
