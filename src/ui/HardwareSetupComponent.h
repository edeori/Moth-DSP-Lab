#pragma once
#include "CollectionPanel.h"
#include "LabTheme.h"
#include "PlannerPanel.h"
#include "Plot.h"
#include "hardware/SetupTest.h"
#include "measurement/GuidedWorkflow.h"
#include "measurement/HardwareComparison.h"
#include "measurement/MeasurementReport.h"
#include <future>
namespace lab
{
class HardwareSetupComponent final : public juce::Component,
                                     private juce::Timer,
                                     private juce::AudioIODeviceCallback
{
  public:
    explicit HardwareSetupComponent(juce::File project, std::function<void(juce::File)> openComparison = {});
    ~HardwareSetupComponent() override;
    void showSavedResult(const juce::File &folder);
    bool isWorking() const
    {
        return testing || analysing || preparing || permissionPending || planner != nullptr ||
               collection != nullptr;
    }
    void resized() override;
    void paint(juce::Graphics &) override;

  private:
    LabTheme theme;
    juce::TextButton openRunFolder{juce::String::fromUTF8("Mérési sorozat mappája")};
    juce::TextButton exportHandoff{juce::String::fromUTF8("Codex átadócsomag")};
    juce::TextButton planButton{juce::String::fromUTF8("Eszköz / fotó / AI terv")};
    juce::TextButton mergeRuns{juce::String::fromUTF8("Sorozatok összevonása")};
    juce::TextButton excludeResult{juce::String::fromUTF8("Hibás — kihagyás az exportból")};
    std::unique_ptr<CollectionPanel> collection;
    std::unique_ptr<PlannerPanel> planner;
    juce::var activeScenarioStep;
    void refreshScenarioResults();
    void openPlanner();
    void closePlanner(bool approved);

    juce::TextEditor sessionName;
    juce::Label libraryLabel, notesLabel;
    juce::TextButton duplicate{juce::String::fromUTF8("Ismétlés ezekkel a beállításokkal")};
    juce::Rectangle<int> taskCard, feedbackCard, stagesArea;
    int currentStage = 0;
    bool cleanComplete = false, driveComplete = false, bypassCurrent = false;
    juce::TextButton repeatBypass{juce::String::fromUTF8("Jelút változott? Bypass újraellenőrzése")};
    Plot resultPlot;
    juce::ToggleButton details{juce::String::fromUTF8("Mérési részletek")};
    juce::ToggleButton difference{juce::String::fromUTF8("Eltérés mutatása")};
    juce::Label taskTitle, feedbackTitle, resultSummary, inputLevel;
    juce::String displayedResultText;
    bool hasPlot = false;
    double captureProgress = 0;
    juce::ProgressBar captureBar{captureProgress};
    std::atomic<float> inputPeak{0};
    void presentResult();
    GuidedWorkflow workflow;
    juce::ComboBox workflowHistory, resultHistory;
    juce::TextButton retryRender{juce::String::fromUTF8("Mentett felvétel feldolgozása újra — hang nélkül")};
    juce::File recoveryFolder() const;
    void retrySavedCapture();
    juce::TextButton newWorkflow{juce::String::fromUTF8("Új mérési folyamat")};
    juce::Label stageOverview, savedStatus;
    juce::Array<juce::File> workflowFiles;
    bool restoring = true;
    juce::String saveError;
    void restoreWorkflow(const juce::File &);
    void refreshHistory();
    void saveProgress();
    void showResult(int mode);
    juce::TextEditor tutorial, results, physicalNotes;
    juce::ComboBox measurementMode;
    juce::TextButton preparePlugin{juce::String::fromUTF8("DarkBass profil betöltése")},
        openComparisonButton{juce::String::fromUTF8("Grafikonok megnyitása")};
    juce::ToggleButton chainUnchanged{
        juce::String::fromUTF8("A reamp és a hangkártya beállításai a bypass-teszt óta változatlanok")};
    juce::Label heading, route, progress, nextAction;
    juce::TextButton advance{juce::String::fromUTF8("Következő mérési lépés")};
    int completedMode = 0;
    juce::ComboBox driver, interface, input, output;
    juce::TextButton previous{juce::String::fromUTF8("Előző")}, next{juce::String::fromUTF8("Következő")},
        rescan{juce::String::fromUTF8("Eszközlista frissítése")},
        start{juce::String::fromUTF8("Bekötve — teszt indítása")},
        stop{juce::String::fromUTF8("VÉSZLEÁLLÍTÁS")}, showReport{juce::String::fromUTF8("Riport mappája")};
    juce::ToggleButton wiring{juce::String::fromUTF8("A kábeleket és a választott csatornákat ellenőriztem")};
    juce::ToggleButton monitoring{
        juce::String::fromUTF8("Hangfalak leválasztva; Direct Monitor és DAW monitoring KI")};
    juce::ToggleButton levels{
        juce::String::fromUTF8("Pedál BYPASS; alacsony kezdőszintek; teszt közben nem állítok rajtuk")};
    juce::AudioDeviceManager enumeration;
    std::unique_ptr<juce::AudioIODevice> audio;
    SetupProbeRunner runner;
    juce::File projectRoot, reportFolder;
    std::future<juce::String> analysis;
    std::atomic_bool deviceFault{false};
    bool testing = false, analysing = false, permissionPending = false;
    int step = 0, initialXruns = -1;
    double startedAt = 0;
    juce::var runMetadata, bypassCheck;
    juce::File bypassFolder, comparisonFolder;
    PreparedDarkbass prepared;
    std::future<PreparedDarkbass> preparation;
    std::atomic_bool cancelWork{false};
    bool preparing = false;
    int preparedMode = 0, runMode = 1;
    std::shared_ptr<juce::File> completedComparison;
    std::function<void(juce::File)> openComparison;
    void modeChanged();
    void prepareCandidate();
    juce::String measurementInstructions() const;
    void showStep();
    void scanInterfaces();
    void selectInterface();
    void invalidate();
    void updateControls();
    void requestStart();
    void beginTest();
    void finishTest();
    void timerCallback() override;
    void audioDeviceIOCallbackWithContext(const float *const *, int, float *const *, int, int,
                                          const juce::AudioIODeviceCallbackContext &) override;
    void audioDeviceAboutToStart(juce::AudioIODevice *) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String &) override;
};
} // namespace lab
