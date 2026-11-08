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
    inline dsp::ModulationRoutingSettings modulationRouting(const RenderControls& p,bool amplitudeReceiver=false,bool available=true)
    {
        const auto consumed=[&](const LfoSettings& lfo) {
            if(!available)return false;
            if(lfo.dest==6)return true; // Every prepared pan signal belongs to output.
            if(!lfo.nativeSharedClock)return false;
            return (amplitudeReceiver && lfo.dest==3)
                || (p.nativePitchLfo && (lfo.dest==0 || lfo.dest==1))
                || (p.nativePulseWidthLfo && lfo.dest==4) || (p.nativeFmLfo && lfo.dest==5)
                || (p.nativeFilterLfo && lfo.dest==2);
        };
        dsp::ModulationRoutingSettings result{p.lfo1,p.lfo2,{consumed(p.lfo1),consumed(p.lfo2)}};
        if(!available)result.lfo1.on=result.lfo2.on=false;
        return result;
    }
    inline dsp::AmplitudeBlockSettings amplitudeBlockSettings(const dsp::ModulationRoutingSettings& routing,int gainOffset,int eventHorizon=0,int boundaryFrames=0)
    {
        const std::array<const LfoSettings*,2> lfos{&routing.lfo1,&routing.lfo2};
        for(size_t slot=0;slot<lfos.size();++slot)
            if(lfos[slot]->on && lfos[slot]->dest==3 && !routing.consumedLfos[slot])
                return {gainOffset,64};
        return {gainOffset,eventHorizon,eventHorizon>0 ? 64 : 0,boundaryFrames};
    }
    inline dsp::OscillatorControlSettings oscillatorControls(const RenderControls& p,double rate,bool bypass)
    {
        return {p.modEnv.on && p.modEnv.dest>=0 && p.modEnv.dest<=3 && !bypass,
            p.nativePitchLfo || p.nativePulseWidthLfo || p.nativeFmLfo,p.sourcePortamentoDial>0,
            p.modEnv.dest==2,p.nativeFmLfo,p.sourcePortamentoDial,rate,p.osc1FmAmount,p.ringMod>0.0f};
    }
    inline dsp::FilterControlSettings filterControls(const RenderControls& p,bool bypass)
    { return {!bypass,p.nativeFilterLfo,filterControlIndex(p)}; }


}
