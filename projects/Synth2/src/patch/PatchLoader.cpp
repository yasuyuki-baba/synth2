#include "PatchLoader.h"
#include <Synth1PatchParser.h>
#include <sstream>
#include <cctype>

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
        if(!line.empty())
        {
            if(line.compare(0,4,"ver=")==0)return false;
            const auto comma=line.find(',');
            // A comma in a textual title is not a parameter separator. Keep
            // numeric-looking first lines on the strict parameter parsing path.
            const bool numericStart=std::isdigit(static_cast<unsigned char>(line[0])) ||
                ((line[0]=='+' || line[0]=='-') && line.size()>1 &&
                 std::isdigit(static_cast<unsigned char>(line[1])));
            return comma==std::string::npos || (comma!=0 && !numericStart);
        }
    }
    return false;
}
Synth1PatchDocument parsePatchDocument(std::string_view text)
{
    std::istringstream input{std::string(text)};std::string line,cleaned;
    const bool named=hasPatchTitle(text);
    while(std::getline(input,line))
    {
        line=trimPatchLine(line);
        if(line.empty())continue;
        if(cleaned.empty() && !named)cleaned="Synth2 preset\n";
        cleaned+=line+'\n';
    }
    return parseSynth1PatchText(cleaned,true);
}
}
