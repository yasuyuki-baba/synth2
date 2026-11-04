#include "PatchLoader.h"
#include <Synth1PatchParser.h>
#include <sstream>

namespace synth2
{
std::string trimPatchLine(std::string_view value)
{
    value=value.substr(0,value.find_first_of("#;"));
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string_view::npos)return {};
    return std::string(value.substr(first,value.find_last_not_of(" \t\r\n")-first+1));
}
bool hasPatchTitle(std::string_view text)
{
    std::istringstream input{std::string(text)};std::string line;
    while(std::getline(input,line))
    {
        line=trimPatchLine(line);
        if(!line.empty())return line.compare(0,4,"ver=")!=0 && line.find(',')==std::string::npos;
    }
    return false;
}
Synth1PatchDocument parsePatchDocument(std::string_view text)
{
    std::istringstream input{std::string(text)};std::string line,cleaned;
    while(std::getline(input,line))
    {
        line=trimPatchLine(line);
        if(line.empty())continue;
        if(cleaned.empty() && (line.compare(0,4,"ver=")==0 || line.find(',')!=std::string::npos))cleaned="Synth2 preset\n";
        cleaned+=line+'\n';
    }
    return parseSynth1PatchText(cleaned,true);
}
}
