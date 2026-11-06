#pragma once

#include "Synth1PatchSemantics.h"
#include "Synth1Patch.h"
#include "Synth1NativeLoading.h"

#include <string>
#include <string_view>

// Public semantic patch import/export API.
// Helper codecs and raw remap tables stay in internal-only headers.

// Imports a raw patch document into the semantic patch model after applying
// family-specific raw remaps and raw implicit legacy defaults.
Synth1Patch importSynth1Patch(const Synth1PatchDocument& document);

// Decode the audited complete native context without repeating family adaptation.
Synth1Patch importNativeSynth1Patch(const Synth1PatchDocument& document);
Synth1Patch decodeNativePatchControls(const synth1::patch::NativePatchControls&, const Synth1PatchDocument& source);

// Convenience entry point for loading .sy1 text into the semantic patch model.
// This uses the same family-normalizing import path as importSynth1Patch.
Synth1Patch parseSynth1PatchModelText(std::string_view text);

// Export always writes the canonical latest .sy1 format and does not apply
// host/runtime-only normalization that depends on plugin probing or program slot.
Synth1PatchDocument exportSynth1Patch(const Synth1Patch& patch);
std::string formatSynth1PatchText(const Synth1Patch& patch);
// Caller-owned working storage for repeated runtime-dial adaptation. Prepare it
// outside the audio callback; semantic conversion and operation order are shared.
struct Synth1PatchImportWorkspace {
    Synth1PatchImportWorkspace() { normalizedEntries.reserve(256); implicitEntries.reserve(128); }
    std::vector<Synth1PatchEntry> normalizedEntries, implicitEntries;
};
void prepareSynth1PatchImportTables();
Synth1Patch importSynth1Patch(const Synth1PatchDocument&, Synth1PatchImportWorkspace&);
Synth1Patch decodeNativePatchControls(const synth1::patch::NativePatchControls&,
    const Synth1PatchDocument&, Synth1PatchImportWorkspace&);
