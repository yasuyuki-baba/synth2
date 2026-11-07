#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>

namespace synth2::editor
{
enum class LibraryNavigation { previous,next,pagePrevious,pageNext,first,last };
inline std::optional<std::size_t> editorLibraryNavigation(std::size_t count,std::size_t pageSize,
    std::optional<std::size_t> cursor,std::size_t visibleStart,LibraryNavigation action)
{
    if(count==0)return {};
    const bool valid=cursor && *cursor<count;
    const auto position=valid ? *cursor : std::min(visibleStart,count-1);
    switch(action) {
        case LibraryNavigation::first:return 0;
        case LibraryNavigation::last:return count-1;
        case LibraryNavigation::previous:return valid && position>0 ? position-1 : position;
        case LibraryNavigation::next:return valid ? std::min(position+1,count-1) : position;
        case LibraryNavigation::pagePrevious:return position>pageSize ? position-pageSize : 0;
        case LibraryNavigation::pageNext:return position+std::min(pageSize,count-1-position);
    }
    return position;
}
}
