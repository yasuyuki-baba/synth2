#include "Synth1PatchDocumentIo.h"

#include "Synth1ParameterDescriptor.h"

#include <algorithm>

Synth1PatchEntry* upsertSynth1PatchEntryByIndex(Synth1PatchDocument& document, int index, int rawValue) {
    auto it = std::find_if(document.entries.begin(), document.entries.end(), [&](const Synth1PatchEntry& entry) {
        return entry.index == index;
    });
    if (it != document.entries.end()) {
        it->rawValue = rawValue;
        return &*it;
    }

    document.entries.push_back({index, rawValue});
    return &document.entries.back();
}

Synth1PatchEntry* upsertSynth1PatchEntryByName(Synth1PatchDocument& document, std::string_view name, int rawValue) {
    const std::optional<int> index = findSynth1ParameterIndexByName(name);
    if (!index.has_value()) {
        return nullptr;
    }
    return upsertSynth1PatchEntryByIndex(document, index.value(), rawValue);
}

bool eraseSynth1PatchEntryByIndex(Synth1PatchDocument& document, int index) {
    const auto it = std::remove_if(document.entries.begin(), document.entries.end(), [&](const Synth1PatchEntry& entry) {
        return entry.index == index;
    });
    const bool removed = it != document.entries.end();
    document.entries.erase(it, document.entries.end());
    return removed;
}

bool eraseSynth1PatchEntryByName(Synth1PatchDocument& document, std::string_view name) {
    const std::optional<int> index = findSynth1ParameterIndexByName(name);
    if (!index.has_value()) {
        return false;
    }
    return eraseSynth1PatchEntryByIndex(document, index.value());
}

std::string formatSynth1PatchText(const Synth1PatchDocument& document) {
    std::string text;
    text += document.name;
    text += "\r\n";

    if (document.version().has_value()) {
        text += "ver=";
        text += std::to_string(document.version().value());
        text += "\r\n";
    }

    for (const Synth1PatchEntry& entry : document.entries) {
        text += std::to_string(entry.index);
        text += ",";
        text += std::to_string(entry.rawValue);
        text += "\r\n";
    }

    return text;
}