#pragma once

#include "Synth1PatchSemantics.h"

#include <string>
#include <string_view>

Synth1PatchEntry* upsertSynth1PatchEntryByIndex(Synth1PatchDocument& document, int index, int rawValue);
Synth1PatchEntry* upsertSynth1PatchEntryByName(Synth1PatchDocument& document, std::string_view name, int rawValue);
bool eraseSynth1PatchEntryByIndex(Synth1PatchDocument& document, int index);
bool eraseSynth1PatchEntryByName(Synth1PatchDocument& document, std::string_view name);
std::string formatSynth1PatchText(const Synth1PatchDocument& document);