#include "Synth1ParameterDescriptor.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {

bool isWithinClosedRange(int value, int begin, int end) {
    return value >= begin && value <= end;
}

std::string normalizeParameterName(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());

    bool previousUnderscore = false;
    for (char ch : name) {
        const unsigned char current = static_cast<unsigned char>(ch);
        if (std::isalnum(current)) {
            normalized.push_back(static_cast<char>(std::tolower(current)));
            previousUnderscore = false;
            continue;
        }

        if (!previousUnderscore && !normalized.empty()) {
            normalized.push_back('_');
            previousUnderscore = true;
        }
    }

    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }

    return normalized;
}

std::optional<Synth1ParameterSection> classifyKnownSection(int index) {
    if (isWithinClosedRange(index, 0, 13)
        || index == 45
        || index == 71
        || index == 72
        || index == 76
        || index == 91
        || isWithinClosedRange(index, 95, 97)) {
        return Synth1ParameterSection::oscillators;
    }

    if (isWithinClosedRange(index, 14, 24)) {
        return Synth1ParameterSection::filter;
    }

    if (isWithinClosedRange(index, 25, 30)) {
        return Synth1ParameterSection::amplifier;
    }

    if (isWithinClosedRange(index, 31, 34) || index == 59) {
        return Synth1ParameterSection::arpeggiator;
    }

    if (isWithinClosedRange(index, 35, 37) || index == 65 || index == 82 || index == 83 || index == 98) {
        return Synth1ParameterSection::tempoDelay;
    }

    if (index == 38 || index == 39 || isWithinClosedRange(index, 73, 75) || index == 84 || index == 85 || isWithinClosedRange(index, 92, 94)) {
        return Synth1ParameterSection::voice;
    }

    if (index == 40 || index == 50 || index == 51 || isWithinClosedRange(index, 86, 89)) {
        return Synth1ParameterSection::wheelMidi;
    }

    if (isWithinClosedRange(index, 41, 49) || index == 57 || index == 58 || isWithinClosedRange(index, 67, 70)) {
        return Synth1ParameterSection::lfo;
    }

    if (isWithinClosedRange(index, 52, 56) || index == 64 || index == 66) {
        return Synth1ParameterSection::chorusFlanger;
    }

    if (isWithinClosedRange(index, 60, 63) || index == 90) {
        return Synth1ParameterSection::equalizerPan;
    }

    if (isWithinClosedRange(index, 77, 81)) {
        return Synth1ParameterSection::effect;
    }

    if (index == 252) {
        return Synth1ParameterSection::hidden;
    }

    return std::nullopt;
}

