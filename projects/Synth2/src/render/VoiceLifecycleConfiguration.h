#pragma once
#include "../RenderControls.h"
#include "FilterIndex.h"
#include "../dsp/VoiceCorePolicy.h"
#include "../dsp/VoiceEnvelopeConfigurator.h"

namespace synth2::render
{
    inline dsp::AmplitudeEnvelopeSettings amplitudeEnvelope(const RenderControls& p)
    { return {p.ampAttackSec,p.ampDecaySec,p.ampSustain,p.ampReleaseSec,p.ampGainMultiplier,p.ampVelocity}; }
    inline dsp::ModulationEnvelopeSettings modulationEnvelope(const RenderControls& p)
    { return {p.modEnv.attackDial,p.modEnv.decayDial,p.modEnv.amountDial,p.modEnv.dest==3,p.modEnv.dest==2}; }
    inline dsp::FilterEnvelopeSettings filterEnvelope(const RenderControls& p)
    {
        return {p.filterEnv.attackSec,p.filterEnv.decaySec,p.filterEnv.sustain,p.filterEnv.releaseSec,
            filterControlIndex(p),p.filterKeyTrackNorm,p.filterEnvAmount,p.filterVelocity>.5f};
    }
}
