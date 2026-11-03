#include "MainComponent.h"
#include "measurement/HardwareComparison.h"
#include <iostream>
namespace lab
{
class Window final : public juce::DocumentWindow
{
  public:
    Window(juce::String name, juce::Component *content, std::function<void()> close)
        : DocumentWindow(name, juce::Colour(0xff111925), allButtons), onClose(std::move(close))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(content, true);
        setResizable(true, true);
        if (dynamic_cast<MainComponent *>(content) != nullptr)
            setResizeLimits(1080, 820, 2200, 1600);
        centreWithSize(content->getWidth(), content->getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        onClose();
    }

  private:
    std::function<void()> onClose;
};
class Application final : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override
    {
        return "Moth DSP Lab";
    }
    const juce::String getApplicationVersion() override
    {
        return version;
    }
    bool moreThanOneInstanceAllowed() override
    {
        const auto args = getCommandLineParameterArray();
        return args.size() == 2 && (args[0] == "--worker" || args[0] == "--compare" ||
                                    args[0] == "--compare-hardware" || args[0] == "--storage-layout");
    }
    void anotherInstanceStarted(const juce::String &commandLine) override
    {
        if (commandLine.isNotEmpty())
            return;
        if (auto *modal = juce::Component::getCurrentlyModalComponent())
            modal->toFront(true);
        else if (window)
            window->toFront(true);
    }
    void initialise(const juce::String &) override
    {
        auto args = getCommandLineParameterArray();
        if (args.size() == 2 && args[0] == "--storage-layout")
        {
            try
            {
                auto layout = object();
                for (const auto &file : juce::File(args[1])
                                            .getChildFile("workflows")
                                            .findChildFiles(juce::File::findFiles, false, "*.json"))
                    set(layout, juce::Identifier(file.getFileNameWithoutExtension()),
                        SessionStore::runPath(readJson(file), file.getFileNameWithoutExtension()));
                std::cout << juce::JSON::toString(layout) << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << std::endl;
                setApplicationReturnValue(1);
            }
            quit();
            return;
        }
        if (args.size() == 2 && args[0] == "--worker")
        {
            jobFile = juce::File(args[1]);
            try
            {
                auto job = readJson(jobFile);
                if (job["action"].toString() == "edit")
                {
                    target = std::make_unique<PluginTarget>(TargetSpec::fromJson(job["target"]),
                                                            RenderSettings::fromJson(job["settings"]));
                    auto *editor = new juce::GenericAudioProcessorEditor(target->instance());
                    editor->setSize(600, std::max(200, editor->getHeight()));
                    window = std::make_unique<Window>(
                        juce::String::fromUTF8("Plugin parameters — close to save state"), editor,
                        [this]
                        {
                            auto result = object();
                            set(result, "state", encodeState(target->state()));
                            writeJson(jobFile.getSiblingFile("edit-result.json"), result);
                            quit();
                        });
                }
                else
                {
                    executeWorkerJob(jobFile);
                    quit();
                }
            }
            catch (const std::exception &e)
            {
                auto result = object();
                set(result, "error", juce::String::fromUTF8(e.what()));
                writeJson(jobFile.getSiblingFile(jobFile.getFileNameWithoutExtension() + "-result.json"),
                          result);
                setApplicationReturnValue(1);
                quit();
            }
            return;
        }
        if (args.size() == 2 && args[0] == "--compare-hardware")
        {
            try
            {
                auto job = readJson(juce::File(args[1]));
                auto folder = compareHardwareCapture(
                    juce::File::getSpecialLocation(juce::File::currentExecutableFile),
                    juce::File(job["project"].toString()), readWav(juce::File(job["input"].toString())),
                    readWav(juce::File(job["capture"].toString())), TargetSpec::fromJson(job["candidate"]),
                    job["metadata"], job["bypassCheck"], juce::File(job["bypassFolder"].toString()));
                std::cout << folder.getFullPathName() << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << std::endl;
                setApplicationReturnValue(1);
            }
            quit();
            return;
        }
        if (args.size() == 2 && args[0] == "--compare")
        {
            try
            {
                auto job = readJson(juce::File(args[1]));
                MeasurementEngine engine(juce::File::getSpecialLocation(juce::File::currentExecutableFile));
                auto folder = engine.compare(
                    juce::File(job["project"].toString()), juce::File(job["input"].toString()),
                    TargetSpec::fromJson(job["reference"]), TargetSpec::fromJson(job["candidate"]),
                    RenderSettings::fromJson(job["settings"]));
                std::cout << folder.getFullPathName() << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << std::endl;
                setApplicationReturnValue(1);
            }
            quit();
            return;
        }
        window = std::make_unique<Window>(getApplicationName(), new MainComponent(), [this] { quit(); });
    }
    void shutdown() override
    {
        window.reset();
        target.reset();
    }
    void systemRequestedQuit() override
    {
        if (window)
            window->closeButtonPressed();
        else
            quit();
    }

  private:
    std::unique_ptr<PluginTarget> target;
    std::unique_ptr<Window> window;
    juce::File jobFile;
};
} // namespace lab
START_JUCE_APPLICATION(lab::Application)
