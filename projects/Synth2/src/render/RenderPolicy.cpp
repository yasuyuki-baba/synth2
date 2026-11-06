#include "RenderPolicy.h"
#include "../RenderControls.h"
#include "../dsp/SubOscillatorStack.h"
#include <algorithm>

namespace synth2::render
{
    namespace
    {
        constexpr float pulseWave=1.0f/3.0f;
        bool pwLfo(const LfoSettings& p)
        { return p.on && (p.wave==0 || p.wave==1) && p.dest==4 && !p.tempoSync; }
        bool mixedPitchLfo(const LfoSettings& p)
        { return p.on && (p.wave==0 || p.wave==1) && (p.dest==0 || p.dest==1) && !p.tempoSync; }

        // Explicit dependencies passed from oscillator admission to later phases.
        struct OscillatorRoutes
        {
            bool trianglePanRoute {};
            bool trianglePitchRoute {};
            bool squarePitchRoute {};
            bool randomReceiverRoute {};
            bool pwPitchEnvelope {};
            bool syncPitchRoute {};
            bool syncRingRoute {};
            bool ringPitchRoute {};
            bool sawOnlyPw {};

            bool panLfo(const LfoSettings& p) const
            {
                return p.on && (p.wave==0 || (p.wave==1 && trianglePanRoute))
                    && p.dest==6 && !p.tempoSync;
            }
            bool pitchLfo(const LfoSettings& p) const
            {
                return p.on && (p.wave==0 || (p.wave==1 && trianglePitchRoute)
                    || (p.wave==3 && squarePitchRoute) || (p.wave>=4 && randomReceiverRoute))
                    && (p.dest==0 || p.dest==1) && !p.tempoSync;
            }
        };

        bool supportsLp24(const RenderPolicyInputs& input,const RenderControls& target)
        {
            const bool detuneModulation = (!target.modEnv.on || target.modEnv.nativePulseWidth) && !target.arpeggiator.enabled
                && ((!target.lfo1.on && !target.lfo2.on) || target.lfo1.nativeSharedClock || target.lfo2.nativeSharedClock);
            const bool independentLp24 = target.unisonVoices >= 2 && target.osc1FmAmount == 0
                && (target.unisonDetune == 0 || detuneModulation);
            return target.filterType != 1
                || (target.unisonVoices == 1 && !input.unisonControlActive)
                || (target.unisonVoices >= 2 && target.oscillatorPhaseDial == 1)
                || independentLp24;
        }

        bool isResonantNoiseRoute(const RenderPolicyInputs& input,const RenderControls& target,bool lp24Route)
        {
            return (target.osc1WaveIndex == pulseWave || target.osc1WaveIndex == 1.0f)
                && target.osc2WaveIndex == 4.0f/3.0f && target.osc1FmAmount == 0
                && target.playMode == 0 && target.unisonVoices == 1 && !target.arpeggiator.enabled
                && !target.modEnv.on && !target.lfo1.on && !target.lfo2.on
                && !input.stackControlActive && target.subOscGain == 0
                && target.sourcePortamentoDial<=0 && target.osc2Sync == 0 && target.ringMod == 0
                && (!input.effectControlActive || target.nativePhaser) && lp24Route;
        }

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

        bool canComposeReceivers(const RenderPolicyInputs& input,const RenderControls& target)
        {
            // Legacy random ARP has no admitted CRT/load history. Keep its
            // existing lifecycle restrictions even with deterministic LFOs.
            if(!input.nativeSource && target.arpeggiator.enabled && target.arpeggiator.type==4)return false;
            const auto supported=[&](const LfoSettings& lfo) {
                return !lfo.on || lfo.wave<4 || (input.nativeSource && input.randomHistoryValid);
            };
            return supported(target.lfo1) && supported(target.lfo2);
        }

        void initializePolicy(const RenderPolicyInputs& input,RenderControls& target)
        {
            const bool randomArpRoute=input.nativeSource && input.randomHistoryValid && admitRandomArp(input.arpeggio);
            target.arpeggiator.nativeRandom=randomArpRoute;
            target.modEnv.nativeFm=false;target.nativeFmLfo=false;
            const int effectType=input.effectType;
            target.nativeMultiEffectType=input.effectEnabled && effectType<=5 ? effectType : -1;
            target.nativePhaser=input.effectEnabled && effectType>=6 && effectType<=9;
            target.nativePhaserPairs=effectType==7 ? 2 : effectType==8 ? 4 : effectType==9 ? 6 : 1;
        }

