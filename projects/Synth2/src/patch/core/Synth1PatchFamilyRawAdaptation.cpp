#include "Synth1PatchFamilyRules.h"
#include "Synth1PatchNativeControls.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <map>
#include <optional>

namespace synth1patch::legacy {

template <typename Profile, size_t N>
std::optional<int> applyProfileRemappers(
    Profile profile,
    const std::array<ProfileRawRemapper<Profile>, N>& remappers,
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue) {
    for (const ProfileRawRemapper<Profile>& candidate : remappers) {
        if (candidate.profile != profile) {
            continue;
        }

        if (const std::optional<int> remappedValue = candidate.remapper(document, paramIndex, rawValue)) {
            return remappedValue;
        }
    }

    return std::nullopt;
}

template <size_t N>
std::optional<int> applyOrderedRawRemappers(
    const std::array<RawRemapFn, N>& remappers,
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue) {
    for (const RawRemapFn remapper : remappers) {
        if (const std::optional<int> remappedValue = remapper(document, paramIndex, rawValue)) {
            return remappedValue;
        }
    }

    return std::nullopt;
}

std::optional<int> remapFormat106ModDecay23RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106FilterDecay105RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
Format106Profile detectFormat106Profile(const Synth1PatchDocument& document);
Format107Profile detectFormat107Profile(const Synth1PatchDocument& document);
Format108Profile detectFormat108Profile(const Synth1PatchDocument& document);
std::optional<int> remapFormat106Delay37RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Chorus55RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Chorus54RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Chorus52RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Amplifier26To28RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Filter18RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Filter16RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Oscillator13RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106Filter21RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> findProfileOverrideRawValue(const std::map<int, FamilyRawMap>& maps, int paramIndex, int rawValue);
std::optional<int> remapFormat107ProfileMappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    Format107Profile expectedProfile,
    const std::map<int, FamilyRawMap>& maps);
std::optional<int> remapFormat108ProfileMappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    Format108Profile expectedProfile,
    const std::map<int, FamilyRawMap>& maps,
    std::optional<int> expectedParamIndex = std::nullopt);

std::optional<int> remapFormat107AmpLevel80RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat107ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format107Profile::ampLevel80,
        format107AmpLevel80OverrideMaps());
}

std::optional<int> remapFormat107AmpLevel91RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat107ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format107Profile::ampLevel91,
        format107AmpLevel91OverrideMaps());
}

std::optional<int> remapFormat108PackedFilter16RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::packedGeneral,
        format108PackedGeneralOverrideMaps(),
        16);
}

std::optional<int> remapFormat108PackedAmplifierRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::packedGeneral,
        format108PackedGeneralOverrideMaps());
}

std::optional<int> remapFormat108PackedDelay37RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::packedGeneral,
        format108PackedGeneralOverrideMaps(),
        37);
}

std::optional<int> remapFormat108PackedChorus54RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::packedGeneral,
        format108PackedGeneralOverrideMaps(),
        54);
}

std::optional<int> remapFormat108Packed001InactiveSectionRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::packedLfoRate43,
        format108PackedLfoRate43OverrideMaps());
}

std::optional<int> remapFormat108UnpackedFilterEnvelope128RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::unpackedFilterEnvelope128,
        format108UnpackedFilterEnvelope128OverrideMaps());
}

const FamilyRawMap* findRawCorrectionMap(const std::map<int, FamilyRawMap>& maps, int paramIndex) {
    const auto it = maps.find(paramIndex);
    if (it == maps.end()) {
        return nullptr;
    }
    return &it->second;
}

const FamilyRawMap* getSharedFormat105RawCorrectionMap(int paramIndex) {
    return findRawCorrectionMap(sharedFormat105RawCorrectionMaps(), paramIndex);
}

const FamilyRawMap* getSharedFormat105RawCorrectionMapExceptParam37(int paramIndex) {
    if (paramIndex == 37) {
        return nullptr;
    }
    return findRawCorrectionMap(sharedFormat105RawCorrectionMaps(), paramIndex);
}