std::optional<int> resolveExplicitParameterName(std::string_view normalizedName) {
    static constexpr std::array<std::pair<std::string_view, int>, 6> aliases = {{
        {"osc_mix", 5},
        {"midi_src1", 86},
        {"midi_assign1", 87},
        {"midi_src2", 88},
        {"midi_assign2", 89},
        {"hidden_program_slot", 252},
    }};

    const auto it = std::find_if(aliases.begin(), aliases.end(), [&](const auto& alias) {
        return alias.first == normalizedName;
    });
    if (it == aliases.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Synth1ParameterSection> parseSectionPrefix(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, Synth1ParameterSection>, 12> sections = {{
        {"oscillators", Synth1ParameterSection::oscillators},
        {"filter", Synth1ParameterSection::filter},
        {"amplifier", Synth1ParameterSection::amplifier},
        {"arpeggiator", Synth1ParameterSection::arpeggiator},
        {"tempo_delay", Synth1ParameterSection::tempoDelay},
        {"chorus_flanger", Synth1ParameterSection::chorusFlanger},
        {"equalizer_pan", Synth1ParameterSection::equalizerPan},
        {"effect", Synth1ParameterSection::effect},
        {"voice", Synth1ParameterSection::voice},
        {"wheel_midi", Synth1ParameterSection::wheelMidi},
        {"lfo", Synth1ParameterSection::lfo},
        {"hidden", Synth1ParameterSection::hidden},
    }};

    const auto it = std::find_if(sections.begin(), sections.end(), [&](const auto& section) {
        return section.first == name;
    });
    if (it == sections.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::string makeCanonicalName(Synth1ParameterSection section, int index) {
    switch (section) {
    case Synth1ParameterSection::oscillators:
        if (index == 5) {
            return "osc_mix";
        }
        break;
    case Synth1ParameterSection::wheelMidi:
        if (index == 86) {
            return "midi_src1";
        }
        if (index == 87) {
            return "midi_assign1";
        }
        if (index == 88) {
            return "midi_src2";
        }
        if (index == 89) {
            return "midi_assign2";
        }
        break;
    case Synth1ParameterSection::hidden:
        if (index == 252) {
            return "hidden_program_slot";
        }
        break;
    default:
        break;
    }

    return std::string(synth1ParameterSectionName(section)) + "_" + std::to_string(index);
}

std::string makeDisplayName(Synth1ParameterSection section, int index) {
    const std::string sectionName = std::string(synth1ParameterSectionName(section));
    return sectionName + " " + std::to_string(index);
}

} // namespace

const char* synth1ParameterSectionName(Synth1ParameterSection section) {
    switch (section) {
    case Synth1ParameterSection::oscillators:
        return "oscillators";
    case Synth1ParameterSection::filter:
        return "filter";
    case Synth1ParameterSection::amplifier:
        return "amplifier";
    case Synth1ParameterSection::arpeggiator:
        return "arpeggiator";
    case Synth1ParameterSection::tempoDelay:
        return "tempo_delay";
    case Synth1ParameterSection::chorusFlanger:
        return "chorus_flanger";
    case Synth1ParameterSection::equalizerPan:
        return "equalizer_pan";
    case Synth1ParameterSection::effect:
        return "effect";
    case Synth1ParameterSection::voice:
        return "voice";
    case Synth1ParameterSection::wheelMidi:
        return "wheel_midi";
    case Synth1ParameterSection::lfo:
        return "lfo";
    case Synth1ParameterSection::hidden:
        return "hidden";
    }

    return "unknown";
}

std::optional<Synth1ParameterSection> classifySynth1ParameterSection(int index) {
    return classifyKnownSection(index);
}

std::optional<Synth1ParameterDescriptor> describeSynth1Parameter(int index) {
    const std::optional<Synth1ParameterSection> section = classifyKnownSection(index);
    if (!section.has_value()) {
        return std::nullopt;
    }

    return Synth1ParameterDescriptor {
        index,
        makeCanonicalName(section.value(), index),
        makeDisplayName(section.value(), index),
        section.value(),
    };
}

std::optional<int> findSynth1ParameterIndexByName(std::string_view name) {
    const std::string normalizedName = normalizeParameterName(name);
    if (normalizedName.empty()) {
        return std::nullopt;
    }

    if (const std::optional<int> explicitIndex = resolveExplicitParameterName(normalizedName); explicitIndex.has_value()) {
        return explicitIndex;
    }

    const size_t separator = normalizedName.find_last_of('_');
    if (separator == std::string::npos || separator + 1 >= normalizedName.size()) {
        return std::nullopt;
    }

    const std::optional<Synth1ParameterSection> section = parseSectionPrefix(std::string_view(normalizedName).substr(0, separator));
    if (!section.has_value()) {
        return std::nullopt;
    }

    const std::string_view suffix = std::string_view(normalizedName).substr(separator + 1);
    if (!std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return std::nullopt;
    }

    const int index = std::stoi(std::string(suffix));
    if (classifyKnownSection(index) != section) {
        return std::nullopt;
    }
    return index;
}

std::optional<Synth1ParameterDescriptor> findSynth1ParameterByName(std::string_view name) {
    const std::optional<int> index = findSynth1ParameterIndexByName(name);
    if (!index.has_value()) {
        return std::nullopt;
    }
    return describeSynth1Parameter(index.value());
}

std::vector<Synth1ParameterDescriptor> listSynth1ParameterDescriptors(Synth1ParameterSection section) {
    std::vector<Synth1ParameterDescriptor> descriptors;

    for (int index = 0; index <= 252; ++index) {
        if (classifyKnownSection(index) != section) {
            continue;
        }

        descriptors.push_back(describeSynth1Parameter(index).value());
    }

    return descriptors;
}

const Synth1PatchEntry* findSynth1PatchEntryByIndex(const Synth1PatchDocument& document, int index) {
    const auto it = std::find_if(document.entries.begin(), document.entries.end(), [&](const Synth1PatchEntry& entry) {
        return entry.index == index;
    });
    if (it == document.entries.end()) {
        return nullptr;
    }
    return &*it;
}

const Synth1PatchEntry* findSynth1PatchEntryByName(const Synth1PatchDocument& document, std::string_view name) {
    const std::optional<int> index = findSynth1ParameterIndexByName(name);
    if (!index.has_value()) {
        return nullptr;
    }
    return findSynth1PatchEntryByIndex(document, index.value());
}

std::vector<const Synth1PatchEntry*> listSynth1PatchEntriesBySection(const Synth1PatchDocument& document, Synth1ParameterSection section) {
    std::vector<const Synth1PatchEntry*> entries;
    entries.reserve(document.entries.size());

    for (const Synth1PatchEntry& entry : document.entries) {
        if (classifyKnownSection(entry.index) == section) {
            entries.push_back(&entry);
        }
    }

    return entries;
}