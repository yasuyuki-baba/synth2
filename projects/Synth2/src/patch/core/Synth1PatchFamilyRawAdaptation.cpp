#include "Synth1PatchFamilyRawAdaptationInternal.h"
#include "Synth1PatchNativeControls.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <map>
#include <optional>

namespace {

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
    talkingLeader,
    powerArp,
};

enum class Format107Profile {
    defaultCase,
    known001Like,
    known013Like,
    sensitivityOutlier,
};

enum class Format108Profile {
    defaultCase,
    nonPacked001Like,
    packedGeneral,
    packed001Like,
};

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

std::optional<int> findPresetRawValue(const Synth1PatchDocument& document, int paramIndex);
std::optional<int> remapFormat106PowerArpRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
std::optional<int> remapFormat106TalkingLeaderRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue);
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
const std::map<int, FamilyRawMap>& format107Known013LikeOverrideMaps();
const std::map<int, FamilyRawMap>& format107Known001LikeOverrideMaps();
const std::map<int, FamilyRawMap>& format108NonPacked001LikeOverrideMaps();
const std::map<int, FamilyRawMap>& format108PackedGeneralOverrideMaps();
const std::map<int, FamilyRawMap>& format108Packed001LikeOverrideMaps();
const std::map<int, FamilyRawMap>& format106PowerArpOverrideMaps();
const std::map<int, FamilyRawMap>& format106TalkingLeaderOverrideMaps();
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

std::optional<int> remapFormat107Known013LikeRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat107ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format107Profile::known013Like,
        format107Known013LikeOverrideMaps());
}

std::optional<int> remapFormat107Known001LikeRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat107ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format107Profile::known001Like,
        format107Known001LikeOverrideMaps());
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
        Format108Profile::packed001Like,
        format108Packed001LikeOverrideMaps());
}

std::optional<int> remapFormat108NonPacked001LikeRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat108ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format108Profile::nonPacked001Like,
        format108NonPacked001LikeOverrideMaps());
}

