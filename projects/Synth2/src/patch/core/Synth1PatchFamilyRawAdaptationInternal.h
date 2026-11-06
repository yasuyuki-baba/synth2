#pragma once

#include "Synth1PatchSemantics.h"

#include <vector>

int remapFamilySpecificRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);

std::vector<Synth1PatchEntry> buildRemappedSynth1PatchEntries(const Synth1PatchDocument& document);

std::vector<Synth1PatchEntry> deriveFamilySpecificRawImplicitEntries(const Synth1PatchDocument& document);
void deriveFamilySpecificRawImplicitEntries(const Synth1PatchDocument&, std::vector<Synth1PatchEntry>&);
