#pragma once
#include <array>
#include <vector>

namespace synth2::editor
{
struct EditorPage
{
    const char* name;
    std::vector<int> parameters;
};
inline constexpr std::array<const char*,3> editorViews {"Synth","MIDI","Library"};
inline constexpr int editorLibraryView = 2;

// Presentation only. Host editing steps must not redefine the source ranges or
// quantization used by the loading adapter or normalize recalled source words.
inline const std::array<EditorPage,6>& editorPages()
{
    static const std::array<EditorPage,6> pages {{
        {"Oscillators", {0,1,2,3,4,5,6,7,8,9,10,11,12,13,45,71,72,76,91,95,96,97}},
        {"Filter / Amp", {14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,90}},
        {"LFO", {41,42,43,44,46,47,48,49,57,58,67,68,69,70}},
        {"Performance", {31,32,33,34,38,39,40,59,73,74,75,84,85,92,93,94}},
        {"Effects", {35,36,37,52,53,54,55,56,60,61,62,63,64,65,66,77,78,79,80,81,82,83,98}},
        {"MIDI", {50,51,86,87,88,89}}
    }};
    return pages;
}
}
