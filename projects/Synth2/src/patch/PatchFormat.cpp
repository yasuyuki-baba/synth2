#include "PatchFormat.h"
#include "PatchLoader.h"
#include "Synth1SourceRange.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace synth2
{
namespace
{
template<class Value> Value number(std::string_view text)
{
    Value value{};
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())
        throw std::runtime_error("Invalid patch number");
    return value;
}
void validate(int index,float value)
{
    if(index<0 || index>=129 || index==126 || !std::isfinite(value)
        || value<(index==128 ? 0 : sourceMinimum(index))
        || value>(index==128 ? 1 : sourceMaximum(index)))
        throw std::runtime_error("Patch value is outside the supported parameter range");
}
}
bool isContinuousPatch(std::string_view text)
{
    std::istringstream input{std::string(text)};
    for(std::string line;std::getline(input,line);)
    {
        line=trimPatchLine(line);
        if(line.starts_with("ver="))
        {
            const auto token=trimPatchLine(line.substr(4));int version=0;
            const auto parsed=std::from_chars(token.data(),token.data()+token.size(),version);
            return parsed.ec==std::errc{} && parsed.ptr==token.data()+token.size()
                && version==currentPatchFormatVersion;
        }
    }
    return false;
}
ContinuousPatch parseContinuousPatch(std::string_view text)
{
    ContinuousPatch patch;std::array<bool,129> seen{};
    std::istringstream input{std::string(text)};bool header=false,first=true,named=false;
    for(std::string line;std::getline(input,line);)
    {
        // A quoted name is metadata, so comment markers inside it are literal.
        const auto start=line.find_first_not_of(" \t\r");
        if(start!=std::string::npos && line.substr(start).starts_with("name="))
        {
            if(!header || named)throw std::runtime_error("Invalid patch name header");
            std::istringstream name(line.substr(start+5));
            name>>std::ws;if(name.peek()!='\"')throw std::runtime_error("Patch name must be quoted");
            if(!(name>>std::quoted(patch.name)) || !(name>>std::ws).eof())throw std::runtime_error("Invalid patch name");
            named=true;continue;
        }
        line=trimPatchLine(line);if(line.empty())continue;
        if(first && !line.starts_with("ver=")) {patch.name=line;first=false;continue;}
        first=false;
        if(line.starts_with("ver="))
        {
            if(header || number<int>(trimPatchLine(line.substr(4)))!=currentPatchFormatVersion)
                throw std::runtime_error("Unsupported or duplicate patch version");
            header=true;continue;
        }
        const auto comma=line.find(',');
        if(!header || comma==std::string::npos)throw std::runtime_error("Malformed Synth2 patch entry");
        const int index=number<int>(trimPatchLine(line.substr(0,comma)));
        const float value=number<float>(trimPatchLine(line.substr(comma+1)));
        validate(index,value);
        if(seen[index])throw std::runtime_error("Duplicate patch parameter");
        patch.values[index]=value;seen[index]=true;
    }
    if(!header)throw std::runtime_error("Missing Synth2 patch version");
    // Complete snapshots avoid defaults changing the sound in future releases.
    for(int i=0;i<129;++i)if(i!=126 && !seen[i])throw std::runtime_error("Incomplete Synth2 patch");
    patch.values[126]=113;
    if(patch.name.empty())patch.name="Untitled";
    return patch;
}
std::string formatContinuousPatch(const ContinuousPatch& patch)
{
    if(patch.name.find_first_of("\r\n")!=std::string::npos)
        throw std::runtime_error("Patch name contains unsupported characters");
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<std::setprecision(std::numeric_limits<float>::max_digits10);
    out<<"ver="<<currentPatchFormatVersion<<"\nname="<<std::quoted(patch.name.empty() ? "Untitled" : patch.name)<<'\n';
    for(int i=0;i<129;++i)if(i!=126){validate(i,patch.values[i]);out<<i<<','<<patch.values[i]<<'\n';}
    return out.str();
}
}