const std::map<int, FamilyRawMap>& sharedFormat105RawCorrectionMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {13, {{0, 0}, {13, 0}, {31, 28}, {32, 29}, {36, 34}, {47, 47}, {58, 57}, {60, 59}, {66, 64}, {96, 91}, {104, 99}}},
        {16, {{0, 0}, {19, 6}, {21, 10}, {25, 18}, {34, 32}, {43, 42}, {45, 45}, {52, 52}, {54, 53}, {55, 54}, {60, 59}, {61, 60}, {62, 61}, {63, 62}, {64, 62}, {65, 63}, {66, 64}, {67, 65}, {71, 68}, {75, 72}, {76, 73}, {77, 74}, {78, 74}, {80, 76}, {81, 77}, {82, 78}, {84, 80}, {87, 82}, {90, 85}, {92, 87}, {93, 88}, {94, 89}, {95, 90}, {96, 91}, {97, 92}, {98, 93}, {101, 96}, {102, 97}, {105, 100}, {106, 101}, {107, 102}, {109, 104}, {113, 109}, {114, 110}, {118, 115}, {124, 123}}},
        {18, {{0, 0}, {33, 30}, {39, 38}, {61, 60}, {64, 62}, {65, 63}, {70, 68}, {73, 70}, {75, 72}, {76, 73}, {77, 74}, {78, 74}, {79, 75}, {80, 76}, {83, 79}, {84, 80}, {88, 83}, {90, 85}, {91, 86}, {93, 88}, {97, 92}, {99, 94}, {100, 95}, {102, 97}, {103, 98}, {104, 99}, {105, 100}, {108, 103}, {111, 106}, {127, 127}}},
        {21, {{0, 63}, {13, 70}, {15, 71}, {17, 72}, {18, 72}, {22, 74}, {24, 75}, {25, 76}, {27, 77}, {29, 78}, {31, 79}, {34, 80}, {35, 81}, {36, 81}, {37, 82}, {39, 83}, {40, 83}, {41, 84}, {42, 84}, {43, 85}, {44, 85}, {45, 86}, {48, 87}, {53, 90}, {54, 90}, {57, 92}, {61, 94}, {64, 95}, {65, 96}, {66, 96}, {68, 97}, {69, 98}, {73, 100}, {79, 103}, {81, 104}, {82, 104}, {83, 105}, {85, 106}, {87, 107}, {88, 107}, {94, 110}, {95, 111}, {100, 113}, {102, 114}, {107, 117}, {114, 120}, {116, 121}, {127, 127}}},
        {26, {{0, 0}, {3, 0}, {25, 18}, {28, 23}, {35, 33}, {42, 41}, {54, 53}, {56, 55}, {63, 62}, {64, 62}, {66, 64}, {67, 65}, {68, 66}, {71, 68}, {78, 74}, {79, 75}, {80, 76}, {81, 77}, {82, 78}, {85, 81}, {87, 82}, {93, 88}, {94, 89}, {96, 91}, {97, 92}, {98, 93}, {99, 94}, {100, 95}, {101, 96}, {102, 97}, {107, 102}, {110, 105}, {114, 110}}},
        {28, {{0, 0}, {8, 0}, {19, 6}, {22, 12}, {28, 23}, {32, 29}, {34, 32}, {35, 33}, {36, 34}, {39, 38}, {40, 39}, {41, 40}, {42, 41}, {43, 42}, {45, 45}, {47, 47}, {49, 49}, {51, 51}, {52, 52}, {53, 52}, {54, 53}, {59, 58}, {63, 62}, {64, 62}, {65, 63}, {66, 64}, {67, 65}, {68, 66}, {70, 68}, {71, 68}, {74, 71}, {76, 73}, {77, 74}, {78, 74}, {81, 77}, {82, 78}, {85, 81}, {87, 82}, {88, 83}, {89, 84}, {97, 92}, {99, 94}, {104, 99}, {107, 102}, {111, 106}}},
        {37, {{0, 0}, {2, 1}, {3, 1}, {4, 2}, {5, 2}, {6, 3}, {7, 3}, {9, 4}, {10, 5}, {13, 6}, {17, 8}, {20, 10}, {22, 11}, {24, 12}, {25, 12}, {26, 13}, {29, 14}, {31, 15}, {32, 16}, {33, 16}, {35, 17}, {38, 19}, {39, 19}, {40, 20}, {41, 20}, {42, 21}, {43, 21}, {44, 22}, {50, 25}, {62, 31}, {64, 32}, {68, 34}, {69, 34}, {73, 36}, {89, 44}, {117, 58}}},
        {54, {{19, 24}, {26, 29}, {56, 46}, {64, 50}}},
        {55, {{0, 64}, {60, 64}, {78, 64}, {127, 6}}},
    };

    return maps;
}

const std::map<int, FamilyRawMap>& pre105RawCorrectionMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {12, {{7, 17}, {31, 49}}},
        {15, {{15, 33}, {24, 43}, {35, 52}, {85, 84}}},
        {16, {{58, 57}, {59, 58}, {100, 95}}},
        {18, {{54, 53}, {98, 93}}},
        {21, {{90, 108}, {91, 109}, {96, 111}}},
        {3, {{58, 53}, {61, 58}, {62, 66}, {64, 66}, {65, 66}, {66, 70}, {68, 73}, {69, 75}, {70, 76}}},
        {25, {{4, 9}, {5, 12}, {10, 24}, {18, 37}, {40, 55}, {90, 88}}},
        {26, {{74, 71}, {112, 108}}},
        {27, {{64, 93}, {71, 97}}},
        {28, {{30, 26}}},
        {29, {{30, 63}, {41, 74}, {43, 76}, {45, 78}, {49, 81}, {50, 82}, {52, 84}, {55, 86}, {65, 93}, {67, 95}, {70, 97}, {75, 100}, {107, 118}}},
        {30, {{63, 22}, {64, 22}, {106, 51}, {127, 85}}},
        {37, {{12, 6}, {20, 10}, {28, 14}, {32, 16}, {36, 18}, {45, 22}, {66, 33}, {71, 35}}},
        {43, {{7, 15}, {9, 19}, {43, 53}, {46, 55}, {48, 56}, {60, 63}, {64, 65}, {76, 71}, {86, 76}, {87, 76}}},
        {48, {{64, 65}}},
        {54, {{30, 32}, {36, 35}, {37, 36}}},
    };

    return maps;
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

