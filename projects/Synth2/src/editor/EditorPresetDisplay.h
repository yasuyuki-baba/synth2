#pragma once
#include <string>
#include <string_view>

namespace synth2::editor
{
inline std::string editorPresetDisplay(std::string_view name, std::string_view bank, bool modified)
{
    std::string label(name);
    if (!bank.empty())
        label += " / " + std::string(bank);
    if (modified)
        label += " *";
    return label;
}
}