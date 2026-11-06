#include "Synth1PatchIo.h"
#include "Synth1PatchIoCodecInternal.h"
#include "Synth1PatchFamilyRawAdaptationInternal.h"
#include "Synth1PatchParser.h"

// The optional-field semantic API retains historical model adaptations. Native
// file loading resolves complete controls before decoding and never calls these
// family/profile corrections. Keep their contracts and call paths distinct.

namespace {
Synth1PatchDocument normalizeDocumentForTypedImport(const Synth1PatchDocument& document) {
    Synth1PatchDocument normalized = Synth1PatchDocument::makeParsed(
        document.name,
        document.version(),
        buildRemappedSynth1PatchEntries(document));

    for (const Synth1PatchEntry& entry : deriveFamilySpecificRawImplicitEntries(document)) {
        upsertSynth1PatchEntryByIndex(normalized, entry.index, entry.rawValue);
    }

    return normalized;
}

} // namespace

Synth1Patch importSynth1Patch(const Synth1PatchDocument& document) {
    return decodePatchControls(normalizeDocumentForTypedImport(document),document);
}


Synth1Patch parseSynth1PatchModelText(std::string_view text) {
    return importSynth1Patch(parseSynth1PatchText(text));
}


Synth1Patch importSynth1Patch(const Synth1PatchDocument& document, Synth1PatchImportWorkspace& workspace) {
    auto normalized=Synth1PatchDocument::makeParsed(document.name,document.version(),{});
    normalized.entries.swap(workspace.normalizedEntries);
    normalized.entries.clear();
    for(const auto& entry:document.entries)
        normalized.entries.push_back({entry.index,remapFamilySpecificRawValue(document,entry.index,entry.rawValue)});
    deriveFamilySpecificRawImplicitEntries(document,workspace.implicitEntries);
    for(const auto& entry:workspace.implicitEntries)
        upsertSynth1PatchEntryByIndex(normalized,entry.index,entry.rawValue);
    auto patch=decodePatchControls(normalized,document);
    normalized.entries.swap(workspace.normalizedEntries);
    return patch;
}


void prepareSynth1PatchImportTables() {
    // Historical immutable correction maps initialize once, before rendering.
    static const bool ready=[] {
        auto source=Synth1PatchDocument::makeParsed("Synth2",1,{});
        for(int index=0;index<126;++index)source.entries.push_back({index,0});
        for(int version=1;version<=113;++version) {
            auto next=Synth1PatchDocument::makeParsed("Synth2",version,source.entries);
            (void)importSynth1Patch(next);
        }
        return true;
    }();
    (void)ready;
}
