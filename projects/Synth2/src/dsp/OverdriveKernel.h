#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "BiasedOverdriveKernel.h"
namespace synth2::dsp
{
    // MultiFX overdrive modes0/1/2: waveshaper0x83d0 and post-filter0x7dc0.
    class OverdriveKernel
    {
        struct Biquad
        {
            float b0=0,b1=0,b2=0,a1=0,a2=0;
            std::array<float,2> x1{},x2{},y1{},y2{};
            float process(float input,int channel) noexcept
            {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
                float output=x1[channel]*b1+input*b0;
                output+=b2*x2[channel];output-=y1[channel]*a1;output-=a2*y2[channel];
                x2[channel]=x1[channel];x1[channel]=input;y2[channel]=y1[channel];y1[channel]=output;return output;
            }
            void configure(float frequency,float q,int rate,bool highPass) noexcept
            {
                const float omega=static_cast<float>((static_cast<float>(frequency)*6.283185307179586f)/rate);
                // Keep the coefficient and filter history in the same Float32 domain.
                const float sine=std::sin(omega),cosine=static_cast<float>(std::cos(static_cast<float>(omega))),alpha=sine/(q*2.0f);
                const float reciprocal=1.0f/(alpha+1.0f);
                float numerator=highPass ? cosine+1.0f : 1.0f-cosine;
                b0=(numerator*.5f)*reciprocal;b1=(highPass ? -numerator : numerator)*reciprocal;b2=b0;
                a1=(cosine*-2.0f)*reciprocal;a2=(1.0f-alpha)*reciprocal;
            }
        };
    public:
        void prepare(int rate,bool preserve=false) noexcept
        {
            sampleRate=rate;if(!preserve){filters={};silentFrames=0;}
            overdriveOne.prepare(rate,preserve);configureFilters();
        }
        void setDials(float first,float second,float third) noexcept
        {
            if(first!=previous[0])
            {
                drive=static_cast<float>(std::pow(10.0f,((first*(1.0f/127.0f))*70.0f-10.0f)*.05f));
                const float v=(drive+1.0f)*(drive+1.0f)*drive+1.0f;
                normalization=1.0f/(-1.0f/v+1.0f);
                normalizationOne=2.0f/BiasedOverdriveKernel::shape(drive);
            }
            if(second!=previous[1]){tone=second;configureFilters();}
            level=static_cast<float>(third+2)*(1.0f/129.0f);previous={first,second,third};
        }
        bool process(int type,float* left,float* right,int count,bool silent) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(silent)
            {
                if(silentFrames>static_cast<int>(sampleRate*.025f))return true;
                silentFrames+=count;
            }
            else silentFrames=0;
            const float gain=level*normalization;
            const auto shape=[&](float input){
                if(type==1)
                {
                    const float value=(input+.0001f)*drive,absolute=std::abs(value);
                    float denominator=absolute+1.0f;denominator*=denominator;denominator*=absolute;denominator+=1.0f;
                    return std::copysign((-1.0f/denominator+1.0f)*gain,value);
                }
                float value=drive*input;
                if(value>.7f || value<-.7f)value=std::abs(std::abs(std::fmod(value-.7f,.7f*4.0f))-.7f*2.0f)-.7f;
                return value*gain;
            };
            for(int i=0;i<count;++i)
            {
                const float l=silent ? 0.0f : left[i];
                const float shapedLeft=type==0 ? overdriveOne.process(l*drive,0)*(level*normalizationOne) : shape(l);
                left[i]=filters[1].process(filters[0].process(shapedLeft,0),0);
                if(right)
                {
                    const float r=silent ? 0.0f : right[i];
                    const float shapedRight=type==0 ? overdriveOne.process(r*drive,1)*(level*normalizationOne) : shape(r);
                    right[i]=filters[1].process(filters[0].process(shapedRight,1),1);
                }
            }
            return false;
        }
    private:
        void configureFilters() noexcept
        {
            const float frequency=static_cast<float>(std::exp(std::log(40.0f)*(tone*(1.0f/127.0f)))*400.0f);
            filters[0].configure(frequency,3.0f,sampleRate,false);filters[1].configure(100.0f,2.3f,sampleRate,true);
        }
        int sampleRate=44100,silentFrames=0;float tone=64;
        float drive=1,normalization=1,normalizationOne=1,level=1;
        BiasedOverdriveKernel overdriveOne;
        std::array<float,3> previous{-1,-1,-1};std::array<Biquad,2> filters{};
    };
}
