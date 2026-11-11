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
        void configure (float sampleRate,float index,float resonance) noexcept
        {
            index=std::clamp(index,0.0f,1023.0f);
            if (sampleRate==lastRate && index==lastIndex && resonance==lastResonance) return;
            lastRate=sampleRate; lastIndex=index; lastResonance=resonance;
            // Coefficient preparation keeps its double intermediates; storage and recurrence stay Float32.
            const float exponent=static_cast<float>(index)*(1.0f/1024.0f);
            const float frequency=static_cast<float>(std::exp(std::log(1066.6666666666667)*static_cast<double>(exponent))*15.0);
            const double boundedFrequency=std::min(static_cast<double>(frequency),static_cast<double>(sampleRate)*.5);
            f=static_cast<float>(2.0*std::sin(boundedFrequency*3.141592653589793/(static_cast<int>(sampleRate)*2)));
            configureCoefficients(f,resonance);
        }
        void configureCoefficients (float coefficient,float resonance) noexcept
        {
            clipping=static_cast<float>(resonance)>0.9f;
            const float r=std::clamp(resonance,0.0f,1.0f);
            damping=r-1.0f;
            // The Chamberlin step is stable when f^2-2*damping*f < 4.
            // Keep the existing coefficient whenever it is inside that domain;
            // low-rate/high-cutoff inputs must not cross the marginal pole.
            const float d=damping;
            const float limit=std::nextafter(static_cast<float>(d+std::sqrt(d*d+4.0f)),0.0f);
            f=std::min(coefficient,limit);
            gain=static_cast<float>(1.0-static_cast<double>(r)*.7);
        }
        float process (float input,int mode=0) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const float scaled=input*gain;
            low+=f*band;
            float high=(scaled-low)+damping*band;
            band+=f*high;
            low+=f*band;
            high=(scaled-low)+damping*band;
            band+=f*high;
            const float output=mode==2 ? high : mode==3 ? band : low;
            return clipping ? std::clamp(output,-20.0f,20.0f) : output;
        }
        float getCoefficient() const noexcept { return f; }
        float getGain() const noexcept { return gain; }
        float getLow() const noexcept { return low; }
        float getBand() const noexcept { return band; }
    private:
        float lastRate {-1}; float lastIndex {-1}; float lastResonance {-1};
        bool clipping {};
        float f {},damping {-1},gain {1},low {},band {};
    };
}
