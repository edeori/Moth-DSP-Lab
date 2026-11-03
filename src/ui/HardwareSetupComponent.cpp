#include "HardwareSetupComponent.h"
#include "AudioPermission.h"
namespace lab
{
namespace
{
const juce::StringArray steps{
    juce::String::fromUTF8(
        "ELŐKÉSZÍTÉS • 1/4\n\nScarlett → DACCAPO → B7K Ultra → Scarlett.\n\nVálaszd le a monitorhangfalakat. "
        "Kapcsold ki a Direct Monitor és a DAW bemeneti monitorozását; a Solo fejhallgató-kimenetén se "
        "hallgasd a tesztet.\n\nÚj bekötésnél alacsony Scarlett MONITOR- és DACCAPO LEVEL-állással kezdj. "
        "Folytatáskor tartsd meg a feljegyzett szinteket. A pedálhoz megfelelő tápegységet használj."),
    juce::String::fromUTF8(
        "KIMENET • 2/4\n\nScarlett LEFT / Output 1 → TRS jack–XLR apa kábel → DACCAPO BALANCED LINE "
        "INPUT.\nDACCAPO ISOLATED OUTPUT → TS hangszerkábel → B7K INPUT.\n\nJegyezd fel a DACCAPO "
        "szintkapcsolóját és LEVEL-állását. Új bekötéshez: +4 dBu, alacsony LEVEL. A program nem emeli "
        "automatikusan a hangerőt."),
    juce::String::fromUTF8(
        "VISSZATÉRÉS • 3/4\n\nB7K jack OUTPUT → TS kábel → Scarlett jack bemenet. A Solo 2./3. generáción ez "
        "az Input 2. Az alkalmazásban ezt a fizikai csatornát válaszd, ne az XLR mikrofonbemenetet.\n\nÚj "
        "bekötés: LINE (INST KI), alacsony gain. Ellenőrzött jelútnál tartsd meg a rögzített INST/gain "
        "állást. Szintet csak leállított tesztnél módosíts.\n\nEbben a jelútban nincs XLR Direct Out vagy "
        "kabinet-szimuláció."),
    juce::String::fromUTF8(
        "JELÚT-ELLENŐRZÉS • 4/4\n\nPedál teljes BYPASS: preamp/EQ és torzítás KIKAPCSOLVA.\nKorábbi bevált "
        "szintek: Input 2 INST, gain kb. 12 óra; MONITOR közép; DACCAPO LEVEL kb. 9 óra, GND.\n\nVálaszd ki "
        "a hangkártyát és a csatornákat, majd igazold a három feltételt.\n\nA PROGRAM\n2,6 s alatt "
        "zajszintet és két halk próbát mér (−36 dBFS csúcs). Túl magas bemeneti szintnél leáll; a "
        "VÉSZLEÁLLÍTÁS bármikor megszakítja.\n\nA siker stabil visszatérő jelet jelent, nem abszolút "
        "szintkalibrációt. Bekötés vagy szint változtatása után ismételd meg.")};

juce::String failure(SetupProbeRunner::State state)
{
    using State = SetupProbeRunner::State;
    if (state == State::overload)
        return juce::String::fromUTF8(
            "Túl magas bemeneti csúcs: a kimenet leállt. Csökkentsd a szinteket, majd tesztelj újra.");
    if (state == State::loudBaseline)
        return juce::String::fromUTF8(
            "Már a csendes szakaszban magas a bemeneti szint. Ellenőrizd a monitorozást, zajt és gain "
            "beállítást.");
    if (state == State::invalidInput)
        return juce::String::fromUTF8("Érvénytelen audioadat érkezett; a kimenet leállt.");
    if (state == State::missingChannel)
        return juce::String::fromUTF8("A kiválasztott csatorna nem elérhető.");
    return juce::String::fromUTF8("A teszt megszakadt. Nincs érvényes setup-eredmény.");
}
} // namespace
HardwareSetupComponent::HardwareSetupComponent(juce::File project, std::function<void(juce::File)> openResult)
    : workflow(project), projectRoot(project == juce::File{} ? GuidedWorkflow::defaultProject() : project),
      openComparison(std::move(openResult))
{
    setLookAndFeel(&theme);
    setSize(1208, 806);
    heading.setText(juce::String::fromUTF8("Vezetett mérés — B7K Ultra / DarkBass"),
                    juce::dontSendNotification);
    heading.setFont(juce::FontOptions(24.f));
    route.setText(juce::String::fromUTF8("Jelút: driver → hangkártya → visszatérő bemenet → kiküldő kimenet"),
                  juce::dontSendNotification);
    for (auto *c : std::vector<juce::Component *>{&mergeRuns,
                                                  &excludeResult,
                                                  &openRunFolder,
                                                  &exportHandoff,
                                                  &planButton,
                                                  &duplicate,
                                                  &sessionName,
                                                  &libraryLabel,
                                                  &notesLabel,
                                                  &repeatBypass,
                                                  &resultPlot,
                                                  &details,
                                                  &difference,
                                                  &taskTitle,
                                                  &feedbackTitle,
                                                  &resultSummary,
                                                  &inputLevel,
                                                  &captureBar,
                                                  &heading,
                                                  &nextAction,
                                                  &advance,
                                                  &retryRender,
                                                  &workflowHistory,
                                                  &resultHistory,
                                                  &newWorkflow,
                                                  &stageOverview,
                                                  &savedStatus,
                                                  &tutorial,
                                                  &results,
                                                  &route,
                                                  &progress,
                                                  &driver,
                                                  &interface,
                                                  &input,
                                                  &output,
                                                  &previous,
                                                  &next,
                                                  &rescan,
                                                  &start,
                                                  &stop,
                                                  &showReport,
                                                  &wiring,
                                                  &monitoring,
                                                  &levels,
                                                  &measurementMode,
                                                  &preparePlugin,
                                                  &physicalNotes,
                                                  &chainUnchanged,
                                                  &openComparisonButton})
        addAndMakeVisible(c);
    for (auto *editor : {&tutorial, &results})
    {
        editor->setMultiLine(true);
        editor->setReadOnly(true);
        editor->setScrollbarsShown(true);
        editor->setFont(juce::FontOptions(15.f));
    }
    libraryLabel.setText(juce::String::fromUTF8("MÉRÉSI SOROZAT"), juce::dontSendNotification);
    libraryLabel.setFont(juce::FontOptions(11.f));
    libraryLabel.setColour(juce::Label::textColourId, LabTheme::muted());
    notesLabel.setText(juce::String::fromUTF8("Eltérések / megjegyzés (nem kötelező)"),
                       juce::dontSendNotification);
    notesLabel.setFont(juce::FontOptions(12.f));
    notesLabel.setColour(juce::Label::textColourId, LabTheme::muted());
    sessionName.setInputRestrictions(80);
    sessionName.setTextToShowWhenEmpty(juce::String::fromUTF8("Adj nevet: pl. B7K alapbeállítás"),
                                       LabTheme::muted());
    sessionName.setTitle(juce::String::fromUTF8("A mérési sorozat neve"));
    sessionName.setComponentID("workflow-name");
    sessionName.onTextChange = [this]
    {
        saveProgress();
        if (!restoring)
            refreshHistory();
    };
    sessionName.onReturnKey = [this]
    {
        refreshHistory();
        sessionName.giveAwayKeyboardFocus();
    };
    duplicate.setComponentID("duplicate-setup");
    duplicate.onClick = [this]
    {
        if (isWorking())
            return;
        try
        {
            saveProgress();
            workflow.duplicateSetup();
            restoreWorkflow(workflow.file);
            refreshHistory();
        }
        catch (const std::exception &e)
        {
            results.setText(juce::String::fromUTF8(e.what()));
        }
    };
    for (auto *button : {&start, &next, &advance, &preparePlugin, &retryRender})
        LabTheme::primary(*button);
    stop.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa83242));
    for (auto *editor : {&tutorial, &results})
    {
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, LabTheme::muted());
        editor->setIndents(2, 6);
    }
    taskTitle.setFont(juce::FontOptions(20.f));
    feedbackTitle.setFont(juce::FontOptions(20.f));
    feedbackTitle.setText(juce::String::fromUTF8("Mérési visszajelzés"), juce::dontSendNotification);
    resultSummary.setFont(juce::FontOptions(16.f));
    repeatBypass.onClick = [this]
    {
        wiring.setToggleState(false, juce::dontSendNotification);
        monitoring.setToggleState(false, juce::dontSendNotification);
        measurementMode.setSelectedId(1, juce::dontSendNotification);
        step = 3;
        modeChanged();
    };
    results.onTextChange = [this]
    {
        if (results.getText() != displayedResultText)
            details.setToggleState(true, juce::dontSendNotification);
        resized();
    };
    details.onClick = [this] { resized(); };
    difference.onClick = [this] { resultPlot.setDifference(difference.getToggleState()); };
    resultPlot.setComponentID("inline-result-plot");
    captureBar.setComponentID("capture-progress");
    resultSummary.setComponentID("result-summary");
    taskTitle.setComponentID("current-task");
    newWorkflow.setButtonText(juce::String::fromUTF8("Új mérés"));
    wiring.setButtonText(juce::String::fromUTF8("Bekötés és csatornák ellenőrizve"));
    monitoring.setButtonText(juce::String::fromUTF8("Hangfalak leválasztva; minden monitorozás KI"));
    chainUnchanged.setButtonText(juce::String::fromUTF8("Scarlett / DACCAPO beállításai változatlanok"));
    physicalNotes.setTextToShowWhenEmpty(
        juce::String::fromUTF8("Üresen hagyva: pontosan a kért pedálállások."), juce::Colours::grey);
    captureBar.setTextToDisplay(juce::String::fromUTF8("Nincs aktív felvétel"));
    int focusOrder = 1;
    for (auto *control : std::vector<juce::Component *>{
             &sessionName,   &workflowHistory, &newWorkflow,    &tutorial,      &previous, &next,
             &driver,        &interface,       &input,          &output,        &wiring,   &monitoring,
             &preparePlugin, &physicalNotes,   &chainUnchanged, &levels,        &start,    &stop,
             &advance,       &retryRender,     &repeatBypass,   &resultHistory, &details,  &difference,
             &showReport,    &duplicate})
        control->setExplicitFocusOrder(focusOrder++);
    duplicate.setTooltip(
        juce::String::fromUTF8("Új sorozat a kiválasztott hangkártyával és a korábbi jegyzetekkel. Friss "
                               "bypass-próba szükséges; a régi eredmények megmaradnak."));
    newWorkflow.setTooltip(
        juce::String::fromUTF8("Új, üres mérési sorozat indítása a bekötési útmutatótól."));
    workflowHistory.setTooltip(
        juce::String::fromUTF8("Korábbi sorozat megnyitása vagy félbehagyott munka folytatása."));
    previous.onClick = [this]
    {
        --step;
        showStep();
    };
    next.onClick = [this]
    {
        ++step;
        showStep();
    };
    rescan.onClick = [this] { scanInterfaces(); };
    driver.onChange = [this] { scanInterfaces(); };
    interface.onChange = [this] { selectInterface(); };
    input.onChange = [this] { invalidate(); };
    output.onChange = [this] { invalidate(); };
    for (auto *toggle : {&wiring, &monitoring})
        toggle->onClick = [this] { invalidate(); };
    levels.onClick = [this]
    {
        if (measurementMode.getSelectedId() == 1)
            invalidate();
        else
            updateControls();
    };
    chainUnchanged.onClick = [this] { updateControls(); };
    physicalNotes.setMultiLine(true);
    physicalNotes.setTitle(juce::String::fromUTF8("Eltérések / megjegyzés (nem kötelező)"));
    workflowHistory.setTitle(juce::String::fromUTF8("Mentett mérési folyamatok"));
    workflowHistory.setTextWhenNothingSelected(
        juce::String::fromUTF8("Mentett mérési folyamat kiválasztása"));
    resultHistory.setTitle(juce::String::fromUTF8("Elkészült eredmények"));
    resultHistory.setTextWhenNothingSelected(juce::String::fromUTF8("Még nincs összehasonlítás"));
    physicalNotes.setTextToShowWhenEmpty(
        juce::String::fromUTF8("Csak eltérés esetén töltsd ki. Üresen hagyva: a kért állások."),
        juce::Colours::grey);
    physicalNotes.onTextChange = [this]
    {
        levels.setToggleState(false, juce::dontSendNotification);
        updateControls();
    };
    measurementMode.addItem(juce::String::fromUTF8("1. Bypass ellenőrzés"), 1);
    measurementMode.addItem(juce::String::fromUTF8("2. Aktív pedál / tiszta EQ"), 2);
    measurementMode.addItem(juce::String::fromUTF8("3. Aktív pedál / drive"), 3);
    measurementMode.setSelectedId(1, juce::dontSendNotification);
    measurementMode.onChange = [this] { modeChanged(); };
    preparePlugin.onClick = [this] { prepareCandidate(); };
    openComparisonButton.onClick = [this]
    {
        if (openComparison && comparisonFolder.isDirectory())
            openComparison(comparisonFolder);
    };
    workflowHistory.onChange = [this]
    {
        auto index = workflowHistory.getSelectedId() - 1;
        if (juce::isPositiveAndBelow(index, workflowFiles.size()))
        {
            saveProgress();
            restoreWorkflow(workflowFiles[index]);
        }
    };
    newWorkflow.onClick = [this]
    {
        try
        {
            saveProgress();
            workflow.create();
            set(workflow.state, "initializationRequired", true);
            workflow.save();
            restoreWorkflow(workflow.file);
            refreshHistory();
        }
        catch (const std::exception &e)
        {
            results.setText(juce::String::fromUTF8(e.what()));
        }
    };
    retryRender.onClick = [this] { retrySavedCapture(); };
    start.setComponentID("capture-start");
    measurementMode.setComponentID("measurement-mode");
    physicalNotes.setComponentID("physical-notes");
    tutorial.setComponentID("measurement-instructions");
    nextAction.setComponentID("next-action");
    resultHistory.setComponentID("result-history");
    workflowHistory.setComponentID("workflow-history");
    advance.setComponentID("advance-measurement");
    newWorkflow.setComponentID("new-workflow");
    retryRender.setComponentID("retry-render");
    resultHistory.onChange = [this] { showResult(resultHistory.getSelectedId()); };
    nextAction.setFont(juce::FontOptions(16.f));
    nextAction.setColour(juce::Label::backgroundColourId, juce::Colour(0xff203b38));
    planButton.setComponentID("ai-planner");
    openRunFolder.setComponentID("open-run-folder");
    openRunFolder.onClick = [this]
    {
        try
        {
            set(workflow.state, "storageRun",
                SessionStore::runPath(workflow.state, workflow.file.getFileNameWithoutExtension()));
            saveProgress();
            SessionStore::runFolder(projectRoot, workflow.file.getFileNameWithoutExtension())
                .startAsProcess();
        }
        catch (const std::exception &e)
        {
            results.setText(juce::String::fromUTF8(e.what()));
        }
    };
    mergeRuns.setComponentID("merge-runs");
    excludeResult.setComponentID("exclude-result");
    mergeRuns.onClick = [this]
    {
        if (isWorking())
            return;
        saveProgress();
        auto safe = juce::Component::SafePointer<HardwareSetupComponent>(this);
        collection = std::make_unique<CollectionPanel>(workflow,
                                                       [safe]
                                                       {
                                                           juce::MessageManager::callAsync(
                                                               [safe]
                                                               {
                                                                   if (safe)
                                                                   {
                                                                       safe->collection->exitModalState(0);
                                                                       safe->collection.reset();
                                                                       safe->updateControls();
                                                                       safe->resized();
                                                                   }
                                                               });
                                                       });
        collection->setComponentID("collection-panel");
        addAndMakeVisible(*collection);
        collection->enterModalState(false);
        resized();
    };
    excludeResult.onClick = [this]
    {
        if (isWorking() || !comparisonFolder.isDirectory())
            return;
        try
        {
            auto review = object();
            set(review, "schemaVersion", 1);
            set(review, "status", "excluded");
            set(review, "reason", "Operator marked this capture invalid in the measurement application");
            set(review, "reviewedUtc", utcNow());
            writeJson(comparisonFolder.getChildFile("operator-review.json"), review);
            writeHardwareReport(comparisonFolder);
            workflow.load(workflow.file);
            refreshScenarioResults();
            results.setText(juce::String::fromUTF8(
                "Hibásnak jelölve. A következő összevont ZIP nem tartalmazza; a lépés újramérhető."));
            updateControls();
        }
        catch (const std::exception &e)
        {
            results.setText(e.what());
        }
    };
    exportHandoff.onClick = [this]
    {
        if (isWorking())
            return;
        try
        {
            saveProgress();
            auto folder = exportMeasurementHandoff(projectRoot, workflow.state);
            folder.getSiblingFile(folder.getFileName() + ".zip").revealToUser();
            results.setText(juce::String::fromUTF8("Ezt az egy ZIP-et add át: ") +
                            folder.getSiblingFile(folder.getFileName() + ".zip").getFullPathName());
        }
        catch (const std::exception &e)
        {
            results.setText(juce::String::fromUTF8(e.what()));
        }
    };
    exportHandoff.setTooltip(juce::String::fromUTF8("Helyi pillanatkép: részletes riportok, beállítások, "
                                                    "WAV-ok és kizárások. Nem küld adatot az AI-nak."));
    planButton.onClick = [this] { openPlanner(); };
    advance.onClick = [this]
    {
        const int nextMode = workflow.nextMeasurement();
        if (nextMode > 0)
        {
            activeScenarioStep = workflow.nextScenarioStep().clone();
            measurementMode.setSelectedId(nextMode, juce::dontSendNotification);
            modeChanged();
            prepareCandidate();
        }
    };
    start.onClick = [this] { requestStart(); };
    stop.onClick = [this]
    {
        cancelWork = true;
        runner.cancel();
        if (audio)
            audio->stop();
        if (testing)
            finishTest();
    };
    showReport.onClick = [this]
    {
        if (reportFolder.isDirectory())
            reportFolder.revealToUser();
    };
    const auto &types = enumeration.getAvailableDeviceTypes();
    for (int i = 0; i < types.size(); ++i)
        driver.addItem(types[i]->getTypeName(), i + 1);
    if (!types.isEmpty())
        driver.setSelectedId(1, juce::dontSendNotification);
    interface.setTextWhenNothingSelected(juce::String::fromUTF8("Válassz hangkártyát"));
    input.setTextWhenNothingSelected(juce::String::fromUTF8("Visszatérő bemenet"));
    output.setTextWhenNothingSelected(juce::String::fromUTF8("Kiküldő kimenet"));
    scanInterfaces();
    showStep();
    try
    {
        workflow.importLegacySessions();
        const auto history = workflow.history();
        if (history.isEmpty())
        {
            workflow.create();
            set(workflow.state, "initializationRequired", true);
            workflow.save();
        }
        else
            workflow.load(history[0]);
        restoreWorkflow(workflow.file);
        refreshHistory();
    }
    catch (const std::exception &e)
    {
        // Keep damaged state untouched and offer a new, separately stored workflow.
        restoring = false;
        saveError = juce::String::fromUTF8(e.what());
        results.setText(juce::String::fromUTF8(
                            "A korábbi folyamat nem tölthető be. Válaszd az Új mérési folyamat gombot.\n") +
                        saveError);
        updateControls();
    }
    startTimerHz(20);
}
juce::File HardwareSetupComponent::recoveryFolder() const
{
    const auto path = workflow.state["captureFolder"].toString();
    if (!juce::File::isAbsolutePath(path))
        return {};
    juce::File folder(path);
    try
    {
        auto job = readJson(folder.getChildFile("recovery-job.json"));
        const int mode = static_cast<int>(job["metadata"]["measurementMode"]);
        if (mode < 2 || mode > 3 || workflow.hasResult(mode))
            return {};
        if (folder.getChildFile("capture.wav").existsAsFile() &&
            folder.getChildFile("probe.wav").existsAsFile())
            return folder;
    }
    catch (...)
    {
    }
    return {};
}
void HardwareSetupComponent::retrySavedCapture()
{
    const auto folder = recoveryFolder();
    if (folder == juce::File{} || testing || analysing || preparing)
        return;
    try
    {
        auto job = readJson(folder.getChildFile("recovery-job.json"));
        runMode = static_cast<int>(job["metadata"]["measurementMode"]);
        completedMode = 0;
        completedComparison = std::make_shared<juce::File>();
        auto completed = completedComparison;
        auto root = projectRoot;
        cancelWork = false;
        analysing = true;
        set(workflow.state, "phase", "processing");
        saveProgress();
        require(saveError.isEmpty(), saveError);
        analysis = std::async(
            std::launch::async,
            [this, job, folder, completed, root]
            {
                *completed = compareHardwareCapture(
                    juce::File::getSpecialLocation(juce::File::currentExecutableFile), root,
                    readWav(folder.getChildFile("probe.wav")), readWav(folder.getChildFile("capture.wav")),
                    bool(job["metadata"]["captureOnly"]) ? TargetSpec{}
                                                         : TargetSpec::fromJson(job["candidate"]),
                    job["metadata"], job["bypassCheck"], juce::File(job["bypassFolder"].toString()),
                    &cancelWork);
                return GuidedWorkflow::resultDescription(*completed);
            });
        results.setText(juce::String::fromUTF8("A megőrzött hardverfelvétel feldolgozása folyamatban. Nem "
                                               "kell a pedálhoz nyúlni; nincs hangkiküldés."));
        updateControls();
    }
    catch (const std::exception &e)
    {
        analysing = false;
        set(workflow.state, "phase", "interrupted");
        results.setText(juce::String::fromUTF8(e.what()));
        updateControls();
    }
}
void HardwareSetupComponent::refreshHistory()
{
    workflowFiles = workflow.history();
    workflowHistory.clear(juce::dontSendNotification);
    for (int i = 0; i < workflowFiles.size(); ++i)
    {
        try
        {
            auto state = readJson(workflowFiles[i]);
            const auto timestamp = state["createdUtc"].toString();
            const auto parsed = juce::Time::fromISO8601(timestamp);
            // Imported journals may contain microseconds unsupported by JUCE's parser.
            const auto date = parsed.toMilliseconds() == 0 ? timestamp.replaceCharacter('T', ' ')
                                                           : parsed.formatted("%Y. %m. %d. %H:%M:%S");
            workflowHistory.addItem(
                (state["name"].toString().isEmpty() ? date : state["name"].toString() + " | " + date) +
                    (state["phase"].toString() == "complete" ? juce::String::fromUTF8(" — kész")
                                                             : juce::String::fromUTF8(" — folytatható")),
                i + 1);
        }
        catch (...)
        {
            workflowHistory.addItem(
                juce::String::fromUTF8("Nem olvasható mentés: ") + workflowFiles[i].getFileName(), i + 1);
        }
        if (workflowFiles[i] == workflow.file)
            workflowHistory.setSelectedId(i + 1, juce::dontSendNotification);
    }
}
void HardwareSetupComponent::saveProgress()
{
    if (restoring || workflow.file == juce::File{})
        return;
    try
    {
        set(workflow.state, "name", sessionName.getText().trim());
        set(workflow.state, "mode", measurementMode.getSelectedId());
        set(workflow.state, "step", step);
        const auto mode = measurementMode.getSelectedId();
        if (mode > 1)
        {
            set(workflow.state, juce::Identifier("notes" + juce::String(mode)), physicalNotes.getText());
            if (activeScenarioStep.isObject())
            {
                auto notes = workflow.state["scenarioNotes"];
                if (!notes.isObject())
                    notes = object();
                set(notes, juce::Identifier(activeScenarioStep["id"].toString()), physicalNotes.getText());
                set(workflow.state, "scenarioNotes", notes);
            }
        }
        set(workflow.state, "driver", driver.getText());
        set(workflow.state, "device", interface.getText());
        set(workflow.state, "input", input.getText());
        set(workflow.state, "output", output.getText());
        workflow.save();
        saveError.clear();
        savedStatus.setText(juce::String::fromUTF8("Automatikusan mentve • Bezárás után folytatható"),
                            juce::dontSendNotification);
    }
    catch (const std::exception &e)
    {
        saveError = juce::String::fromUTF8(e.what());
        savedStatus.setText(juce::String::fromUTF8("MENTÉSI HIBA — a folytatás nincs biztosítva: ") +
                                saveError,
                            juce::dontSendNotification);
    }
}
void HardwareSetupComponent::restoreWorkflow(const juce::File &file)
{
    activeScenarioStep = juce::var();
    try
    {
        restoring = true;
        workflow.load(file);
        const auto saved = workflow.state.clone();
        sessionName.setText(saved["name"].toString(), false);
        hasPlot = false;
        resultPlot.clear();
        captureProgress = 0;
        inputLevel.setText(juce::String::fromUTF8("Bemeneti szint: a felvétel alatt látszik"),
                           juce::dontSendNotification);
        resultSummary.setText(juce::String::fromUTF8("Még nincs eredmény ebben a folyamatban."),
                              juce::dontSendNotification);
        bypassCheck = juce::var();
        bypassFolder = comparisonFolder = reportFolder = juce::File{};
        completedMode = 0;
        measurementMode.setSelectedId(1, juce::dontSendNotification);
        modeChanged();
        auto selectText = [](juce::ComboBox &box, const juce::String &text)
        {
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemText(i) == text)
                    box.setSelectedId(box.getItemId(i), juce::dontSendNotification);
        };
        selectText(driver, saved["driver"].toString());
        scanInterfaces();
        selectText(interface, saved["device"].toString());
        selectInterface();
        selectText(input, saved["input"].toString());
        selectText(output, saved["output"].toString());
        for (auto *toggle : {&wiring, &monitoring, &levels, &chainUnchanged})
            toggle->setToggleState(false, juce::dontSendNotification);
        step = static_cast<int>(saved["mode"]) > 1 || workflow.hasResult(1)
                   ? 3
                   : juce::jlimit(0, 3, static_cast<int>(saved["step"]));
        workflow.state = saved;
        resultHistory.clear(juce::dontSendNotification);
        for (int mode : {2, 3})
            if (workflow.hasResult(mode))
                resultHistory.addItem(GuidedWorkflow::modeName(mode) + juce::String::fromUTF8(" — kész"),
                                      mode);
        refreshScenarioResults();
        showStep();
        if (workflow.nextMeasurement() == 0)
        {
            completedMode = 3;
            tutorial.setText(juce::String::fromUTF8(
                "A MÉRÉSI SOROZAT ELKÉSZÜLT\n\nA tiszta/EQ és a drive felvétel is mentve van. Nincs további "
                "pedálbeállítás.\nVálassz eredményt lent, majd nyisd meg a grafikonokat. Új sorozathoz az Új "
                "mérési folyamat gombot használd.\n\nAz eredmény diagnosztikai összevetés; abszolút analóg "
                "szintkalibráció nélkül nem modellpontossági értékelés."));
        }
        else
        {
            results.setText(
                juce::String::fromUTF8("Következő hiányzó mérés: ") +
                GuidedWorkflow::modeName(workflow.nextMeasurement()) +
                juce::String::fromUTF8(
                    ".\n\nVisszatérés után először állítsd a pedált teljes BYPASS-ba, ellenőrizd a "
                    "csatornákat és a visszaigazolásokat, majd futtasd a bypass-próbát. A korábbi eredmények "
                    "és pedáljegyzetek megmaradtak.\nA félbeszakadt hangfelvételt meg kell ismételni; "
                    "folytatáskor semmi nem indul automatikusan.") +
                (saved["phase"].toString() == "interrupted"
                     ? juce::String::fromUTF8("\n\nAz előző művelet félbeszakadt. Az esetleg megőrzött nyers "
                                              "felvétel mappája: ") +
                           saved["captureFolder"].toString()
                     : juce::String{}));
        }
        if (!workflow.hasResult(1) && !workflow.hasResult(2) && !workflow.hasResult(3) &&
            static_cast<int>(saved["mode"]) <= 1 && static_cast<int>(saved["step"]) == 0 &&
            saved["phase"].toString() != "interrupted")
            results.setText(juce::String::fromUTF8(
                "ÚJ MÉRÉSI FOLYAMAT\n\nElőször kövesd a bekötési útmutató négy oldalát a Következő gombbal. "
                "Ezután jön a bypass-próba, a tiszta/EQ mérés, majd a drive mérés.\n\nA program minden "
                "lépésnél kiírja a következő teendőt. A mentés automatikus; az ablakot később is "
                "újranyithatod. Ezen az oldalon nincs hangkiküldés."));
        if (resultHistory.getNumItems() > 0)
        {
            // Retain the resume instructions for incomplete workflows.
            const auto resumeText = results.getText();
            showResult(resultHistory.getItemId(resultHistory.getNumItems() - 1));
            if (workflow.nextMeasurement() != 0)
                results.setText(resumeText);
        }
        progress.setText(
            juce::String::fromUTF8("SZÜNET / NINCS HANGKIKÜLDÉS — csak az Indítás gomb indít tesztjelet."),
            juce::dontSendNotification);
        restoring = false;
        updateControls();
    }
    catch (const std::exception &e)
    {
        restoring = false;
        results.setText(juce::String::fromUTF8("A folyamat nem tölthető be: ") + e.what());
    }
}
void HardwareSetupComponent::showResult(int mode)
{
    if ((mode < 100 && !workflow.hasResult(mode)) || mode < 2)
        return;
    try
    {
        if (mode >= 100)
        {
            auto steps = workflow.state["scenario"]["steps"];
            require(mode - 100 < steps.size(), "Missing scenario result");
            auto path =
                workflow.state["scenarioResults"][juce::Identifier(steps[mode - 100]["id"].toString())]
                    .toString();
            require(juce::File::isAbsolutePath(path), "Missing scenario result");
            comparisonFolder = reportFolder = juce::File(path);
        }
        else
            comparisonFolder = reportFolder = workflow.result(mode);
        resultHistory.setSelectedId(mode, juce::dontSendNotification);
        results.setText(GuidedWorkflow::resultDescription(comparisonFolder));
        presentResult();
        updateControls();
    }
    catch (const std::exception &e)
    {
        results.setText(juce::String::fromUTF8(e.what()));
    }
}
void HardwareSetupComponent::presentResult()
{
    resultPlot.load(comparisonFolder);
    displayedResultText = results.getText();
    hasPlot = true;
    details.setToggleState(false, juce::dontSendNotification);
    const auto m = readJson(comparisonFolder.getChildFile("manifest.json"));
    const auto summary = readJson(comparisonFolder.getChildFile("analysis_summary.json"));
    if (bool(m["hardware"]["captureOnly"]))
    {
        resultSummary.setText(
            m["hardware"]["scenarioStep"]["title"].toString() +
                juce::String::fromUTF8(" • MENTVE\nFelvétel: ") +
                juce::String(double(summary["referenceRmsDbFS"]), 2) +
                juce::String::fromUTF8(" dBFS RMS • alapreferencia − felvétel: ") +
                juce::String(double(summary["gainDifferenceDb"]), 2) +
                juce::String::fromUTF8(" dB\nRögzített hardverreferencia, nem plugin-összehasonlítás."),
            juce::dontSendNotification);
        return;
    }
    resultSummary.setText(
        (GuidedWorkflow::resultUsable(comparisonFolder)
             ? juce::String{}
             : juce::String::fromUTF8("ÚJRAMÉRENDŐ — a kért pedálállás nem igazolt.\n")) +
            (m["hardware"]["scenarioStep"]["title"].toString().isNotEmpty()
                 ? m["hardware"]["scenarioStep"]["title"].toString()
                 : GuidedWorkflow::modeName(static_cast<int>(m["hardware"]["measurementMode"]))) +
            juce::String::fromUTF8(" • MENTVE • ") +
            juce::Time::fromISO8601(m["createdUtc"].toString()).formatted("%m. %d. %H:%M") +
            juce::String::fromUTF8("\nDarkBass − pedál: ") +
            juce::String(static_cast<double>(summary["gainDifferenceDb"]), 2) +
            juce::String::fromUTF8(" dB • Korreláció: ") +
            juce::String(static_cast<double>(summary["alignmentCorrelation"]), 2) +
            (static_cast<bool>(summary["alignmentReliable"])
                 ? juce::String::fromUTF8("\nDiagnosztikai eredmény; a szintillesztés nincs kalibrálva.")
                 : juce::String::fromUTF8("\nBizonytalan korreláció; a szintillesztés nincs kalibrálva.")),
        juce::dontSendNotification);
}
void HardwareSetupComponent::showSavedResult(const juce::File &folder)
{
    // Opening a graph must not replace the ongoing workflow or invent completed steps.
    for (int mode : {2, 3})
        if (workflow.result(mode) == folder)
            showResult(mode);
}
HardwareSetupComponent::~HardwareSetupComponent()
{
    stopTimer();
    if (planner)
        planner->exitModalState(0);
    planner.reset();
    if (collection)
        collection->exitModalState(0);
    collection.reset();
    sessionName.onTextChange = nullptr;
    sessionName.onReturnKey = nullptr;
    setLookAndFeel(nullptr);
    if (testing || analysing || preparing)
        set(workflow.state, "phase", "interrupted");
    saveProgress();
    cancelWork = true;
    runner.cancel();
    if (audio)
    {
        audio->stop();
        audio->close();
    }
    if (analysis.valid())
        analysis.wait();
    if (preparation.valid())
        preparation.wait();
}
juce::String HardwareSetupComponent::measurementInstructions() const
{
    if (genericDevice(workflow.state))
    {
        if (measurementMode.getSelectedId() == 1)
            return juce::String::fromUTF8("BEÁLLÍTÁS\n") +
                   workflow.state["scenario"]["calibration"].toString() +
                   juce::String::fromUTF8("\n\nA PROGRAM\nJelút-ellenőrzést végez és elmenti az "
                                          "összehasonlítás alapját. Ez nem abszolút feszültségkalibráció.");
        auto text = activeScenarioStep["title"].toString() + juce::String::fromUTF8("\n\nBEÁLLÍTÁS\n") +
                    activeScenarioStep["settings"].toString() + juce::String::fromUTF8("\n\nCÉL: ") +
                    activeScenarioStep["reason"].toString();
        if (activeScenarioStep["expected"].toString().trim() !=
            activeScenarioStep["reason"].toString().trim())
            text += juce::String::fromUTF8("\nVÁRHATÓ: ") + activeScenarioStep["expected"].toString();
        return text + juce::String::fromUTF8("\n\nA PROGRAM\n2,6 s zajpróbát küld −36 dBFS csúccsal, majd a "
                                             "felvételt a mentett alapreferenciához hasonlítja.");
    }
    if (measurementMode.getSelectedId() == 1)
        return steps[3];
    if (activeScenarioStep.isObject())
    {
        juce::var previous;
        if (auto *all = workflow.state["scenario"]["steps"].getArray())
            for (int i = 0; i < all->size(); ++i)
                if ((*all)[i]["id"] == activeScenarioStep["id"] && i > 0)
                    previous = (*all)[i - 1];
        return scenarioStepInstructions(activeScenarioStep, previous);
    }
    const bool drive = measurementMode.getSelectedId() == 3;
    return juce::String::fromUTF8("BEÁLLÍTÁS\nPreamp / EQ: BE • Torzítás: ") + (drive ? "BE" : "KI") +
           juce::String::fromUTF8(
               "\nEQ: középállás; Low Mid: 500 Hz; High Mid: 1,5 kHz.\nAttack és Grunt: FLAT; Blend: "
               "maximum.\nDrive és Master: 9 óra; Level: középállás.\n\nA jelút és a hangkártya/reamp "
               "szintje maradjon változatlan. Csak az eltéréseket írd a megjegyzésbe.\n\nA PROGRAM\n2,6 s "
               "halk zajpróbát rögzít, majd összehasonlítja a DarkBass renderével. Az eredményt "
               "automatikusan menti.");
}
void HardwareSetupComponent::modeChanged()
{
    completedMode = 0;
    physicalNotes.setText(
        workflow.state[juce::Identifier("notes" + juce::String(measurementMode.getSelectedId()))].toString(),
        false);
    if (activeScenarioStep.isObject())
        physicalNotes.setText(
            workflow.state["scenarioNotes"][juce::Identifier(activeScenarioStep["id"].toString())].toString(),
            false);
    levels.setToggleState(false, juce::dontSendNotification);
    chainUnchanged.setToggleState(false, juce::dontSendNotification);
    preparedMode = 0;
    prepared = PreparedDarkbass{};
    const bool active = measurementMode.getSelectedId() > 1;
    levels.setButtonText(
        active ? juce::String::fromUTF8("A kért pedálállások beállítva; az eltéréseket jeleztem")
               : juce::String::fromUTF8("Preamp/EQ és torzítás KIKAPCSOLVA; szintek ellenőrizve"));
    start.setButtonText(active ? juce::String::fromUTF8("Beállítva — mérés indítása")
                               : juce::String::fromUTF8("Bekötve — teszt indítása"));
    if (!active)
    {
        bypassCheck = juce::var();
        bypassFolder = juce::File{};
    }
    results.setText(juce::String::fromUTF8(
        "Kövesd a fenti útmutatót. A beállítások változtatása nem indít hangkiküldést."));
    showStep();
}
void HardwareSetupComponent::prepareCandidate()
{
    if (genericDevice(workflow.state))
        return;
    if (!preparePlugin.isEnabled())
        return;
    auto bundle = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                      .getChildFile("Library/Audio/Plug-Ins/Components/DarkBass.component");
    if (!bundle.exists())
        bundle = juce::File("/Library/Audio/Plug-Ins/Components/DarkBass.component");
    preparing = true;
    set(workflow.state, "phase", "preparing");
    cancelWork = false;
    preparedMode = 0;
    const bool distortion = measurementMode.getSelectedId() == 3;
    auto overrides = activeScenarioStep["parameters"].clone();
    results.setText(juce::String::fromUTF8(
        "DarkBass ellenőrzése és a választott profil előkészítése… Nem küldünk ki hangot."));
    preparation = std::async(std::launch::async,
                             [this, bundle, distortion, overrides]
                             {
                                 return prepareDarkbass(
                                     juce::File::getSpecialLocation(juce::File::currentExecutableFile),
                                     bundle, distortion, &cancelWork, overrides);
                             });
    updateControls();
}
void HardwareSetupComponent::showStep()
{
    step = juce::jlimit(0, steps.size() - 1, step);
    if (measurementMode.getSelectedId() == 1)
    {
        const juce::StringArray compact{
            juce::String::fromUTF8(
                "1. Válaszd le a hangfalakat.\n2. Direct Monitor és DAW monitorozás: KI.\n3. A pedál a "
                "megfelelő tápegységéről működjön.\n\nÚj bekötésnél alacsony szintekkel kezdj. Folytatáskor "
                "őrizd meg a feljegyzett, bevált állásokat."),
            juce::String::fromUTF8("Scarlett Output 1 (hátsó jack)\n↓ TRS–XLR apa kábel\nDACCAPO BALANCED "
                                   "LINE INPUT\n↓ DACCAPO ISOLATED OUTPUT, TS kábel\nB7K Ultra "
                                   "INPUT\n\nJegyezd fel a DACCAPO kapcsolóit és LEVEL állását."),
            juce::String::fromUTF8(
                "B7K Ultra jack OUTPUT\n↓ TS kábel\nScarlett előlapi jack bemenet\n\nA "
                "bevált 2i2 jelútnál: Input 2, INST; gain kb. 12 óra. Új bekötésnél LINE "
                "és alacsony gain az indulás. A preamp/EQ és a torzítás is legyen KIKAPCSOLVA."),
            juce::String::fromUTF8(
                "1. Kapcsold ki a teljes pedált: preamp/EQ és torzítás KIKAPCSOLVA.\n2. Ellenőrizd a "
                "hangkártyát és a csatornákat "
                "lent.\n3. Jelöld be a három visszaigazolást.\n\nA próba 2,6 másodperc, halk (-36 dBFS). A "
                "siker után továbblépünk az aktív pedál mérésére.")};
        tutorial.setText(compact[step]);
    }
    else
        tutorial.setText(measurementInstructions());
    tutorial.moveCaretToTop(false);
    previous.setEnabled(step > 0);
    next.setEnabled(step < steps.size() - 1);
    updateControls();
}
void HardwareSetupComponent::scanInterfaces()
{
    if (testing || analysing || permissionPending || preparing)
        return;
    if (audio)
    {
        audio->stop();
        audio->close();
        audio.reset();
    }
    interface.clear(juce::dontSendNotification);
    input.clear(juce::dontSendNotification);
    output.clear(juce::dontSendNotification);
    int index = driver.getSelectedId() - 1;
    const auto &types = enumeration.getAvailableDeviceTypes();
    if (juce::isPositiveAndBelow(index, types.size()))
    {
        auto *type = types[index];
        type->scanForDevices();
        auto inputs = type->getDeviceNames(true);
        int id = 1;
        for (const auto &name : type->getDeviceNames(false))
            if (inputs.contains(name))
                interface.addItem(name, id++);
    }
    invalidate();
    progress.setText(
        juce::String::fromUTF8("Válassz kétirányú audiointerfészt. Nem nyitunk meg automatikusan eszközt."),
        juce::dontSendNotification);
}
void HardwareSetupComponent::selectInterface()
{
    if (testing || analysing || permissionPending || preparing)
        return;
    if (audio)
    {
        audio->stop();
        audio->close();
        audio.reset();
    }
    input.clear(juce::dontSendNotification);
    output.clear(juce::dontSendNotification);
    int index = driver.getSelectedId() - 1;
    const auto &types = enumeration.getAvailableDeviceTypes();
    if (interface.getSelectedId() > 0 && juce::isPositiveAndBelow(index, types.size()))
    {
        audio.reset(types[index]->createDevice(interface.getText(), interface.getText()));
        if (audio)
        {
            auto inputs = audio->getInputChannelNames(), outputs = audio->getOutputChannelNames();
            for (int i = 0; i < inputs.size(); ++i)
                input.addItem(juce::String(i + 1) + ": " + inputs[i], i + 1);
            for (int i = 0; i < outputs.size(); ++i)
                output.addItem(juce::String(i + 1) + ": " + outputs[i], i + 1);
        }
    }
    wiring.setToggleState(false, juce::dontSendNotification);
    invalidate();
}
void HardwareSetupComponent::invalidate()
{
    completedMode = 0;
    if (testing)
        return;
    bypassCheck = juce::var();
    bypassFolder = juce::File{};
    results.setText(juce::String::fromUTF8(
        "Nincs aktuális setup-ellenőrzés. Beállítás vagy bekötés változtatása után futtasd újra."));
    updateControls();
}
void HardwareSetupComponent::updateControls()
{
    const bool locked = testing || analysing || permissionPending || preparing;
    mergeRuns.setEnabled(!locked);
    excludeResult.setEnabled(!locked && comparisonFolder.isDirectory() &&
                             GuidedWorkflow::resultUsable(comparisonFolder));
    planButton.setEnabled(!locked);
    exportHandoff.setEnabled(!locked);
    openRunFolder.setEnabled(!locked);
    const bool active = measurementMode.getSelectedId() > 1;
    const bool calibrated = bypassCheck["status"].toString() == "passed";
    const bool finished = workflow.nextMeasurement() == 0;
    if (finished)
        progress.setText(
            juce::String::fromUTF8("ELKÉSZÜLT — az eredmények mentve vannak. Nincs hangkiküldés."),
            juce::dontSendNotification);
    if (finished)
        tutorial.setText(
            juce::String::fromUTF8("A tiszta/EQ és a drive mérés is elkészült.\n\nMost nincs további "
                                   "beállítási feladat.\nJobbra választhatsz a két eredmény között.\n\nÚj "
                                   "sorozat: Új mérés.\nA korábbi eredmények megmaradnak."));
    taskTitle.setText((finished    ? juce::String::fromUTF8("Kész — eredmények")
                       : testing   ? juce::String::fromUTF8("Felvétel folyamatban")
                       : analysing ? juce::String::fromUTF8("Kiértékelés")
                       : preparing ? juce::String::fromUTF8("DarkBass előkészítése")
                                   : juce::String::fromUTF8("Aktuális feladat")) +
                          (finished || locked
                               ? juce::String{}
                               : juce::String::fromUTF8(" • ") +
                                     (active ? GuidedWorkflow::modeName(measurementMode.getSelectedId())
                                             : (step < 3 ? juce::String::fromUTF8("Bekötés") : "Bypass"))),
                      juce::dontSendNotification);
    heading.setVisible(false);
    openComparisonButton.setVisible(false);
    retryRender.setVisible(!finished && recoveryFolder() != juce::File{});
    retryRender.setEnabled(!locked);
    duplicate.setEnabled(!locked);
    sessionName.setEnabled(!locked);
    cleanComplete = workflow.hasResult(2);
    driveComplete = workflow.hasResult(3);
    bypassCurrent = calibrated;
    currentStage = finished ? 4 : active ? measurementMode.getSelectedId() : step < 3 ? 0 : 1;
    stageOverview.setVisible(false);
    notesLabel.setVisible(active && !finished);
    workflowHistory.setEnabled(!locked);
    newWorkflow.setEnabled(!locked);
    resultHistory.setEnabled(!locked && resultHistory.getNumItems() > 0);
    stageOverview.setText(
        juce::String::fromUTF8("1. Bekötés  →  2. Bypass ") +
            (calibrated ? juce::String::fromUTF8("✓")
             : finished ? "(mentett)"
                        : juce::String::fromUTF8("(ellenőrzendő)")) +
            juce::String::fromUTF8("  →  3. Tiszta/EQ ") +
            (workflow.hasResult(2) ? juce::String::fromUTF8("✓") : juce::String::fromUTF8("○")) +
            juce::String::fromUTF8("  →  4. Drive ") +
            (workflow.hasResult(3) ? juce::String::fromUTF8("✓") : juce::String::fromUTF8("○")) +
            juce::String::fromUTF8("  →  5. Eredmények"),
        juce::dontSendNotification);
    for (auto *c : std::vector<juce::Component *>{
             &measurementMode, &preparePlugin, &physicalNotes, &chainUnchanged, &previous, &next, &rescan,
             &route, &driver, &interface, &input, &output, &wiring, &monitoring, &levels, &start})
        c->setVisible(!finished);
    const bool wiringGuide = !active && step < 3;
    for (auto *c : std::vector<juce::Component *>{&rescan, &route, &driver, &interface, &input, &output,
                                                  &wiring, &monitoring, &levels, &start})
        c->setVisible(!finished && !wiringGuide);
    stop.setVisible(!finished && !wiringGuide);
    physicalNotes.setVisible(!finished && active);
    chainUnchanged.setVisible(!finished && active);
    preparePlugin.setVisible(!finished && active);
    previous.setVisible(!finished && !active);
    next.setVisible(!finished && !active);
    measurementMode.setVisible(false);
    repeatBypass.setVisible(active && !finished);
    repeatBypass.setEnabled(!locked);
    measurementMode.setEnabled(!locked);
    measurementMode.setItemEnabled(2, calibrated);
    measurementMode.setItemEnabled(3, calibrated);
    preparePlugin.setEnabled(!locked && active && calibrated);
    physicalNotes.setEnabled(!locked && active);
    chainUnchanged.setEnabled(!locked && active);
    openComparisonButton.setEnabled(!locked && comparisonFolder.isDirectory() &&
                                    static_cast<bool>(openComparison));
    previous.setEnabled(!locked && !active && step > 0);
    next.setEnabled(!locked && !active && step < steps.size() - 1);
    for (auto *c : std::vector<juce::Component *>{&driver, &interface, &input, &output, &rescan, &wiring,
                                                  &monitoring, &levels})
        c->setEnabled(!locked);
    start.setEnabled(!finished && !locked && (active || step == 3) && audio && input.getSelectedId() > 0 &&
                     output.getSelectedId() > 0 && wiring.getToggleState() && monitoring.getToggleState() &&
                     levels.getToggleState() &&
                     (!active || (calibrated && preparedMode == measurementMode.getSelectedId() &&
                                  prepared.target.state.getSize() > 0 && chainUnchanged.getToggleState())));
    stop.setEnabled(testing || preparing || analysing);
    showReport.setEnabled(reportFolder.isDirectory() && !locked);
    advance.setVisible(completedMode > 0 && !finished);
    advance.setEnabled(!locked && calibrated);
    advance.setButtonText((workflow.nextMeasurement() == 2
                               ? juce::String::fromUTF8("Tovább: tiszta/EQ mérés")
                               : juce::String::fromUTF8("Tovább: torzított mérés")));
    juce::String action;
    if (testing)
        action =
            juce::String::fromUTF8("Mérés folyamatban — ne állíts semmit. A hang 2,6 másodperc után leáll.");
    else if (locked)
        action = juce::String::fromUTF8("Előkészítés / feldolgozás folyamatban. Most nincs teendőd.");
    else if (retryRender.isVisible())
        action = juce::String::fromUTF8("A hardverfelvétel megvan, a feldolgozás félbemaradt. Következő: "
                                        "Mentett felvétel feldolgozása újra — hang nélkül.");
    else if (finished)
        action = juce::String::fromUTF8("MINDKÉT MÉRÉS KÉSZ. Az eredmény és a grafikon jobbra látható.");
    else if (completedMode > 0)
        action = juce::String::fromUTF8("Ez a mérés kész. Következő: Tovább a következő mérési pontra.");
    else if (active && !calibrated)
        action =
            juce::String::fromUTF8("A jelút ellenőrzése érvénytelen. Nyomd meg: Bypass újraellenőrzése.");
    else if (!active && workflow.hasResult(1) && step == 3 && !calibrated)
        action = juce::String::fromUTF8("FOLYTATÁS: pedál teljes BYPASS → csatornák és jelölőnégyzetek "
                                        "ellenőrzése → bypass-teszt. Utána: ") +
                 GuidedWorkflow::modeName(workflow.nextMeasurement()) + ".";
    else if (!active && step < 3)
        action =
            juce::String::fromUTF8("Bekötési útmutató: olvasd el ezt az oldalt, majd nyomd meg: Következő.");
    else if (!audio || input.getSelectedId() == 0 || output.getSelectedId() == 0)
        action = juce::String::fromUTF8(
            "Válaszd ki a hangkártyát, a visszatérő bemenetet és a kiküldő kimenetet.");
    else if (active && preparedMode != measurementMode.getSelectedId())
        action =
            juce::String::fromUTF8("Nyomd meg: DarkBass profil betöltése. Ez még nem indít teszthangot.");
    else if (!wiring.getToggleState())
        action = juce::String::fromUTF8(
            "Következő: ellenőrizd a kábeleket és a csatornákat, majd jelöld be a Bekötés sort.");
    else if (!monitoring.getToggleState())
        action = juce::String::fromUTF8(
            "Következő: válaszd le a hangfalakat, kapcsold ki a monitorozást, majd igazold vissza.");
    else if (active && !chainUnchanged.getToggleState())
        action = juce::String::fromUTF8(
            "Következő: igazold vissza, hogy a Scarlett / DACCAPO beállításai változatlanok.");
    else if (!levels.getToggleState())
        action = juce::String::fromUTF8(
            "Következő: ellenőrizd és igazold vissza a kért pedálállást. Ezután indítható a próba.");
    else if (!start.isEnabled())
        action =
            juce::String::fromUTF8("Ellenőrizd és jelöld be a bekötés, a kikapcsolt monitorozás és a "
                                   "pedálállás visszaigazolását; aktív mérésnél a változatlan jelutat is.");
    else
        action = juce::String::fromUTF8("Minden kész. Nyomd meg: ") + start.getButtonText() +
                 juce::String::fromUTF8(". Rövid, halk teszthang következik.");
    // Only show controls relevant to the current task; route changes live in the bypass step.
    for (auto *c : std::vector<juce::Component *>{&driver, &interface, &input, &output, &route, &rescan})
        c->setVisible(!finished && !active && !wiringGuide);
    wiring.setVisible(!finished && !active && !wiringGuide);
    monitoring.setVisible(!finished && !active && !wiringGuide);
    preparePlugin.setVisible(!finished && active && preparedMode != measurementMode.getSelectedId());
    if (analysing || preparing)
    {
        captureProgress = -1;
        captureBar.setTextToDisplay(juce::String::fromUTF8("Feldolgozás… nincs hangkiküldés"));
    }
    else if (!testing)
    {
        captureProgress = completedMode > 0 || finished ? 1 : 0;
        captureBar.setTextToDisplay((captureProgress == 1
                                         ? juce::String::fromUTF8("Elkészült és mentve")
                                         : juce::String::fromUTF8("Várakozás — nincs hangkiküldés")));
    }
    if (workflow.hasScenario())
    {
        const int total = workflow.state["scenario"]["steps"].size();
        taskTitle.setText(juce::String::fromUTF8("Forgatókönyv • ") +
                              juce::String(workflow.scenarioCompleted()) + "/" + juce::String(total) +
                              juce::String::fromUTF8(" kész"),
                          juce::dontSendNotification);
        if (finished)
        {
            tutorial.setText(juce::String::fromUTF8("A felvételek mentve.\n\nÉRTÉKELENDŐ\n") +
                             workflow.state["scenario"]["completion"].toString());
            action =
                juce::String::fromUTF8("Átadáshoz: Codex átadócsomag. Új tervhez: Eszköz / fotó / AI terv.");
        }
        advance.setButtonText(juce::String::fromUTF8("Következő: ") +
                              workflow.nextScenarioStep()["title"].toString());
    }
    if (genericDevice(workflow.state))
    {
        const bool ready = workflow.hasScenario() &&
                           bool(workflow.state["deviceSetup"]["controlsConfirmed"]) &&
                           bool(workflow.state["deviceSetup"]["routeConfirmed"]) &&
                           juce::JSON::toString(workflow.state["approvedDeviceSetup"]) ==
                               juce::JSON::toString(workflow.state["deviceSetup"]);
        preparePlugin.setVisible(false);
        physicalNotes.setTextToShowWhenEmpty(
            juce::String::fromUTF8("Üresen hagyva: pontosan a kért állások."), juce::Colours::grey);
        monitoring.setButtonText(
            juce::String::fromUTF8("Monitorozás KI; fejnél a szükséges terhelés csatlakozik"));
        levels.setButtonText(
            active ? juce::String::fromUTF8("A kért kezelőszerv-állásokat beállítottam")
                   : juce::String::fromUTF8(
                         "A jóváhagyott alapreferencia-állás és alacsony kezdőszint beállítva"));
        repeatBypass.setButtonText(juce::String::fromUTF8("Jelút változott? Alapreferencia újramérése"));
        chainUnchanged.setButtonText(
            juce::String::fromUTF8("A hangkártya / reamp / loadbox szintje és jelútja változatlan"));
        route.setText(workflow.state["deviceSetup"]["routeNotes"].toString(), juce::dontSendNotification);
        if (!finished)
        {
            tutorial.setText(
                ready
                    ? measurementInstructions()
                    : juce::String::fromUTF8(
                          "Indítás: AI terv → eszköztípus → frontpanel fotó vagy kézi kezelőszervlista → "
                          "ellenőrzött visszatérő jelút → jóváhagyott kalibrációs és mérési terv.\n\nFejnél: "
                          "hangszórókimenet csak megfelelő impedanciájú és teljesítményű loadbox-ra. A "
                          "hangkártyára kizárólag a loadbox dokumentált line/DI kimenete mehet. A szükséges "
                          "terhelést ne válaszd le. A fotó a névleges terhelést és szinteket nem igazolja."));
            start.setButtonText(active ? juce::String::fromUTF8("Beállítva — felvétel indítása")
                                       : juce::String::fromUTF8("Alapreferencia ellenőrzése"));
            start.setEnabled(
                ready && !locked && (active || step == 3) && audio && input.getSelectedId() > 0 &&
                output.getSelectedId() > 0 && wiring.getToggleState() && monitoring.getToggleState() &&
                levels.getToggleState() && (!active || (calibrated && chainUnchanged.getToggleState())));
            if (!ready)
                action =
                    juce::String::fromUTF8("Előbb hagyd jóvá az eszköz beállításait és a mérési tervet.");
            else if (completedMode > 0 && advance.isEnabled())
                action = juce::String::fromUTF8("Nyomd meg: ") + advance.getButtonText() + ".";
            else if (!active && step < 3)
                action = juce::String::fromUTF8("Olvasd el az előkészítést, majd: Következő.");
            else if (!audio || input.getSelectedId() == 0 || output.getSelectedId() == 0)
                action = juce::String::fromUTF8("Válaszd ki a hangkártyát és a két fizikai csatornát.");
            else if (active && !calibrated)
                action = juce::String::fromUTF8("Mérd újra az alapreferenciát.");
            else if (!wiring.getToggleState())
                action = juce::String::fromUTF8("Ellenőrizd a bekötést, majd jelöld be a visszaigazolást.");
            else if (!monitoring.getToggleState())
                action = juce::String::fromUTF8("Kapcsold ki a monitorozást, majd igazold vissza.");
            else if (active && !chainUnchanged.getToggleState())
                action = juce::String::fromUTF8("Igazold vissza, hogy a jelút és a szintek változatlanok.");
            else if (!levels.getToggleState())
                action =
                    juce::String::fromUTF8("Állítsd be a bal oldalon kért állásokat, majd igazold vissza.");
            else
                action = juce::String::fromUTF8("Indítható: ") + start.getButtonText() + ".";
        }
        taskTitle.setText(finished ? juce::String::fromUTF8("Mérési sor elkészült")
                          : active ? juce::String::fromUTF8("Aktuális mérési állás")
                                   : juce::String::fromUTF8("Előkészítés / alapreferencia"),
                          juce::dontSendNotification);
    }
    if (bool(workflow.state["initializationRequired"]))
    {
        start.setEnabled(false);
        previous.setVisible(false);
        next.setVisible(false);
        taskTitle.setText(juce::String::fromUTF8("Új mérés előkészítése"), juce::dontSendNotification);
        tutorial.setText(
            juce::String::fromUTF8("1. Válassz eszköztípust.\n2. Tölts be fotót, vagy írd be a "
                                   "kezelőszerveket.\n3. Ellenőrizd a listát és a jelutat.\n4. Készíts és "
                                   "hagyj jóvá mérési tervet.\n\nAz előkészítés nem indít teszthangot."));
        action = juce::String::fromUTF8("Következő: Eszköz / fotó / AI terv");
    }
    // Runtime state takes priority over setup prompts, including the generic-device branch.
    if (testing)
    {
        taskTitle.setText(juce::String::fromUTF8("Felvétel folyamatban"), juce::dontSendNotification);
        action = juce::String::fromUTF8("Ne állíts a kezelőszerveken. Szükség esetén: VÉSZLEÁLLÍTÁS.");
    }
    else if (analysing)
    {
        taskTitle.setText(juce::String::fromUTF8("Mentés és kiértékelés"), juce::dontSendNotification);
        action = juce::String::fromUTF8("Várj az eredményre. A teszthang már leállt.");
    }
    else if (preparing)
    {
        taskTitle.setText(juce::String::fromUTF8("Pluginprofil betöltése"), juce::dontSendNotification);
        action = juce::String::fromUTF8("Várj a betöltésre. Mérés még nem indul.");
    }
    else if (permissionPending)
        action = juce::String::fromUTF8("Engedélyezd az audiobemenet használatát a rendszerablakban.");
    saveProgress();
    if (saveError.isNotEmpty())
    {
        start.setEnabled(false);
        action = juce::String::fromUTF8(
            "A mentés sikertelen. Szabadíts fel helyet / ellenőrizd a projektmappát; a mérés nem indulhat.");
    }
    nextAction.setColour(juce::Label::backgroundColourId, saveError.isNotEmpty() ? juce::Colour(0xff4c3027)
                                                          : testing              ? juce::Colour(0xff233b59)
                                                          : finished             ? juce::Colour(0xff203b38)
                                                                                 : juce::Colour(0xff1e2d40));
    nextAction.setText(action, juce::dontSendNotification);
    resized();
    repaint();
}
void HardwareSetupComponent::requestStart()
{
    if (!start.isEnabled())
        return;
    permissionPending = true;
    updateControls();
    juce::Component::SafePointer<HardwareSetupComponent> safe(this);
    requestAudioInputPermission(
        [safe](bool granted)
        {
            juce::MessageManager::callAsync(
                [safe, granted]
                {
                    if (!safe)
                        return;
                    safe->permissionPending = false;
                    if (granted)
                        safe->beginTest();
                    else
                    {
                        safe->results.setText(
                            juce::String::fromUTF8("A bemenet rögzítéséhez mikrofon/audio-bemenet "
                                                   "engedély szükséges a rendszerbeállításokban."));
                        safe->updateControls();
                    }
                });
        });
}
void HardwareSetupComponent::beginTest()
{
    try
    {
        require(!bool(workflow.state["initializationRequired"]),
                "Device initialization and plan approval required");
        if (genericDevice(workflow.state))
        {
            require(
                workflow.hasScenario() && bool(workflow.state["deviceSetup"]["controlsConfirmed"]) &&
                    bool(workflow.state["deviceSetup"]["routeConfirmed"]) &&
                    juce::JSON::toString(workflow.state["approvedDeviceSetup"]) ==
                        juce::JSON::toString(workflow.state["deviceSetup"]),
                "Confirm device controls, return route and approve the matching plan before emitting audio");
            require(wiring.getToggleState() && monitoring.getToggleState() && levels.getToggleState(),
                    "Physical setup confirmations required");
        }
        require(audio != nullptr, juce::String::fromUTF8("Nincs audioeszköz"));
        completedMode = 0;
        hasPlot = false;
        resultPlot.clear();
        inputPeak.store(0);
        resultSummary.setText(juce::String::fromUTF8("Új felvétel — az eredmény automatikusan megjelenik."),
                              juce::dontSendNotification);
        runMode = measurementMode.getSelectedId();
        set(workflow.state, "phase", "capturing");
        set(workflow.state, "captureFolder", "");
        saveProgress();
        require(saveError.isEmpty(), saveError);
        cancelWork = false;
        if (runMode == 1)
        {
            bypassCheck = juce::var();
            bypassFolder = juce::File{};
        }
        if (runMode > 1 && workflow.hasScenario())
        {
            require(activeScenarioStep.isObject() &&
                        activeScenarioStep["id"] == workflow.nextScenarioStep()["id"] &&
                        int(activeScenarioStep["mode"]) == runMode,
                    "Scenario step changed; select the next step again");
        }
        if (runMode > 1)
            require(bypassCheck["status"].toString() == "passed" &&
                        (genericDevice(workflow.state) || preparedMode == runMode),
                    "Fresh bypass check and prepared plugin required");
        juce::BigInteger ins, outs;
        ins.setBit(input.getSelectedId() - 1);
        outs.setBit(output.getSelectedId() - 1);
        auto error = audio->open(ins, outs, 48000, 256);
        require(error.isEmpty(), error);
        require(audio->getActiveInputChannels() == ins && audio->getActiveOutputChannels() == outs,
                juce::String::fromUTF8("Az eszköz eltérő csatornákat nyitott meg; a teszt nem indul."));
        const auto sampleRate = audio->getCurrentSampleRate();
        if (runMode > 1)
            require(sampleRate == static_cast<double>(bypassCheck["sampleRate"]),
                    "Sample rate changed; repeat bypass check");
        set(workflow.state, "storageRun",
            SessionStore::runPath(workflow.state, workflow.file.getFileNameWithoutExtension()));
        workflow.save();
        runner.start(sampleRate, 0, 0);
        initialXruns = audio->getXRunCount();
        deviceFault = false;
        runMetadata = object();
        set(runMetadata, "schemaVersion", 1);
        set(runMetadata, "captureOnly", genericDevice(workflow.state));
        set(runMetadata, "deviceSetup", workflow.state["deviceSetup"].clone());
        set(runMetadata, "panelImages", workflow.state["panelImages"].clone());
        set(runMetadata, "calibrationInstructions", workflow.state["scenario"]["calibration"]);
        set(runMetadata, "workflowId", workflow.file.getFileNameWithoutExtension());
        set(runMetadata, "createdUtc", utcNow());
        set(runMetadata, "device", audio->getName());
        set(runMetadata, "driver", driver.getText());
        set(runMetadata, "inputChannel", input.getSelectedId());
        set(runMetadata, "outputChannel", output.getSelectedId());
        set(runMetadata, "inputName", input.getText());
        set(runMetadata, "outputName", output.getText());
        set(runMetadata, "sampleRate", sampleRate);
        set(runMetadata, "blockSize", audio->getCurrentBufferSizeSamples());
        set(runMetadata, "outputPeakDbFS", -36);
        set(runMetadata, "profile",
            runMode == 1 ? "B7K full bypass"
                         : (runMode == 2 ? "B7K clean EQ / DarkBass" : "B7K drive / DarkBass"));
        set(runMetadata, "measurementMode", runMode);
        set(runMetadata, "scenarioRevision", workflow.state["scenarioRevision"]);
        set(runMetadata, "scenarioStep", activeScenarioStep.clone());
        set(runMetadata, "operatorInstructionsVersion", 2);
        set(runMetadata, "requestedSettings", measurementInstructions());
        const auto deviations = physicalNotes.getText().trim();
        const bool requestedSettingsMatch = runMode > 1 && deviations.isEmpty();
        set(runMetadata, "operatorNotes", runMode > 1 ? deviations : juce::String{});
        set(runMetadata, "operatorConfirmedRequestedSettingsExactly", requestedSettingsMatch);
        set(runMetadata, "operatorPhysicalSettings",
            runMode == 1 ? juce::String("Full bypass; operator confirmed routing and low levels")
            : requestedSettingsMatch
                ? juce::String::fromUTF8(
                      "A kezelő visszaigazolta: pontosan a kért pedálállások, eltérés nélkül.\n") +
                      (activeScenarioStep.isObject() ? activeScenarioStep["settings"].toString()
                                                     : measurementInstructions())
                : deviations);
        if (runMode > 1)
        {
            set(runMetadata, "pluginParameters", prepared.parameters);
            set(runMetadata, "interfaceAndReampUnchangedConfirmed", chainUnchanged.getToggleState());
        }
        set(runMetadata, "wiringConfirmed", true);
        set(runMetadata, "monitoringDisabledConfirmed", true);
        if (genericDevice(workflow.state))
        {
            set(runMetadata, "profile", workflow.state["deviceSetup"]["type"]);
            if (runMode == 1)
                set(runMetadata, "operatorPhysicalSettings", workflow.state["scenario"]["calibration"]);
            set(runMetadata, "baselineKind", "recorded_low_gain_state_not_bypass");
        }
        set(runMetadata, "bypassAndLowLevelsConfirmed", runMode == 1);
        set(runMetadata, "requestedPedalStateConfirmed", true);
        set(runMetadata, "analogLevelCalibration", "not performed; physical knob positions are not measured");
        startedAt = juce::Time::getMillisecondCounterHiRes();
        testing = true;
        results.setText(juce::String::fromUTF8("Teszt folyamatban. Ne állíts a potmétereken."));
        audio->start(this);
        updateControls();
    }
    catch (const std::exception &e)
    {
        runner.cancel();
        if (audio)
        {
            audio->stop();
            audio->close();
        }
        testing = false;
        bypassCheck = juce::var();
        set(workflow.state, "phase", "interrupted");
        results.setText(juce::String::fromUTF8(e.what()));
        updateControls();
    }
}
void HardwareSetupComponent::finishTest()
{
    if (!testing)
        return;
    int finalXruns = -1;
    if (audio)
    {
        finalXruns = audio->getXRunCount();
        audio->stop();
        audio->close();
    }
    if (initialXruns >= 0 && finalXruns > initialXruns)
        deviceFault = true;
    set(runMetadata, "initialXruns", initialXruns);
    set(runMetadata, "finalXruns", finalXruns);
    testing = false;
    analysing = true;
    progress.setText(juce::String::fromUTF8("A HANGKIKÜLDÉS LEÁLLT — mentés és kiértékelés folyamatban."),
                     juce::dontSendNotification);
    const auto state = runner.state();
    const bool deviceFailed = deviceFault.load();
    if (deviceFailed || state != SetupProbeRunner::State::complete)
    {
        bypassCheck = juce::var();
        bypassFolder = juce::File{};
    }
    auto probe = runner.stimulus(), capture = runner.recording();
    auto metadata = runMetadata;
    set(metadata, "capturedSamples", runner.capturedSamples());
    set(metadata, "deviceError", deviceFailed);
    auto root = projectRoot == juce::File{}
                    ? juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("Moth DSP Lab")
                    : projectRoot;
    reportFolder = SessionStore::runFolder(root, workflow.file.getFileNameWithoutExtension())
                       .getChildFile("captures")
                       .getChildFile(utcNow().removeCharacters(":-") + "-" + juce::Uuid().toString());
    auto folder = reportFolder;
    set(workflow.state, "phase", "processing");
    set(workflow.state, "captureFolder", folder.getFullPathName());
    const auto activeRun = runMode > 1;
    const auto check = bypassCheck;
    const auto checkFolder = bypassFolder;
    const auto target = prepared.target;
    completedComparison = std::make_shared<juce::File>();
    auto completed = completedComparison;
    analysis = std::async(
        std::launch::async,
        [probe = std::move(probe), capture = std::move(capture), metadata, folder, state, deviceFailed,
         activeRun, check, checkFolder, target, root, completed, this]() mutable
        {
            juce::String text;
            if (state == SetupProbeRunner::State::complete && !deviceFailed)
            {
                if (activeRun)
                {
                    // Preserve the raw physical capture before third-party code runs.
                    require(folder.createDirectory().wasOk(), "Cannot preserve active capture");
                    set(metadata, "status", "captured");
                    writeJson(folder.getChildFile("setup-check.json"), metadata);
                    writeWav(folder.getChildFile("probe.wav"), probe);
                    writeWav(folder.getChildFile("capture.wav"), capture);
                    auto recovery = object();
                    set(recovery, "candidate", target.toJson());
                    set(recovery, "metadata", metadata);
                    set(recovery, "bypassCheck", check);
                    set(recovery, "bypassFolder", checkFolder.getFullPathName());
                    writeJson(folder.getChildFile("recovery-job.json"), recovery);
                    *completed = compareHardwareCapture(
                        juce::File::getSpecialLocation(juce::File::currentExecutableFile), root, probe,
                        capture, target, metadata, check, checkFolder, &cancelWork);
                    auto summary = readJson(completed->getChildFile("analysis_summary.json"));
                    return (bool(metadata["captureOnly"])
                                ? juce::String::fromUTF8(
                                      "FELVÉTEL MENTVE — összevetés a rögzített "
                                      "alapreferenciával\n\nAlapreferencia − felvétel RMS: ")
                                : juce::String::fromUTF8("MÉRÉS ÉS DARKBASS RENDER ELKÉSZÜLT\n\nB−A RMS: ")) +
                           juce::String(static_cast<double>(summary["gainDifferenceDb"]), 2) +
                           juce::String::fromUTF8(" dB\nResidual: ") +
                           juce::String(static_cast<double>(summary["residualRmsDbFS"]), 2) +
                           juce::String::fromUTF8(
                               " dBFS\n\nNincs abszolút szintkalibráció: ez diagnosztikai összevetés, nem "
                               "hasonlósági pontszám.\nA hullámforma és a spektrum az Összehasonlítás "
                               "megnyitása gombbal érhető el.\n\nMentés: ") +
                           completed->getFullPathName() +
                           (bool(metadata["captureOnly"])
                                ? juce::String::fromUTF8("\n\nKövetkező: a jóváhagyott terv következő "
                                                         "állása; semmi nem indul automatikusan.")
                            : static_cast<int>(metadata["measurementMode"]) == 2
                                ? juce::String::fromUTF8(
                                      "\n\nKövetkező: a 3. Aktív pedál / drive lépés. Kövesd az új "
                                      "beállításkérést; semmi nem indul automatikusan.")
                                : juce::String::fromUTF8(
                                      "\n\nA drive mérési pont elkészült. További jelszintekhez előbb "
                                      "analóg szintkalibráció szükséges."));
                }
                auto result = analyseSetupCapture(probe, capture);
                set(metadata, "status", result.passed ? "passed" : "needs_attention");
                set(metadata, "analysis", result.toJson());
                text = (result.passed ? juce::String::fromUTF8("SIKERES JELÚT-ELLENŐRZÉS")
                                      : juce::String::fromUTF8("BEAVATKOZÁS SZÜKSÉGES")) +
                       juce::String(juce::String::fromUTF8("\n\nBemeneti csúcs: ")) +
                       juce::String(result.peakDb, 1) + " dBFS | Zaj: " + juce::String(result.noiseDb, 1) +
                       " dBFS\nJel/zaj: " + juce::String(result.snrDb, 1) +
                       juce::String::fromUTF8(" dB | Korreláció: ") + juce::String(result.correlation, 3);
                if (result.correlation >= .7)
                    text += juce::String::fromUTF8("\nBecsült körút-késleltetés: ") +
                            juce::String(result.latencySamples / probe.sampleRate * 1000, 2) +
                            juce::String::fromUTF8(" ms | Ismétlés eltérése: ") +
                            juce::String(result.repeatDriftSamples, 1) + " minta";
                for (const auto &issue : result.issues)
                    text += juce::String::fromUTF8("\n• ") + issue;
                text += juce::String::fromUTF8(
                    "\n\nEz jelút-ellenőrzés, nem teljes szintkalibráció. Bekötés vagy gain "
                    "változtatása után ismételd meg.");
            }
            else
            {
                text = deviceFailed ? juce::String::fromUTF8(
                                          "Audioeszköz-hiba vagy kapcsolatvesztés. A teszt érvénytelen.")
                                    : failure(state);
                set(metadata, "status", "aborted");
                set(metadata, "reason", text);
            }
            require(folder.createDirectory().wasOk(), juce::String::fromUTF8("Nem menthető a setup-riport"));
            writeWav(folder.getChildFile("probe.wav"), probe);
            writeWav(folder.getChildFile("capture.wav"), capture);
            writeJson(folder.getChildFile("setup-check.json"), metadata);
            return text + "\n\nRiport: " + folder.getFullPathName();
        });
    updateControls();
}
void HardwareSetupComponent::timerCallback()
{
    if (preparing && preparation.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    {
        try
        {
            prepared = preparation.get();
            preparedMode = measurementMode.getSelectedId();
            results.setText(
                juce::String::fromUTF8("DarkBass profil betöltve. Állítsd be a pedált az útmutató szerint, "
                                       "csak az eltéréseket írd be, és jelöld be a visszaigazolásokat."));
        }
        catch (const std::exception &e)
        {
            preparedMode = 0;
            results.setText(juce::String::fromUTF8(e.what()));
        }
        preparing = false;
        set(workflow.state, "phase", "idle");
        updateControls();
    }
    if (testing)
    {
        captureProgress =
            juce::jlimit(0.0, 1.0, runner.capturedSamples() / (runner.stimulus().sampleRate * 2.6));
        captureBar.setTextToDisplay(juce::String(captureProgress * 100, 0) + "%");
        inputLevel.setText(juce::String::fromUTF8("Bemeneti csúcs: ") +
                               juce::String(juce::Decibels::gainToDecibels(inputPeak.load(), -100.f), 1) +
                               " dBFS",
                           juce::dontSendNotification);
        progress.setText(
            "Teszt: " + juce::String(runner.capturedSamples() / runner.stimulus().sampleRate, 1) +
                juce::String::fromUTF8(" / 2,6 s — kimeneti csúcs legfeljebb -36 dBFS"),
            juce::dontSendNotification);
        if (juce::Time::getMillisecondCounterHiRes() - startedAt > 8000)
        {
            deviceFault = true;
            runner.cancel();
        }
        if (runner.state() != SetupProbeRunner::State::running || deviceFault.load())
            finishTest();
    }
    if (analysing && analysis.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    {
        try
        {
            results.setText(analysis.get());
            if (completedComparison && completedComparison->isDirectory())
            {
                completedMode = runMode;
                comparisonFolder = *completedComparison;
                reportFolder = comparisonFolder;
                workflow.recordScenarioResult(
                    readJson(comparisonFolder.getChildFile("manifest.json"))["hardware"], comparisonFolder);
                workflow.recordResult(runMode, comparisonFolder);
                if (resultHistory.indexOfItemId(runMode) < 0)
                    resultHistory.addItem(
                        GuidedWorkflow::modeName(runMode) + juce::String::fromUTF8(" — kész"), runMode);
                refreshScenarioResults();
                int selected = runMode;
                if (workflow.hasScenario())
                {
                    auto steps = workflow.state["scenario"]["steps"];
                    for (int i = 0; i < steps.size(); ++i)
                        if (steps[i]["id"] == readJson(comparisonFolder.getChildFile(
                                                  "manifest.json"))["hardware"]["scenarioStep"]["id"])
                            selected = 100 + i;
                }
                resultHistory.setSelectedId(selected, juce::dontSendNotification);
                results.setText(GuidedWorkflow::resultDescription(comparisonFolder));
                presentResult();
            }
            else if (runMode == 1 && reportFolder.getChildFile("setup-check.json").existsAsFile())
            {
                auto check = readJson(reportFolder.getChildFile("setup-check.json"));
                resultSummary.setText((check["status"].toString() == "passed"
                                           ? juce::String::fromUTF8("BYPASS: SIKERES — a jelút használható.")
                                           : juce::String::fromUTF8(
                                                 "BYPASS: ellenőrzés szükséges. A részletek lent láthatók.")),
                                      juce::dontSendNotification);
                if (genericDevice(workflow.state))
                    resultSummary.setText(
                        check["status"].toString() == "passed"
                            ? juce::String::fromUTF8(
                                  "ALAPREFERENCIA: sikeres jelút-ellenőrzés; nem abszolút szintkalibráció.")
                            : juce::String::fromUTF8(
                                  "ALAPREFERENCIA: ellenőrzés szükséges; nézd meg a részleteket."),
                        juce::dontSendNotification);
                if (check["status"].toString() == "passed")
                {
                    completedMode = 1;
                    bypassCheck = check;
                    bypassFolder = reportFolder;
                    workflow.recordResult(1, reportFolder);
                    results.setText(
                        results.getText() +
                        (genericDevice(workflow.state)
                             ? juce::String::fromUTF8("\n\nAz alapreferencia mentve. Következő: a "
                                                      "jóváhagyott mérési terv első állása.")
                             : juce::String::fromUTF8("\n\nKövetkező: válaszd a 2. Aktív pedál / tiszta EQ "
                                                      "lépést. A felület megmutatja az új pedálállásokat.")));
                }
            }
            levels.setToggleState(false, juce::dontSendNotification);
        }
        catch (const std::exception &e)
        {
            results.setText(juce::String(juce::String::fromUTF8("A teszt nem értékelhető/menthető: ")) +
                            e.what());
        }
        analysing = false;
        set(workflow.state, "phase",
            completedMode > 0 ? (workflow.nextMeasurement() == 0 ? "complete" : "idle") : "interrupted");
        refreshHistory();
        progress.setText(juce::String::fromUTF8("Teszt lezárva. Nincs aktív audio-kiküldés."),
                         juce::dontSendNotification);
        updateControls();
    }
}
void HardwareSetupComponent::audioDeviceIOCallbackWithContext(const float *const *in, int ni,
                                                              float *const *out, int no, int count,
                                                              const juce::AudioIODeviceCallbackContext &)
{
    // Backends may pack active channels or leave null slots at physical indices.
    const float *selectedInput = nullptr;
    float *selectedOutput = nullptr;
    int activeInputs = 0, activeOutputs = 0;
    for (int c = 0; c < ni; ++c)
        if (in[c])
        {
            selectedInput = in[c];
            ++activeInputs;
        }
    for (int c = 0; c < no; ++c)
        if (out[c])
        {
            juce::FloatVectorOperations::clear(out[c], count);
            selectedOutput = out[c];
            ++activeOutputs;
        }
    if (activeInputs != 1 || activeOutputs != 1)
    {
        runner.process(nullptr, 0, nullptr, 0, count);
        return;
    }
    float peak = 0;
    for (int i = 0; i < count; ++i)
        peak = juce::jmax(peak, std::abs(selectedInput[i]));
    inputPeak.store(juce::jmax(inputPeak.load(std::memory_order_relaxed), peak), std::memory_order_relaxed);
    runner.process(&selectedInput, 1, &selectedOutput, 1, count);
}
void HardwareSetupComponent::audioDeviceAboutToStart(juce::AudioIODevice *device)
{
    if (device->getCurrentSampleRate() != runner.stimulus().sampleRate)
    {
        deviceFault = true;
        runner.cancel();
    }
}
void HardwareSetupComponent::audioDeviceStopped()
{
    runner.cancel();
}
void HardwareSetupComponent::audioDeviceError(const juce::String &)
{
    deviceFault = true;
    runner.cancel();
}
void HardwareSetupComponent::paint(juce::Graphics &g)
{
    g.fillAll(LabTheme::background());
    for (auto card : {taskCard, feedbackCard})
    {
        g.setColour(LabTheme::panel());
        g.fillRoundedRectangle(card.toFloat(), 12);
        g.setColour(juce::Colour(0xff2b394e));
        g.drawRoundedRectangle(card.toFloat().reduced(.5f), 12, 1);
    }
    if (workflow.hasScenario())
    {
        g.setColour(LabTheme::accent());
        g.setFont(juce::FontOptions(16.f));
        g.drawText((genericDevice(workflow.state)
                        ? juce::String::fromUTF8("Terv jóváhagyva → Alapreferencia → ")
                        : juce::String::fromUTF8("Terv jóváhagyva → Bypass ellenőrzés → ")) +
                       juce::String(workflow.scenarioCompleted()) + "/" +
                       juce::String(workflow.state["scenario"]["steps"].size()) +
                       juce::String::fromUTF8(" mérés kész → Eredmények értékelése"),
                   stagesArea.reduced(8), juce::Justification::centredLeft);
        return;
    }
    const juce::StringArray names{
        juce::String::fromUTF8("Bekötés"), genericDevice(workflow.state) ? "Alapreferencia" : "Bypass",
        genericDevice(workflow.state) ? "Felvétel" : "Tiszta / EQ",
        genericDevice(workflow.state) ? "Ismétlés" : "Drive", juce::String::fromUTF8("Eredmények")};
    auto track = stagesArea;
    const auto width = track.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto segment = track.removeFromLeft(i == 4 ? track.getWidth() : width).reduced(3, 4);
        const bool done = i == 0   ? currentStage > 0
                          : i == 1 ? (bypassCurrent || (cleanComplete && driveComplete))
                          : i == 2 ? cleanComplete
                          : i == 3 ? driveComplete
                                   : cleanComplete && driveComplete;
        const bool current = currentStage == i;
        g.setColour(current ? juce::Colour(0xff25473e) : juce::Colour(0xff1c2634));
        g.fillRoundedRectangle(segment.toFloat(), 7);
        g.setColour(current ? LabTheme::accent() : done ? juce::Colour(0xffd4e8df) : LabTheme::muted());
        g.setFont(juce::FontOptions(13.f));
        g.drawText((done ? juce::String::fromUTF8("✓ ") : juce::String(i + 1) + ". ") + names[i],
                   segment.reduced(10, 0), juce::Justification::centredLeft);
        if (current)
            g.fillRect(segment.getX() + 10, segment.getBottom() - 3, segment.getWidth() - 20, 2);
    }
}
void HardwareSetupComponent::resized()
{
    if (collection)
    {
        collection->setBounds(getLocalBounds());
        return;
    }
    if (planner)
    {
        planner->setBounds(getLocalBounds());
        return;
    }
    auto area = getLocalBounds().reduced(8);
    auto libraryRow = area.removeFromTop(28);
    exportHandoff.setBounds(libraryRow.removeFromRight(190));
    openRunFolder.setBounds(libraryRow.removeFromRight(210));
    mergeRuns.setBounds(libraryRow.removeFromRight(200));
    libraryLabel.setBounds(libraryRow);
    auto row = area.removeFromTop(34);
    newWorkflow.setBounds(row.removeFromRight(110).reduced(2));
    planButton.setBounds(row.removeFromRight(240).reduced(2));
    sessionName.setBounds(row.removeFromLeft(juce::jmin(340, row.getWidth() / 2)).reduced(2));
    workflowHistory.setBounds(row.reduced(2));
    stagesArea = area.removeFromTop(44);
    nextAction.setBounds(area.removeFromTop(50).reduced(2));
    auto footer = area.removeFromBottom(28);
    excludeResult.setBounds(footer.removeFromRight(260));
    savedStatus.setBounds(footer);
    area.removeFromTop(10);
    taskCard = area.removeFromLeft(juce::jmin(480, area.getWidth() * 44 / 100));
    area.removeFromLeft(14);
    feedbackCard = area;
    auto left = taskCard.reduced(16);
    auto right = feedbackCard.reduced(16);
    taskTitle.setBounds(left.removeFromTop(30));
    feedbackTitle.setBounds(right.removeFromTop(30));
    const bool finished = workflow.nextMeasurement() == 0;
    const bool active = measurementMode.getSelectedId() > 1;
    const bool guide = !active && step < 3;
    duplicate.setVisible(finished);
    if (finished)
    {
        duplicate.setBounds(left.removeFromBottom(40));
        LabTheme::primary(duplicate);
    }
    tutorial.setBounds(left.removeFromTop(finished ? 180 : guide ? 210 : active ? 160 : 118));
    left.removeFromTop(6);
    if (retryRender.isVisible())
        retryRender.setBounds(left.removeFromTop(34));
    if (!finished)
    {
        if (active)
        {
            if (preparePlugin.isVisible())
                preparePlugin.setBounds(left.removeFromTop(30));
            notesLabel.setBounds(left.removeFromTop(18));
            physicalNotes.setBounds(left.removeFromTop(48));
            chainUnchanged.setBounds(left.removeFromTop(26));
        }
        else
        {
            row = left.removeFromTop(30);
            previous.setBounds(row.removeFromLeft(80).reduced(2));
            next.setBounds(row.removeFromLeft(106).reduced(2));
            if (!guide)
            {
                rescan.setBounds(row.reduced(2));
                route.setBounds(left.removeFromTop(20));
                row = left.removeFromTop(30);
                driver.setBounds(row.removeFromLeft(100).reduced(2));
                interface.setBounds(row.reduced(2));
                row = left.removeFromTop(30);
                input.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
                output.setBounds(row.reduced(2));
                wiring.setBounds(left.removeFromTop(26));
                monitoring.setBounds(left.removeFromTop(26));
            }
        }
        if (!guide)
            levels.setBounds(left.removeFromTop(26));
        left.removeFromTop(4);
        row = left.removeFromTop(38);
        if (advance.isVisible())
            advance.setBounds(row);
        else if (!guide)
        {
            start.setBounds(
                row.removeFromLeft(isWorking() ? juce::jmax(200, row.getWidth() - 135) : row.getWidth())
                    .reduced(2));
            stop.setBounds(row.reduced(2));
        }
        if (repeatBypass.isVisible())
            repeatBypass.setBounds(left.removeFromTop(28).reduced(2));
        start.setVisible(!guide && !advance.isVisible());
        stop.setVisible(isWorking());
    }
    captureBar.setVisible(isWorking());
    inputLevel.setVisible(testing || inputPeak.load() > 0);
    if (captureBar.isVisible())
        captureBar.setBounds(right.removeFromTop(24));
    if (inputLevel.isVisible())
        inputLevel.setBounds(right.removeFromTop(22));
    progress.setBounds(right.removeFromTop(34));
    row = right.removeFromTop(32);
    resultHistory.setBounds(row.removeFromLeft(juce::jmin(220, row.getWidth() / 2)).reduced(2));
    details.setBounds(row.reduced(2));
    resultSummary.setBounds(right.removeFromTop(hasPlot ? 76 : 30));
    row = right.removeFromBottom(30);
    difference.setBounds(row.removeFromLeft(180));
    showReport.setBounds(row.reduced(2));
    details.setEnabled(hasPlot);
    const bool showPlot = hasPlot && !details.getToggleState();
    resultPlot.setVisible(showPlot);
    difference.setVisible(showPlot);
    results.setVisible(!showPlot);
    resultPlot.setBounds(right);
    results.setBounds(right);
}
void HardwareSetupComponent::refreshScenarioResults()
{
    if (!workflow.hasScenario())
        return;
    resultHistory.clear(juce::dontSendNotification);
    auto steps = workflow.state["scenario"]["steps"];
    for (int i = 0; i < steps.size(); ++i)
    {
        auto path = workflow.state["scenarioResults"][juce::Identifier(steps[i]["id"].toString())].toString();
        if (juce::File::isAbsolutePath(path) && juce::File(path).getChildFile("manifest.json").existsAsFile())
            resultHistory.addItem(juce::String(i + 1) + ". " + steps[i]["title"].toString(), 100 + i);
    }
}
void HardwareSetupComponent::openPlanner()
{
    if (isWorking())
        return;
    auto safe = juce::Component::SafePointer<HardwareSetupComponent>(this);
    planner = std::make_unique<PlannerPanel>(
        workflow,
        [safe]
        {
            juce::MessageManager::callAsync(
                [safe]
                {
                    if (safe)
                        safe->closePlanner(false);
                });
        },
        [safe]
        {
            juce::MessageManager::callAsync(
                [safe]
                {
                    if (safe)
                        safe->closePlanner(true);
                });
        });
    planner->setComponentID("planner-panel");
    addAndMakeVisible(*planner);
    planner->enterModalState(false);
    planner->toFront(true);
    resized();
}
void HardwareSetupComponent::closePlanner(bool approved)
{
    if (planner)
        planner->exitModalState(0);
    planner.reset();
    if (approved)
    {
        refreshScenarioResults();
        activeScenarioStep = juce::var();
        measurementMode.setSelectedId(1, juce::dontSendNotification);
        step = 3;
        modeChanged(); // New approval always requires a fresh physical bypass check.
    }
    updateControls();
    resized();
}
} // namespace lab
