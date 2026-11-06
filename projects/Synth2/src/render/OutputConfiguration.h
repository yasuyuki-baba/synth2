#pragma once
#include "../RenderControls.h"
#include "../dsp/OutputPipelineSettings.h"
#include "PanConfiguration.h"

namespace synth2::render
{
    inline dsp::OutputPipelineSettings outputSettings(const RenderControls& p)
    {
        dsp::OutputPipelineSettings settings;
        settings.panLfo=outputPanLfo(p);
        settings.nativeGainSilence=p.ampGainMultiplier>0 && !p.nativePhaser;
        settings.nativeSilenceGraph=p.nativePhaser || settings.nativeGainSilence;
        settings.nativePhaser=p.nativePhaser;settings.nativeMultiEffectType=p.nativeMultiEffectType;
        settings.nativeMultiEffectDials=p.nativeMultiEffectDials;
        settings.nativePhaserDepth=p.nativePhaserDepth;settings.nativePhaserRate=p.nativePhaserRate;
        settings.nativePhaserFeedback=p.nativePhaserFeedback;settings.nativePhaserPairs=p.nativePhaserPairs;
        settings.pan=p.pan;settings.masterVolume=p.masterVolume;settings.hostBpm=p.hostBpm;
        settings.eq=p.eq;settings.chorus=p.chorus;settings.delay=p.delay;
        return settings;
    }
}