const std::map<int, FamilyRawMap>& format107Known013LikeOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {16, {{90, 95}}},
        {18, {{62, 64}}},
        {26, {{97, 102}}},
        {50, {{20, 69}}},
        {51, {{20, 68}}},
        {75, {{48, 1}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format107Known001LikeOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {3, {{63, 66}}},
        {15, {{28, 47}}},
        {21, {{108, 117}}},
        {28, {{92, 87}}},
        {29, {{91, 109}}},
        {30, {{64, 22}}},
        {32, {{0, 1}}},
        {37, {{8, 4}}},
        {41, {{6, 4}}},
        {43, {{64, 65}}},
        {46, {{6, 4}}},
        {48, {{64, 65}}},
        {50, {{20, 82}}},
        {51, {{20, 80}}},
        {54, {{50, 43}}},
        {71, {{2, 0}}},
        {72, {{35, 64}}},
        {83, {{95, 66}}},
    };
    return maps;
}


const std::map<int, FamilyRawMap>& format108PackedGeneralOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {16, {{62, 62}, {81, 81}, {84, 84}, {114, 114}, {124, 124}}},
        {26, {{79, 79}, {102, 102}}},
        {28, {{8, 8}, {87, 87}}},
        {37, {{13, 13}, {17, 17}, {35, 35}, {73, 73}, {125, 125}}},
        {54, {{19, 19}, {26, 26}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format108NonPacked001LikeOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {3, {{81, 94}}},
        {16, {{64, 62}}},
        {18, {{64, 62}}},
        {21, {{128, 127}}},
        {29, {{107, 118}}},
        {30, {{64, 22}}},
        {32, {{0, 1}}},
        {37, {{20, 10}}},
        {43, {{64, 65}}},
        {46, {{5, 4}}},
        {48, {{64, 65}}},
        {50, {{20, 100}}},
        {51, {{20, 80}}},
        {54, {{50, 43}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format108Packed001LikeOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {3, {{44, 31}}},
        {12, {{122, 120}}},
        {13, {{8, 0}}},
        {15, {{15, 33}}},
        {18, {{57, 56}}},
        {21, {{12, 69}}},
        {25, {{35, 52}}},
        {26, {{111, 106}}},
        {27, {{36, 70}}},
        {28, {{58, 57}}},
        {29, {{110, 119}}},
        {30, {{52, 17}}},
        {32, {{1, 2}}},
        {37, {{8, 4}}},
        {41, {{1, 2}}},
        {43, {{43, 53}}},
        {48, {{85, 75}}},
        {50, {{37, 67}}},
        {51, {{104, 96}}},
        {52, {{9, 14}}},
        {54, {{103, 68}}},
        {55, {{41, 64}}},
        {67, {{1, 0}}},
        {68, {{1, 0}}},
        {69, {{1, 0}}},
        {75, {{97, 48}}},
        {76, {{106, 0}}},
        {79, {{80, 64}}},
        {80, {{29, 64}}},
        {81, {{123, 64}}},
        {83, {{68, 66}}},
        {84, {{39, 64}}},
        {85, {{48, 24}}},
        {90, {{53, 64}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106PowerArpOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {13, {{23, 14}}},
        {52, {{20, 22}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106TalkingLeaderOverrideMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {3, {{58, 53}}},
        {15, {{62, 66}}},
        {16, {{105, 95}}},
        {18, {{78, 71}}},
        {21, {{60, 110}}},
        {26, {{28, 14}}},
        {27, {{114, 121}}},
        {29, {{85, 106}}},
        {30, {{28, 8}}},
        {32, {{1, 2}}},
        {37, {{17, 4}}},
        {41, {{5, 4}}},
        {43, {{43, 53}}},
        {48, {{15, 29}}},
        {50, {{127, 72}}},
        {51, {{127, 98}}},
        {54, {{64, 43}}},
        {70, {{1, 0}}},
        {71, {{2, 0}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106Oscillator13Maps() {
    static const std::map<int, FamilyRawMap> maps = {
        {13, {{18, 4}, {21, 10}, {41, 40}, {59, 58}, {65, 63}, {69, 67}, {77, 74}, {79, 75}, {107, 102}, {111, 106}, {118, 115}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106Filter16Maps() {
    static const std::map<int, FamilyRawMap> maps = {
        {16, {{56, 55}, {59, 58}, {115, 111}, {117, 114}, {119, 116}, {120, 117}, {121, 119}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106Filter18Maps() {
    static const std::map<int, FamilyRawMap> maps = {
        {18, {{41, 40}, {63, 62}, {66, 64}, {72, 69}, {82, 78}, {86, 81}, {87, 82}, {95, 90}, {101, 96}, {106, 101}, {109, 104}, {110, 105}, {117, 114}, {118, 115}, {120, 117}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106Chorus52Maps() {
    static const std::map<int, FamilyRawMap> maps = {
        {52, {{55, 54}, {59, 58}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106Chorus54Maps() {
    static const std::map<int, FamilyRawMap> maps = {
        {54, {{4, 13}, {5, 13}, {7, 15}, {9, 16}, {11, 18}, {12, 18}, {15, 21}, {18, 23}, {21, 25}, {22, 26}, {23, 27}, {24, 28}, {29, 31}, {37, 36}, {58, 47}}},
    };
    return maps;
}

const std::map<int, FamilyRawMap>& format106AmplifierMaps() {
    static const std::map<int, FamilyRawMap> maps = {
        {26, {{2, 0}, {44, 43}, {74, 71}, {89, 84}, {91, 86}, {95, 90}}},
        {28, {{18, 4}, {27, 21}, {44, 43}, {58, 57}, {72, 69}, {73, 70}, {79, 75}, {84, 80}, {91, 86}, {100, 95}, {101, 96}, {102, 97}, {103, 98}, {104, 99}, {105, 100}, {106, 101}, {107, 102}, {108, 103}, {109, 104}, {114, 110}}},
    };
    return maps;
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

bool documentMatchesRawValues(const Synth1PatchDocument& document, std::initializer_list<Synth1PatchEntry> expectedEntries) {
    return std::all_of(expectedEntries.begin(), expectedEntries.end(), [&](const Synth1PatchEntry& entry) {
        return findPresetRawValue(document, entry.index) == std::optional<int>(entry.rawValue);
    });
}


bool containsFormat108PackedMidiBlock(const Synth1PatchDocument& document) {
    for (int index = 86; index <= 89; ++index) {
        const std::optional<int> rawValue = findPresetRawValue(document, index);
        if (rawValue.has_value() && rawValue.value() > 127) {
            return true;
        }
    }
    return false;
}

Format106Profile detectFormat106Profile(const Synth1PatchDocument& document) {
    if (document.version() == 106 && documentMatchesRawValues(document, {
        {16, 105}, {18, 78}, {21, 60}, {26, 28}, {29, 85}, {37, 17},
        {43, 43}, {44, 98}, {53, 16}, {54, 64}, {55, 29},
    })) {
        return Format106Profile::talkingLeader;
    }

    if (document.version() == 106 && documentMatchesRawValues(document, {
        {13, 23}, {16, 64}, {18, 101}, {26, 74}, {28, 42}, {30, 22},
        {37, 55}, {43, 48}, {44, 97}, {50, 0}, {51, 0}, {52, 20},
        {53, 122}, {54, 58}, {55, 33},
    })) {
        return Format106Profile::powerArp;
    }

    return Format106Profile::defaultCase;
}

Format107Profile detectFormat107Profile(const Synth1PatchDocument& document) {
    if (document.version() == 107 && documentMatchesRawValues(document, {
        {3, 63}, {15, 28}, {21, 108}, {28, 92}, {29, 91}, {30, 64},
        {32, 0}, {37, 8}, {41, 6}, {43, 64}, {46, 6}, {48, 64},
        {50, 20}, {51, 20}, {54, 50}, {71, 2}, {72, 35}, {83, 95},
    })) {
        return Format107Profile::known001Like;
    }

    if (document.version() == 107 && documentMatchesRawValues(document, {
        {3, 63}, {15, 28}, {21, 108}, {28, 92}, {29, 80}, {30, 64},
        {32, 0}, {37, 8}, {41, 6}, {43, 64}, {46, 6}, {48, 64},
        {50, 20}, {51, 20}, {54, 50}, {71, 2}, {72, 35}, {83, 95},
    })) {
        return Format107Profile::known013Like;
    }

    if (documentMatchesRawValues(document, {
        {32, 0}, {38, 0}, {43, 64}, {44, 58}, {48, 64}, {71, 2}, {72, 35}, {83, 95},
    })) {
        return Format107Profile::sensitivityOutlier;
    }

    return Format107Profile::defaultCase;
}

Format108Profile detectFormat108Profile(const Synth1PatchDocument& document) {
    if (document.version() == 108
        && !containsFormat108PackedMidiBlock(document)
        && documentMatchesRawValues(document, {
            {3, 81}, {16, 64}, {18, 64}, {21, 128},
            {29, 107}, {30, 64}, {32, 0}, {37, 20},
            {43, 64}, {46, 5}, {48, 64}, {50, 20},
            {51, 20}, {54, 50}, {71, 0}, {72, 64}, {83, 66},
        })) {
        return Format108Profile::nonPacked001Like;
    }

    if (document.version() == 108
        && containsFormat108PackedMidiBlock(document)
        && documentMatchesRawValues(document, {
            {38, 0}, {43, 43}, {44, 115}, {50, 37}, {51, 104}, {75, 97},
        })) {
        return Format108Profile::packed001Like;
    }

    if (containsFormat108PackedMidiBlock(document)) {
        return Format108Profile::packedGeneral;
    }

    return Format108Profile::defaultCase;
}

const std::array<ProfileRawRemapper<Format106Profile>, 2>& format106ProfileRemappers() {
    static const std::array<ProfileRawRemapper<Format106Profile>, 2> remappers = {{
        {Format106Profile::talkingLeader, remapFormat106TalkingLeaderRawValue},
        {Format106Profile::powerArp, remapFormat106PowerArpRawValue},
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
        {Format107Profile::known001Like, remapFormat107Known001LikeRawValue},
        {Format107Profile::known013Like, remapFormat107Known013LikeRawValue},
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
        {Format108Profile::nonPacked001Like, remapFormat108NonPacked001LikeRawValue},
        {Format108Profile::packed001Like, remapFormat108Packed001InactiveSectionRawValue},
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

bool isFormat106NonTalkingLeaderParam(const Synth1PatchDocument& document, int paramIndex, int expectedParamIndex) {
    return isFormat106Param(document, paramIndex, expectedParamIndex)
        && detectFormat106Profile(document) != Format106Profile::talkingLeader;
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
    if (detectFormat106Profile(document) == Format106Profile::talkingLeader) {
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
    if (!isFormat106NonTalkingLeaderParam(document, paramIndex, 37)) {
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

std::optional<int> remapFormat106PowerArpRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format106Profile::powerArp,
        format106PowerArpOverrideMaps());
}

std::optional<int> remapFormat106Oscillator13RawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106MappedRawValue(document, paramIndex, rawValue, format106Oscillator13Maps(), 13);
}

std::optional<int> remapFormat106TalkingLeaderRawValue(const Synth1PatchDocument& document, int paramIndex, int rawValue) {
    return remapFormat106ProfileMappedRawValue(
        document,
        paramIndex,
        rawValue,
        Format106Profile::talkingLeader,
        format106TalkingLeaderOverrideMaps());
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

const FamilyRawMap& pre105ImplicitParam50RawMap() {
    static const FamilyRawMap map = {
        {2, 126}, {4, 125}, {5, 124}, {13, 120}, {16, 119}, {20, 117}, {33, 110}, {34, 110}, {64, 95},
    };
    return map;
}

const FamilyRawMap& pre105ImplicitParam51RawMap() {
    static const FamilyRawMap map = {
        {7, 123}, {9, 122}, {43, 105}, {46, 104}, {48, 103}, {60, 97}, {64, 95}, {76, 89}, {86, 84}, {87, 83},
    };
    return map;
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

std::optional<int> deriveFormat108CompatibilitySensitivity(int sourceSensitivity, int sourceLfoAmount) {
    if (sourceSensitivity < 0 || sourceSensitivity > 127 || sourceLfoAmount < 0 || sourceLfoAmount > 127) {
        return std::nullopt;
    }

    const float retainedDepth = 1.0f - (static_cast<float>(sourceLfoAmount) / 127.0f);
    const float scaledSensitivity = retainedDepth * static_cast<float>(sourceSensitivity) * (63.0f / 127.0f);
    return 64 + static_cast<int>(scaledSensitivity);
}

const std::array<int, 128>& format108PlayModeZeroDetuneMap() {
    static const std::array<int, 128> map = {{
        1, 5, 7, 10, 13, 15, 18, 20, 22, 25, 27, 29, 31, 33, 35, 36,
        38, 40, 41, 43, 45, 46, 48, 49, 51, 52, 53, 55, 56, 57, 59, 60,
        61, 62, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
        78, 79, 80, 81, 82, 83, 83, 84, 85, 86, 87, 88, 88, 89, 90, 91,
        91, 92, 93, 94, 94, 95, 96, 96, 97, 98, 99, 99, 100, 100, 101, 102,
        102, 103, 104, 104, 105, 105, 106, 107, 107, 108, 108, 109, 110, 110, 111, 111,
        112, 112, 113, 113, 114, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 119,
        120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 125, 125, 125, 126, 126, 127,
    }};
    return map;
}

const std::array<int, 128>& format108PlayModeUnisonDetuneMap() {
    static const std::array<int, 128> map = {{
        1, 7, 12, 16, 21, 25, 28, 32, 35, 38, 41, 44, 47, 50, 52, 55,
        57, 59, 62, 64, 66, 68, 70, 72, 73, 75, 77, 79, 80, 82, 83, 85,
        86, 88, 89, 91, 92, 93, 95, 96, 97, 98, 100, 101, 102, 103, 104, 105,
        106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
        121, 122, 123, 124, 125, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    }};
    return map;
}

std::optional<int> deriveFormat108CompatibilityDetune(int playMode, int sourceDetune) {
    if (sourceDetune < 0 || sourceDetune > 127) {
        return std::nullopt;
    }

    if (playMode == 0) {
        return format108PlayModeZeroDetuneMap()[static_cast<size_t>(sourceDetune)];
    }

    if (playMode == 1 || playMode == 2) {
        return format108PlayModeUnisonDetuneMap()[static_cast<size_t>(sourceDetune)];
    }

    return std::nullopt;
}

void appendMappedImplicitRawEntry(
    std::vector<Synth1PatchEntry>& derived,
    const Synth1PatchDocument& document,
    const ImplicitRawDerivationRule& rule) {
    const std::optional<int> sourceValue = findPresetRawValue(document, rule.sourceIndex);
    if (!sourceValue.has_value() || rule.rawValueMap == nullptr) {
        return;
    }

    const auto it = rule.rawValueMap->find(sourceValue.value());
    if (it == rule.rawValueMap->end()) {
        return;
    }

    derived.push_back({rule.targetIndex, it->second});
}

void appendDerivedRawEntryIfPresent(std::vector<Synth1PatchEntry>& derived, int index, const std::optional<int>& rawValue) {
    if (rawValue.has_value()) {
        derived.push_back({index, rawValue.value()});
    }
}

void appendDerivedSensitivityEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    const std::optional<int> source44 = findPresetRawValue(document, 44);
    const std::optional<int> source50 = findPresetRawValue(document, 50);
    if (source44.has_value() && source50.has_value()) {
        appendDerivedRawEntryIfPresent(derived, 50, deriveFormat108CompatibilitySensitivity(source50.value(), source44.value()));
    }

    const std::optional<int> source43 = findPresetRawValue(document, 43);
    const std::optional<int> source51 = findPresetRawValue(document, 51);
    if (source43.has_value() && source51.has_value()) {
        appendDerivedRawEntryIfPresent(derived, 51, deriveFormat108CompatibilitySensitivity(source51.value(), source43.value()));
    }
}

void appendDerivedDetuneEntry(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    const std::optional<int> source38 = findPresetRawValue(document, 38);
    const std::optional<int> source75 = findPresetRawValue(document, 75);
    if (!source38.has_value() || !source75.has_value()) {
        return;
    }

    appendDerivedRawEntryIfPresent(derived, 75, deriveFormat108CompatibilityDetune(source38.value(), source75.value()));
}

void appendPlayModeVoiceCompatibilityEntries(
    std::vector<Synth1PatchEntry>& derived,
    const std::optional<int>& source38,
    int playModeZeroDetune,
    int playModeUnisonDetune,
    int playModeUnisonSpread) {
    if (!source38.has_value()) {
        return;
    }

    if (source38.value() == 0) {
        derived.push_back({75, playModeZeroDetune});
        return;
    }

    if (source38.value() == 1 || source38.value() == 2) {
        derived.push_back({75, playModeUnisonDetune});
        derived.push_back({93, playModeUnisonSpread});
    }
}

void appendPre105ModeImplicitRawEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    const std::optional<int> source38 = findPresetRawValue(document, 38);
    if (!source38.has_value()) {
        return;
    }

    if (source38.value() == 0) {
        derived.push_back({75, 48});
        derived.push_back({93, 1});
        return;
    }

    if (source38.value() == 1) {
        derived.push_back({75, 70});
        derived.push_back({93, 5});
    }
}

bool shouldUseFormat108DetuneForFormat107(const Synth1PatchDocument& document) {
    return document.version() == 107 && detectFormat107Profile(document) == Format107Profile::defaultCase;
}

void appendFormat106To107ImplicitSensitivityEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    const Format106Profile format106Profile = detectFormat106Profile(document);
    const Format107Profile format107Profile = detectFormat107Profile(document);

    if (format106Profile == Format106Profile::talkingLeader) {
        const std::optional<int> source50 = findPresetRawValue(document, 50);
        if (source50 == std::optional<int>(127)) {
            derived.push_back({50, 72});
        }

        const std::optional<int> source51 = findPresetRawValue(document, 51);
        if (source51 == std::optional<int>(127)) {
            derived.push_back({51, 98});
        }
        return;
    }

    if (document.version() == 106 || format107Profile == Format107Profile::defaultCase) {
        appendDerivedSensitivityEntries(derived, document);
    }
}

void appendFormat106To107ImplicitVoiceEntries(
    std::vector<Synth1PatchEntry>& derived,
    const Synth1PatchDocument& document,
    const std::optional<int>& source38) {
    if (!source38.has_value()) {
        return;
    }

    if (shouldUseFormat108DetuneForFormat107(document)) {
        appendDerivedDetuneEntry(derived, document);

        if (source38.value() == 1 || source38.value() == 2) {
            derived.push_back({93, 4});
        }
        return;
    }

    appendPlayModeVoiceCompatibilityEntries(derived, source38, 48, 70, 4);
}

void appendFormat106To107ImplicitRawEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    appendFormat106To107ImplicitSensitivityEntries(derived, document);

    const std::optional<int> source38 = findPresetRawValue(document, 38);
    appendFormat106To107ImplicitVoiceEntries(derived, document, source38);
}

void appendFormat105ImplicitRawEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    appendDerivedSensitivityEntries(derived, document);
}

void appendFormat108ImplicitVoiceEntries(
    std::vector<Synth1PatchEntry>& derived,
    const std::optional<int>& source38) {
    if (!source38.has_value()) {
        return;
    }

    if (source38.value() == 1 || source38.value() == 2) {
        derived.push_back({93, 4});
    }
}

void appendFormat108ImplicitRawEntries(std::vector<Synth1PatchEntry>& derived, const Synth1PatchDocument& document) {
    const Format108Profile profile = detectFormat108Profile(document);
    if (profile == Format108Profile::packed001Like) {
        return;
    }

    appendDerivedDetuneEntry(derived, document);
    if (profile != Format108Profile::nonPacked001Like) {
        appendDerivedSensitivityEntries(derived, document);
    }

    const std::optional<int> source38 = findPresetRawValue(document, 38);
    appendFormat108ImplicitVoiceEntries(derived, source38);
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

} // namespace

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