        OscillatorRoutes selectFallbackOscillatorReceivers(const RenderPolicyInputs& input,RenderControls& target,bool randomArpRoute)
        {
            OscillatorRoutes routes;
            const bool native=input.nativeSource;
            // Initial shared LFO lifecycle route: one sine tremolo in poly mode.
            // Native rate smoothing, integer waveform steps and other routes remain work.
            const bool sharedLfoRoute = target.playMode == 0 && !target.arpeggiator.enabled && !target.modEnv.on
                && target.osc1FmAmount == 0
                && (target.lfo1.on != target.lfo2.on);
            target.lfo1.nativeSharedClock = sharedLfoRoute && target.lfo1.on && target.lfo1.wave == 0 && target.lfo1.dest == 3 && !target.lfo1.tempoSync;
            target.lfo2.nativeSharedClock = sharedLfoRoute && target.lfo2.on && target.lfo2.wave == 0 && target.lfo2.dest == 3 && !target.lfo2.tempoSync;
            // Triangle Pan shares the signed CLFO query with pitch, but its
            // receiving balance processor is separate. Bound new compositions to
            // one core until stacked voices and simultaneous Pan LFOs are observed.
            routes.trianglePanRoute = (target.lfo1.on != target.lfo2.on)
                && target.unisonVoices == 1 && !target.modEnv.on
                && target.osc1StackDial == 0 && target.subOscGain == 0;
            // The native VCO schedules its OSC2 envelope at the same held CLFO
            // boundaries as PW, and sums any pitch LFO before frequency conversion.
            routes.pwPitchEnvelope = target.modEnv.on && target.modEnv.dest == 0
                && target.playMode == 0 && target.sourcePortamentoDial == 0
                && target.resonanceNorm <= 112.0f/127.0f && target.osc2KbdTrack > .5f
                && (target.osc1WaveIndex == pulseWave || target.osc1WaveIndex == 0.0f)
                && (target.osc2WaveIndex == pulseWave || target.osc2WaveIndex == 0.0f);
            const bool mixedPwPitchRoute=(!target.modEnv.on || routes.pwPitchEnvelope) && target.unisonVoices == 1
                && target.osc1StackDial == 0 && target.subOscGain == 0
                && ((pwLfo(target.lfo1) && mixedPitchLfo(target.lfo2))
                    || (mixedPitchLfo(target.lfo1) && pwLfo(target.lfo2)));
            const bool dualPwRoute = !target.portamentoAuto && !target.modEnv.on && target.unisonVoices == 1
                && target.osc1StackDial == 0 && target.subOscGain == 0
                && pwLfo(target.lfo1) && pwLfo(target.lfo2);
            routes.trianglePitchRoute = ((target.lfo1.on != target.lfo2.on) || mixedPwPitchRoute) && (!target.modEnv.on || target.modEnv.dest == 0)
                && target.unisonVoices == 1 && target.osc1StackDial == 0 && target.subOscGain == 0
                && (!input.effectControlActive || target.nativePhaser);
            routes.squarePitchRoute=target.playMode == 0 && target.unisonVoices == 1 && !target.arpeggiator.enabled
                && target.filterType == 1
                && !target.modEnv.on && target.osc1FmAmount == 0 && target.osc2Sync == 0 && target.ringMod == 0
                && target.osc1StackDial == 0 && target.subOscGain == 0 && target.sourcePortamentoDial<=0 && !target.portamentoAuto && !input.effectControlActive && target.resonanceNorm <= 112.0f/127.0f
                && target.osc1WaveIndex == 2.0f/3.0f && target.osc2WaveIndex == 2.0f/3.0f
                && (target.lfo1.on != target.lfo2.on);
            const auto freeRandomPitch=[](const LfoSettings& p) {
                return !p.on || (p.wave>=4 && (p.dest==0 || p.dest==1));
            };
            const bool randomMonoPitchRoute=target.playMode==1
                && target.osc1WaveIndex==pulseWave && (target.osc2WaveIndex==pulseWave || target.osc2WaveIndex==0.0f)
                && (target.lfo1.on!=target.lfo2.on) && freeRandomPitch(target.lfo1) && freeRandomPitch(target.lfo2);
            routes.randomReceiverRoute=native && input.randomHistoryValid
                && ((target.lfo1.on && target.lfo1.wave>=4) || (target.lfo2.on && target.lfo2.wave>=4))
                && (target.playMode==0 || randomMonoPitchRoute) && target.unisonVoices==1 && !target.arpeggiator.enabled
                && !target.modEnv.on && target.osc1StackDial==0 && target.subOscGain==0
                && (target.sourcePortamentoDial<=0 || randomMonoPitchRoute) && !target.portamentoAuto && target.osc1FmAmount==0
                && target.osc2Sync==0 && target.ringMod==0 && !input.effectControlActive
                && (((target.osc1WaveIndex==pulseWave || target.osc1WaveIndex==0.0f) && target.osc2WaveIndex==pulseWave)
                    || (target.osc1WaveIndex==pulseWave && target.osc2WaveIndex==4.0f/3.0f)
                    || (target.osc1WaveIndex==2.0f/3.0f && (target.osc2WaveIndex==pulseWave || target.osc2WaveIndex==4.0f/3.0f))
                    || randomMonoPitchRoute)
                && target.filterType==1 && target.resonanceNorm<=112.0f/127.0f
                && !target.lfo1.keySync && !target.lfo2.keySync && !target.lfo1.tempoSync && !target.lfo2.tempoSync;
            routes.syncRingRoute=target.osc2Sync != 0 && target.ringMod != 0
                && target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == pulseWave
                && target.unisonVoices == 1 && !target.arpeggiator.enabled && !target.modEnv.on
                && !target.portamentoAuto && target.osc1FmAmount == 0
                && target.osc1StackDial == 0 && target.subOscGain == 0 && !input.effectControlActive
                && target.filterType == 1 && target.resonanceNorm <= 112.0f/127.0f
                && ((!target.lfo1.on && !target.lfo2.on) || ((target.lfo1.on != target.lfo2.on)
                    && (routes.pitchLfo(target.lfo1) || routes.pitchLfo(target.lfo2))));
            // Pitch controls are applied before the ordinary Ring renderer. Ring
            // still takes priority over a nonzero configured FM amount.
            routes.ringPitchRoute=target.ringMod != 0 && target.osc1WaveIndex == 1.0f
                && target.osc2WaveIndex == 2.0f/3.0f && target.playMode == 0
                && target.unisonVoices == 1 && !target.arpeggiator.enabled && !target.modEnv.on
                && !target.portamentoAuto && target.sourcePortamentoDial<=0 && target.osc2Sync == 0
                && target.osc1StackDial == 0 && target.subOscGain == 0
                && (!input.effectControlActive || target.nativePhaser)
                && (target.lfo1.on != target.lfo2.on) && (routes.pitchLfo(target.lfo1) || routes.pitchLfo(target.lfo2));
            routes.sawOnlyPw=target.osc1WaveIndex == 0.0f && target.osc2WaveIndex == 0.0f
                && target.playMode == 0 && target.sourcePortamentoDial<=0 && !target.modEnv.on
                && (target.lfo1.on != target.lfo2.on) && (pwLfo(target.lfo1) || pwLfo(target.lfo2));
            const bool pwLfoRoute=((target.lfo1.on != target.lfo2.on) || mixedPwPitchRoute || dualPwRoute)
                && (!target.modEnv.on || routes.pwPitchEnvelope)
                && target.unisonVoices == 1 && target.osc1StackDial == 0 && target.subOscGain == 0
                && ((target.oscMix < 1 && target.osc1WaveIndex == pulseWave)
                    || (target.oscMix > 0 && target.osc2WaveIndex == pulseWave) || routes.sawOnlyPw);
            // Ordinary Sync uses the same integer pitch receiver and held CLFO
            // controls. Keep PW/pan combinations and multi-core Sync separate.
            routes.syncPitchRoute=target.osc2Sync != 0 && target.osc1FmAmount == 0
                && target.osc2WaveIndex != 4.0f/3.0f
                && target.unisonVoices == 1 && target.osc1StackDial == 0 && target.subOscGain == 0
                && ((!target.lfo1.on && !target.lfo2.on)
                    || ((target.lfo1.on != target.lfo2.on)
                        && (routes.pitchLfo(target.lfo1) || routes.pitchLfo(target.lfo2))));
            const bool nativePerformance = target.unisonVoices == 1 && !target.arpeggiator.enabled
                && !target.portamentoAuto && (target.osc1FmAmount == 0 || routes.ringPitchRoute)
                && !target.modEnv.on && target.osc1StackDial == 0 && target.subOscGain == 0
                && (target.osc2Sync == 0 || routes.syncPitchRoute || routes.syncRingRoute) && (target.ringMod == 0 || routes.ringPitchRoute || routes.syncRingRoute) && target.osc2WaveIndex != 4.0f/3.0f
                && (!target.lfo1.on || routes.pitchLfo(target.lfo1)
                    || ((routes.panLfo(target.lfo1) || (pwLfoRoute && pwLfo(target.lfo1)))
                        && (!input.effectControlActive || target.nativePhaser)))
                && (!target.lfo2.on || routes.pitchLfo(target.lfo2)
                    || ((routes.panLfo(target.lfo2) || (pwLfoRoute && pwLfo(target.lfo2)))
                        && (!input.effectControlActive || target.nativePhaser)))
                && (!(target.lfo1.on && target.lfo2.on) || dualPwRoute);
            target.nativeVoicePerformance = nativePerformance;
            const bool nativeOscillatorLfos = (target.playMode == 0 || nativePerformance) && (!target.arpeggiator.enabled || randomArpRoute) && (target.osc1FmAmount == 0 || routes.ringPitchRoute) && (target.sourcePortamentoDial<=0 || (nativePerformance && target.sourcePortamentoDial>0)) && (target.osc2Sync == 0 || routes.syncPitchRoute || routes.syncRingRoute) && (target.ringMod == 0 || routes.ringPitchRoute || routes.syncRingRoute)
                && (!input.effectControlActive || target.nativePhaser)
                && !(routes.panLfo(target.lfo1) && routes.panLfo(target.lfo2))
                && (!target.lfo1.on || routes.panLfo(target.lfo1) || routes.pitchLfo(target.lfo1) || (pwLfoRoute && pwLfo(target.lfo1)))
                && (!target.lfo2.on || routes.panLfo(target.lfo2) || routes.pitchLfo(target.lfo2) || (pwLfoRoute && pwLfo(target.lfo2)));
            target.nativePanLfo = nativeOscillatorLfos && (routes.panLfo(target.lfo1) || routes.panLfo(target.lfo2));
            target.nativePitchLfo = nativeOscillatorLfos && (routes.pitchLfo(target.lfo1) || routes.pitchLfo(target.lfo2));
            target.nativePulseWidthLfo = nativeOscillatorLfos && pwLfoRoute && (pwLfo(target.lfo1) || pwLfo(target.lfo2));
            if(target.nativePanLfo || target.nativePitchLfo || target.nativePulseWidthLfo) {
                target.lfo1.nativeSharedClock=target.lfo1.on;
                target.lfo2.nativeSharedClock=target.lfo2.on;
            }
            return routes;
        }

