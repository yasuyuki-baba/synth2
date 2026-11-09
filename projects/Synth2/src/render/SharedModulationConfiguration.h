#pragma once
#include "../RenderControls.h"
#include "RenderHistory.h"
#include "../dsp/SharedModulationSettings.h"

namespace synth2::render
{
    inline dsp::SharedModulationSettings sharedModulation(const RenderControls& p,const RenderHistory& history={})
    {
        return {{{p.lfo1,p.lfo2}},p.hostBpm,
            p.hasPitchLfo(),p.hasLfo(2),p.hasLfo(4),
            history.nativeMidiRoutingLoadHistory,history.nativeMidiRoutingControls};
    }
}
