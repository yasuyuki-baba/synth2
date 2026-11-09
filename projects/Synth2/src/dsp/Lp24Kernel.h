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
            smoothing=1.0-static_cast<double>(std::pow(0.020000000000000018,1.0/(rate*0.001)));
            if (!preserveState) reset();
            else { lastIndex=-1; lastResonance=-1; }
        }
        void reset() noexcept
        {
            std::fill_n(stage,4,0.0); std::fill_n(previous,4,0.0);
            initialized=false; lastIndex=-1; lastResonance=-1;
        }
        void configure (double index,double resonance) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            index=std::clamp(index,0.0,1023.0);
            if(index==lastIndex && resonance==lastResonance)return;
            lastIndex=index;lastResonance=resonance;clipping=static_cast<double>(resonance)>0.9;
            const double exponent=static_cast<double>(index)*(1.0/1024.0);
            const double frequency=static_cast<double>(std::exp(std::log(1066.6666666666667)*static_cast<double>(exponent))*15.0);
            // Above-Nyquist source cutoffs cannot be represented at low rates.
            // Keep the same recurrence with its pole strictly inside the unit
            // circle rather than switching to another filter implementation.
            const double f=std::min((frequency*2.0)/static_cast<double>(sampleRate),1.0);
            const double poleLimit=std::nextafter(1.0,0.0);
            targetA=std::clamp(static_cast<double>((static_cast<double>(f)*3.6-(static_cast<double>(f)*1.6)*f)-1.0),-poleLimit,poleLimit);
            targetB=(targetA+1.0)*0.5;
            const double square=(f*2.5)*f;
            double shape=static_cast<double>(f)*4.3-static_cast<double>(square);
            shape-=1.0;shape+=1.0;
            targetFeedback=static_cast<double>(std::exp((1.0-shape*0.5)*1.386249))*resonance;
            if(!initialized){a=targetA;b=targetB;feedback=targetFeedback;initialized=true;}
        }
        double process (double input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            double value=input-stage[3]*feedback;
            for(int i=0;i<4;++i)
            {
                const double oldInput=value;
                value=(value+previous[i])*b-stage[i]*a;
                previous[i]=oldInput;stage[i]=value;
            }
            if(clipping)stage[3]=value=std::clamp(value,-20.0,20.0);
            a+=(targetA-a)*smoothing;b+=(targetB-b)*smoothing;
            feedback+=(targetFeedback-feedback)*smoothing;
            return value;
        }
        void copyState (double* output) const noexcept
        {
            std::copy_n(stage,4,output);std::copy_n(previous,4,output+4);
            output[8]=targetA;output[9]=targetB;output[10]=targetFeedback;
            output[11]=a;output[12]=b;output[13]=feedback;
        }
        double getA() const noexcept{return targetA;}
        double getB() const noexcept{return targetB;}
        double getFeedback() const noexcept{return targetFeedback;}
    private:
        int sampleRate {44100}; double lastIndex {-1}; double lastResonance {-1};
        double stage[4] {},previous[4] {},smoothing {},a {},b {},feedback {};
        double targetA {},targetB {},targetFeedback {}; bool initialized {},clipping {};
    };
}