        void selectFallbackOscillatorKernels(const RenderPolicyInputs& input,RenderControls& target,const OscillatorRoutes& routes)
        {
            // OSC2 pitch envelopes follow physical cores. Multi-core extension excludes
            // detune/FM; other destinations and modulation/glide retain prior routing.
            // The native VCO sums pitch LFO controls and the OSC2 envelope. A
            // zero-depth triangle contributes zero without disabling its clock.
            // Active single triangle pitch modulation also follows this summed route.
            const auto neutralPitchLfo=[](const LfoSettings& p) {
                return !p.on || (p.wave == 1 && (p.dest == 0 || p.dest == 1)
                    && p.amount == 0.0f && !p.tempoSync);
            };
            // Scalar triangle Ring uses the native mix kernel and OSC2's integer
            // pitch envelope; Sync composes the same audited phase recurrence.
            const bool triangleRingRoute=target.ringMod != 0 && (!target.modEnv.on || target.modEnv.dest == 0)
                && target.osc1WaveIndex == 2.0f/3.0f && target.osc2WaveIndex == 2.0f/3.0f
                && target.playMode == 0 && target.unisonVoices == 1 && !target.arpeggiator.enabled
                && !target.lfo1.on && !target.lfo2.on && target.osc1FmAmount == 0
                && target.osc1StackDial == 0 && target.subOscGain == 0
                && !target.portamentoAuto && target.sourcePortamentoDial<=0 && !input.effectControlActive
                && (target.filterType == 0 || target.filterType == 1) && target.resonanceNorm <= 112.0f/127.0f;
            target.modEnv.nativePitch = target.modEnv.dest == 0 && target.playMode == 0
                && (target.unisonVoices == 1 || (target.unisonDetune == 0 && target.osc1FmAmount == 0))
                && ((routes.pwPitchEnvelope && target.nativePulseWidthLfo)
                    || (neutralPitchLfo(target.lfo1) && neutralPitchLfo(target.lfo2))
                    || (routes.trianglePitchRoute && target.nativePitchLfo
                        && ((target.lfo1.on && target.lfo1.wave == 1) || (target.lfo2.on && target.lfo2.wave == 1))))
                && target.sourcePortamentoDial<=0 && (target.osc2Sync == 0 || routes.syncPitchRoute)
                && (target.ringMod == 0 || triangleRingRoute);
            // The ordinary FM kernel interpolates its triangle modulator before
            // converting FM to a phase increment, including an inaudible OSC2 mix.
            const auto ordinaryAmplitudeLfo=[](const LfoSettings& p) {
                return p.on && (p.wave == 0 || p.wave == 1) && p.dest == 3 && !p.tempoSync;
            };
            const bool singleAmplitudeLfo=(target.lfo1.on != target.lfo2.on)
                && (ordinaryAmplitudeLfo(target.lfo1) || ordinaryAmplitudeLfo(target.lfo2));
            const bool sineTriangle = target.osc1WaveIndex == 1.0f && target.osc2WaveIndex == 2.0f/3.0f
                && target.playMode == 0 && target.unisonVoices == 1
                && !target.arpeggiator.enabled && !target.modEnv.on
                && ((!target.lfo1.on && !target.lfo2.on) || singleAmplitudeLfo || routes.ringPitchRoute)
                && target.osc1StackDial == 0 && target.subOscGain == 0
                && target.sourcePortamentoDial<=0 && target.osc2Sync == 0;
            if(sineTriangle && singleAmplitudeLfo)
            {
                // The amplitude receiver partitions the VCA; FM/Ring remains in
                // the VCO. Both consume the existing shared integer CLFO clock.
                target.lfo1.nativeSharedClock=target.lfo1.on;
                target.lfo2.nativeSharedClock=target.lfo2.on;
            }
            const bool pulseSource = (target.oscMix == 0 && target.osc1WaveIndex == pulseWave)
                || (target.oscMix == 1 && target.osc2WaveIndex == pulseWave)
                || (target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == pulseWave);
            target.modEnv.nativePulseWidth = target.modEnv.dest == 3 && pulseSource && target.playMode == 0
                && !target.arpeggiator.enabled && target.osc1FmAmount == 0 && (target.subOscGain == 0 || target.osc1StackDial > 0)
                && ((!target.lfo1.on && !target.lfo2.on) || target.nativePanLfo || target.nativePitchLfo)
                && target.sourcePortamentoDial<=0 && target.osc2Sync == 0 && target.ringMod == 0;
            if((target.nativePanLfo || target.nativePitchLfo) && target.modEnv.on && !target.modEnv.nativePulseWidth
                && !(target.nativePitchLfo && target.modEnv.nativePitch)) {
                target.nativePanLfo=target.nativePitchLfo=false;target.lfo1.nativeSharedClock=target.lfo2.nativeSharedClock=false;
            }
            const bool stackRoute = target.playMode == 0 && !target.arpeggiator.enabled
                && target.osc1FmAmount == 0 && (target.subOscGain == 0 || target.osc1StackDial > 0) && ((!target.lfo1.on && !target.lfo2.on) || target.nativePanLfo || target.nativePitchLfo) && target.sourcePortamentoDial<=0 && target.osc2Sync == 0 && target.ringMod == 0
                && (!target.modEnv.on || target.modEnv.nativePulseWidth);
            if(!stackRoute)target.osc1StackDial=0;
            target.modEnv.amountDial = input.modulationAmountDial;
            target.modEnv.attackDial = input.modulationAttackDial;
            target.modEnv.decayDial = input.modulationDecayDial;
        }