std::optional<int> findProfileOverrideRawValue(const std::map<int, FamilyRawMap>& maps, int paramIndex, int rawValue) {
    const FamilyRawMap* map = findRawCorrectionMap(maps, paramIndex);
    if (map == nullptr) {
        return std::nullopt;
    }

    const auto it = map->find(rawValue);
    if (it == map->end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<int> remapFormat107ProfileMappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    Format107Profile expectedProfile,
    const std::map<int, FamilyRawMap>& maps) {
    if (detectFormat107Profile(document) != expectedProfile) {
        return std::nullopt;
    }

    return findProfileOverrideRawValue(maps, paramIndex, rawValue);
}

std::optional<int> remapFormat108ProfileMappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    Format108Profile expectedProfile,
    const std::map<int, FamilyRawMap>& maps,
    std::optional<int> expectedParamIndex) {
    if (document.version() != 108) {
        return std::nullopt;
    }

    if (expectedParamIndex && paramIndex != *expectedParamIndex) {
        return std::nullopt;
    }

    if (detectFormat108Profile(document) != expectedProfile) {
        return std::nullopt;
    }

    return findProfileOverrideRawValue(maps, paramIndex, rawValue);
}

const std::array<ProfileRawRemapper<Format106Profile>, 2>& format106ProfileRemappers() {
    static const std::array<ProfileRawRemapper<Format106Profile>, 2> remappers = {{
        {Format106Profile::filterDecay105, remapFormat106FilterDecay105RawValue},
        {Format106Profile::modDecay23, remapFormat106ModDecay23RawValue},
    }};
    return remappers;
}

std::optional<int> applyFormat106ProfileOverride(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return applyProfileRemappers(
        detectFormat106Profile(document),
        format106ProfileRemappers(),
        document,
        paramIndex,
        rawValue);
}

const std::array<ProfileRawRemapper<Format107Profile>, 2>& format107ProfileRemappers() {
    static const std::array<ProfileRawRemapper<Format107Profile>, 2> remappers = {{
        {Format107Profile::ampLevel91, remapFormat107AmpLevel91RawValue},
        {Format107Profile::ampLevel80, remapFormat107AmpLevel80RawValue},
    }};
    return remappers;
}

std::optional<int> applyFormat107ProfileOverride(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return applyProfileRemappers(
        detectFormat107Profile(document),
        format107ProfileRemappers(),
        document,
        paramIndex,
        rawValue);
}

const std::array<ProfileRawRemapper<Format108Profile>, 6>& format108ProfileRemappers() {
    static const std::array<ProfileRawRemapper<Format108Profile>, 6> remappers = {{
        {Format108Profile::unpackedFilterEnvelope128, remapFormat108UnpackedFilterEnvelope128RawValue},
        {Format108Profile::packedLfoRate43, remapFormat108Packed001InactiveSectionRawValue},
        {Format108Profile::packedGeneral, remapFormat108PackedFilter16RawValue},
        {Format108Profile::packedGeneral, remapFormat108PackedAmplifierRawValue},
        {Format108Profile::packedGeneral, remapFormat108PackedDelay37RawValue},
        {Format108Profile::packedGeneral, remapFormat108PackedChorus54RawValue},
    }};
    return remappers;
}

std::optional<int> applyFormat108ProfileOverride(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return applyProfileRemappers(
        detectFormat108Profile(document),
        format108ProfileRemappers(),
        document,
        paramIndex,
        rawValue);
}

std::optional<int> applyProfileOverrides(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (document.version() == 106) {
        return applyFormat106ProfileOverride(document, paramIndex, rawValue);
    }

    if (document.version() == 107) {
        return applyFormat107ProfileOverride(document, paramIndex, rawValue);
    }

    if (document.version() == 108) {
        return applyFormat108ProfileOverride(document, paramIndex, rawValue);
    }

    return std::nullopt;
}

bool shouldPreserveFormat107FilterEnvelopeRawValue(const Synth1PatchDocument& document, int paramIndex) {
    if (paramIndex != 16 && paramIndex != 18 && paramIndex != 21) {
        return false;
    }

    if (document.version() != 107) {
        return false;
    }

    return detectFormat107Profile(document) == Format107Profile::defaultCase;
}

bool shouldPreserveFormat107AmplifierRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (rawValue != 64) {
        return false;
    }

    if (paramIndex != 26 && paramIndex != 28) {
        return false;
    }

    if (document.version() != 107) {
        return false;
    }

    return detectFormat107Profile(document) == Format107Profile::defaultCase;
}

bool shouldPreserveFormat108FilterRawValue(const Synth1PatchDocument& document, int paramIndex) {
    if (document.version() != 108) {
        return false;
    }

    const Format108Profile profile = detectFormat108Profile(document);
    if (profile == Format108Profile::defaultCase) {
        return paramIndex == 13 || paramIndex == 16 || paramIndex == 18 || paramIndex == 21 || paramIndex == 26 || paramIndex == 28 || paramIndex == 37;
    }

    return paramIndex == 18 || paramIndex == 21;
}

