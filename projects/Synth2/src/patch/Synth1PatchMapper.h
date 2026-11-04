#pragma once
#include "PatchMapper.h"
#include <Synth1PatchIo.h>

namespace synth2
{
    class Synth1PatchMapper : public PatchMapper
    {
    public:
        // Shared typed importer across supported serialized version families.
        explicit Synth1PatchMapper (int version = 105) : sourceVersion (version) {
            mappingEntries.reserve(126);
            prepareSynth1PatchImportTables();
        }
        void resetSourceVersion(int version) noexcept {
            sourceVersion=version;
            hasMapped=false; mappedTarget=nullptr; loadHistory={};
            controllerSources={45057,45057}; controllerDestinations={-1,-1};
        }
        void mapParameterValues (const std::array<std::atomic<float>*,126>&, RenderControls&) override;
        // Hosts opt in for fractional edits; direct source buffers retain the
        // versioned integer import contract by default.
        void setContinuousControls(bool enabled) noexcept { continuousControls=enabled; hasMapped=false; }
        // Synth2 files already contain resolved dials; skip source conversion.
        void setDirectControls(bool enabled) noexcept {
            if(directControls!=enabled){directControls=enabled;hasMapped=false;}
        }
        // Buffer elements are source .sy1 values normalized to 0..1, not DSP units.
        void mapFromRawBuffer (const float*, RenderControls&) override;
        bool readNativeControls(std::array<int,99>& out) const noexcept override
        { if(!mappedNativeSourceLoading || !hasMapped)return false;out=resolvedNativeControls;return true; }
        int getSourceVersion() const noexcept override { return sourceVersion; }
        const std::array<float,128>& getSourceValues() const noexcept override { return mappedSourceValues; }
        static int sy1ToAu (int index) { return index >= 0 && index < 126 ? index : -1; }
    private:
        int sourceVersion;
        std::vector<Synth1PatchEntry> mappingEntries;
        Synth1PatchImportWorkspace importWorkspace;
        const RenderControls* mappedTarget = nullptr;
        std::array<float,128> mappedSourceValues {};
        bool hasMapped = false;
        bool continuousControls = false;
        bool directControls = false;
        bool mappedRandomHistoryValid = false;
        bool mappedNativeSourceLoading = false;
        std::array<uint64_t,2> mappedNativePresence {};
        std::array<int,99> mappedNativeInitial {};
        std::array<uint64_t,2> mappedNativeEdits {};
        std::array<int,99> resolvedNativeControls {};
        std::array<int,2> controllerSources {{45057,45057}},controllerDestinations {{-1,-1}};
        unsigned mappedMissingEnvelopeFields = 0;
    };
}
