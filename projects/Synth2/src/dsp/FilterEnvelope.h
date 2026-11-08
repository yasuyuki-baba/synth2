#pragma once

#include "CompatibleControlCurve.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synth2::dsp
{
    // CEnvelopedObject with CVcf's non-amplitude resolution flag, Win64 0xccc0.
    // Emits signed real targets and exact segment lengths, without a gain ramp.
    class FilterEnvelope
    {
    public:
        enum class Phase { Idle, Attack, Decay, Sustain, Release };
        static float boundedAmount (int baseIndex,float amount) noexcept
        {
            const float headroom=static_cast<float>(1024-baseIndex)*(1.0f/1024.0f);
            return std::clamp(amount,headroom-1.0f,headroom);
        }
        static float boundedAmount(double baseIndex,float amount) noexcept
        {
            return static_cast<float>(compatibleControlCurve(baseIndex,
                [&](int index){return boundedAmount(index,amount);}));
        }
        void configure (int attackFrames,int decayFrames,double peakLevel,double sustainLevel,
                        int releaseFrames,bool negative=false,bool linear=false) noexcept
        {
            const double nextPeak=std::max(0.0,peakLevel);
            const double nextSustain=std::clamp(sustainLevel,0.0,nextPeak);
            // Keep normalized real state through every peak edit, independently
            // of whether the stored level/peaks happen to be integer-valued.
            if(phase==Phase::Sustain)level=nextSustain;
            else if(nextPeak!=peak)
            {
                if(peak<=0){level=0;if(phase==Phase::Release)initial=0;}
                else
                {
                    level=level*nextPeak/peak;
                    // Release keeps the real starting level captured by noteOff.
                    // The cutoff receiver slews live edits; rescaling this anchor
                    // would replace the remaining release trajectory.
                }
            }
            attack=std::max(0,attackFrames);decay=std::max(0,decayFrames);
            release=std::max(0,releaseFrames);peak=nextPeak;
            sustain=nextSustain;inverted=negative;linearDecay=linear;
        }
        void reset() noexcept { phase=Phase::Idle;level=counter=boundary=step=initial=0; }
        void noteOn() noexcept
        {
            phase=Phase::Attack;
            // Restart position is discrete metadata; retain the real level.
            const double restartLevel=std::trunc(level);
            counter=level!=0 && peak>0 ? static_cast<int>(static_cast<double>(attack)*restartLevel/peak) : 0;
        }
        void noteOff() noexcept
        {
            if(phase==Phase::Idle || phase==Phase::Release)return;
            phase=Phase::Release;initial=level;counter=boundary=0;updateResolution();
        }
        bool isActive() const noexcept { return phase!=Phase::Idle; }
        Phase getPhase() const noexcept { return phase; }
        double getLevel() const noexcept { return level; }
        int getCounter() const noexcept { return counter; }
        int getBoundary() const noexcept { return boundary; }
        int getStep() const noexcept { return step; }

        template <typename Segment>
        void render (int frames,Segment&& segment) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(phase==Phase::Attack)
            {
                const int stride=std::max(1,static_cast<int>(attack/std::max(1.0,peak)));
                while(frames>0 && counter<attack)
                {
                    const int count=std::min({frames,stride,attack-counter});
                    level=std::min(peak,compatibleControlCurve(peak,[&](int top) {
                        return std::trunc(static_cast<double>(counter+count)*top/attack);
                    }));
                    segment(inverted ? -level : level,count);
                    counter+=count;frames-=count;
                }
                if(counter>=attack || level>=peak)
                {
                    phase=Phase::Decay;level=peak;counter=boundary=0;updateResolution();
                }
                else return;
            }
            if(phase==Phase::Decay || phase==Phase::Release)
            {
                const bool releasing=phase==Phase::Release;
                const int duration=releasing ? release : decay;
                const double end=releasing ? 0 : sustain;
                const double start=releasing ? initial : peak;
                const float reciprocal=1.0f/static_cast<float>(duration);
                while(frames>0 && counter<duration && level>end)
                {
                    const int count=std::min({frames,step,duration-counter});
                    const float remaining=static_cast<float>(duration-counter-count)*reciprocal;
                    if(linearDecay && !releasing)
                    {
                        level=std::max(end,start-compatibleControlCurve(start-end,[&](int range) {
                            const float slope=static_cast<float>(range)*reciprocal;
                            return std::trunc(static_cast<float>(counter+count)*slope);
                        }));
                    }
                    else
                    {
                        const float curve=std::exp((remaining-1.0f)*8.0f)*remaining;
                        level=compatibleControlCurve(start-end,[&](int range) {
                            return std::trunc(curve*static_cast<float>(range));
                        })+end;
                    }
                    segment(inverted ? -level : level,count);
                    counter+=count;frames-=count;
                    if(counter>=boundary)updateResolution();
                }
                if(counter>=duration || level<=end)
                {
                    if(releasing){phase=Phase::Idle;level=0;}
                    else {phase=Phase::Sustain;level=sustain;counter=0;}
                }
                else return;
            }
            if(frames>0)segment(phase==Phase::Sustain ? (inverted ? -sustain : sustain) : 0,frames);
        }

    private:
        void updateResolution() noexcept
        {
            const int duration=phase==Phase::Decay ? decay : release;
            const double range=phase==Phase::Decay ? peak-sustain : initial;
            const int unit=static_cast<int>(duration/std::max(1.0,range));
            if(linearDecay){boundary=duration;step=std::max(1,unit);return;}
            if(boundary>=duration/2){boundary=duration;step=unit*5;}
            else if(boundary>=duration/3){boundary=duration/2;step=unit*2;}
            else if(boundary>=duration/4){boundary=duration/3;step=unit;}
            else if(boundary>=duration/10){boundary=duration/4;step=unit/4;}
            else if(boundary>=duration/20){boundary=duration/10;step=unit/6;}
            else {boundary=duration/20;step=unit/9;}
            step=std::max(1,step);
        }
        Phase phase {Phase::Idle};
        int attack {},decay {},release {},counter {},boundary {},step {};
        double peak {},sustain {},level {},initial {};
        bool inverted {},linearDecay {};
    };
}
