#pragma once
#include "../RenderControls.h"
#include "RenderHistory.h"
#include "../dsp/SharedModulationSettings.h"

namespace synth2::render
{
    inline dsp::SharedModulationSettings sharedModulation(const RenderControls& p,const RenderHistory& history={})
    {
        return {{{p.lfo1,p.lfo2}},p.hostBpm,
            p.nativePitchLfo,p.nativeFilterLfo,p.nativePulseWidthLfo,
            history.nativeMidiRoutingLoadHistory,history.nativeMidiRoutingControls};
    }
}
