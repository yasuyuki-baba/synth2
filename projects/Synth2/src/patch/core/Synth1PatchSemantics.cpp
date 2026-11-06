#include "Synth1PatchSemantics.h"

#include <algorithm>
#include <cctype>

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

} // namespace

Synth1PatchDocument::Synth1PatchDocument(Synth1PatchFormatFamily family, std::optional<int> version)
    : family_(family), version_(version) {}

Synth1PatchFormatFamily Synth1PatchDocument::family() const noexcept {
    return family_;
}

const std::optional<int>& Synth1PatchDocument::version() const noexcept {
    return version_;
}

Synth1PatchDocument Synth1PatchDocument::makeParsed(std::optional<int> version) {
    if (!version.has_value()) {
        return Synth1PatchDocument(Synth1PatchFormatFamily::pre105, std::nullopt);
    }

    if (version.value() < 105) {
        return Synth1PatchDocument(Synth1PatchFormatFamily::pre105, version);
    }
    if (version.value() == 105) {
        return Synth1PatchDocument(Synth1PatchFormatFamily::format105, version);
    }
    if (version.value() <= 107) {
        return Synth1PatchDocument(Synth1PatchFormatFamily::format106To107, version);
    }
    if (version.value() == 108) {
        return Synth1PatchDocument(Synth1PatchFormatFamily::format108, version);
    }

    return Synth1PatchDocument(Synth1PatchFormatFamily::format109Plus, version);
}

Synth1PatchDocument Synth1PatchDocument::makeParsed(std::string name, std::optional<int> version, std::vector<Synth1PatchEntry> entries) {
    Synth1PatchDocument document = makeParsed(version);
    document.name = trim(std::move(name));
    document.entries = std::move(entries);
    return document;
}

std::string describeSynth1PatchVersionLabel(const Synth1PatchDocument& document) {
    if (!document.version().has_value()) {
        return "pre105-inferred";
    }
    return std::to_string(document.version().value());
}