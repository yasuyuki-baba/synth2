#pragma once
#include "Synth1PatchFamilyRawAdaptationInternal.h"
#include <array>
#include <map>
#include <optional>
#include <initializer_list>

// Internal legacy typed-import rules, separate from native control resolution.
namespace synth1patch::legacy {
using FamilyRawMap = std::map<int, int>;
using RawMapLookup = const FamilyRawMap* (*)(int);
using RawRemapFn = std::optional<int> (*)(const Synth1PatchDocument&, int, int);
using ImplicitAppendFn = void (*)(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&);

struct ImplicitRawDerivationRule {
    int sourceIndex = 0;
    int targetIndex = 0;
    const FamilyRawMap* rawValueMap = nullptr;
};

struct FamilyImplicitAppender {
    Synth1PatchFormatFamily family;
    ImplicitAppendFn appender;
};

template <typename Profile>
struct ProfileRawRemapper {
    Profile profile;
    RawRemapFn remapper;
};

struct Synth1PatchModelFamilyRules {
    std::array<RawMapLookup, 2> rawCorrectionMapLookups{};
    size_t rawCorrectionMapLookupCount = 0;
    const ImplicitRawDerivationRule* implicitRawValueRules = nullptr;
    size_t implicitRawValueRuleCount = 0;
};

struct FamilyRuleRegistryEntry {
    Synth1PatchFormatFamily family;
    const Synth1PatchModelFamilyRules* rules;
};

enum class Format106Profile {
    defaultCase,
    filterDecay105,
    modDecay23,
};

enum class Format107Profile {
    defaultCase,
    ampLevel91,
    ampLevel80,
    sensitivityOutlier,
};

enum class Format108Profile {
    defaultCase,
    unpackedFilterEnvelope128,
    packedGeneral,
    packedLfoRate43,
};


const std::map<int, FamilyRawMap>& sharedFormat105RawCorrectionMaps();
const std::map<int, FamilyRawMap>& pre105RawCorrectionMaps();
const std::map<int, FamilyRawMap>& format107AmpLevel80OverrideMaps();
const std::map<int, FamilyRawMap>& format107AmpLevel91OverrideMaps();
const std::map<int, FamilyRawMap>& format108PackedGeneralOverrideMaps();
const std::map<int, FamilyRawMap>& format108UnpackedFilterEnvelope128OverrideMaps();
const std::map<int, FamilyRawMap>& format108PackedLfoRate43OverrideMaps();
const std::map<int, FamilyRawMap>& format106ModDecay23OverrideMaps();
const std::map<int, FamilyRawMap>& format106FilterDecay105OverrideMaps();
const std::map<int, FamilyRawMap>& format106Oscillator13Maps();
const std::map<int, FamilyRawMap>& format106Filter16Maps();
const std::map<int, FamilyRawMap>& format106Filter18Maps();
const std::map<int, FamilyRawMap>& format106Chorus52Maps();
const std::map<int, FamilyRawMap>& format106Chorus54Maps();
const std::map<int, FamilyRawMap>& format106AmplifierMaps();
const FamilyRawMap& pre105ImplicitParam50RawMap();
const FamilyRawMap& pre105ImplicitParam51RawMap();
Format106Profile detectFormat106Profile(const Synth1PatchDocument&);
Format107Profile detectFormat107Profile(const Synth1PatchDocument&);
Format108Profile detectFormat108Profile(const Synth1PatchDocument&);
bool documentMatchesRawValues(const Synth1PatchDocument&, std::initializer_list<Synth1PatchEntry>);
bool containsFormat108PackedMidiBlock(const Synth1PatchDocument&);
std::optional<int> findPresetRawValue(const Synth1PatchDocument&, int);

void appendPre105ModeImplicitRawEntries(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&);
void appendFormat105ImplicitRawEntries(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&);
void appendFormat106To107ImplicitRawEntries(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&);
void appendFormat108ImplicitRawEntries(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&);
void appendMappedImplicitRawEntry(std::vector<Synth1PatchEntry>&, const Synth1PatchDocument&, const ImplicitRawDerivationRule&);
}
