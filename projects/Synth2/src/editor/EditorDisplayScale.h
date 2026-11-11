#pragma once
#include "EditorResize.h"
#include <array>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace synth2::editor
{
inline constexpr std::array<int,7> editorScaleChoices{50,75,100,125,150,175,200};

// UI-only, locale-independent percentage entry. Invalid input preserves the view.
inline std::optional<float> editorScaleFromText(std::string_view text)
{
    const auto trim=[](std::string_view value) {
        const auto first=value.find_first_not_of(" \t\r\n");
        return first==value.npos ? std::string_view{} :
            value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
    };
    text=trim(text);
    if(!text.empty() && text.back()=='%')text=trim(text.substr(0,text.size()-1));
    if(text.empty() || text.size()>32)return {};
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    float percent=0;
    input>>percent;
    if(!input || input.peek()!=std::char_traits<char>::eof() || !std::isfinite(percent) ||
       percent<editorMinimumScale*100.f || percent>editorMaximumScale*100.f)return {};
    return percent/100.f;
}

inline std::string editorScaleText(float scale)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output<<std::fixed<<std::setprecision(1)<<scale*100.f;
    auto text=output.str();
    if(text.size()>2 && text.substr(text.size()-2)==".0")text.resize(text.size()-2);
    return text+"%";
}
}
