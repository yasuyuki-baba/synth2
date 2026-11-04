#pragma once
#include "ModulationSettings.h"
#include "LfoRange.h"
#include "FractionalDial.h"
#include "CompatibleControlCurve.h"
#include <algorithm>

namespace synth2::dsp
{
    // Unit conversion only: no clock, history query or random draw.
    struct LfoControlProjection
    {
        static double fromNormalized(const LfoSettings& p,float value) noexcept
        {
            if(!p.on || p.amount<=0)return 0;
            const double position=(std::clamp(static_cast<double>(value)/p.amount,-1.0,1.0)+1.0)*.5;
            return compatibleControlCurve(static_cast<double>(fractionalDial(p.amount)),[&](int dial) {
                const auto range=lfoDepthRange(p.dest+1,dial);
                return range.first+(static_cast<double>(range.second)-range.first)*position;
            });
        }
    };
}
