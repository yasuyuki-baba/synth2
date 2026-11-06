#pragma once
#include "ModulationSettings.h"
#include "LfoControlProjection.h"
#include <algorithm>
#include <array>

namespace synth2::dsp
{
    // Adapt prepared normalized signals to the physical receiver coordinates.
    // Consumption is resolved by the caller; no history flags or clocks here.
    class ModulationRouter
    {
    public:
        struct ModResults
        {
            double firstPitch {},secondPitch {},filterControl {},pulseControl {},fmControl {};
            std::array<double,2> amplitudeControl {};
        };
        static ModResults calculate(const ModulationRoutingSettings& params,float first,float second)
        {
            ModResults result;
            const std::array<const LfoSettings*,2> lfos{&params.lfo1,&params.lfo2};
            const std::array<float,2> values{first,second};
            for(size_t slot=0;slot<2;++slot)
            {
                const auto& p=*lfos[slot];
                if(params.consumedLfos[slot] || !p.on)continue;
                const double control=LfoControlProjection::fromNormalized(p,values[slot]);
                switch(p.dest)
                {
                    case 0:result.secondPitch+=control;break;
                    case 1:result.firstPitch+=control;result.secondPitch+=control;break;
                    case 2:result.filterControl+=control;break;
                    case 3:result.amplitudeControl[slot]=control;break;
                    case 4:result.pulseControl+=control;break;
                    case 5:result.fmControl+=control;break;
                }
            }
            return result;
        }
    };
}
