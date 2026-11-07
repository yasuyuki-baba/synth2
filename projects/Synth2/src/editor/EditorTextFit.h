#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace synth2::editor
{
// Fit using the actual renderer's text metrics; preserve UTF-8 boundaries.
// Bank paths keep their distinguishing tail, while titles keep their beginning.
template<class Measure>
std::string editorFitText(std::string_view text,float width,Measure measure,bool keepEnd=false)
{
    if(width<=0)return {};
    if(measure(text)<=width)return std::string(text);
    const std::string dots="...";
    if(measure(dots)>width)return {};
    std::vector<std::size_t> boundaries{0};
    for(std::size_t i=1;i<text.size();++i)
        if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)boundaries.push_back(i);
    boundaries.push_back(text.size());
    const auto candidate=[&](std::size_t count) {
        return keepEnd ? dots+std::string(text.substr(boundaries[boundaries.size()-1-count]))
                       : std::string(text.substr(0,boundaries[count]))+dots;
    };
    std::size_t low=0,high=boundaries.size()-1;
    while(low<high) {
        const auto middle=(low+high+1)/2;
        if(measure(candidate(middle))<=width)low=middle;else high=middle-1;
    }
    return candidate(low);
}
}
