#pragma once
#include "../RenderControls.h"
#include "../dsp/OscillatorAudioRenderer.h"
#include "../dsp/FilterAudioRenderer.h"

namespace synth2::render
{
    inline dsp::OscillatorAudioSettings oscillatorAudio(const RenderControls& p,bool bypass,bool bend=false)
    {
        return {p.osc2KbdTrack>.5f,p.nativePitchLfo || p.sourcePortamentoDial>0 || bend
                || (p.lfo1.on && (p.lfo1.dest==0 || p.lfo1.dest==1))
                || (p.lfo2.on && (p.lfo2.dest==0 || p.lfo2.dest==1)),
            (p.modEnv.dest==0 || p.modEnv.dest==1) && p.modEnv.on && !bypass,p.nativePulseWidthLfo
                || (p.lfo1.on && p.lfo1.dest==4) || (p.lfo2.on && p.lfo2.dest==4),
            p.modEnv.dest==3 && p.modEnv.on && !bypass,
            p.pulseWidth,p.subOscWave,p.subOscOctave,!bypass,p.modEnv.dest==1};
    }
    inline dsp::FilterAudioSettings filterAudio(const RenderControls& p,bool bypass)
    {
        return {p.filterType,p.resonanceNorm,p.filterSaturation,bypass};
    }
}
