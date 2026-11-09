#pragma once
// Offline inspection of production selection flags, not native parity evidence.
#include "SynthDiagnostics.h"
#include "dsp/FractionalDial.h"
#include <map>
#include <string>
inline std::map<std::string,double> dspRouteSnapshot(const synth2::MappedSynthSnapshot& p)
{
    return {
        {"source_version",p.patchVersion},{"random_history_valid",p.nativeRandomLfoHistoryValid},
        {"osc1_shape",std::lround(p.osc1WaveIndex*3)},
        {"osc2_shape",std::lround(p.osc2WaveIndex*3)},{"sync",p.osc2Sync!=0},{"ring",p.ringMod!=0},
        {"stack_dial",p.osc1StackDial},{"sub_gain",p.subOscGain},
        {"fm_amount",p.osc1FmAmount},{"native_fm_lfo",p.nativeFmLfo},
        {"mod_env_on",p.modEnv.on},{"mod_env_dest",p.modEnv.dest},{"native_mod_pitch",p.modEnv.nativePitch},
        {"native_mod_pw",p.modEnv.nativePulseWidth},{"native_mod_fm",p.modEnv.nativeFm},
        {"filter_type",p.filterType},{"resonance",p.resonanceNorm},{"native_filter_envelope",p.nativeFilterEnvelope},
        {"native_filter_kernel",p.nativeOrdinaryStateVariable},{"native_filter_lfo",p.nativeFilterLfo},
        {"native_voice_performance",p.nativeVoicePerformance},
        {"portamento_dial",p.sourcePortamentoDial},{"play_mode",p.playMode},
        {"unison_voices",p.unisonVoices},{"arp_on",p.arpeggiator.enabled},{"arp_type",p.arpeggiator.type},
        {"multifx_type",p.nativeMultiEffectType},
        {"native_phaser",p.nativePhaser},
        {"chorus_on",p.chorus.enabled},{"delay_on",p.delay.enable},{"eq_on",synth2::dsp::fractionalDial(p.eq.tone)!=64 || synth2::dsp::fractionalDial(p.eq.level)!=64},
        {"native_pan_lfo",p.nativePanLfo},
        {"native_pitch_lfo",p.nativePitchLfo},{"native_pw_lfo",p.nativePulseWidthLfo},
        {"lfo1_on",p.lfo1.on},{"lfo1_wave",p.lfo1.wave},{"lfo1_dest",p.lfo1.dest},
        {"lfo1_clock",p.lfo1.nativeSharedClock},{"lfo2_on",p.lfo2.on},{"lfo2_wave",p.lfo2.wave},
        {"lfo2_dest",p.lfo2.dest},{"lfo2_clock",p.lfo2.nativeSharedClock}
    };
}
