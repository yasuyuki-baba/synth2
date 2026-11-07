#pragma once
#include <algorithm>
#include <cstddef>

namespace synth2::editor
{
inline double editorLibraryScrollLimit(std::size_t count,std::size_t rows)
{
    return count>rows ? static_cast<double>(count-rows) : 0.;
}
inline double editorLibraryScrollOffset(std::size_t count,std::size_t rows,double offset)
{
    return std::clamp(offset,0.,editorLibraryScrollLimit(count,rows));
}
inline double editorLibraryReveal(std::size_t count,std::size_t rows,double offset,std::size_t candidate)
{
    if(candidate<offset)offset=static_cast<double>(candidate);
    else if(candidate+1>offset+rows)offset=static_cast<double>(candidate+1)-rows;
    return editorLibraryScrollOffset(count,rows,offset);
}
}
