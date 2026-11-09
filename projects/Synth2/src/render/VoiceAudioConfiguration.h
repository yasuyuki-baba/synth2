#pragma once
#include "../RenderControls.h"
#include "../dsp/OscillatorAudioRenderer.h"
#include "../dsp/FilterAudioRenderer.h"

namespace synth2::render
{
    inline dsp::OscillatorAudioSettings oscillatorAudio(const RenderControls& p,bool bypass,bool bend=false)
    {
        return {p.osc2KbdTrack>.5f,p.hasPitchLfo() || p.sourcePortamentoDial>0 || bend,
            p.modEnv.dest==0 && p.modEnv.on && !bypass,p.hasLfo(4),
            p.modEnv.dest==3 && p.modEnv.on && !bypass,
            p.pulseWidth,p.subOscWave,p.subOscOctave,!bypass};
    }
    inline dsp::FilterAudioSettings filterAudio(const RenderControls& p,bool bypass)
    {
        return {p.filterType,p.resonanceNorm,p.filterSaturation,bypass};
    }
}
