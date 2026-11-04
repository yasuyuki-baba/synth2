#pragma once
#include "../RenderControls.h"
#include "../patch/Synth1EnvelopeTime.h"
#include "../patch/Synth1Tuning.h"
#include "../dsp/SubOscillatorStack.h"
#include "../dsp/Portamento.h"

namespace synth2::render
{
    // Native MIDI destinations use modern dials. This adapter changes render
    // controls only; it never edits source files, APVTS or saved patch metadata.
    inline bool applyControllerDial(RenderControls& p,int destination,int dial)
    {
        const float unit=static_cast<float>(dial)/127.0f;
        switch(destination)
        {
            case 2:p.osc2PitchSemis=static_cast<float>(coarseTuneSemitones(dial));return true;
            case 5:p.oscMix=unit;return true;
            case 8:p.pulseWidth=unit;return true;
            case 11:p.modEnv.amountDial=dial;return true;
            case 12:p.modEnv.attackDial=dial;return true;
            case 13:p.modEnv.decayDial=dial;return true;
            case 15:p.filterEnv.attackSec=envelopeAttackSeconds(dial);return true;
            case 16:p.filterEnv.decaySec=envelopeDecayReleaseSeconds(dial);return true;
            case 17:p.filterEnv.sustain=unit;return true;
            case 18:p.filterEnv.releaseSec=envelopeDecayReleaseSeconds(dial);return true;
            case 19:p.cutoffIndex=unit*1023.0f;p.filterCutoffAdjustment=0;return true;
            case 20:p.resonanceNorm=unit;return true;
            case 21:p.filterEnvAmount=(dial-63.0f)/64.0f;return true;
            case 22:p.filterKeyTrackNorm=unit;p.filterTracking=std::log2(1.0f+unit);return true;
            case 23:p.filterSaturation=unit;return true;
            case 25:p.ampAttackSec=envelopeAttackSeconds(dial);return true;
            case 26:p.ampDecaySec=envelopeDecayReleaseSeconds(dial);return true;
            case 27:p.ampSustain=ampSustainLevel(dial);return true;
            case 28:p.ampReleaseSec=envelopeDecayReleaseSeconds(dial);return true;
            case 29:p.ampGainMultiplier=unit;return true;
            case 34:p.arpeggiator.gate=dial;return true;
            case 35:p.delay.time=unit;return true;
            case 36:p.delay.feedback=unit;return true;
            case 37:p.delay.level=unit;return true;
            case 39:p.sourcePortamentoDial=dial;return true;
            case 43:p.lfo1.speed=unit;return true;
            case 44:p.lfo1.amount=unit;return true;
            case 45:p.osc1FmAmount=unit;return true;
            case 48:p.lfo2.speed=unit;return true;
            case 49:p.lfo2.amount=unit;return true;
            case 52:p.chorus.time=unit;return true;
            case 53:p.chorus.depth=unit;return true;
            case 54:p.chorus.rate=unit;return true;
            case 55:p.chorus.feedback=unit;return true;
            case 56:p.chorus.level=unit;return true;
            case 60:p.eq.tone=unit;return true;
            case 61:p.eq.freq=unit;return true;
            case 62:p.eq.level=unit;return true;
            case 63:p.eq.q=unit;return true;
            case 75:p.unisonDetune=unit;return true;
            case 76:p.osc1StackDial=dial;return true;
            case 79:p.nativeMultiEffectDials[0]=p.nativePhaserDepth=dial;return true;
            case 80:p.nativeMultiEffectDials[1]=p.nativePhaserRate=dial;return true;
            case 81:p.nativeMultiEffectDials[2]=p.nativePhaserFeedback=dial;return true;
            case 83:p.delay.spread=unit;return true;
            case 84:p.unisonPan=(dial-64.0f)*(1.0f/64.0f);return true;
            case 85:p.unisonPitchDial=dial;return true;
            case 90:p.ampVelocity=unit;return true;
            case 91:p.oscillatorPhaseDial=dial;return true;
            case 92:p.unisonPhaseDial=dial;return true;
            case 95:p.subOscGain=dsp::SubOscillatorStack::level(dial);return true;
            case 98:p.delay.tone=unit;return true;
            default:return false;
        }
    }
}
