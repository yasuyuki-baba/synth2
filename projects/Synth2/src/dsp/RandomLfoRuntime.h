#pragma once
#include "LfoRange.h"
#include "CompatibleControlCurve.h"
#include "RandomLfo.h"
#include "MidiRoutingHistory.h"
#include <array>

namespace synth2::dsp
{
    // Shared random state follows serialized native loading and render events.
    // No preset identity or load-time reseeding. Untracked consumers mark the
    // observed history incomplete; ARP keeps consuming this same retained stream.
    class RandomLfoRuntime
    {
    public:
        struct Control { int destination=1,wave=0,depth=0; };
        std::array<Control,2> controls{};
        std::array<std::pair<int,int>,2> ranges{};
        std::array<RandomLfo,2> targets{};
        CrtRandom random;
        bool valid=true;
        bool arpeggioSeen=false;

        void load(const std::array<int,4>& routes,const std::array<Control,2>& next) noexcept
        {
            routing.load(routes,[&](int destination) {
                if(destination==44)setDepth(0,next[0].depth);
                if(destination==49)setDepth(1,next[1].depth);
            });
            for(size_t slot=0;slot<2;++slot)
            {
                controls[slot].destination=std::clamp(next[slot].destination,1,7);recalculate(slot);
                controls[slot].wave=next[slot].wave;
                targets[slot].setWave(next[slot].wave,ranges[slot].first,ranges[slot].second,random);
                setDepth(slot,next[slot].depth);
            }
        }
        void edit(int parameter,int value) noexcept
        {
            const size_t slot=parameter>=46 && parameter<=49 ? 1u:0u;
            if(parameter==44 || parameter==49)setDepth(slot,value);
            else if(parameter==41 || parameter==46){controls[slot].destination=std::clamp(value,1,7);recalculate(slot);}
            else if(parameter==42 || parameter==47){controls[slot].wave=value;targets[slot].setWave(value,ranges[slot].first,ranges[slot].second,random);}
            else valid=false;
        }
        std::pair<double,double> effectiveRange(size_t slot,double depth=-1) const noexcept
        {
            // Whole render dials leave the observed setter history authoritative.
            // Fractional edits extend that history without redrawing held targets.
            if(depth<0 || depth==std::trunc(depth))return {ranges[slot].first,ranges[slot].second};
            return {compatibleControlCurve(depth,[&](int dial) {return lfoDepthRange(controls[slot].destination,dial).first;}),
                compatibleControlCurve(depth,[&](int dial) {return lfoDepthRange(controls[slot].destination,dial).second;})};
        }
        void resetPhase(size_t slot,int& counter,double depth=-1) noexcept
        {
            const auto range=effectiveRange(slot,depth);
            targets[slot].resetPhase(counter,controls[slot].wave,range.first,range.second,random);
        }
        void advance(size_t slot,int samples,int counter,int period,double depth=-1) noexcept
        {
            const auto range=effectiveRange(slot,depth);
            targets[slot].advance(samples,counter,period,controls[slot].wave,range.first,range.second,random);
        }
        std::pair<double,int> query(size_t slot,int counter,int period,int step,double depth=-1) const noexcept
        {
            const auto range=effectiveRange(slot,depth);
            return targets[slot].query(controls[slot].wave,range.first,range.second,counter,period,step);
        }
    private:
        MidiRoutingHistory routing;
        void recalculate(size_t slot) noexcept
        { ranges[slot]=lfoDepthRange(controls[slot].destination,controls[slot].depth); }
        void setDepth(size_t slot,int value) noexcept
        { controls[slot].depth=std::clamp(value,0,127);recalculate(slot); }
    };
}
