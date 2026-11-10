#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // CVcf LP24 kernel, Win64 RVA 0x390b0. Voice routing selects native cutoff
    // scheduling, post-filter gain and output Tone together.
    class Lp24Kernel
    {
    public:
        void prepare (int rate, bool preserveState = false) noexcept
        {
            sampleRate=rate;
            smoothing=1.0f-static_cast<float>(std::pow(0.020000000000000018f,1.0f/(rate*0.001f)));
            if (!preserveState) reset();
            else { lastIndex=-1; lastResonance=-1; }
        }
        void reset() noexcept
        {
            std::fill_n(stage,4,0.0f); std::fill_n(previous,4,0.0f);
            initialized=false; lastIndex=-1; lastResonance=-1;
        }
        void configure (float index,float resonance) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            index=std::clamp(index,0.0f,1023.0f);
            if(index==lastIndex && resonance==lastResonance)return;
            lastIndex=index;lastResonance=resonance;clipping=static_cast<float>(resonance)>0.9f;
            const float exponent=static_cast<float>(index)*(1.0f/1024.0f);
            const float frequency=static_cast<float>(std::exp(std::log(1066.6666666666667f)*static_cast<float>(exponent))*15.0f);
            // Above-Nyquist source cutoffs cannot be represented at low rates.
            // Keep the same recurrence with its pole strictly inside the unit
            // circle rather than switching to another filter implementation.
            const float f=std::min((frequency*2.0f)/static_cast<float>(sampleRate),1.0f);
            const float poleLimit=std::nextafter(1.0f,0.0f);
            targetA=std::clamp(static_cast<float>((static_cast<float>(f)*3.6f-(static_cast<float>(f)*1.6f)*f)-1.0f),-poleLimit,poleLimit);
            targetB=(targetA+1.0f)*0.5f;
            const float square=(f*2.5f)*f;
            float shape=static_cast<float>(f)*4.3f-static_cast<float>(square);
            shape-=1.0f;shape+=1.0f;
            targetFeedback=static_cast<float>(std::exp((1.0f-shape*0.5f)*1.386249f))*resonance;
            if(!initialized){a=targetA;b=targetB;feedback=targetFeedback;initialized=true;}
        }
        float process (float input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            float value=input-stage[3]*feedback;
            for(int i=0;i<4;++i)
            {
                const float oldInput=value;
                value=(value+previous[i])*b-stage[i]*a;
                previous[i]=oldInput;stage[i]=value;
            }
            if(clipping)stage[3]=value=std::clamp(value,-20.0f,20.0f);
            a+=(targetA-a)*smoothing;b+=(targetB-b)*smoothing;
            feedback+=(targetFeedback-feedback)*smoothing;
            return value;
        }
        void copyState (float* output) const noexcept
        {
            std::copy_n(stage,4,output);std::copy_n(previous,4,output+4);
            output[8]=targetA;output[9]=targetB;output[10]=targetFeedback;
            output[11]=a;output[12]=b;output[13]=feedback;
        }
        float getA() const noexcept{return targetA;}
        float getB() const noexcept{return targetB;}
        float getFeedback() const noexcept{return targetFeedback;}
    private:
        int sampleRate {44100}; float lastIndex {-1}; float lastResonance {-1};
        float stage[4] {},previous[4] {},smoothing {},a {},b {},feedback {};
        float targetA {},targetB {},targetFeedback {}; bool initialized {},clipping {};
    };
}
