#pragma once
#include "IPlug_include_in_plug_hdr.h"
#include "ProcessorBridge.h"
#include "PatchCatalog.h"
#include <atomic>
#include <memory>

class Synth2 final : public iplug::Plugin
{
public:
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
    void syncParametersFromProcessor();
    void layoutEditor(iplug::igraphics::IGraphics* graphics);
    void showPage(int page);
    void refreshBrowser();
    void showPresetMenu(iplug::igraphics::IControl* caller);
    void promptForBank();
    void promptForFolder();
    void showImportMenu(iplug::igraphics::IControl* caller);
    void importBank(const std::string& path);
    void reloadCatalog();
    void showBankMenu(iplug::igraphics::IControl* caller);
    void showTagMenu(iplug::igraphics::IControl* caller);
    void removeLibraryPatch(const std::string& id);
    void removeLibraryBank();
    void changeFilter(const char* query);
    void loadLibraryPatch(std::size_t index);
    void stepPatch(int direction);
    void notifyHostOfPatch();
    synth2::host::ProcessorBridge processor;
    bool prepared = false;
    synth2::host::PatchCatalog catalog {synth2::host::PatchCatalog::userDirectory()};
    synth2::host::CatalogFilter libraryFilter;
    bool catalogLoaded = false;
    std::string selectedPatchId;
    std::size_t resultOffset = 0;
    int page = 0;
    std::string currentPatch = "Init";
    std::string browserStatus;
    std::unique_ptr<iplug::igraphics::IPopupMenu> presetMenu;
    std::shared_ptr<int> dialogLifetime;
    std::atomic<bool> stateRestored {false};
};
