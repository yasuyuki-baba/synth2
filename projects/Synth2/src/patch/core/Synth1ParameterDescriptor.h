#pragma once

#include "Synth1PatchSemantics.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class Synth1ParameterSection {
    oscillators,
    filter,
    amplifier,
    arpeggiator,
    tempoDelay,
    chorusFlanger,
    equalizerPan,
    effect,
    voice,
    wheelMidi,
    lfo,
    hidden,
};

struct Synth1ParameterDescriptor {
    int index = -1;
    std::string canonicalName;
    std::string displayName;
    Synth1ParameterSection section = Synth1ParameterSection::oscillators;
};

const char* synth1ParameterSectionName(Synth1ParameterSection section);
std::optional<Synth1ParameterSection> classifySynth1ParameterSection(int index);
std::optional<Synth1ParameterDescriptor> describeSynth1Parameter(int index);
std::optional<int> findSynth1ParameterIndexByName(std::string_view name);
std::optional<Synth1ParameterDescriptor> findSynth1ParameterByName(std::string_view name);
std::vector<Synth1ParameterDescriptor> listSynth1ParameterDescriptors(Synth1ParameterSection section);
const Synth1PatchEntry* findSynth1PatchEntryByIndex(const Synth1PatchDocument& document, int index);
const Synth1PatchEntry* findSynth1PatchEntryByName(const Synth1PatchDocument& document, std::string_view name);
std::vector<const Synth1PatchEntry*> listSynth1PatchEntriesBySection(const Synth1PatchDocument& document, Synth1ParameterSection section);