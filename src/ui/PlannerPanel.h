#pragma once
#include "LabTheme.h"
#include "ai-bridge/DeviceProfile.h"
#include "ai-bridge/Scenario.h"
#include "measurement/GuidedWorkflow.h"
#include <future>
namespace lab
{
class PlannerPanel final : public juce::Component, private juce::Timer
{
  public:
    PlannerPanel(GuidedWorkflow &w, std::function<void()> close, std::function<void()> approved)
        : workflow(w), closePanel(std::move(close)), onApproved(std::move(approved))
    {
        for (auto *c :
             std::vector<juce::Component *>{&heading,      &profile,        &goal,         &preview,
                                            &status,       &consent,        &generate,     &approve,
                                            &back,         &cancel,         &view,         &rawData,
                                            &profileLabel, &goalLabel,      &previewLabel, &savedProfiles,
                                            &saveProfile,  &openProfiles,   &deviceType,   &addPhoto,
                                            &clearPhotos,  &recognize,      &controls,     &controlsConfirmed,
                                            &routeNotes,   &routeConfirmed, &photoStatus,  &photoPreview,
                                            &controlsLabel})
            addAndMakeVisible(c);
        heading.setText(juce::String::fromUTF8("Mérési terv készítése"), juce::dontSendNotification);
        profileLabel.setText(juce::String::fromUTF8("Eszközleírás / modell (opcionális)"),
                             juce::dontSendNotification);
        goalLabel.setText(juce::String::fromUTF8("Mérési cél"), juce::dontSendNotification);
        previewLabel.setText(juce::String::fromUTF8("Terv és következő teendő"), juce::dontSendNotification);
        approve.setComponentID("approve-plan");
        generate.setComponentID("generate-plan");
        back.setComponentID("close-planner");
        preview.setComponentID("plan-preview");
        savedProfiles.setComponentID("saved-profiles");
        saveProfile.setComponentID("save-profile");
        savedProfiles.setTextWhenNothingSelected(juce::String::fromUTF8("Mentett mérési profil betöltése…"));
        savedProfiles.onChange = [this]
        {
            auto index = savedProfiles.getSelectedId() - 1;
            if (!juce::isPositiveAndBelow(index, profileFiles.size()))
                return;
            try
            {
                auto loaded = GuidedWorkflow::loadProfile(profileFiles[index]);
                auto before = workflow.state.clone();
                set(workflow.state, "aiProfile", loaded["deviceProfile"]);
                set(workflow.state, "aiGoal", loaded["goal"]);
                legacyProfileSelected = !loaded["deviceSetup"].isObject();
                if (loaded["deviceSetup"].isObject())
                {
                    auto setup = loaded["deviceSetup"].clone();
                    set(setup, "routeConfirmed", false);
                    set(workflow.state, "deviceSetup", setup);
                    workflow.save();
                    deviceType.setSelectedId(setup["type"].toString() == "head"       ? 2
                                             : setup["type"].toString() == "outboard" ? 3
                                                                                      : 1,
                                             juce::dontSendNotification);
                    controls.setText(setup["controlsText"].toString(), false);
                    controlsConfirmed.setToggleState(bool(setup["controlsConfirmed"]),
                                                     juce::dontSendNotification);
                    routeNotes.setText(setup["routeNotes"].toString(), false);
                    routeConfirmed.setToggleState(false, juce::dontSendNotification);
                    set(workflow.state, "panelImages", loaded["panelImages"].clone());
                }
                if (legacyProfileSelected)
                {
                    set(workflow.state, "deviceSetup", juce::var());
                    set(workflow.state, "panelImages", juce::var());
                    deviceType.setSelectedId(0, juce::dontSendNotification);
                    controls.setText("", false);
                    routeNotes.setText("", false);
                }
                set(workflow.state, "aiDraft", loaded["scenario"].clone());
                try
                {
                    workflow.save();
                }
                catch (...)
                {
                    workflow.state = before;
                    throw;
                }
                profile.setText(loaded["deviceProfile"].toString(), false);
                goal.setText(loaded["goal"].toString(), false);
                draft = loaded["scenario"].clone();
                showingPlan = true;
                consent.setToggleState(false, juce::dontSendNotification);
                preview.setText(describeScenario(draft));
                refresh();
                status.setText(
                    juce::String::fromUTF8(
                        "Profil betöltve átnézésre. A jelenlegi mérési sor jóváhagyásig változatlan."),
                    juce::dontSendNotification);
            }
            catch (const std::exception &e)
            {
                status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
            }
        };
        saveProfile.onClick = [this]
        {
            try
            {
                auto saved = workflow.saveProfile(draft.isObject() ? draft : workflow.state["scenario"],
                                                  profile.getText(), goal.getText());
                refreshProfiles();
                status.setText(juce::String::fromUTF8("Profil mentve: ") + saved.getFileName(),
                               juce::dontSendNotification);
            }
            catch (const std::exception &e)
            {
                status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
            }
        };
        openProfiles.onClick = [this]
        {
            auto folder = workflow.profilesFolder();
            if (folder.createDirectory().wasOk())
                folder.startAsProcess();
        };
        deviceType.addItem(juce::String::fromUTF8("Pedál"), 1);
        deviceType.addItem(juce::String::fromUTF8("Erősítőfej"), 2);
        deviceType.addItem(juce::String::fromUTF8("Egyéb outboard"), 3);
        deviceType.setTextWhenNothingSelected(juce::String::fromUTF8("Eszköztípus kiválasztása…"));
        auto setup = workflow.state["deviceSetup"];
        deviceType.setSelectedId(setup["type"].toString() == "head"       ? 2
                                 : setup["type"].toString() == "outboard" ? 3
                                 : setup["type"].toString() == "pedal"    ? 1
                                                                          : 0,
                                 juce::dontSendNotification);
        deviceType.setComponentID("device-type");
        controlsLabel.setText(juce::String::fromUTF8("Kezelőszervek — soronként javítható"),
                              juce::dontSendNotification);
        controls.setFont(juce::FontOptions(15.f));
        controls.setComponentID("device-controls");
        controls.setMultiLine(true);
        controls.setScrollbarsShown(true);
        controls.setTextToShowWhenEmpty(
            juce::String::fromUTF8("Kezelőszervek: soronként egy. Pl. CH1 Gain | poti 0–10"),
            LabTheme::muted());
        controls.setText(setup["controlsText"].toString(), false);
        controls.setInputRestrictions(16000);
        routeNotes.setComponentID("device-route");
        routeNotes.setInputRestrictions(2000);
        routeNotes.setText(setup["routeNotes"].toString(), false);
        routeNotes.setTextToShowWhenEmpty(
            juce::String::fromUTF8("Jelút / visszatérés, típusok és ellenőrzött szintek, terhelés"),
            LabTheme::muted());
        controlsConfirmed.setComponentID("controls-confirmed");
        controlsConfirmed.setToggleState(bool(setup["controlsConfirmed"]), juce::dontSendNotification);
        routeConfirmed.setToggleState(bool(setup["routeConfirmed"]), juce::dontSendNotification);
        addPhoto.setComponentID("add-panel-photo");
        recognize.setComponentID("recognize-panel");
        auto invalidateDevice = [this]
        {
            consent.setToggleState(false, juce::dontSendNotification);
            draft = juce::var();
            set(workflow.state, "aiDraft", juce::var());
            persistDevice();
            showPayload();
            refresh();
            try
            {
                workflow.save();
            }
            catch (const std::exception &e)
            {
                status.setText(e.what(), juce::dontSendNotification);
            }
        };
        deviceType.onChange = [this, invalidateDevice]
        {
            legacyProfileSelected = false;
            controlsConfirmed.setToggleState(false, juce::dontSendNotification);
            routeConfirmed.setToggleState(false, juce::dontSendNotification);
            profile.setText("", false);
            routeNotes.setText(
                deviceType.getSelectedId() == 2
                    ? juce::String::fromUTF8("Hangkártya → reamp → fej INPUT; hangszórókimenet → megfelelő "
                                             "loadbox; loadbox LINE OUT → hangkártya LINE IN")
                : deviceType.getSelectedId() == 1
                    ? juce::String::fromUTF8("Hangkártya → reamp → pedál INPUT; pedál OUTPUT → megfelelő "
                                             "szintre állított hangkártya bemenet")
                    : juce::String::fromUTF8(
                          "Hangkártya LINE OUT → outboard LINE IN; outboard LINE OUT → hangkártya LINE IN"),
                false);
            goal.setText(juce::String::fromUTF8("Kezelőszervek hatásának elkülönítése, alacsony szintű "
                                                "alapreferenciával és ismétlésekkel."),
                         false);
            invalidateDevice();
        };
        controls.onTextChange = [this, invalidateDevice]
        {
            controlsConfirmed.setToggleState(false, juce::dontSendNotification);
            invalidateDevice();
        };
        routeNotes.onTextChange = [this, invalidateDevice]
        {
            routeConfirmed.setToggleState(false, juce::dontSendNotification);
            invalidateDevice();
        };
        controlsConfirmed.onClick = routeConfirmed.onClick = [this]
        {
            consent.setToggleState(false, juce::dontSendNotification);
            persistDevice();
            refresh();
            try
            {
                workflow.save();
            }
            catch (const std::exception &e)
            {
                status.setText(e.what(), juce::dontSendNotification);
            }
        };
        clearPhotos.onClick = [this, invalidateDevice]
        {
            set(workflow.state, "panelImages", juce::var(juce::Array<juce::var>{}));
            controlsConfirmed.setToggleState(false, juce::dontSendNotification);
            invalidateDevice();
        };
        addPhoto.onClick = [this]
        {
            photoChooser = std::make_unique<juce::FileChooser>(
                juce::String::fromUTF8("Frontpanel fotó — JPEG / PNG (telefonról is bemásolható)"),
                juce::File{}, "*.jpg;*.jpeg;*.png");
            auto safe = juce::Component::SafePointer<PlannerPanel>(this);
            photoChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser &chooser)
                {
                    if (!safe || chooser.getResult() == juce::File{})
                        return;
                    try
                    {
                        auto photos = safe->workflow.state["panelImages"].clone();
                        if (!photos.isArray())
                            photos = juce::var(juce::Array<juce::var>{});
                        require(photos.size() < 4, "Legfeljebb 4 fotó csatolható");
                        auto file =
                            importPanelPhoto(safe->workflow.file.getParentDirectory().getParentDirectory(),
                                             chooser.getResult());
                        auto entry = object();
                        set(entry, "path", file.getFullPathName());
                        juce::FileInputStream in(file);
                        set(entry, "sha256", juce::SHA256(in).toHexString());
                        photos.append(entry);
                        set(safe->workflow.state, "panelImages", photos);
                        safe->draft = juce::var();
                        set(safe->workflow.state, "aiDraft", juce::var());
                        safe->consent.setToggleState(false, juce::dontSendNotification);
                        safe->controlsConfirmed.setToggleState(false, juce::dontSendNotification);
                        safe->persistDevice();
                        safe->workflow.save();
                        safe->showPayload();
                        safe->refresh();
                    }
                    catch (const std::exception &e)
                    {
                        safe->status.setText(e.what(), juce::dontSendNotification);
                    }
                });
        };
        recognize.onClick = [this]
        {
            try
            {
                require(consent.getToggleState(), "A képküldést engedélyezni kell");
                persistDevice();
                workflow.save();
                auto context = object();
                set(context, "task", "recognize_panel");
                set(context, "panelImages", workflow.state["panelImages"].clone());
                set(context, "deviceType", workflow.state["deviceSetup"]["type"]);
                require(context["panelImages"].size() > 0, "Előbb csatolj frontpanel fotót");
                cancelled = false;
                busy = recognizing = true;
                started = juce::Time::getMillisecondCounterHiRes();
                future = std::async(std::launch::async,
                                    [context, this] { return runCodexPlanner(context, cancelled); });
                refresh();
                startTimer(150);
            }
            catch (const std::exception &e)
            {
                status.setText(e.what(), juce::dontSendNotification);
            }
        };
        refreshProfiles();
        heading.setFont(juce::FontOptions(22.f));
        for (auto *e : {&profile, &goal, &preview})
        {
            e->setMultiLine(true);
            e->setScrollbarsShown(true);
            e->setFont(juce::FontOptions(15.f));
        }
        preview.setReadOnly(true);
        profile.setText(
            w.state["aiProfile"].toString().isNotEmpty() ? w.state["aiProfile"].toString()
            : (deviceType.getSelectedId() > 0 || bool(workflow.state["initializationRequired"]))
                ? juce::String{}
                : juce::String::fromUTF8(
                      "Darkglass Microtubes B7K Ultra → DarkBass összehasonlítás.\nForrás: "
                      "https://www.darkglass.com/en-gb/pages/microtubes-b7k-ultra-manual\nBass 100 Hz ±12 "
                      "dB; Lo Mid 250/500/1000 Hz; Hi Mid 750/1500/3000 Hz; Treble 5 kHz ±12 dB. Blend "
                      "tiszta/torzított keverés; Drive telítés; Level torzított ág; Master teljes kimenet; "
                      "Attack magas, Grunt mély előkiemelés. Preamp és distortion külön kapcsolható.\nJelút: "
                      "Scarlett → DACCAPO → pedál jack → Scarlett. Gain és routing a bypass után "
                      "változatlan. Ellenőrizd a pedál pontos változatát és ezt a profilt."));
        goal.setText(
            w.state["aiGoal"].toString().isNotEmpty() ? w.state["aiGoal"].toString()
            : (deviceType.getSelectedId() > 0 || bool(workflow.state["initializationRequired"]))
                ? juce::String::fromUTF8(
                      "Kezelőszervek hatásának elkülönítése, alapreferencia és ismétlések.")
                : juce::String::fromUTF8("Fokozatos, megismételhető B7K / DarkBass összehasonlítás: EQ, "
                                         "Drive, Blend és kapcsolók hatása. A már elkészült mérésekből "
                                         "indulj; jelezd, ami a jelenlegi próbajellel nem dönthető el."));
        draft = w.state["aiDraft"].clone();
        try
        {
            if (draft.isObject())
            {
                showingPlan = true;
                preview.setText(describeScenario(draft));
            }
        }
        catch (...)
        {
            draft = juce::var();
        }
        if (!draft.isObject() && workflow.hasScenario())
        {
            showingPlan = true;
            preview.setText(describeScenario(workflow.state["scenario"]) +
                            juce::String::fromUTF8("\n\nAktív terv • elkészült lépések: ") +
                            juce::String(workflow.scenarioCompleted()));
        }
        if (!showingPlan)
            showPayload();
        profile.setInputRestrictions(6000);
        goal.setInputRestrictions(2000);
        profile.onTextChange = goal.onTextChange = [this]
        {
            consent.setToggleState(false, juce::dontSendNotification);
            draft = juce::var();
            set(workflow.state, "aiDraft", juce::var());
            showPayload();
            refresh();
        };
        consent.onClick = [this]
        {
            showPayload();
            refresh();
        };
        rawData.setComponentID("planner-raw-data");
        rawData.onClick = [this]
        {
            showingPlan = false;
            preview.setText(juce::JSON::toString(makeContext()));
            refresh();
        };
        view.setComponentID("planner-summary");
        view.onClick = [this]
        {
            if ((draft.isObject() || workflow.hasScenario()) && !showingPlan)
            {
                preview.setText(describeScenario(draft.isObject() ? draft : workflow.state["scenario"]));
                showingPlan = true;
            }
            else
                showPayload();
            refresh();
        };
        generate.onClick = [this]
        {
            try
            {
                require(consent.getToggleState(), "Adatküldés nincs engedélyezve");
                require(deviceType.getSelectedId() > 0 || legacyProfileSelected ||
                            !bool(workflow.state["initializationRequired"]),
                        "Előbb válassz eszköztípust");
                persistDevice();
                set(workflow.state, "aiProfile", profile.getText());
                set(workflow.state, "aiGoal", goal.getText());
                workflow.save();
                auto context = makeContext();
                if (deviceType.getSelectedId() > 0)
                    context = devicePlannerContext(context, workflow.state["deviceSetup"]);
                set(workflow.state, "aiLastRequest", context);
                workflow.save();
                cancelled = false;
                busy = true;
                draft = juce::var();
                started = juce::Time::getMillisecondCounterHiRes();
                preview.setText(juce::String::fromUTF8(
                    "Mérési terv készül. Várj az eredményre; utána átnézheted és jóváhagyhatod."));
                future = std::async(std::launch::async,
                                    [context, this] { return CodexAdvisor(cancelled).propose(context); });
                refresh();
                startTimer(150);
            }
            catch (const std::exception &e)
            {
                status.setText(e.what(), juce::dontSendNotification);
            }
        };
        cancel.onClick = [this]
        {
            cancelled = true;
            status.setText(juce::String::fromUTF8("Megszakítás…"), juce::dontSendNotification);
        };
        approve.onClick = [this]
        {
            try
            {
                require(deviceType.getSelectedId() > 0 || legacyProfileSelected ||
                            !bool(workflow.state["initializationRequired"]),
                        "Előbb válassz eszköztípust");
                validateScenario(draft);
                if (deviceType.getSelectedId() > 0)
                {
                    devicePlannerContext(makeContext(), workflow.state["deviceSetup"]);
                    validateDevicePlan(draft, workflow.state["deviceSetup"]);
                }
                auto revisions = workflow.state["scenarioHistory"].clone();
                if (!revisions.isArray())
                    revisions = juce::var(juce::Array<juce::var>{});
                if (workflow.state["scenario"].isObject())
                {
                    auto old = object();
                    set(old, "plan", workflow.state["scenario"].clone());
                    set(old, "results", workflow.state["scenarioResults"].clone());
                    set(old, "revision", workflow.state["scenarioRevision"]);
                    set(old, "notes", workflow.state["scenarioNotes"].clone());
                    revisions.append(old);
                }
                auto before = workflow.state.clone();
                set(workflow.state, "aiDraft", juce::var());
                set(workflow.state, "scenarioHistory", revisions);
                set(workflow.state, "scenario", draft.clone());
                set(workflow.state, "initializationRequired", false);
                set(workflow.state, "approvedDeviceSetup", workflow.state["deviceSetup"].clone());
                set(workflow.state, "storageRun", juce::var());
                set(workflow.state, "scenarioRevision", juce::Uuid().toString());
                set(workflow.state, "scenarioApprovedUtc", utcNow());
                set(workflow.state, "scenarioResults", object());
                set(workflow.state, "scenarioNotes", object());
                set(workflow.state, "phase", "idle");
                try
                {
                    workflow.save();
                }
                catch (...)
                {
                    workflow.state = before;
                    throw;
                }
                onApproved();
            }
            catch (const std::exception &e)
            {
                status.setText(e.what(), juce::dontSendNotification);
            }
        };
        back.onClick = [this]
        {
            if (!busy)
            {
                try
                {
                    persistDevice();
                    set(workflow.state, "aiProfile", profile.getText());
                    set(workflow.state, "aiGoal", goal.getText());
                    workflow.save();
                    closePanel();
                }
                catch (const std::exception &e)
                {
                    status.setText(e.what(), juce::dontSendNotification);
                }
            }
        };
        refresh();
    }
    ~PlannerPanel() override
    {
        cancelled = true;
        if (future.valid())
            future.wait();
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(LabTheme::background());
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced(20);
        auto row = a.removeFromTop(40);
        back.setBounds(row.removeFromRight(180));
        rawData.setBounds(row.removeFromRight(170));
        view.setBounds(row.removeFromRight(150));
        heading.setBounds(row);
        row = a.removeFromTop(38);
        openProfiles.setBounds(row.removeFromRight(140).reduced(2));
        saveProfile.setBounds(row.removeFromRight(230).reduced(2));
        savedProfiles.setBounds(row.reduced(2));
        row = a.removeFromTop(36);
        deviceType.setBounds(row.removeFromLeft(210).reduced(2));
        addPhoto.setBounds(row.removeFromLeft(160).reduced(2));
        clearPhotos.setBounds(row.removeFromLeft(110).reduced(2));
        recognize.setBounds(row.removeFromLeft(210).reduced(2));
        photoStatus.setBounds(row);
        routeNotes.setBounds(a.removeFromTop(32).reduced(2));
        routeConfirmed.setBounds(a.removeFromTop(30));
        status.setBounds(a.removeFromBottom(44));
        row = a.removeFromBottom(40);
        approve.setBounds(row.removeFromRight(260).reduced(2));
        cancel.setBounds(row.removeFromRight(110).reduced(2));
        generate.setBounds(row.reduced(2));
        consent.setBounds(a.removeFromBottom(40));
        auto l = a.removeFromLeft(a.getWidth() / 3);
        a.removeFromLeft(14);
        profileLabel.setBounds(l.removeFromTop(24));
        profile.setBounds(l.removeFromTop(juce::jmax(45, l.getHeight() / 4)).reduced(2));
        controlsConfirmed.setBounds(l.removeFromBottom(40));
        auto inventory = l.removeFromBottom(juce::jmax(90, l.getHeight() / 2));
        controlsLabel.setBounds(inventory.removeFromTop(24));
        controls.setBounds(inventory.reduced(2));
        goalLabel.setBounds(l.removeFromTop(24));
        goal.setBounds(l.reduced(2));
        if (photoPreview.getImage().isValid())
            photoPreview.setBounds(a.removeFromTop(120).reduced(2));
        else
            photoPreview.setBounds({});
        previewLabel.setBounds(a.removeFromTop(24));
        preview.setBounds(a.reduced(2));
    }

  private:
    GuidedWorkflow &workflow;
    std::function<void()> closePanel, onApproved;
    juce::ComboBox deviceType;
    juce::TextButton addPhoto{juce::String::fromUTF8("Frontpanel fotó…")},
        clearPhotos{juce::String::fromUTF8("Fotók levétele")},
        recognize{juce::String::fromUTF8("Kezelőszervek felismerése")};
    juce::TextEditor controls, routeNotes;
    juce::ToggleButton controlsConfirmed{juce::String::fromUTF8("A kezelőszervek listáját ellenőriztem")},
        routeConfirmed{juce::String::fromUTF8("A visszatérő jel szintjét, és fejnél a loadbox "
                                              "terhelését/kimenetét a kézikönyv alapján ellenőriztem")};
    juce::Label photoStatus, controlsLabel;
    juce::ImageComponent photoPreview;
    std::unique_ptr<juce::FileChooser> photoChooser;
    bool recognizing = false, legacyProfileSelected = false;
    void persistDevice()
    {
        if (deviceType.getSelectedId() == 0)
            return;
        auto setup = object();
        set(setup, "type",
            deviceType.getSelectedId() == 2   ? "head"
            : deviceType.getSelectedId() == 3 ? "outboard"
                                              : "pedal");
        set(setup, "controlsText", controls.getText());
        set(setup, "controlsConfirmed", controlsConfirmed.getToggleState());
        set(setup, "routeNotes", routeNotes.getText());
        set(setup, "routeConfirmed", routeConfirmed.getToggleState());
        set(workflow.state, "deviceSetup", setup);
    }
    juce::Label heading, status, profileLabel, goalLabel, previewLabel;
    juce::TextEditor profile, goal, preview;
    juce::ToggleButton consent{juce::String::fromUTF8(
        "A megjelenített adatokat ÉS a csatolt frontpanel fotókat elküldheti a Codexnek")};
    juce::TextButton generate{juce::String::fromUTF8("Terv készítése / újratervezés")},
        approve{juce::String::fromUTF8("Átnéztem — terv jóváhagyása")},
        back{juce::String::fromUTF8("Vissza a méréshez")}, cancel{juce::String::fromUTF8("Megszakítás")},
        view{juce::String::fromUTF8("Terv / összefoglaló")},
        rawData{juce::String::fromUTF8("Küldendő adatok")};
    juce::ComboBox savedProfiles;
    juce::TextButton saveProfile{juce::String::fromUTF8("Terv mentése profilként")},
        openProfiles{juce::String::fromUTF8("Profilok mappája")};
    juce::Array<juce::File> profileFiles;
    void refreshProfiles()
    {
        savedProfiles.clear(juce::dontSendNotification);
        profileFiles.clear();
        for (const auto &file : workflow.profiles())
            try
            {
                auto p = GuidedWorkflow::loadProfile(file);
                profileFiles.add(file);
                savedProfiles.addItem(
                    p["name"].toString() + juce::String::fromUTF8(" • ") +
                        juce::Time::fromISO8601(p["createdUtc"].toString()).formatted("%m.%d. %H:%M"),
                    profileFiles.size());
            }
            catch (...)
            {
            }
        openProfiles.setTooltip(workflow.profilesFolder().getFullPathName());
    }
    juce::var draft;
    bool busy = false, showingPlan = false;
    double started = 0;
    std::atomic_bool cancelled{false};
    std::future<juce::var> future;
    juce::var makeContext()
    {
        juce::Array<juce::var> evidence;
        juce::StringArray included;
        auto add = [&evidence, &included](juce::String path)
        {
            if (!juce::File::isAbsolutePath(path) || included.contains(path))
                return;
            try
            {
                auto f = juce::File(path);
                auto m = readJson(f.getChildFile("manifest.json"));
                auto e = object();
                set(e, "settings", m["hardware"]["operatorPhysicalSettings"]);
                set(e, "step", m["hardware"]["scenarioStep"]);
                set(e, "review", GuidedWorkflow::resultReview(f));
                if (GuidedWorkflow::resultUsable(f))
                    set(e, "analysis", readJson(f.getChildFile("analysis_summary.json")));
                evidence.add(e);
                included.add(path);
            }
            catch (...)
            {
            }
        };
        for (auto k : {"result2", "result3"})
            add(workflow.state[k].toString());
        if (auto *results = workflow.state["scenarioResults"].getDynamicObject())
            for (const auto &item : results->getProperties())
                add(item.value.toString());
        if (auto *history = workflow.state["scenarioHistory"].getArray())
            for (const auto &revision : *history)
                if (auto *results = revision["results"].getDynamicObject())
                    for (const auto &item : results->getProperties())
                        add(item.value.toString());
        auto context = plannerContext(profile.getText(), goal.getText(), juce::var(evidence));
        set(context, "previousPlan", workflow.state["scenario"].clone());
        set(context, "panelImages", workflow.state["panelImages"].clone());
        if (deviceType.getSelectedId() > 0)
        {
            set(context, "deviceSetup", workflow.state["deviceSetup"].clone());
            try
            {
                context = devicePlannerContext(context, workflow.state["deviceSetup"]);
            }
            catch (...)
            {
                set(context, "capabilities",
                    juce::String::fromUTF8("Modellfüggetlen hardverfelvétel a saját alapreferenciához. "
                                           "Tervkérés előtt ellenőrizd a kezelőszerveket és a jelutat. Nincs "
                                           "DarkBass-render vagy fotóból számított szintkalibráció."));
                set(context, "coverageRequirements",
                    juce::String::fromUTF8("A jóváhagyott kezelőszervlista minden elemére kérünk tervezett "
                                           "mérést vagy indokolt halasztást."));
            }
        }
        if (deviceType.getSelectedId() == 0 && bool(workflow.state["initializationRequired"]) &&
            !legacyProfileSelected)
        {
            set(context, "capabilities",
                juce::String::fromUTF8(
                    "Válassz eszköztípust: fej, pedál vagy egyéb outboard. Még nincs küldhető tervkérés."));
            set(context, "coverageRequirements", "Pending confirmed inventory");
        }
        return compactPlannerContext(context);
    }
    void showPayload()
    {
        showingPlan = false;
        auto context = makeContext();
        juce::String text = juce::String::fromUTF8("AZ AI EZEKET KAPJA MEG\n");
        if (profile.getText().trim().isNotEmpty())
            text += "\n" + profile.getText().trim() + "\n";
        if (deviceType.getSelectedId() > 0)
            text += juce::String::fromUTF8("\nEszköz: ") + deviceType.getText() +
                    "\nJelút: " + routeNotes.getText() + "\n";
        text += juce::String::fromUTF8("\nCél: ") + goal.getText() + "\n";
        if (controls.getText().trim().isNotEmpty())
            text += juce::String::fromUTF8("\nKezelőszervek\n") + controls.getText() + "\n";
        text +=
            "\n" + juce::String(workflow.state["panelImages"].size()) + juce::String::fromUTF8(" fotó; ") +
            juce::String(context["evidence"].size()) +
            juce::String::fromUTF8(
                " mérés összesítője; a korábbi terv, ha van.\nNyers hangot és forráskódot nem küldünk.\n\n") +
            juce::String::fromUTF8("A fotófelismerés kezelőszervlistát készít. A tervkészítés az ellenőrzött "
                                   "listából indul. Egyik sem indít mérést.");
        preview.setText(text);
    }
    void refresh()
    {
        const bool generic = deviceType.getSelectedId() > 0;
        const bool selectedType =
            generic || legacyProfileSelected || !bool(workflow.state["initializationRequired"]);
        generate.setEnabled(
            selectedType && !busy && consent.getToggleState() &&
            (!generic || (controlsConfirmed.getToggleState() && routeConfirmed.getToggleState() &&
                          controls.getText().isNotEmpty() && routeNotes.getText().length() >= 10)));
        recognize.setEnabled(!busy && generic && consent.getToggleState() &&
                             workflow.state["panelImages"].size() > 0);
        deviceType.setEnabled(!busy && !workflow.hasScenario());
        for (auto *c : std::vector<juce::Component *>{&addPhoto, &clearPhotos, &controls, &controlsConfirmed,
                                                      &routeNotes, &routeConfirmed})
            c->setEnabled(!busy && generic);
        auto photos = workflow.state["panelImages"];
        photoPreview.setImage(photos.size() > 0 ? juce::ImageFileFormat::loadFrom(juce::File(
                                                      photos[photos.size() - 1]["path"].toString()))
                                                : juce::Image{});
        photoPreview.setTooltip(juce::String::fromUTF8(
            "Legutóbb csatolt frontpanel fotó; az engedély az összes csatolt fotóra vonatkozik."));
        resized();
        photoStatus.setText(juce::String(workflow.state["panelImages"].size()) +
                                juce::String::fromUTF8(" fotó"),
                            juce::dontSendNotification);
        approve.setEnabled(
            selectedType && !busy && draft.isObject() && showingPlan &&
            (!generic || (controlsConfirmed.getToggleState() && routeConfirmed.getToggleState())));
        savedProfiles.setEnabled(!busy);
        saveProfile.setEnabled(!busy && (draft.isObject() || workflow.hasScenario()));
        openProfiles.setEnabled(!busy);
        view.setEnabled(!busy);
        rawData.setEnabled(!busy);
        cancel.setEnabled(busy);
        back.setEnabled(!busy);
        profile.setEnabled(!busy);
        goal.setEnabled(!busy);
        consent.setEnabled(!busy);
        if (!busy)
        {
            juce::String next;
            if (!selectedType)
                next = juce::String::fromUTF8("Válassz eszköztípust.");
            else if (generic && controls.getText().trim().isEmpty())
                next =
                    workflow.state["panelImages"].size() == 0
                        ? juce::String::fromUTF8("Tölts be frontpanel-fotót, vagy írd be a kezelőszerveket.")
                    : !consent.getToggleState()
                        ? juce::String::fromUTF8("Engedélyezd a fotó küldését, majd indítsd a felismerést.")
                        : juce::String::fromUTF8("Nyomd meg: Kezelőszervek felismerése.");
            else if (generic && !controlsConfirmed.getToggleState())
                next = juce::String::fromUTF8("Javítsd és igazold vissza a kezelőszervlistát.");
            else if (generic && (!routeConfirmed.getToggleState() || routeNotes.getText().length() < 10))
                next = juce::String::fromUTF8(
                    "Ellenőrizd a jelutat és a megfelelő szinteket, majd jelöld be a visszaigazolást.");
            else if (draft.isObject())
                next =
                    showingPlan
                        ? juce::String::fromUTF8("Olvasd át a tervet, majd hagyd jóvá.")
                        : juce::String::fromUTF8("A jóváhagyáshoz nyisd meg a tervet: Terv / összefoglaló.");
            else if (!consent.getToggleState())
                next = juce::String::fromUTF8("Engedélyezd az adatok küldését a tervkészítéshez.");
            else
                next = juce::String::fromUTF8("Nyomd meg: Terv készítése / újratervezés.");
            status.setText(next, juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        if (!busy)
            return;
        if (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            status.setText(
                (recognizing ? juce::String::fromUTF8("Fotó feldolgozása… ")
                             : juce::String::fromUTF8("Mérési terv készül… ")) +
                    juce::String(int((juce::Time::getMillisecondCounterHiRes() - started) / 1000)) +
                    " s / legfeljebb " + juce::String(plannerTimeoutMs / 1000) + " s",
                juce::dontSendNotification);
            return;
        }
        busy = false;
        stopTimer();
        try
        {
            auto answer = future.get();
            if (recognizing)
            {
                recognizing = false;
                controls.setText(answer["controlsText"].toString(), false);
                profile.setText(answer["summary"].toString(), false);
                controlsConfirmed.setToggleState(false, juce::dontSendNotification);
                consent.setToggleState(false, juce::dontSendNotification);
                set(workflow.state, "aiDraft", juce::var());
                set(workflow.state, "panelRecognition", answer.clone());
                persistDevice();
                workflow.save();
                draft = juce::var();
                showingPlan = false;
                refresh();
                preview.setText(juce::String::fromUTF8("FELISMERÉS KÉSZ\nBizonytalan részletek:\n") +
                                answer["uncertainties"].toString());
                return;
            }
            draft = answer;
            set(workflow.state, "aiDraft", draft.clone());
            workflow.save();
            preview.setText(describeScenario(draft));
            showingPlan = true;
            refresh();
        }
        catch (const std::exception &e)
        {
            recognizing = false;
            draft = juce::var();
            refresh();
            status.setText(e.what(), juce::dontSendNotification);
            showPayload();
        }
    }
};
} // namespace lab
