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
            double b0=0,b1=0,b2=0,a1=0,a2=0;
            std::array<double,2> x1{},x2{},y1{},y2{};
            double process(double input,int channel) noexcept
            {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
                double output=x1[channel]*b1+input*b0;
                output+=b2*x2[channel];output-=y1[channel]*a1;output-=a2*y2[channel];
                x2[channel]=x1[channel];x1[channel]=input;y2[channel]=y1[channel];y1[channel]=output;return output;
            }
            void configure(double frequency,double q,int rate,bool highPass) noexcept
            {
                const double omega=static_cast<double>((static_cast<double>(frequency)*6.283185307179586)/rate);
                // Keep the coefficient and filter history in the same double domain.
                const double sine=std::sin(omega),cosine=static_cast<double>(std::cos(static_cast<double>(omega))),alpha=sine/(q*2.0);
                const double reciprocal=1.0/(alpha+1.0);
                double numerator=highPass ? cosine+1.0 : 1.0-cosine;
                b0=(numerator*.5)*reciprocal;b1=(highPass ? -numerator : numerator)*reciprocal;b2=b0;
                a1=(cosine*-2.0)*reciprocal;a2=(1.0-alpha)*reciprocal;
            }
        };
    public:
        void prepare(int rate,bool preserve=false) noexcept
        {
            sampleRate=rate;if(!preserve){filters={};silentFrames=0;}
            overdriveOne.prepare(rate,preserve);configureFilters();
        }
        void setDials(double first,double second,double third) noexcept
        {
            if(first!=previous[0])
            {
                drive=static_cast<double>(std::pow(10.0,((first*(1.0/127.0))*70.0-10.0)*.05));
                const double v=(drive+1.0)*(drive+1.0)*drive+1.0;
                normalization=1.0/(-1.0/v+1.0);
                normalizationOne=2.0/BiasedOverdriveKernel::shape(drive);
            }
            if(second!=previous[1]){tone=second;configureFilters();}
            level=static_cast<double>(third+2)*(1.0/129.0);previous={first,second,third};
        }
        bool process(int type,double* left,double* right,int count,bool silent) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(silent)
            {
                if(silentFrames>static_cast<int>(sampleRate*.025))return true;
                silentFrames+=count;
            }
            else silentFrames=0;
            const double gain=level*normalization;
            const auto shape=[&](double input){
                if(type==1)
                {
                    const double value=(input+.0001)*drive,absolute=std::abs(value);
                    double denominator=absolute+1.0;denominator*=denominator;denominator*=absolute;denominator+=1.0;
                    return std::copysign((-1.0/denominator+1.0)*gain,value);
                }
                double value=drive*input;
                if(value>.7 || value<-.7)value=std::abs(std::abs(std::fmod(value-.7,.7*4.0))-.7*2.0)-.7;
                return value*gain;
            };
            for(int i=0;i<count;++i)
            {
                const double l=silent ? 0.0 : left[i];
                const double shapedLeft=type==0 ? overdriveOne.process(l*drive,0)*(level*normalizationOne) : shape(l);
                left[i]=filters[1].process(filters[0].process(shapedLeft,0),0);
                if(right)
                {
                    const double r=silent ? 0.0 : right[i];
                    const double shapedRight=type==0 ? overdriveOne.process(r*drive,1)*(level*normalizationOne) : shape(r);
                    right[i]=filters[1].process(filters[0].process(shapedRight,1),1);
                }
            }
            return false;
        }
    private:
        void configureFilters() noexcept
        {
            const double frequency=static_cast<double>(std::exp(std::log(40.0)*(tone*(1.0/127.0)))*400.0);
            filters[0].configure(frequency,3.0,sampleRate,false);filters[1].configure(100.0,2.3,sampleRate,true);
        }
        int sampleRate=44100,silentFrames=0;double tone=64;
        double drive=1,normalization=1,normalizationOne=1,level=1;
        BiasedOverdriveKernel overdriveOne;
        std::array<double,3> previous{-1,-1,-1};std::array<Biquad,2> filters{};
    };
}
