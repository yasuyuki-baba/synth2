#pragma once
#include "EditorLayout.h"

namespace synth2::host
{
// Host/UI editing policy only. Loading and state recall retain source words,
// including historical noncanonical values; this must not quantize the bridge.
inline bool parameterUsesIntegerSteps(int index)
{
    if (editorToggle(index) || !editorChoices(index).empty()) return true;
    if (index>=99 && index<=127) return true; // Serialized metadata/reserved words.
    switch(index)
    {
        case 2: case 9: case 40: case 85: // Quantized tuning.
        case 86: case 87: case 88: case 89: // MIDI destination/source words.
        case 93: case 94: return true; // Voice counts.
        default: return false;
    }
}
}