std::optional<int> applyFormat107PreserveRules(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (shouldPreserveFormat107FilterEnvelopeRawValue(document, paramIndex)) {
        return rawValue;
    }

    if (shouldPreserveFormat107AmplifierRawValue(document, paramIndex, rawValue)) {
        return rawValue;
    }

    return std::nullopt;
}

std::optional<int> applyFormat108PreserveRules(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (shouldPreserveFormat108FilterRawValue(document, paramIndex)) {
        return rawValue;
    }

    return std::nullopt;
}

std::optional<int> applyPreserveRules(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (document.version() == 107) {
        return applyFormat107PreserveRules(document, paramIndex, rawValue);
    }

    if (document.version() == 108) {
        return applyFormat108PreserveRules(document, paramIndex, rawValue);
    }

    return std::nullopt;
}

std::optional<int> remapFormat106MappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    const std::map<int, FamilyRawMap>& maps,
    std::optional<int> expectedParamIndex = std::nullopt) {
    if (document.version() != 106) {
        return std::nullopt;
    }

    if (expectedParamIndex && paramIndex != *expectedParamIndex) {
        return std::nullopt;
    }

    return findProfileOverrideRawValue(maps, paramIndex, rawValue);
}

std::optional<int> remapFormat106ProfileMappedRawValue(
    const Synth1PatchDocument& document,
    int paramIndex,
    int rawValue,
    Format106Profile expectedProfile,
    const std::map<int, FamilyRawMap>& maps) {
    if (detectFormat106Profile(document) != expectedProfile) {
        return std::nullopt;
    }

    return findProfileOverrideRawValue(maps, paramIndex, rawValue);
}

bool isFormat106Param(const Synth1PatchDocument& document, int paramIndex, int expectedParamIndex) {
    return document.version() == 106 && paramIndex == expectedParamIndex;
}

bool isFormat106NonFilterDecay105Param(const Synth1PatchDocument& document, int paramIndex, int expectedParamIndex) {
    return isFormat106Param(document, paramIndex, expectedParamIndex)
        && detectFormat106Profile(document) != Format106Profile::filterDecay105;
}

std::optional<int> remapFormat106Chorus54SpecialRawValue(int rawValue) {
    if (rawValue >= 21 && rawValue <= 24) {
        return rawValue + 4;
    }

    return std::nullopt;
}

std::optional<int> remapFormat106Filter21SpecialRawValue(int rawValue) {
    if (rawValue == 77) {
        return 102;
    }

    return std::nullopt;
}

bool shouldApplyFormat106Filter21Curve(const Synth1PatchDocument& document) {
    if (detectFormat106Profile(document) == Format106Profile::filterDecay105) {
        return false;
    }

    return findPresetRawValue(document, 59) == std::optional<int>(0);
}

std::optional<int> remapFormat106Chorus54SpecialCase(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (!isFormat106Param(document, paramIndex, 54)) {
        return std::nullopt;
    }

    return remapFormat106Chorus54SpecialRawValue(rawValue);
}

std::optional<int> remapFormat106Chorus54MappedCase(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Chorus54Maps(), 54);
}

std::optional<int> remapFormat106Filter21SpecialCase(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (!isFormat106Param(document, paramIndex, 21)) {
        return std::nullopt;
    }

    return remapFormat106Filter21SpecialRawValue(rawValue);
}

std::optional<int> remapFormat106Filter21CurveCase(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (!isFormat106Param(document, paramIndex, 21) || !shouldApplyFormat106Filter21Curve(document)) {
        return std::nullopt;
    }

    return 63 + ((rawValue + 1) / 2);
}

std::optional<int> remapFormat106Delay37RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (!isFormat106NonFilterDecay105Param(document, paramIndex, 37)) {
        return std::nullopt;
    }

    return rawValue / 2;
}

std::optional<int> remapFormat106Chorus55RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (!isFormat106Param(document, paramIndex, 55)) {
        return std::nullopt;
    }

    if (rawValue <= 0 || rawValue >= 127) {
        return std::nullopt;
    }

    return 64;
}

std::optional<int> remapFormat106Chorus54RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> specialRawValue = remapFormat106Chorus54SpecialCase(document, paramIndex, rawValue)) {
        return specialRawValue;
    }

    return remapFormat106Chorus54MappedCase(document, paramIndex, rawValue);
}

std::optional<int> remapFormat106Chorus52RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Chorus52Maps(), 52);
}

std::optional<int> remapFormat106Amplifier26To28RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106AmplifierMaps());
}

std::optional<int> remapFormat106Filter18RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Filter18Maps(), 18);
}

