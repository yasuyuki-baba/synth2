#pragma once

#include <optional>
#include <string>
#include <vector>

enum class Synth1PatchFormatFamily;

struct Synth1PatchEntry {
    int index = 0;
    int rawValue = 0;
};

struct Synth1PatchDocument {
    static Synth1PatchDocument makeParsed(std::optional<int> version = std::nullopt);
    static Synth1PatchDocument makeParsed(std::string name, std::optional<int> version, std::vector<Synth1PatchEntry> entries);

    Synth1PatchFormatFamily family() const noexcept;
    const std::optional<int>& version() const noexcept;

    std::string name;
    std::vector<Synth1PatchEntry> entries;

private:
    Synth1PatchFormatFamily family_;
    std::optional<int> version_;
    Synth1PatchDocument(Synth1PatchFormatFamily family, std::optional<int> version);
};

enum class Synth1AppliedValueKind {
    raw,
    normalized,
};

struct Synth1ImplicitParamValue {
    static Synth1ImplicitParamValue fromRaw(int index, int rawValue) {
        return {index, Synth1AppliedValueKind::raw, rawValue, 0.0f};
    }

    static Synth1ImplicitParamValue fromNormalized(int index, float normalizedValue) {
        return {index, Synth1AppliedValueKind::normalized, 0, normalizedValue};
    }

    bool usesRawValue() const noexcept {
        return kind == Synth1AppliedValueKind::raw;
    }

    bool usesNormalizedValue() const noexcept {
        return kind == Synth1AppliedValueKind::normalized;
    }

    int index = 0;
    Synth1AppliedValueKind kind = Synth1AppliedValueKind::raw;
    int rawValue = 0;
    float normalizedValue = 0.0f;
};

enum class Synth1PatchFormatFamily {
    pre105,
    format105,
    format106To107,
    format108,
    format109Plus,
};

std::string describeSynth1PatchVersionLabel(const Synth1PatchDocument& document);
