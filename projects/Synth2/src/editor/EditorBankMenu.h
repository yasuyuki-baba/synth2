#pragma once
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace synth2::editor
{
struct EditorBankMenuItem
{
    std::string label;
    std::string path;
    std::string bankId; // Empty for classification-only folders.
    std::vector<EditorBankMenuItem> children;
};

// Input contains only banks with sounds. Intermediate folders cannot be selected.
inline std::vector<EditorBankMenuItem> editorBankMenu(
    const std::vector<std::pair<std::string,std::string>>& banks)
{
    std::vector<EditorBankMenuItem> result;
    for(const auto& [id,name]:banks)
    {
        auto* level=&result;
        std::size_t start=0;
        for(;;)
        {
            const auto slash=id.find('/',start);
            const auto label=slash==std::string::npos && start==0 ? name : id.substr(start,slash-start);
            const auto path=id.substr(0,slash);
            auto found=std::find_if(level->begin(),level->end(),[&](const auto& item){return item.path==path;});
            if(found==level->end()) {level->push_back({label,path,{},{}});found=level->end()-1;}
            if(slash==std::string::npos) {found->bankId=id;break;}
            level=&found->children;start=slash+1;
        }
    }
    const auto sort=[](auto&& self,auto& level)->void {
        std::sort(level.begin(),level.end(),[](const auto& a,const auto& b){return a.label<b.label;});
        for(auto& item:level)self(self,item.children);
    };
    sort(sort,result);
    return result;
}
}
