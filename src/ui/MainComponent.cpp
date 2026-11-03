#include "MainComponent.h"
#include "HardwareSetupComponent.h"
namespace lab
{
MainComponent::MainComponent()
{
    setLookAndFeel(&theme);
    setSize(1240, 880);
    title.setText("MOTH DSP LAB", juce::dontSendNotification);
    title.setFont(juce::FontOptions(26.f));
    description.setText(juce::String::fromUTF8("Türkiz: referencia / pedál   •   Narancs: DarkBass   •   A "
                                               "mérések és a folyamat helyben mentődnek"),
                        juce::dontSendNotification);
    settingsLabel.setText("Sample rate          Block size          Channels             Tail (seconds)",
                          juce::dontSendNotification);
    status.setText(
        juce::String::fromUTF8("A vezetett mérés automatikusan menti a haladást és az eredményeket."),
        juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(0xff4fdfc0));
    for (auto *c : std::vector<juce::Component *>{&offlineMode,   &sessionHeading,
                                                  &nextAction,    &title,
                                                  &description,   &status,
                                                  &settingsLabel, &chooseA,
                                                  &chooseB,       &editA,
                                                  &editB,         &chooseInput,
                                                  &chooseProject, &run,
                                                  &open,          &playA,
                                                  &playB,         &stop,
                                                  &rate,          &block,
                                                  &channels,      &tail,
                                                  &plot,          &differenceView,
                                                  &hardware})
        addAndMakeVisible(c);
    sessionHeading.setFont(juce::FontOptions(19.f));
    nextAction.setColour(juce::Label::backgroundColourId, juce::Colour(0xff234438));
    offlineMode.onClick = [this]
    {
        if (setupPanel && setupPanel->isWorking())
            offlineMode.setToggleState(false, juce::dontSendNotification);
        transport.stop();
        device.removeAudioCallback(&player);
        device.closeAudioDevice();
        refresh();
        resized();
    };
    open.setButtonText(juce::String::fromUTF8("Mentett eredmény megnyitása"));
    stop.setButtonText(juce::String::fromUTF8("Leállítás / megszakítás"));
    differenceView.setButtonText(juce::String::fromUTF8("Különbségjel / spektrumeltérés"));
    rate.addItem("44100 Hz", 44100);
    rate.addItem("48000 Hz", 48000);
    rate.addItem("96000 Hz", 96000);
    rate.addItem("192000 Hz", 192000);
    rate.setSelectedId(48000);
    for (int n : {64, 128, 256, 512, 1024, 2048})
        block.addItem(juce::String(n), n);
    block.setSelectedId(512);
    channels.addItem("Mono", 1);
    channels.addItem("Stereo", 2);
    channels.setSelectedId(2);
    tail.setText("2.0");
    tail.setInputRestrictions(5, "0123456789.");
    hardware.setButtonText(juce::String::fromUTF8("Mérés"));
    offlineMode.setButtonText("Plugin A/B");
    hardware.onClick = [this]
    {
        transport.stop();
        device.removeAudioCallback(&player);
        device.closeAudioDevice();
        offlineMode.setToggleState(false, juce::dontSendNotification);
        refresh();
        resized();
    };
    chooseA.onClick = [this] { pickPlugin(true); };
    chooseB.onClick = [this] { pickPlugin(false); };
    editA.onClick = [this] { editPlugin(true); };
    editB.onClick = [this] { editPlugin(false); };
    chooseInput.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Input WAV", input, "*.wav");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser &c)
            {
                if (c.getResult() == juce::File{})
                    return;
                try
                {
                    auto data = readWav(c.getResult());
                    input = c.getResult();
                    int hz = static_cast<int>(data.sampleRate);
                    if (rate.indexOfItemId(hz) < 0)
                        rate.addItem(juce::String(hz) + " Hz", hz);
                    rate.setSelectedId(hz);
                    chooseInput.setButtonText(input.getFileName());
                    refresh();
                }
                catch (const std::exception &e)
                {
                    status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
                }
            });
    };
    chooseProject.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Project folder", project);
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser &c)
            {
                if (c.getResult() != juce::File{})
                {
                    project = c.getResult();
                    chooseProject.setButtonText(juce::String::fromUTF8("Projekt: ") + project.getFileName());
                    chooseProject.setTooltip(project.getFullPathName());
                    rememberProject();
                    attachWorkflow();
                    refresh();
                }
            });
    };
    run.onClick = [this]
    {
        try
        {
            auto s = settings();
            auto ta = a, tb = b;
            auto in = input, root = project;
            auto result = std::make_shared<juce::File>();
            startTask(
                [=, this]
                {
                    MeasurementEngine engine(
                        juce::File::getSpecialLocation(juce::File::currentExecutableFile));
                    *result = engine.compare(root, in, ta, tb, s, &cancelRequested);
                    return "Measurement saved";
                },
                [this, result] { loadSession(*result); });
        }
        catch (const std::exception &e)
        {
            status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
        }
    };
    open.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Select saved session manifest.json", project,
                                                      "manifest.json");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser &c)
            {
                if (c.getResult() != juce::File{})
                {
                    try
                    {
                        loadSession(c.getResult().getParentDirectory());
                    }
                    catch (const std::exception &e)
                    {
                        status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
                    }
                }
            });
    };
    playA.onClick = [this] { play("reference"); };
    playB.onClick = [this] { play("candidate"); };
    stop.onClick = [this]
    {
        transport.stop();
        if (busy)
            cancelRequested = true;
    };
    differenceView.onClick = [this] { plot.setDifference(differenceView.getToggleState()); };
    readAhead.startThread();
    player.setSource(&transport);
    project = GuidedWorkflow::defaultProject();
    try
    {
        const auto prefs = GuidedWorkflow::defaultProject().getChildFile("last-project.json");
        if (prefs.existsAsFile())
        {
            auto path = readJson(prefs)["project"].toString();
            if (juce::File::isAbsolutePath(path) && juce::File(path).isDirectory())
                project = juce::File(path);
        }
        chooseProject.setButtonText(juce::String::fromUTF8("Projekt: ") + project.getFileName());
        chooseProject.setTooltip(project.getFullPathName());
        auto folders = SessionStore::sessions(project);
        std::sort(folders.begin(), folders.end(),
                  [](const auto &a, const auto &b) { return a.getFileName() > b.getFileName(); });
        for (const auto &folder : folders)
        {
            try
            {
                auto m = SessionStore::load(folder);
                if (m["reference"]["kind"].toString() == "hardware")
                {
                    loadSession(folder);
                    break;
                }
            }
            catch (...)
            {
            }
        }
    }
    catch (const std::exception &e)
    {
        status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
    }
    attachWorkflow();
    refresh();
    startTimerHz(4);
}
MainComponent::~MainComponent()
{
    stopTimer();
    setLookAndFeel(nullptr);
    setupPanel.reset();
    cancelRequested = true;
    if (task.joinable())
        task.join();
    device.removeAudioCallback(&player);
    player.setSource(nullptr);
    transport.setSource(nullptr);
}
RenderSettings MainComponent::settings() const
{
    RenderSettings s;
    s.sampleRate = rate.getSelectedId();
    s.blockSize = block.getSelectedId();
    s.channels = channels.getSelectedId();
    s.tailSeconds = tail.getText().getDoubleValue();
    s.validate();
    return s;
}
void MainComponent::startTask(std::function<juce::String()> work, std::function<void()> done)
{
    if (busy)
        return;
    if (task.joinable())
        task.join();
    busy = true;
    cancelRequested = false;
    transport.stop();
    refresh();
    status.setText(juce::String::fromUTF8("Working in isolated plugin process…"), juce::dontSendNotification);
    juce::Component::SafePointer<MainComponent> safe(this);
    task = std::thread(
        [safe, work = std::move(work), done = std::move(done)]
        {
            juce::String message;
            bool ok = true;
            try
            {
                message = work();
            }
            catch (const std::exception &e)
            {
                message = e.what();
                ok = false;
            }
            juce::MessageManager::callAsync(
                [safe, message, ok, done]
                {
                    if (safe)
                    {
                        safe->busy = false;
                        safe->status.setText(message, juce::dontSendNotification);
                        try
                        {
                            if (ok && done)
                                done();
                        }
                        catch (const std::exception &e)
                        {
                            safe->status.setText(juce::String::fromUTF8(e.what()),
                                                 juce::dontSendNotification);
                        }
                        safe->refresh();
                    }
                });
        });
}
void MainComponent::pickPlugin(bool reference)
{
    chooser = std::make_unique<juce::FileChooser>(
        "Select VST3 or installed AU (.component) bundle",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins"),
        juce::String(JUCE_MAC ? "*.vst3;*.component" : "*.vst3"));
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::canSelectDirectories,
        [this, reference](const juce::FileChooser &c)
        {
            auto plugin = c.getResult();
            if (plugin == juce::File{})
                return;
            auto result = std::make_shared<juce::var>();
            startTask(
                [this, plugin, result]
                {
                    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("moth-scan-" + juce::Uuid().toString());
                    require(dir.createDirectory().wasOk(), "Cannot create scan folder");
                    auto file = dir.getChildFile("scan.json");
                    auto job = object();
                    set(job, "action", "scan");
                    set(job, "plugin", plugin.getFullPathName());
                    writeJson(file, job);
                    *result = runWorker(juce::File::getSpecialLocation(juce::File::currentExecutableFile),
                                        file, 30000, &cancelRequested);
                    dir.deleteRecursively();
                    return "Plugin scanned";
                },
                [this, reference, plugin, result]
                {
                    auto *types = (*result)["types"].getArray();
                    require(types && !types->isEmpty(), "No plugin types");
                    juce::PopupMenu menu;
                    for (int i = 0; i < types->size(); ++i)
                        menu.addItem(i + 1, (*types)[i]["name"].toString() + " (" +
                                                (*types)[i]["format"].toString() + ")");
                    auto assign = [this, reference, plugin, result](int id)
                    {
                        if (id <= 0)
                            return;
                        auto type = (*result)["types"][id - 1];
                        if (reference)
                        {
                            hardwareSession = false;
                            session = juce::File{};
                            plot.clear();
                        }
                        auto &target = reference ? a : b;
                        target.plugin = plugin;
                        target.descriptionXml = type["xml"].toString();
                        target.state.reset();
                        (reference ? chooseA : chooseB).setButtonText(type["name"].toString());
                        refresh();
                    };
                    if (types->size() == 1)
                        assign(1);
                    else
                        menu.showMenuAsync({}, assign);
                });
        });
}
void MainComponent::editPlugin(bool reference)
{
    try
    {
        auto s = settings();
        auto target = reference ? a : b;
        auto result = std::make_shared<juce::var>();
        startTask(
            [this, s, target, result]
            {
                auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("moth-edit-" + juce::Uuid().toString());
                require(dir.createDirectory().wasOk(), "Cannot create editor folder");
                auto file = dir.getChildFile("edit.json");
                auto job = object();
                set(job, "action", "edit");
                set(job, "settings", s.toJson());
                set(job, "target", target.toJson());
                writeJson(file, job);
                *result = runWorker(juce::File::getSpecialLocation(juce::File::currentExecutableFile), file,
                                    3600000, &cancelRequested);
                dir.deleteRecursively();
                return "Plugin state saved";
            },
            [this, reference, result]
            {
                require(decodeState((reference ? a : b).state, (*result)["state"].toString()),
                        "Invalid edited state");
            });
    }
    catch (const std::exception &e)
    {
        status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
    }
}
void MainComponent::loadSession(const juce::File &folder)
{
    auto manifest = SessionStore::load(folder);
    if (bool(manifest["hardware"]["captureOnly"]))
    {
        auto previous = project;
        project = SessionStore::projectForSession(folder);
        session = folder;
        if (!setupPanel || previous != project)
            attachWorkflow();
        setupPanel->showSavedResult(folder);
        return;
    }
    auto s = RenderSettings::fromJson(manifest["settings"]);
    auto ta = TargetSpec::fromJson(manifest["reference"]), tb = TargetSpec::fromJson(manifest["candidate"]);
    hardwareSession = manifest["reference"]["kind"].toString() == "hardware";
    sessionHeading.setText(
        juce::String::fromUTF8("ELKÉSZÜLT • ") +
            (hardwareSession
                 ? GuidedWorkflow::modeName(static_cast<int>(manifest["hardware"]["measurementMode"]))
                 : juce::String("Plugin A/B")) +
            " • " +
            juce::Time::fromISO8601(manifest["createdUtc"].toString()).formatted("%Y. %m. %d. %H:%M:%S"),
        juce::dontSendNotification);
    plot.load(folder);
    session = folder;
    a = ta;
    b = tb;
    input = folder.getChildFile("stimulus.wav");
    const auto previousProject = project;
    project = SessionStore::projectForSession(folder);
    if (setupPanel && previousProject != project)
        attachWorkflow();
    if (rate.indexOfItemId(static_cast<int>(s.sampleRate)) < 0)
        rate.addItem(juce::String(s.sampleRate), static_cast<int>(s.sampleRate));
    rate.setSelectedId(static_cast<int>(s.sampleRate));
    if (block.indexOfItemId(s.blockSize) < 0)
        block.addItem(juce::String(s.blockSize), s.blockSize);
    block.setSelectedId(s.blockSize);
    channels.setSelectedId(s.channels);
    tail.setText(juce::String(s.tailSeconds));
    chooseA.setButtonText(hardwareSession ? juce::String("B7K Ultra (hardware)") : a.plugin.getFileName());
    chooseB.setButtonText(b.plugin.getFileName());
    chooseInput.setButtonText(input.getFileName());
    chooseProject.setButtonText(juce::String::fromUTF8("Projekt: ") + project.getFileName());
    chooseProject.setTooltip(project.getFullPathName());
    rememberProject();
    auto summary = readJson(folder.getChildFile("analysis_summary.json"));
    status.setText(
        juce::String::fromUTF8("B−A: ") + juce::String(static_cast<double>(summary["gainDifferenceDb"]), 2) +
            " dB   |   Delay: " + summary["candidateDelaySamples"].toString() + " samples   |   Residual: " +
            juce::String(static_cast<double>(summary["residualRmsDbFS"]), 2) + " dBFS   |   Alignment: " +
            (static_cast<bool>(summary["alignmentReliable"]) ? "reliable" : "uncertain") +
            juce::String::fromUTF8("   |   Peak ≥ 0 dBFS: ") +
            ((static_cast<bool>(summary["referenceClipped"]) ||
              static_cast<bool>(summary["candidateClipped"]))
                 ? "YES"
                 : "no"),
        juce::dontSendNotification);
    if (hardwareSession)
        status.setText(juce::String::fromUTF8("DarkBass − pedál: ") +
                           juce::String(static_cast<double>(summary["gainDifferenceDb"]), 2) +
                           juce::String::fromUTF8(" dB  •  Korreláció: ") +
                           juce::String(static_cast<double>(summary["alignmentCorrelation"]), 3) +
                           juce::String::fromUTF8(static_cast<bool>(summary["alignmentReliable"])
                                                      ? " (megbízható)"
                                                      : " (bizonytalan)") +
                           juce::String::fromUTF8("\nA szintillesztés nincs kalibrálva. Ez diagnosztikai "
                                                  "összevetés, nem pontossági pontszám."),
                       juce::dontSendNotification);
    refresh();
    resized();
}
void MainComponent::play(const juce::String &side)
{
    try
    {
        transport.stop();
        transport.setSource(nullptr);
        playback.reset();
        if (device.getCurrentAudioDevice() == nullptr)
        {
            auto error = device.initialiseWithDefaultDevices(0, 2);
            require(error.isEmpty(), error);
            device.addAudioCallback(&player);
        }
        juce::WavAudioFormat format;
        auto stream = session.getChildFile(side + ".wav").createInputStream();
        require(stream != nullptr, "Missing capture");
        auto *reader = format.createReaderFor(stream.release(), true);
        require(reader != nullptr, "Cannot play capture");
        auto sampleRate = reader->sampleRate;
        playback = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transport.setSource(playback.get(), 32768, &readAhead, sampleRate);
        transport.setGain(0.25f);
        transport.start();
    }
    catch (const std::exception &e)
    {
        status.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
    }
}
void MainComponent::rememberProject()
{
    try
    {
        auto root = GuidedWorkflow::defaultProject();
        require(root.createDirectory().wasOk(), "Cannot save recent project");
        auto prefs = object();
        set(prefs, "project", project.getFullPathName());
        writeJson(root.getChildFile("last-project.json"), prefs);
    }
    catch (const std::exception &e)
    {
        status.setText(juce::String::fromUTF8("A legutóbbi projekt útvonala nem menthető: ") + e.what(),
                       juce::dontSendNotification);
    }
}
void MainComponent::attachWorkflow()
{
    setupPanel = std::make_unique<HardwareSetupComponent>(project);
    addAndMakeVisible(*setupPanel);
    resized();
}
void MainComponent::timerCallback()
{
    const bool measuring = setupPanel && setupPanel->isWorking();
    offlineMode.setEnabled(!busy && !measuring);
    chooseProject.setEnabled(!busy && !measuring);
    stop.setEnabled(busy || transport.isPlaying());
}
void MainComponent::refresh()
{
    const bool advanced = offlineMode.getToggleState();
    for (auto *c : std::vector<juce::Component *>{&chooseA, &chooseB, &chooseInput, &editA, &editB, &rate,
                                                  &block, &channels, &tail, &run, &settingsLabel})
        c->setVisible(advanced);
    nextAction.setText(juce::String::fromUTF8(
                           busy              ? "FELDOLGOZÁS FOLYAMATBAN — várj az eredményre."
                           : hardwareSession ? "MENTETT EREDMÉNY • Nincs hangkiküldés. A teljes sorozathoz "
                                               "és a következő lépéshez: Vezetett mérés / folytatás."
                                             : "KÖVETKEZŐ: Vezetett mérés / folytatás. A varázsló mutatja a "
                                               "bekötést, a pedálállásokat és a mentett folyamatokat."),
                       juce::dontSendNotification);
    stop.setEnabled(busy || transport.isPlaying());
    if (session == juce::File{})
        sessionHeading.setText(juce::String::fromUTF8("Még nincs betöltött eredmény"),
                               juce::dontSendNotification);
    hardware.setEnabled(!busy);
    hardware.setVisible(advanced);
    for (auto *c : std::vector<juce::Component *>{&chooseA, &chooseB, &chooseInput, &chooseProject, &open,
                                                  &rate, &block, &channels, &tail})
        c->setEnabled(!busy);
    editA.setEnabled(!busy && !hardwareSession && !a.descriptionXml.isEmpty());
    editB.setEnabled(!busy && !hardwareSession && !b.descriptionXml.isEmpty());
    run.setEnabled(!busy && !hardwareSession && !a.descriptionXml.isEmpty() && !b.descriptionXml.isEmpty() &&
                   input.existsAsFile() && project.isDirectory());
    playA.setEnabled(!busy && !hardwareSession && session.isDirectory());
    playB.setEnabled(!busy && !hardwareSession && session.isDirectory());
    for (auto *c : std::vector<juce::Component *>{&description, &status, &sessionHeading, &nextAction, &open,
                                                  &playA, &playB, &stop, &differenceView, &plot})
        c->setVisible(advanced);
    if (setupPanel)
        setupPanel->setVisible(!advanced);
    if (setupPanel && setupPanel->isWorking())
        chooseProject.setEnabled(false);
}
void MainComponent::paint(juce::Graphics &g)
{
    g.fillAll(LabTheme::background());
}
void MainComponent::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto header = r.removeFromTop(42);
    title.setBounds(header.removeFromLeft(juce::jmax(290, header.getWidth() - 540)));
    hardware.setBounds(header.removeFromLeft(100).reduced(2));
    offlineMode.setBounds(header.removeFromLeft(140));
    chooseProject.setBounds(header.reduced(2));
    if (!offlineMode.getToggleState())
    {
        if (setupPanel)
            setupPanel->setBounds(r);
        return;
    }
    description.setBounds(r.removeFromTop(26));
    nextAction.setBounds(r.removeFromTop(44));
    auto row = r.removeFromTop(32);
    open.setBounds(row.removeFromLeft(300));
    row = r.removeFromTop(34);
    chooseA.setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(8));
    chooseB.setBounds(row);
    row = r.removeFromTop(30);
    editA.setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(8));
    editB.setBounds(row);
    settingsLabel.setBounds(r.removeFromTop(24));
    row = r.removeFromTop(30);
    rate.setBounds(row.removeFromLeft(120).withTrimmedRight(8));
    block.setBounds(row.removeFromLeft(120).withTrimmedRight(8));
    channels.setBounds(row.removeFromLeft(120).withTrimmedRight(8));
    tail.setBounds(row.removeFromLeft(80));
    chooseInput.setBounds(row.removeFromLeft(210).reduced(2));
    run.setBounds(row.reduced(2));
    sessionHeading.setBounds(r.removeFromTop(30));
    status.setBounds(r.removeFromTop(44));
    row = r.removeFromBottom(34);
    playA.setBounds(row.removeFromLeft(90));
    playB.setBounds(row.removeFromLeft(90));
    stop.setBounds(row.removeFromLeft(180));
    differenceView.setBounds(row);
    plot.setBounds(r.reduced(0, 8));
}
} // namespace lab
