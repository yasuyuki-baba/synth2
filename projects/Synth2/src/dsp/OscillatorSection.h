#pragma once
#include "Oscillator.h"
#include "OscillatorStack.h"
#include "SubOscillatorStack.h"
#include "OscillatorSync.h"
#include "FmPhaseCorrection.h"
#include "audio/AudioTypes.h"
#include <array>

namespace synth2::dsp {
    struct OscillatorFrameAdvance {
        int64_t first {},second {};
        std::array<int64_t,8> stack {};
    };

    class OscillatorSection {
    public:
        void prepare(const ::synth2::audio::ProcessSpec& spec) {
            sampleRate = spec.sampleRate;
            osc1.setSampleRate(sampleRate);
            osc2.setSampleRate(sampleRate);
        }
        void reset() { osc1.reset(); osc2.reset(); stackPhases.fill(0); subSharedPhase=0; fmPhase.reset(); }
        void resetFmPhase() noexcept { fmPhase.reset(); }
        void resetPhase(double phase01) {
            fmPhase.reset();
            osc1.resetPhase(phase01, params.osc1Wave);
            osc2.resetPhase(phase01, params.osc2Wave);
        }
        uint32_t getOsc1Phase() const noexcept { return osc1.getPhaseFixed27(); }
        std::array<uint32_t,4> readPhaseWords() const noexcept {
            return {{osc1.getPhaseFixed27(),osc2.getPhaseFixed27(),
                     osc1.getPhaseIncFixed27(),osc2.getPhaseIncFixed27()}};
        }

