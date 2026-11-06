#include "Synth1PatchFamilyRules.h"
#include <algorithm>

namespace synth1patch::legacy {
// Historical predicates inspect serialized values only. Names describe distinguishing
// fields, not patch identities. Keep each complete conjunction and its order intact.
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
        return Format106Profile::filterDecay105;
    }

    if (document.version() == 106 && documentMatchesRawValues(document, {
        {13, 23}, {16, 64}, {18, 101}, {26, 74}, {28, 42}, {30, 22},
        {37, 55}, {43, 48}, {44, 97}, {50, 0}, {51, 0}, {52, 20},
        {53, 122}, {54, 58}, {55, 33},
    })) {
        return Format106Profile::modDecay23;
    }

    return Format106Profile::defaultCase;
}

Format107Profile detectFormat107Profile(const Synth1PatchDocument& document) {
    if (document.version() == 107 && documentMatchesRawValues(document, {
        {3, 63}, {15, 28}, {21, 108}, {28, 92}, {29, 91}, {30, 64},
        {32, 0}, {37, 8}, {41, 6}, {43, 64}, {46, 6}, {48, 64},
        {50, 20}, {51, 20}, {54, 50}, {71, 2}, {72, 35}, {83, 95},
    })) {
        return Format107Profile::ampLevel91;
    }

    if (document.version() == 107 && documentMatchesRawValues(document, {
        {3, 63}, {15, 28}, {21, 108}, {28, 92}, {29, 80}, {30, 64},
        {32, 0}, {37, 8}, {41, 6}, {43, 64}, {46, 6}, {48, 64},
        {50, 20}, {51, 20}, {54, 50}, {71, 2}, {72, 35}, {83, 95},
    })) {
        return Format107Profile::ampLevel80;
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
        return Format108Profile::unpackedFilterEnvelope128;
    }

    if (document.version() == 108
        && containsFormat108PackedMidiBlock(document)
        && documentMatchesRawValues(document, {
            {38, 0}, {43, 43}, {44, 115}, {50, 37}, {51, 104}, {75, 97},
        })) {
        return Format108Profile::packedLfoRate43;
    }

    if (containsFormat108PackedMidiBlock(document)) {
        return Format108Profile::packedGeneral;
    }

    return Format108Profile::defaultCase;
}

}
