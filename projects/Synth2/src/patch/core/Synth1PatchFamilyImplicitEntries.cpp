#include "Synth1PatchFamilyRules.h"

namespace synth1patch::legacy {
// Derive absent typed-import fields from the original serialized document.
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

    if (format106Profile == Format106Profile::filterDecay105) {
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
    if (profile == Format108Profile::packedLfoRate43) {
        return;
    }

    appendDerivedDetuneEntry(derived, document);
    if (profile != Format108Profile::unpackedFilterEnvelope128) {
        appendDerivedSensitivityEntries(derived, document);
    }

    const std::optional<int> source38 = findPresetRawValue(document, 38);
    appendFormat108ImplicitVoiceEntries(derived, source38);
}

}
