#pragma once
#include "LabTheme.h"
#include "Plot.h"
#include "audio-host/PluginHost.h"
#include "measurement/MeasurementEngine.h"
#include <atomic>
#include <thread>
namespace lab
{
class HardwareSetupComponent;
class MainComponent final : public juce::Component, private juce::Timer
{
  public:
    MainComponent();
    ~MainComponent() override;
    void resized() override;
    void paint(juce::Graphics &) override;

  private:
    LabTheme theme;
    juce::TooltipWindow tooltips{this, 700};
    juce::Label title, description, status, settingsLabel, sessionHeading, nextAction;
    juce::ToggleButton offlineMode{juce::String::fromUTF8("Haladó: két plugin offline A/B mérése")};
    void timerCallback() override;
    void rememberProject();
    juce::TextButton chooseA{"Choose reference plugin"}, chooseB{"Choose candidate plugin"};
    juce::TextButton editA{"Reference parameters"}, editB{"Candidate parameters"};
    juce::TextButton chooseInput{"Choose WAV"}, chooseProject{"Project folder"}, run{"Render A/B"},
        open{"Open session"}, hardware{juce::String::fromUTF8("Vezetett mérés / folytatás")};
    juce::TextButton playA{"Play A"}, playB{"Play B"}, stop{"Stop / cancel"};
    juce::ToggleButton differenceView{"Residual / spectrum difference"};
    juce::ComboBox rate, block, channels;
    juce::TextEditor tail;
    Plot plot;
    TargetSpec a, b;
    juce::File input, project, session;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<HardwareSetupComponent> setupPanel;
    void attachWorkflow();
    std::thread task;
    bool busy = false, hardwareSession = false;
    std::atomic_bool cancelRequested{false};
    juce::TimeSliceThread readAhead{"Playback file buffering"};
    juce::AudioDeviceManager device;
    juce::AudioSourcePlayer player;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> playback;
    RenderSettings settings() const;
    void pickPlugin(bool reference);
    void editPlugin(bool reference);
    void startTask(std::function<juce::String()> work, std::function<void()> done = {});
    void refresh();
    void loadSession(const juce::File &);
    void play(const juce::String &side);
};
} // namespace lab