        double processSample(double f1, double f2, double pw, double mix, double fmAmt, bool sync, bool ring,
                            double subGain, int subShape, int subOctaveMode, const WaveTableBank* bank, double nativePulseWidthBins = -1, double pitch1Control = 0, double pitch2Control = 0, bool nativePitchLfo = false, bool correctFmPhase = false, OscillatorFrameAdvance* frameAdvance = nullptr) {
            const double base1=f1;
            const double pulseBins=nativePulseWidthBins>=0 ? nativePulseWidthBins
                : ModulationEnvelope::pulseWidthForDial(fractionalDial(pw),0.0);
            if(nativePitchLfo)
            {
                f1=ModulationEnvelope::pitchFrequency(f1,pitch1Control);
                f2=ModulationEnvelope::pitchFrequency(f2,pitch2Control);
            }
            // The fixed VCO ceiling also applies without an active pitch receiver.
            // Limit after modulation, before table selection and phase advancement.
            f1=std::min(f1,22000.0);
            f2=std::min(f2,22000.0);
            const bool nativeStack=std::lround(stackDial)>0 && bank != nullptr;
            uint32_t p1Old = osc1.getPhaseFixed27();
            osc1.updatePhaseIncrement(f1);
            osc2.updatePhaseIncrement(f2);
            double s2=osc2.renderSample(params.osc2Wave,pw,0,bank,pulseBins);

            // FM amount is in 27-bit phase-increment units.
            // The native dispatcher scales noise by .65 only without FM.
            // FM kernels apply it internally; Ring wins dispatch but retains
            // the unscaled noise gain when FM is selected.
            if (params.osc2Wave == 4 && !(ring && fmAmt!=0)) s2 *= .65;
            // Ring wins FM dispatch, but selecting FM also bypasses the
            // pulse modulator's wrapper half-gain (as for noise above).
            if (params.osc2Wave == 1 && ring && fmAmt!=0) s2 *= 2.0;
            const double fmSource = s2;

            // Native isolated two-tone/noise captures use a linear crossfade.
            double vol1 = 1.0 - mix;
            double vol2 = mix;
            double scale = SubOscillatorStack::normalizer(subGain,mix);

            // Native Windows kernels use MULSS then CVTTSS2SI, not nearest rounding.
            // VCO dispatch tests Ring (+0x168) before its FM branch. The
            // audited ring kernel advances both phases without an FM offset.
            const auto fmAdvance=fmPhase.advance(ring ? 0.0 : fmAmt,fmSource,
                osc1.getPhaseIncFixed27(),p1Old,correctFmPhase && !ring);
            const uint32_t phaseAdd=fmAdvance.audibleOffset;
            if(frameAdvance) {
                *frameAdvance={};
                frameAdvance->first=int64_t(osc1.getPhaseIncFixed27())+static_cast<int32_t>(phaseAdd);
                frameAdvance->second=osc2.getPhaseIncFixed27();
            }

            double s1 = osc1.renderSample(params.osc1Wave, pw, 0, bank,pulseBins);
            double nativeSubSample=0.0;
            if(nativeStack)
            {
                if(stackBase != base1 || cachedStackDial != stackDial)
                {
                    stackFrequencies=OscillatorStack::frequencies(base1,stackDial);
                    stackBase=base1;cachedStackDial=stackDial;
                }
                const int shape=params.osc1Wave == 1 ? 5 : params.osc1Wave;
                const auto table=(shape == 0 || shape == 2 || shape == 5)
                    ? bank->getNativeTableFor(shape,static_cast<float>(std::min(f1,22000.0))) : bank->getTableFor(shape,std::min(f1,22000.0));
                if(table.first != nullptr)
                {
                    auto read=[&](uint32_t phase) {
                        const uint32_t index=(phase>>16)&2047;
                        const double value=table.first[index];
                        if(params.osc1Wave!=1)return value;
                        return PulseWidthRead::interpolate(pulseBins,[&](uint32_t offset) {
                            return value-table.first[(index+offset)&2047];
                        });
                    };
                    // Native kernels sum lane 0, centre, then lanes 1..7.
                    s1=read(stackPhases[0])+read(osc1.getPhaseFixed27());
                    for(int lane=1;lane<8;++lane)s1+=read(stackPhases[lane]);
                    s1*=params.osc1Wave == 1 ? 0.15 : 0.3;
                    if(subGain>0)
                    {
                        const bool down=subOctaveMode < 0;
                        const double subFrequency=std::min(f1,22000.0)*(down ? 0.5 : 1.0);
                        const auto subTable=(subShape == 0 || subShape == 1 || subShape == 2)
                            ? bank->getNativeTableFor(subShape,static_cast<float>(subFrequency)) : bank->getTableFor(subShape,subFrequency);
                        if(subTable.first != nullptr)
                        {
                            auto readSub=[&](uint32_t phase) { return subTable.first[SubOscillatorStack::tableIndex(phase,down)]; };
                            nativeSubSample=down ? readSub(subSharedPhase)+readSub(stackPhases[0])
                                                 : readSub(stackPhases[0])+readSub(subSharedPhase);
                            for(int lane=1;lane<8;++lane)nativeSubSample+=readSub(stackPhases[lane]);
                            // With stack, dispatch scales the sub gain by .3
                            // only outside Ring. Ring kernels scale their
                            // carrier sum internally and leave the sub unscaled.
                            if(!ring)nativeSubSample*=0.3;
                        }
                    }
                    for(int lane=0;lane<8;++lane)
                    {
                        const double laneFrequency=pitch1Control != 0
                            ? ModulationEnvelope::pitchFrequency(stackFrequencies[lane],pitch1Control) : std::min(stackFrequencies[lane],22000.0);
                        const uint32_t increment=compatiblePhaseIncrement(laneFrequency,sampleRate);
                        if(frameAdvance)frameAdvance->stack[lane]=int64_t(static_cast<uint32_t>(increment))+static_cast<int32_t>(phaseAdd);
                        stackPhases[lane]+=static_cast<uint32_t>(increment)+phaseAdd;
                    }
                }
            }

            double combined = ring ? ((s2 * (vol2*scale)) * s1 + s1 * (vol1*scale))
                                        : (s1 * vol1 + s2 * vol2) * scale;

            if(subGain>0 && nativeStack)
            {
                combined+=(nativeSubSample*vol1*subGain)*scale;
            }
            else if(subGain>0 && bank!=nullptr)
            {
                const bool down=subOctaveMode<0;
                const double frequency=std::min(f1,22000.0)*(down ? 0.5 : 1.0);
                const auto table=bank->getNativeTableFor(subShape,frequency);
                if(table.first)
                {
                    const uint32_t phase=down ? subSharedPhase>>1 : subSharedPhase;
                    const uint32_t index=(phase>>16)&2047u;
                    double value=table.first[(index+1)&2047u]-table.first[index];
                    value*=static_cast<double>(phase&65535u);value*=1.0/65536.0;value+=table.first[index];
                    combined+=(value*(vol1*scale))*subGain;
                }
            }

            subSharedPhase+=osc1.getPhaseIncFixed27()+phaseAdd;
            osc1.advancePhaseFixedWithOffset(phaseAdd);
            if (params.osc2Wave == 4) {
                if(sync)osc2.synchronizeNoise(fmAdvance.referenceBefore,fmAdvance.referenceAfter);
            } else if (sync) {
                const auto next=OscillatorSync::advance(fmAdvance.referenceBefore,osc2.getPhaseFixed27(),
                    osc1.getPhaseIncFixed27(),osc2.getPhaseIncFixed27(),fmAdvance.referenceOffset);
                osc2.setPhaseFixed27(next.second);
            } else {
                osc2.advancePhase();
            }
            return combined;
        }

