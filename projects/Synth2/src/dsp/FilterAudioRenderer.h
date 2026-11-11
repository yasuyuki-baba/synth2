#pragma once
#include "Filter.h"

namespace synth2::dsp
{
    struct FilterAudioSettings
    {
        int type {};
        float resonance {},saturation {};
        bool bypass {};
    };
    struct FilterAudioInput
    {
        float signal {};
        float cutoffIndex {};
    };
    struct FilterAudioRenderer
    {
        static void configure(Filter& filter,const FilterAudioSettings& config)
        {
            filter.setParams(config.type,0,config.resonance,0,0,config.saturation);
        }
        static float process(Filter& filter,const FilterAudioSettings& config,const FilterAudioInput& input)
        {
            return config.bypass ? input.signal : filter.processNativeIndex(input.signal,input.cutoffIndex);
        }
    };
}
