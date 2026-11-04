#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "OverdriveKernel.h"
namespace synth2::dsp
{
    // Native MultiFX children: overdrive1/2, decimator, Ring and compressor.
    class MultiEffect
    {
    public:
        void prepare(double rate,bool preserve=false) noexcept
        {
            sampleRate=rate;overdrive.prepare(static_cast<int>(rate),preserve);
            if(!preserve){counter=0;held={};envelope={};cosine=0;sine=1;previous={-1,-1,-1};}
            updateFrequency(frequency);
            updateDynamics();
        }
        void setDials(float first,float second,float third) noexcept
        {
            first=std::clamp(first,0.0f,127.0f);second=std::clamp(second,0.0f,127.0f);third=std::clamp(third,0.0f,127.0f);
            if(previous==std::array<float,3>{first,second,third})return;
            overdrive.setDials(first,second,third);
            if(first!=previous[0]){downsample=first;updateFrequency(static_cast<float>(std::pow(8000.0,static_cast<double>(first*(1.0f/127.0f)))));}
            bits=static_cast<int>(second/8);mix=third*(1.0f/127.0f);
            const float thresholdDb=-20.0f-(first*(1.0f/127.0f))*40.0f;
            threshold=std::exp(thresholdDb*0.115129254758358f);
            makeup=std::exp(((-20.0f-thresholdDb)*-1.0f)*-0.115129254758358f);
            milliseconds=static_cast<float>(std::pow(2000.0,static_cast<double>(std::sqrt(second*(1.0f/127.0f))))*0.1);
            updateDynamics();
            level=std::exp(((third*(1.0f/127.0f))*30.0f)*0.115129254758358f-2.3025851249694824f);
            previous={first,second,third};
        }
        bool process(int type,float* left,float* right,int count,bool silent) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(type>=0 && type<=2)return overdrive.process(type,left,right,count,silent);
            if(silent){if(type==5)envelope={};return true;}
            const float dry=1.0f-mix;
            for(int i=0;i<count;++i)
            {
                if(type==3)
                {
                    if(captureFrame())
                    {
                        const uint32_t mask=~uint32_t{0}<<static_cast<unsigned>(bits+1);
                        const auto quantize=[&](float value){const int32_t word=static_cast<int32_t>(value*65536.0f);return static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(word)&mask))*(1.0f/65536.0f);};
                        held={quantize(left[i]),quantize(right ? right[i] : left[i])};
                    }
                    left[i]=held[0]*mix+left[i]*dry;
                    if(right)right[i]=held[1]*mix+right[i]*dry;
                    ++counter;
                }
                else if(type==5)
                {
                    const float gain=level*makeup;
                    const auto compress=[&](float input,int channel){
                        const float desired=std::max(std::abs(input)-threshold,0.0f);
                        float next=(envelope[channel]-desired)*coefficient+desired;
                        if(std::fpclassify(next)==FP_SUBNORMAL)next=0;
                        envelope[channel]=next;
                        return ((threshold*input)/(threshold+next))*gain;
                    };
                    left[i]=compress(left[i],0);if(right)right[i]=compress(right[i],1);
                }
                else if(type==4)
                {
                    cosine-=step*sine;sine+=cosine*step;
                    const float gain=mix*cosine+dry;
                    left[i]*=gain;if(right)right[i]*=gain;
                }
            }
            return false;
        }
        std::array<float,7> snapshot() const noexcept{return {mix,static_cast<float>(downsample),static_cast<float>(bits),cosine,sine,step,static_cast<float>(counter)};}
    private:
        bool captureFrame() const noexcept
        {
            if(downsample==std::floor(downsample))
                return counter%static_cast<uint32_t>(downsample+1)==0;
            const double period=static_cast<double>(downsample)+1.0,frame=counter;
            return std::floor(frame/period)!=std::floor((frame-1.0)/period);
        }
        void updateDynamics() noexcept
        {
            coefficient=static_cast<float>(std::exp(-1.0/((sampleRate*0.001)*static_cast<double>(milliseconds))));
        }
        void updateFrequency(float value) noexcept
        {
            frequency=value;step=static_cast<float>(std::sin(static_cast<double>(value)*3.141592653589793/sampleRate)*2.0);
            cosine=std::clamp(cosine,-1.0f,1.0f);
            const float next=std::sin(std::acos(cosine));sine=sine<0 ? -next : next;
        }
        OverdriveKernel overdrive;
        double sampleRate=44100;
        float downsample=0;int bits=0;uint32_t counter=0;
        float mix=0,frequency=1000,cosine=0,sine=1,step=0;
        float threshold=1,makeup=1,level=1,milliseconds=1,coefficient=0;
        std::array<float,2> envelope{};
        std::array<float,2> held{};std::array<float,3> previous{-1,-1,-1};
    };
}
