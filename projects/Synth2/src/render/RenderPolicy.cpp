#include "RenderPolicy.h"
#include "../RenderControls.h"
#include "../dsp/SubOscillatorStack.h"
#include <algorithm>

namespace synth2::render
{
    namespace
    {
        constexpr float pulseWave=1.0f/3.0f;
        bool needsIdleFilterReset(const RenderPolicyInputs& input,const RenderControls& target)
        {
            return target.playMode == 1 && target.unisonVoices == 1
                && target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == 4.0f/3.0f
                && target.filterType == 0 && target.resonanceNorm == 0 && target.arpeggiator.enabled
                && target.arpeggiator.type >= 1 && target.arpeggiator.type <= 3
                && target.arpeggiator.gate > 0 && target.arpeggiator.gate < 127
                && !target.lfo1.on && !target.lfo2.on && (!target.modEnv.on || target.modEnv.dest == 0)
                && target.sourcePortamentoDial<=0 && !target.portamentoAuto && target.osc2Sync == 0 && target.ringMod == 0
                && !input.stackControlActive && target.subOscGain == 0 && !input.effectControlActive;
        }

    }
    void selectRenderPolicy(const RenderPolicyInputs& input,RenderControls& target)
    {
        const int type=input.effectType;
        target.nativeMultiEffectType=input.effectEnabled && type<=5 ? type : -1;
        target.nativePhaser=input.effectEnabled && type>=6 && type<=9;
        target.nativePhaserPairs=type==7 ? 2 : type==8 ? 4 : type==9 ? 6 : 1;
        target.osc1StackDial=std::clamp(input.stackDial,0,127);
        target.subOscGain=dsp::SubOscillatorStack::level(input.subLevelDial);
        target.modEnv.amountDial=input.modulationAmountDial;
        target.modEnv.attackDial=input.modulationAttackDial;
        target.modEnv.decayDial=input.modulationDecayDial;
        target.resetIdleFilter=needsIdleFilterReset(input,target);
    }
}