        void selectFallbackFilterReceivers(const RenderPolicyInputs& input,RenderControls& target,const OscillatorRoutes& routes)
        {
            // Control scheduling remains source/history-aware, while every
            // route uses the same VCF and canonical output tone/peak kernels.
            // Preserve the existing core-history admission independently of
            // the common audio kernel and output-tone selection.
            const bool staticCutoff = target.filterEnvAmount == 0 && target.filterTracking == 0
                && !(target.lfo1.on && target.lfo1.dest == 2)
                && !(target.lfo2.on && target.lfo2.dest == 2);
            const bool lp24Route=supportsLp24(input,target);
            target.nativeOrdinaryStateVariable = staticCutoff && lp24Route
                && target.resonanceNorm <= 112.0f/127.0f;
            // FM-fed HP12 performance uses native receiver scheduling when its
            // active OSC2 envelope follows native control. Other FM/HP
            // interactions retain their existing control scheduling.
            const bool performanceRoute = target.filterKeyTrackNorm != 0 || target.filterVelocity != 0;
            const bool verifiedInteraction = !performanceRoute || target.filterType != 2 || target.osc1FmAmount == 0
                || (target.modEnv.on && target.modEnv.nativePitch);
            target.nativeFilterEnvelope = (target.filterEnvAmount != 0 || target.filterKeyTrackNorm != 0)
                && verifiedInteraction && lp24Route
                && !(target.lfo1.on && target.lfo1.dest == 2)
                && !(target.lfo2.on && target.lfo2.dest == 2)
                && target.resonanceNorm <= 112.0f/127.0f;
            const auto randomPitchOrFilter=[](const LfoSettings& p) {
                return !p.on || (p.wave>=4 && p.dest>=0 && p.dest<=2);
            };
            const bool randomFilterRoute=routes.randomReceiverRoute && randomPitchOrFilter(target.lfo1) && randomPitchOrFilter(target.lfo2);
            const auto filterLfo=[randomFilterRoute](const LfoSettings& p) {
                return p.on && (p.wave == 0 || p.wave == 1 || (p.wave>=4 && randomFilterRoute)) && p.dest == 2 && !p.tempoSync;
            };
            const auto amplitudeLfo=[](const LfoSettings& p) {
                return p.on && (p.wave == 0 || p.wave == 1) && p.dest == 3 && !p.tempoSync;
            };
            const bool mixedWavePair=(target.osc1WaveIndex == 1.0f || target.osc1WaveIndex == pulseWave)
                && target.osc2WaveIndex == pulseWave;
            const bool mixedAmplitudeFilter=mixedWavePair
                && ((amplitudeLfo(target.lfo1) && filterLfo(target.lfo2))
                    || (filterLfo(target.lfo1) && amplitudeLfo(target.lfo2)));
            // Single-filter FM retains its pulse-pair guard. The combined receiver
            // audit also covers a sine carrier with a pulse modulator.
            const bool filterFmRoute = target.osc1FmAmount == 0
                || (target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == pulseWave) || mixedAmplitudeFilter;
            const bool highResonanceFilterLfo=target.filterType != 1 && !mixedAmplitudeFilter
                && target.osc1FmAmount == 0 && target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == pulseWave;
            const bool lp24FilterPair=target.filterType == 1 && target.osc1FmAmount == 0
                && target.osc1WaveIndex == pulseWave
                && (target.osc2WaveIndex == pulseWave || target.osc2WaveIndex == 4.0f/3.0f);
            const auto squareFilterLfo=[](const LfoSettings& p) {
                return p.on && p.wave == 3 && p.dest == 2 && !p.tempoSync;
            };
            const bool squareLp24=lp24FilterPair && (target.lfo1.on != target.lfo2.on)
                && (squareFilterLfo(target.lfo1) || squareFilterLfo(target.lfo2));
            // Independent pitch and filter receivers query their own held LFOs.
            // Keep key-synced generated-note histories outside this extension.
            const bool mixedPitchFilter=((target.osc1WaveIndex == pulseWave && target.osc2WaveIndex == pulseWave) || randomFilterRoute)
                && target.osc1FmAmount == 0 && !input.effectControlActive && target.resonanceNorm <= 112.0f/127.0f
                && (((mixedPitchLfo(target.lfo1) || (randomFilterRoute && routes.pitchLfo(target.lfo1))) && filterLfo(target.lfo2))
                    || (filterLfo(target.lfo1) && (mixedPitchLfo(target.lfo2) || (randomFilterRoute && routes.pitchLfo(target.lfo2)))));
            const bool dualRandomFilter=randomFilterRoute && filterLfo(target.lfo1) && filterLfo(target.lfo2);
            const bool mixedPitchFilterArpeggio=mixedPitchFilter
                && target.arpeggiator.type >= 1 && target.arpeggiator.type <= 3
                && target.arpeggiator.gate > 0 && target.arpeggiator.gate < 127
                && !target.lfo1.keySync && !target.lfo2.keySync;
            target.nativeFilterLfo = (((target.lfo1.on != target.lfo2.on) && (filterLfo(target.lfo1) || filterLfo(target.lfo2))) || mixedAmplitudeFilter || squareLp24 || mixedPitchFilter || dualRandomFilter)
                && target.playMode == 0 && target.unisonVoices == 1 && (!target.arpeggiator.enabled || mixedPitchFilterArpeggio)
                && !target.modEnv.on && filterFmRoute && !input.stackControlActive && target.subOscGain == 0 && target.sourcePortamentoDial<=0 && target.osc2Sync == 0 && target.ringMod == 0
                && (!input.effectControlActive || target.nativePhaser) && lp24Route
                && (target.resonanceNorm <= 112.0f/127.0f || highResonanceFilterLfo || (lp24FilterPair && !mixedAmplitudeFilter));
            if(target.nativeFilterLfo)
            {
                target.lfo1.nativeSharedClock=target.lfo1.on;
                target.lfo2.nativeSharedClock=target.lfo2.on;
                target.nativeFilterEnvelope=true;
                if(mixedPitchFilter)
                {
                    target.nativePitchLfo=true;
                }
            }
            // Extend the independently verified clipped kernels to the unmodulated
            // sine/pulse + noise receiver. Keep unrelated oscillator and performance paths.
            target.nativeResonantNoise=isResonantNoiseRoute(input,target,lp24Route);
            if(target.nativeResonantNoise)
            {
                target.nativeFilterEnvelope=true;
            }
        }

