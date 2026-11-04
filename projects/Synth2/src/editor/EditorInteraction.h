#pragma once
#include "EditorLayout.h"
#include <cmath>
#include <optional>
#include <string>

namespace synth2::editor
{
// Shared by the native selector and portable interaction tests. Values are
// original source words; popup indices never become host parameter ranges.
inline std::optional<int> editorSelectedSourceValue(const std::vector<EditorChoice>& choices,int selection)
{
    if(selection<0 || selection>=static_cast<int>(choices.size())) return std::nullopt;
    return choices[static_cast<size_t>(selection)].value;
}
inline int editorToggledSourceValue(double source) noexcept { return source!=0 ? 0 : 1; }
inline std::string editorChoiceDisplay(bool toggle,double source,const std::vector<EditorChoice>& choices)
{
    if(toggle) return source!=0 ? "ON" : "OFF";
    for(const auto& choice:choices) if(std::abs(source-choice.value)<.001) return choice.label;
    return source==0 ? "Default" : "Custom";
}
}
