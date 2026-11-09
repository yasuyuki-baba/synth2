#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // CVcf LP12/HP12/BP12, native SSE branch with output-only high-resonance clipping (0x38300 / 0x38c3e).
    // This is two Chamberlin updates using the same input, not a TPT filter.
    class StateVariableFilter
    {
    public:
        void reset() noexcept { low=band=0; }
        void configure (double sampleRate,double index,double resonance) noexcept
        {
            index=std::clamp(index,0.0,1023.0);
            if (sampleRate==lastRate && index==lastIndex && resonance==lastResonance) return;
            lastRate=sampleRate; lastIndex=index; lastResonance=resonance;
            const double exponent=static_cast<double>(index)*(1.0/1024.0);
            const double frequency=static_cast<double>(std::exp(std::log(1066.6666666666667)*static_cast<double>(exponent))*15.0);
            const double boundedFrequency=std::min(static_cast<double>(frequency),sampleRate*.5);
            f=static_cast<double>(2.0*std::sin(boundedFrequency*3.141592653589793/(static_cast<int>(sampleRate)*2)));
            configureCoefficients(f,resonance);
        }
        void configureCoefficients (double coefficient,double resonance) noexcept
        {
            clipping=static_cast<double>(resonance)>0.9;
            const double r=std::clamp(resonance,0.0,1.0);
            damping=r-1.0;
            // The Chamberlin step is stable when f^2-2*damping*f < 4.
            // Keep the existing coefficient whenever it is inside that domain;
            // low-rate/high-cutoff inputs must not cross the marginal pole.
            const double d=damping;
            const double limit=std::nextafter(static_cast<double>(d+std::sqrt(d*d+4.0)),0.0);
            f=std::min(coefficient,limit);
            gain=static_cast<double>(1.0-static_cast<double>(r)*.7);
        }
        double process (double input,int mode=0) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const double scaled=input*gain;
            low+=f*band;
            double high=(scaled-low)+damping*band;
            band+=f*high;
            low+=f*band;
            high=(scaled-low)+damping*band;
            band+=f*high;
            const double output=mode==2 ? high : mode==3 ? band : low;
            return clipping ? std::clamp(output,-20.0,20.0) : output;
        }
        double getCoefficient() const noexcept { return f; }
        double getGain() const noexcept { return gain; }
        double getLow() const noexcept { return low; }
        double getBand() const noexcept { return band; }
    private:
        double lastRate {-1}; double lastIndex {-1}; double lastResonance {-1};
        bool clipping {};
        double f {},damping {-1},gain {1},low {},band {};
    };
}
