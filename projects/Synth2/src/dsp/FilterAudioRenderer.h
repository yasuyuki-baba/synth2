#pragma once
#include "Filter.h"

namespace synth2::dsp
{
    struct FilterAudioSettings
    {
        int type {};
        double resonance {},saturation {};
        bool bypass {};
    };
    struct FilterAudioInput
    {
        double signal {};
        double cutoffIndex {};
    };
    struct FilterAudioRenderer
    {
        static double process(Filter& filter,const FilterAudioSettings& config,const FilterAudioInput& input)
        {
            filter.setParams(config.type,0,config.resonance,0,0,config.saturation);
            return config.bypass ? input.signal : filter.processNativeIndex(input.signal,input.cutoffIndex);
        }
    };
}