        void selectFallbackPerformanceAndOutput(const RenderPolicyInputs& input,RenderControls& target,const OscillatorRoutes& routes)
        {
            // Mono saw/pulse voices receive the same signed triangle pitch controls
            // during ordinary ordered arpeggios. The clocks remain independent;
            // both controls are summed by the VCO before frequency conversion.
            const auto monoTrianglePitch=[](const LfoSettings& p) {
                return !p.on || (p.wave == 1 && (p.dest == 0 || p.dest == 1)
                    && !p.tempoSync && !p.keySync);
            };
            const bool monoPitchArpeggio=!target.arpeggiator.enabled
                || (target.arpeggiator.type >= 1 && target.arpeggiator.type <= 3
                    && target.arpeggiator.gate > 0 && target.arpeggiator.gate < 127);
            const bool monoPitchRoute=target.playMode == 1 && target.unisonVoices == 1
                && target.osc1WaveIndex == 0.0f && target.osc2WaveIndex == pulseWave && target.filterType == 1
                && target.resonanceNorm <= 112.0f/127.0f && !target.modEnv.on && target.osc1FmAmount == 0
                && target.osc2Sync == 0 && target.ringMod == 0 && target.osc1StackDial == 0 && !input.stackControlActive && target.subOscGain == 0
                && target.sourcePortamentoDial<=0 && !target.portamentoAuto && !input.effectControlActive
                && (target.lfo1.on || target.lfo2.on) && monoTrianglePitch(target.lfo1) && monoTrianglePitch(target.lfo2)
                && monoPitchArpeggio;

            if(monoPitchRoute)
            {
                target.nativePitchLfo=true;
                // Generated note replacement must retain a still-sounding mono
                // core before its native envelopes retrigger, just like played notes.
                target.nativeVoicePerformance=true;
                target.lfo1.nativeSharedClock=target.lfo1.on;target.lfo2.nativeSharedClock=target.lfo2.on;
            }
            // PW controls do not change saw samples, but the ordinary scalar reader
            // and sounding mono core still survive ordered generated-note replacement.
            const auto sawPwLfo=[](const LfoSettings& p) {
                return !p.on || ((p.wave == 0 || p.wave == 1) && p.dest == 4
                    && !p.tempoSync && !p.keySync);
            };
            const bool monoSawRoute=target.playMode == 1 && target.unisonVoices == 1
                && target.osc1WaveIndex == 0.0f && target.osc2WaveIndex == 0.0f && target.filterType == 1
                && target.resonanceNorm <= 112.0f/127.0f && !target.modEnv.on && target.osc1FmAmount == 0
                && target.osc2Sync == 0 && target.ringMod == 0 && target.osc1StackDial == 0 && !input.stackControlActive && target.subOscGain == 0
                && target.sourcePortamentoDial<=0 && !target.portamentoAuto && !input.effectControlActive
                && sawPwLfo(target.lfo1) && sawPwLfo(target.lfo2) && monoPitchArpeggio;
            if(monoSawRoute)
            {
                target.nativeVoicePerformance=true;
                target.nativePulseWidthLfo=target.lfo1.on || target.lfo2.on;
                target.lfo1.nativeSharedClock=target.lfo1.on;target.lfo2.nativeSharedClock=target.lfo2.on;
            }
            // Idle pulse/noise mono cores reset the filter envelope before reuse.
            const bool monoNoiseArpeggio=needsIdleFilterReset(input,target);
            target.nativeIdleFilterReset = monoNoiseArpeggio;
            target.nativeVoicePerformance |= monoNoiseArpeggio;

            const bool highResonancePw = target.nativePulseWidthLfo && target.nativeVoicePerformance
                && target.filterType == 1 && target.resonanceNorm > 112.0f/127.0f && target.osc1FmAmount == 0
                && (target.osc1WaveIndex == pulseWave || target.osc1WaveIndex == 0.0f)
                && (target.osc2WaveIndex == pulseWave || target.osc2WaveIndex == 0.0f);
            if(highResonancePw)target.nativeFilterEnvelope=true;
            target.nativeOrdinaryStateVariable |= target.nativeFilterEnvelope;
        }

