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
            sampleRate=rate;overdrive.prepare(static_cast<int>(rate*audioFactor),preserve);
            if(!preserve){counter=0;held={};envelope={};cosine=0;sine=1;previous={-1,-1,-1};}
            updateFrequency(frequency);
            updateDynamics();
        }
        void setOversamplingFactor(int factor) noexcept
        {if(audioFactor!=factor){audioFactor=factor;overdrive.prepare(static_cast<int>(sampleRate*factor));}}
        void setDials(double first,double second,double third) noexcept
        {
            first=std::clamp(first,0.0,127.0);second=std::clamp(second,0.0,127.0);third=std::clamp(third,0.0,127.0);
            if(previous==std::array<double,3>{first,second,third})return;
            overdrive.setDials(first,second,third);
            if(first!=previous[0]){downsample=first;updateFrequency(static_cast<double>(std::pow(8000.0,static_cast<double>(first*(1.0/127.0)))));}
            bits=static_cast<int>(second/8);mix=third*(1.0/127.0);
            const double thresholdDb=-20.0-(first*(1.0/127.0))*40.0;
            threshold=std::exp(thresholdDb*0.115129254758358);
            makeup=std::exp(((-20.0-thresholdDb)*-1.0)*-0.115129254758358);
            milliseconds=static_cast<double>(std::pow(2000.0,static_cast<double>(std::sqrt(second*(1.0/127.0))))*0.1);
            updateDynamics();
            level=std::exp(((third*(1.0/127.0))*30.0)*0.115129254758358-2.3025851249694824);
            previous={first,second,third};
        }
        bool process(int type,double* left,double* right,int count,bool silent) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(type>=0 && type<=2)return overdrive.process(type,left,right,count,silent);
            if(silent){if(type==5)envelope={};return true;}
            const double dry=1.0-mix;
            for(int i=0;i<count;++i)
            {
                if(type==3)
                {
                    if(captureFrame())
                    {
                        const uint32_t mask=~uint32_t{0}<<static_cast<unsigned>(bits+1);
                        const auto quantize=[&](double value){const int32_t word=static_cast<int32_t>(value*65536.0);return static_cast<double>(static_cast<int32_t>(static_cast<uint32_t>(word)&mask))*(1.0/65536.0);};
                        held={quantize(left[i]),quantize(right ? right[i] : left[i])};
                    }
                    left[i]=held[0]*mix+left[i]*dry;
                    if(right)right[i]=held[1]*mix+right[i]*dry;
                    ++counter;
                }
                else if(type==5)
                {
                    const double gain=level*makeup;
                    const auto compress=[&](double input,int channel){
                        const double desired=std::max(std::abs(input)-threshold,0.0);
                        double next=(envelope[channel]-desired)*coefficient+desired;
                        if(std::fpclassify(next)==FP_SUBNORMAL)next=0;
                        envelope[channel]=next;
                        return ((threshold*input)/(threshold+next))*gain;
                    };
                    left[i]=compress(left[i],0);if(right)right[i]=compress(right[i],1);
                }
                else if(type==4)
                {
                    cosine-=step*sine;sine+=cosine*step;
                    const double gain=mix*cosine+dry;
                    left[i]*=gain;if(right)right[i]*=gain;
                }
            }
            return false;
        }
        std::array<double,7> snapshot() const noexcept{return {mix,static_cast<double>(downsample),static_cast<double>(bits),cosine,sine,step,static_cast<double>(counter)};}
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
            coefficient=static_cast<double>(std::exp(-1.0/((sampleRate*0.001)*static_cast<double>(milliseconds))));
        }
        void updateFrequency(double value) noexcept
        {
            frequency=value;step=static_cast<double>(std::sin(static_cast<double>(value)*3.141592653589793/sampleRate)*2.0);
            cosine=std::clamp(cosine,-1.0,1.0);
            const double next=std::sin(std::acos(cosine));sine=sine<0 ? -next : next;
        }
        int audioFactor=1;
        OverdriveKernel overdrive;
        double sampleRate=44100;
        double downsample=0;int bits=0;uint32_t counter=0;
        double mix=0,frequency=1000,cosine=0,sine=1,step=0;
        double threshold=1,makeup=1,level=1,milliseconds=1,coefficient=0;
        std::array<double,2> envelope{};
        std::array<double,2> held{};std::array<double,3> previous{-1,-1,-1};
    };
}
