#include "Synth1PatchParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>
#include <string>

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}


std::runtime_error makeParseError(size_t lineNumber, std::string_view token, std::string_view reason) {
    return std::runtime_error(
        "Failed to parse token at line " + std::to_string(lineNumber) +
        ": token='" + std::string(token) + "', reason=" + std::string(reason));
}

int parseIntToken(std::string_view token, size_t lineNumber) {
    if (token.empty()) {
        throw makeParseError(lineNumber, token, "empty value");
    }

    int value = 0;
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(begin, end, value);

    if (result.ec == std::errc::invalid_argument) {
        throw makeParseError(lineNumber, token, "not a valid integer");
    }
    if (result.ec == std::errc::result_out_of_range) {
        throw makeParseError(lineNumber, token, "integer out of range");
    }
    if (result.ptr != end) {
        throw makeParseError(lineNumber, token, "contains trailing characters");
    }

    return value;
}

} // namespace

Synth1PatchDocument parseSynth1PatchText(std::string_view text, bool allowNativeDefaults) {
    std::string presetName;
    std::optional<int> presetVersion;
    std::vector<Synth1PatchEntry> presetEntries;
    bool firstLine = true;
    size_t lineStart = 0;
    size_t lineNumber = 1;

    while (lineStart <= text.size()) {
        size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = text.size();
        }

        std::string line(text.substr(lineStart, lineEnd - lineStart));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        line = trim(std::move(line));

        if (!line.empty()) {
            if (firstLine) {
                firstLine = false;
                presetName = line;
            } else if (line.rfind("ver=", 0) == 0) {
                if (presetVersion.has_value()) {
                    throw std::runtime_error(
                        "Malformed version header at line " + std::to_string(lineNumber) +
                        ": token='" + line + "', reason=duplicate version header");
                }
                const std::string versionText = trim(line.substr(4));
                presetVersion = parseIntToken(versionText, lineNumber);
            } else if (line.find('=') == std::string::npos) {
                const size_t comma = line.find(',');
                if (comma == std::string::npos) {
                    throw std::runtime_error(
                        "Malformed parameter entry at line " + std::to_string(lineNumber) +
                        ": token='" + line + "', reason=missing comma");
                }

                const std::string indexText = trim(line.substr(0, comma));
                const std::string valueText = trim(line.substr(comma + 1));
                const int index = parseIntToken(indexText, lineNumber);
                const int value = parseIntToken(valueText, lineNumber);
                presetEntries.push_back({index, value});
            }
        }

        if (lineEnd == text.size()) {
            break;
        }
        lineStart = lineEnd + 1;
        ++lineNumber;
    }

    if (presetEntries.empty() && !(allowNativeDefaults && ((!presetVersion && !presetName.empty()) || (presetVersion && (*presetVersion==100 || (*presetVersion>=104 && *presetVersion<=113)))))) {
        throw std::runtime_error("No parameter entries were found in the .sy1 file.");
    }

    return Synth1PatchDocument::makeParsed(std::move(presetName), presetVersion, std::move(presetEntries));
}