        // Advance the authoritative FM/Sync/noise state once at the host rate.
        // Fine reads follow that signed phase trajectory; Sync resets remain at
        // its boundary. Never infer direction from a shortest circular arc.
        void processFrame(double* output,int factor,double f1,double f2,double pw,double mix,
                          double fmAmt,bool sync,bool ring,double subGain,int subShape,int subOctaveMode,
                          const WaveTableBank* bank,double pulseBins=-1,double pitch1=0,double pitch2=0,
                          bool pitchReceiver=false,bool correctFmPhase=false) {
            const auto initial=*this;
            OscillatorFrameAdvance advance;
            output[0]=processSample(f1,f2,pw,mix,fmAmt,sync,ring,subGain,subShape,subOctaveMode,
                                    bank,pulseBins,pitch1,pitch2,pitchReceiver,correctFmPhase,&advance);
            for(int step=1;step<factor;++step) {
                auto view=initial;
                const auto phase=[&](uint32_t start,int64_t delta) {
                    return start+static_cast<uint32_t>(delta*step/factor);
                };
                view.osc1.setPhaseFixed27(phase(initial.osc1.getPhaseFixed27(),advance.first));
                view.osc2.setPhaseFixed27(phase(initial.osc2.getPhaseFixed27(),advance.second));
                view.subSharedPhase=phase(initial.subSharedPhase,advance.first);
                for(size_t lane=0;lane<view.stackPhases.size();++lane)
                    view.stackPhases[lane]=phase(initial.stackPhases[lane],advance.stack[lane]);
                // Noise reads use a disposable copy of the starting state, so
                // they hold the base-frame sample and never advance the real RNG.
                output[step]=view.processSample(f1,f2,pw,mix,fmAmt,sync,ring,subGain,subShape,subOctaveMode,
                                                bank,pulseBins,pitch1,pitch2,pitchReceiver,correctFmPhase);
            }
        }

        struct Settings {
            int osc1Wave{0}, osc2Wave{0};
        } params;
    public:
        uint32_t subSharedPhase {0};
        double stackDial {0}, cachedStackDial {-1};
        double stackBase {-1.0};
        std::array<double,8> stackFrequencies {};
        std::array<uint32_t,8> stackPhases {};
        double sampleRate{44100.0};
        Oscillator osc1, osc2;
        FmPhaseCorrection fmPhase;
    };
}
