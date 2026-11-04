#pragma once
#include "../RenderControls.h"
#include "../dsp/VoiceTuning.h"
#include "../dsp/FractionalDial.h"

namespace synth2::render
{
    inline dsp::VoiceTuningSettings voiceTuning(const RenderControls& p,bool bypass)
    {
        return {p.osc2KbdTrack>.5f,
            bypass ? 0.0f : static_cast<float>(p.octaveBase),bypass ? 0.0f : static_cast<float>(p.keyShift),
            p.globalFineTune,p.osc2PitchSemis,p.osc2FineCents,p.unisonVoices,
            dsp::fractionalDial(p.unisonDetune),p.unisonPitchDial};
    }
}
