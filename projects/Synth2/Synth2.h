#pragma once
#include "IPlug_include_in_plug_hdr.h"
#include "ProcessorBridge.h"
#include "PatchCatalog.h"
#include "editor/ComputerKeyboard.h"
#include <atomic>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>

class Synth2 final : public iplug::Plugin
{
public:
    ~Synth2() override;
    explicit Synth2(const iplug::InstanceInfo& info);
    void ProcessBlock(iplug::sample**, iplug::sample** outputs, int frames) override;
    void ProcessMidiMsg(const iplug::IMidiMsg& message) override;
    void OnReset() override;
    void OnActivate(bool active) override;
    void OnParamChange(int index) override;
    void OnParamReset(iplug::EParamSource source) override;
    bool SerializeState(iplug::IByteChunk& chunk) const override;
    int UnserializeState(const iplug::IByteChunk& chunk, int start) override;
    void OnIdle() override;
    void OnUIOpen() override;
    void OnUIClose() override;

private:
    static constexpr int qualityParameter=synth2::host::ProcessorBridge::parameterCount;
    void syncParametersFromProcessor();
    void syncQualityLatency();
    void refreshOversampling();
    void showOversamplingMenu(iplug::igraphics::IControl* caller);
    int displayedOversampling=0,reportedLatency=0;
    std::unique_ptr<iplug::igraphics::IPopupMenu> qualityMenu;
    void layoutEditor(iplug::igraphics::IGraphics* graphics);
    void showPage(int page);
    void setKeyboardVisible(bool visible);
    void releaseComputerNotes();
    void releaseKeyboardNotes();
    void updateKeyboardInput();
    synth2::editor::ComputerKeyboard computerKeyboard;
    bool keyboardVisible = false;
    void refreshBrowser();
    void showPresetMenu(iplug::igraphics::IControl* caller);
    void promptForBank();
    void promptForFolder();
    void showImportMenu(iplug::igraphics::IControl* caller);
    void importBank(const std::string& path);
    void reloadCatalog(bool queueIfBusy = false);
    void stepBank(int direction);
    void selectBank(const std::string& id);
    void showBankMenu(iplug::igraphics::IControl* caller);
    void showScrollableLibraryMenu(iplug::igraphics::IControl* caller);
    void showAutomaticTagMenu(iplug::igraphics::IControl* caller);
    void showTagMenu(iplug::igraphics::IControl* caller);
    bool handleLibraryKey(const iplug::IKeyPress& key,bool up);
    void changeFilter(const char* query);
    void loadLibraryPatch(std::size_t index);
    void stepPatch(int direction);
    void notifyHostOfPatch();
    void savePreset(bool saveAs);
    void showSaveMenu(iplug::igraphics::IControl*);
    void writePreset(const std::filesystem::path&, bool confirm, const std::string& title = {});
    synth2::host::PresetBrowserState captureBrowser() const;
    void restoreBrowser();
    synth2::host::PresetBrowserState browser;
    bool lastModified = false;
    std::atomic<uint64_t> browserGeneration {0};
    struct ImportPrompt {
        std::mutex mutex;
        std::condition_variable changed;
        bool alive = true, pending = false, answered = false;
        std::filesystem::path path;
        synth2::host::ConflictChoice choice;
    };
    struct LibraryJob {
        synth2::host::PatchCatalog catalog;
        synth2::host::PresetImportResult imported;
        std::string error;
        bool importing = false, saving = false;
        uint64_t generation = 0;
        std::string savedId, savedName;
        std::array<double,synth2::host::ProcessorBridge::parameterCount> savedBaseline{};
    };
    std::future<LibraryJob> libraryJob;
    std::shared_ptr<ImportPrompt> importPrompt;
    bool conflictShown = false;
    std::unique_ptr<iplug::igraphics::IPopupMenu> libraryMenu, conflictMenu, saveMenu;
    synth2::host::ProcessorBridge processor;
    bool prepared = false;
    synth2::host::PatchCatalog catalog {synth2::host::PatchCatalog::userDirectory()};
    synth2::host::CatalogFilter libraryFilter;
    bool catalogLoaded = false;
    bool catalogReloadPending = false;
    std::string selectedPatchId,keyboardPatchId;
    double resultOffset = 0;
    int page = 0;
    std::string currentPatch = "Init";
    std::string browserStatus;
    std::unique_ptr<iplug::igraphics::IPopupMenu> presetMenu;
    std::shared_ptr<int> dialogLifetime;
    std::atomic<bool> stateRestored {false};
};
