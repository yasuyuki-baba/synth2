#pragma once
#include <string>
#include <string_view>

namespace synth2::editor
{
struct EditorPresetDisplay
{
    std::string title, detail;
    std::string tooltip() const {return title+(detail.empty() ? "" : "\n"+detail);}
};
inline EditorPresetDisplay editorPresetDisplay(std::string_view name, std::string_view bank, bool modified)
{
    return {std::string(name)+(modified ? " *" : ""),std::string(bank)};
}
}