        void composeReceivers(const RenderPolicyInputs& input,RenderControls& target)
        {
            // Caller established shared-history eligibility. Composition does not
            // select waveform-specific receiver implementations.
            target.modEnv.amountDial=input.modulationAmountDial;
            target.modEnv.attackDial=input.modulationAttackDial;
            target.modEnv.decayDial=input.modulationDecayDial;
            const auto has=[&](int dest){return (target.lfo1.on && target.lfo1.dest==dest) || (target.lfo2.on && target.lfo2.dest==dest);};
            target.osc1StackDial=std::clamp(input.stackDial,0,127);

            target.modEnv.nativePitch=target.modEnv.dest==0;
            target.modEnv.nativePulseWidth=target.modEnv.dest==3;
            target.nativePitchLfo=has(0)||has(1);
            target.nativeFilterLfo=has(2);
            target.nativePanLfo=has(6);
            target.nativePulseWidthLfo=has(4);
            target.nativeFmLfo=has(5);
            target.modEnv.nativeFm=target.modEnv.dest==2;
            target.lfo1.nativeSharedClock=target.lfo1.on;
            target.lfo2.nativeSharedClock=target.lfo2.on;
            target.nativeVoicePerformance=true;
            target.nativeFilterEnvelope=true;
            target.nativeOrdinaryStateVariable=true;
        }

    }