std::optional<int> remapFormat106Filter16RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Filter16Maps(), 16);
}

std::optional<int> remapFormat106ModDecay23RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format106Profile::modDecay23,
        format106ModDecay23OverrideMaps());
}

std::optional<int> remapFormat106Oscillator13RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Oscillator13Maps(), 13);
}

std::optional<int> remapFormat106FilterDecay105RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format106Profile::filterDecay105,
        format106FilterDecay105OverrideMaps());
}

std::optional<int> remapFormat106Filter21RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> specialRawValue = remapFormat106Filter21SpecialCase(document, paramIndex, rawValue)) {
        return specialRawValue;
    }

    return remapFormat106Filter21CurveCase(document, paramIndex, rawValue);
}

const FamilyRawMap* getPre105RawCorrectionMap(int paramIndex) {
    return findRawCorrectionMap(pre105RawCorrectionMaps(), paramIndex);
}

const std::array<ImplicitRawDerivationRule, 2>& pre105ImplicitRawDerivationRules() {
    static const std::array<ImplicitRawDerivationRule, 2> rules = {{
        {44, 50, &pre105ImplicitParam50RawMap()},
        {43, 51, &pre105ImplicitParam51RawMap()},
    }};
    return rules;
}

std::optional<int> findPresetRawValue(const Synth1PatchDocument& document, int paramIndex) {
    const auto it = std::find_if(document.entries.begin(), document.entries.end(), [&](const Synth1PatchEntry& entry) {
        return entry.index == paramIndex;
    });
    if (it == document.entries.end()) {
        return std::nullopt;
    }
    return it->rawValue;
}

const FamilyRawMap* findFamilySpecificRawCorrectionMap(const Synth1PatchModelFamilyRules& rules, int paramIndex, int rawValue) {
    for (size_t index = 0; index < rules.rawCorrectionMapLookupCount; ++index) {
        const FamilyRawMap* map = rules.rawCorrectionMapLookups[index](paramIndex);
        if (map == nullptr) {
            continue;
        }

        if (map->find(rawValue) != map->end()) {
            return map;
        }
    }

    return nullptr;
}

const Synth1PatchModelFamilyRules& getSynth1PatchModelFamilyRules(Synth1PatchFormatFamily family) {
    static const Synth1PatchModelFamilyRules defaultRules = {};
    static const Synth1PatchModelFamilyRules pre105Rules = {
        {getPre105RawCorrectionMap, getSharedFormat105RawCorrectionMap},
        2,
        pre105ImplicitRawDerivationRules().data(),
        pre105ImplicitRawDerivationRules().size(),
    };
    static const Synth1PatchModelFamilyRules format105Rules = {
        {getSharedFormat105RawCorrectionMap, nullptr},
        1,
        nullptr,
        0,
    };
    static const Synth1PatchModelFamilyRules format106To107Rules = {
        {getSharedFormat105RawCorrectionMapExceptParam37, nullptr},
        1,
        nullptr,
        0,
    };
    static const Synth1PatchModelFamilyRules format108Rules = {
        {getSharedFormat105RawCorrectionMap, nullptr},
        1,
        nullptr,
        0,
    };

    static const std::array<FamilyRuleRegistryEntry, 4> registry = {{
        {Synth1PatchFormatFamily::pre105, &pre105Rules},
        {Synth1PatchFormatFamily::format105, &format105Rules},
        {Synth1PatchFormatFamily::format106To107, &format106To107Rules},
        {Synth1PatchFormatFamily::format108, &format108Rules},
    }};

    for (const FamilyRuleRegistryEntry& entry : registry) {
        if (entry.family == family) {
            return *entry.rules;
        }
    }

    return defaultRules;
}

std::optional<int> applyFormat106OscillatorRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106Oscillator13RawValue(document, paramIndex, rawValue);
}

std::optional<int> applyFormat106DelayRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106Delay37RawValue(document, paramIndex, rawValue);
}

std::optional<int> applyFormat106AmplifierRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106Amplifier26To28RawValue(document, paramIndex, rawValue);
}

