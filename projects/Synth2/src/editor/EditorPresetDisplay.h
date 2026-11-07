#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace synth2::editor
{
struct EditorPresetTag
{
    std::string text;
    bool automatic=false;
};
struct EditorPresetDisplay
{
    std::string title, detail;
    std::vector<EditorPresetTag> tags;
    std::string tooltip() const {
        std::string result=title+(detail.empty() ? "" : "\n"+detail);
        if(!tags.empty()) {
            result+='\n';
            for(std::size_t i=0;i<tags.size();++i) {if(i)result+=", ";result+=tags[i].text;}
        }
        return result;
    }
};
inline EditorPresetDisplay editorPresetDisplay(std::string_view name, std::string_view bank, bool modified,
    const std::vector<std::string>& manual={},const std::vector<std::string>& automatic={})
{
    EditorPresetDisplay display{std::string(name)+(modified ? " *" : ""),std::string(bank),{}};
    for(const auto& tag:manual)display.tags.push_back({tag,false});
    for(const auto& tag:automatic)display.tags.push_back({tag,true});
    return display;
}
}