    void selectRenderPolicy(const RenderPolicyInputs& input,RenderControls& target)
    {
        initializePolicy(input,target);
        if(canComposeReceivers(input,target))
        {
            // Resolved deterministic controls use the common composition regardless
            // of source format. Only random histories need source admission.
            composeReceivers(input,target);
            target.nativeResonantNoise=isResonantNoiseRoute(input,target,supportsLp24(input,target));
            target.nativeIdleFilterReset=needsIdleFilterReset(input,target);
        }
        else
        {
            const bool randomArpRoute=target.arpeggiator.nativeRandom;
            const auto routes=selectFallbackOscillatorReceivers(input,target,randomArpRoute);
            selectFallbackOscillatorKernels(input,target,routes);
            selectFallbackFilterReceivers(input,target,routes);
            selectFallbackPerformanceAndOutput(input,target,routes);
        }
        target.subOscGain=dsp::SubOscillatorStack::level(input.subLevelDial);
        // Deterministic amplitude receivers share the same clock, control knots
        // and VCA interpolation for every source format and history domain.
        // Unknown random histories retain their existing admission rules.
        for(auto* lfo:{&target.lfo1,&target.lfo2})
            if(lfo->on && lfo->dest==3 && lfo->wave<4)lfo->nativeSharedClock=true;
    }
}