std::optional<int> applyFormat106ChorusRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> remappedValue = remapFormat106Chorus55RawValue(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = remapFormat106Chorus54SpecialCase(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = remapFormat106Chorus54MappedCase(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    return remapFormat106Chorus52RawValue(document, paramIndex, rawValue);
}

std::optional<int> applyFormat106FilterRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> remappedValue = remapFormat106Filter18RawValue(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = remapFormat106Filter16RawValue(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = remapFormat106Filter21SpecialCase(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    return remapFormat106Filter21CurveCase(document, paramIndex, rawValue);
}

std::optional<int> applyFormat106FamilyRuleRemappers(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> remappedValue = applyFormat106OscillatorRuleRemappers(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = applyFormat106DelayRuleRemappers(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = applyFormat106ChorusRuleRemappers(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    if (const std::optional<int> remappedValue = applyFormat106AmplifierRuleRemappers(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    return applyFormat106FilterRuleRemappers(document, paramIndex, rawValue);
}

void appendFamilyModeImplicitRawEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    static const std::array<FamilyImplicitAppender, 4> appenders = {{
        {Synth1PatchFormatFamily::pre105, appendPre105ModeImplicitRawEntries},
        {Synth1PatchFormatFamily::format105, appendFormat105ImplicitRawEntries},
        {Synth1PatchFormatFamily::format106To107, appendFormat106To107ImplicitRawEntries},
        {Synth1PatchFormatFamily::format108, appendFormat108ImplicitRawEntries},
    }};

    for (const FamilyImplicitAppender& candidate : appenders) {
        if (candidate.family != document.family()) {
            continue;
        }

        candidate.appender(derived, document);
        return;
    }
}

std::optional<int> applyFamilyRuleMaps(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    if (const std::optional<int> remappedValue = applyFormat106FamilyRuleRemappers(document, paramIndex, rawValue)) {
        return remappedValue;
    }

    const Synth1PatchModelFamilyRules& rules = getSynth1PatchModelFamilyRules(document.family());
    if (rules.rawCorrectionMapLookupCount == 0) {
        return std::nullopt;
    }

    const FamilyRawMap* map = findFamilySpecificRawCorrectionMap(rules, paramIndex, rawValue);
    if (map == nullptr) {
        return std::nullopt;
    }

    const auto it = map->find(rawValue);
    if (it == map->end()) {
        return std::nullopt;
    }

    return it->second;
}

} // namespace synth1patch::legacy

using namespace synth1patch::legacy;

int remapFamilySpecificRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    // DLL-audited rules take precedence over historical sampled/profile maps.
    // Only explicit source entries reach this function; do not synthesize defaults.
    if (document.version().has_value()) {
        const int version = *document.version();
        if (version >= 104 && version <= 113) {
            if (paramIndex == 84)
                return synth1::patch::nativeUnisonSpreadImportDial(version, rawValue);
            if (paramIndex == 91 || paramIndex == 92)
                return synth1::patch::nativePhaseImportDial(version, rawValue);
            if (paramIndex == 52)
                return synth1::patch::nativeChorusTimeDial(version, rawValue);
            if (paramIndex == 13 || paramIndex == 16 || paramIndex == 18
                || paramIndex == 21 || paramIndex == 26 || paramIndex == 28)
                return synth1::patch::nativeEnvelopeImportDial(version, paramIndex, rawValue, rawValue);
        }
    }
    if (const std::optional<int> overrideValue = applyProfileOverrides(document, paramIndex, rawValue)) {
        return overrideValue.value();
    }

    if (const std::optional<int> preservedValue = applyPreserveRules(document, paramIndex, rawValue)) {
        return preservedValue.value();
    }

    if (const std::optional<int> familyRuleValue = applyFamilyRuleMaps(document, paramIndex, rawValue)) {
        return familyRuleValue.value();
    }

    return rawValue;
}

std::vector<Synth1PatchEntry> buildRemappedSynth1PatchEntries(const Synth1PatchDocument& document) {
    std::vector<Synth1PatchEntry> remappedEntries;
    remappedEntries.reserve(document.entries.size());

    for (const Synth1PatchEntry& entry : document.entries) {
        remappedEntries.push_back({
            entry.index,
            remapFamilySpecificRawValue(document, entry.index, entry.rawValue),
        });
    }

    return remappedEntries;
}

std::vector<Synth1PatchEntry> deriveFamilySpecificRawImplicitEntries(const Synth1PatchDocument& document) {
    std::vector<Synth1PatchEntry> derived;
    deriveFamilySpecificRawImplicitEntries(document,derived);
    return derived;
}

void deriveFamilySpecificRawImplicitEntries(const Synth1PatchDocument& document, std::vector<Synth1PatchEntry>& derived) {
    const Synth1PatchModelFamilyRules& rules = getSynth1PatchModelFamilyRules(document.family());
    derived.clear();
    for (size_t index = 0; index < rules.implicitRawValueRuleCount; ++index) {
        appendMappedImplicitRawEntry(derived, document, rules.implicitRawValueRules[index]);
    }
    appendFamilyModeImplicitRawEntries(derived, document);
}
