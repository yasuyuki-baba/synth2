#pragma once
#include "../RenderControls.h"
#include <cmath>

namespace synth2::render
{
    inline double filterControlIndex(const RenderControls& p) noexcept
    {return std::trunc(p.cutoffIndex)+p.filterCutoffAdjustment;}
}
