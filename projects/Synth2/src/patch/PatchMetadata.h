#pragma once
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace synth2
{
// Opaque header lines retain spelling, order, duplicate keys and unknown values.
// Version and name are owned by their codecs, never copied as opaque metadata.
inline std::vector<std::string> readPatchMetadata(std::string_view text)
{
    std::vector<std::string> result;
    std::istringstream input{std::string(text)};
    bool first=true;
    for(std::string line;std::getline(input,line);)
    {
        const auto start=line.find_first_not_of(" \t\r");
        if(start==std::string::npos || line[start]=='#' || line[start]==';')continue;
        line=line.substr(start,line.find_last_not_of(" \t\r")-start+1);
        const bool title=first && !line.starts_with("ver=");
        first=false;
        if(title)continue;
        const auto equals=line.find('=');
        if(equals==std::string::npos)continue;
        const auto key=line.substr(0,equals);
        if(key!="ver" && key!="name")result.push_back(line);
    }
    return result;
}

// Synth1's named patch colors. Unknown/default values have no marker.
inline std::optional<unsigned> patchColor(const std::vector<std::string>& metadata)
{
    std::optional<unsigned> result;
    for(const auto& line:metadata)
    {
        if(!line.starts_with("color="))continue;
        auto value=line.substr(6);
        const auto end=value.find_first_of("#;");value=value.substr(0,end);
        const auto first=value.find_first_not_of(" \t\r");
        value=first==std::string::npos ? "" : value.substr(first,value.find_last_not_of(" \t\r")-first+1);
        result.reset();
        if(value=="red")result=0xff0000;
        else if(value=="green")result=0x00ff00;
        else if(value=="blue")result=0x0000ff;
        else if(value=="yellow")result=0xffff00;
        else if(value=="magenta")result=0xff00ff;
        else if(value=="cyan")result=0x00ffff;
    }
    return result;
}
}
