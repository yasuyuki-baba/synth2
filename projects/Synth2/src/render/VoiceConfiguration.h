#pragma once
#include "../RenderControls.h"
#include "LfoViews.h"
#include "PanConfiguration.h"
#include "FilterIndex.h"
#include "../dsp/OscillatorControlRenderer.h"
#include "../dsp/AmplitudeBlockRenderer.h"
#include "../dsp/FilterControlRenderer.h"
#include <cmath>

namespace synth2::render
{
    // This bridge owns adaptation from the engine aggregate to component inputs.
    inline dsp::AmplitudeBlockSettings amplitudeBlockSettings(int gainOffset,int eventHorizon=0,int boundaryFrames=0)
    { return {gainOffset,eventHorizon,eventHorizon>0 ? 64 : 0,boundaryFrames}; }
    inline dsp::OscillatorControlSettings oscillatorControls(const RenderControls& p,double rate,bool bypass)
    {
        return {p.modEnv.on && p.modEnv.dest>=0 && p.modEnv.dest<=3 && !bypass,
            p.hasPitchLfo() || p.hasLfo(4) || p.hasLfo(5),p.sourcePortamentoDial>0,
            p.modEnv.dest==2,p.hasLfo(5),p.sourcePortamentoDial,rate,p.osc1FmAmount,p.ringMod>0.0f};
    }
    inline dsp::FilterControlSettings filterControls(const RenderControls& p,bool bypass)
    { return {!bypass,p.hasLfo(2),filterControlIndex(p)}; }


}
