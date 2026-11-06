#pragma once
#include "../../tools/host_scenarios.h"
#include <string>
#include <utility>
#include <vector>

namespace testfiles
{
// Self-authored loading/stress inputs; no bundled external bank is needed.
inline std::vector<std::string> generatedSounds()
{
    std::vector<std::string> result;
    for(int i=0;i<128;++i)
    {
        auto text=hostvalidation::patch(1+i%4,i%3);
        text.replace(0,text.find('\n'),"Generated sound "+std::to_string(i));
        text+="19,"+std::to_string(32+i%96)+"\n20,"+std::to_string(i%80)+"\n";
        result.push_back(std::move(text));
    }
    return result;
}
inline std::vector<std::pair<std::string,std::string>> generatedBank()
{
    std::vector<std::pair<std::string,std::string>> result;int index=0;
    for(auto& text:generatedSounds())result.emplace_back(std::to_string(index++)+".sy1",std::move(text));
    return result;
}
}
