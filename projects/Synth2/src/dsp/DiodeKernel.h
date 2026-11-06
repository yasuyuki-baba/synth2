#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // CVcf serialized mode4, Win64 RVA0x398b0. Double diode recurrence,
    // zero-stuffed oversampling and an independent high-pass feedback state.
    class DiodeKernel
    {
    public:
        void reset() noexcept { state.fill(0);lastRate=lastIndex=-1; }
        void configure(int rate,double index,float resonance) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            index=std::clamp(index,0.0,1023.0);
            if(rate==lastRate && index==lastIndex && resonance==lastResonance)return;
            lastRate=rate;lastIndex=index;lastResonance=resonance;
            oversampling=rate<=48000 ? 2 : 1;
            const float exponent=static_cast<float>(index)*(1.0f/1024.0f);
            const float frequency=static_cast<float>(std::exp(std::log(1066.6666666666667)*static_cast<double>(exponent))*15.0);
            const double cutoff=std::min(static_cast<double>(frequency)*1.5,(static_cast<double>(rate)*.5)*.9);
            const double inverseRate=1.0/static_cast<double>(rate*oversampling);
            const double g=std::tan((cutoff*3.141592653589793)*inverseRate);
            a=1.0/(g+1.0);G=a*g;p=G*.5;
            const double bBase=1.0/(1.0-p*p);
            b=bBase*p;c=b*p;
            const double dBase=1.0/(1.0-c);
            d=dBase*p;e=(p*2.0)/(1.0-(p*2.0)*d);
            f=c*d;
            const double hBase=1.0/(1.0-d*p);
            h=(p*2.0)*hBase;
            ib=bBase*a;ic=dBase*a;ia=hBase*a;
            double highpass=30.0;
            if(cutoff>=300.0)highpass=120.0;
            else if(cutoff>=100.0)
            {
                const double lo=std::log(100.0);
                const double fraction=(std::log(cutoff)-lo)/(std::log(300.0)-lo);
                highpass=std::exp(fraction*(std::log(120.0)-std::log(30.0))+std::log(30.0));
            }
            const double hpG=std::tan((highpass*3.141592653589793)*inverseRate);
            const double hpA=1.0/(hpG+1.0);
            hpB=(hpG*2.0)*hpA;hpNegative=-hpA;
            k=static_cast<double>(resonance)*15.0;
            feedback=hpA*k;denominator=1.0/(1.0+feedback*(f*e));
            scale=static_cast<double>(static_cast<float>(oversampling)*0.6499999761581421f)*(1.0+k*0.20000000298023224);
        }
        float process(float input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            double output=0;
            for(int sub=0;sub<oversampling;++sub)
            {
                const double t4=state[3]*a;
                const double t3=state[2]*ib+t4*b;
                const double t2=state[1]*ic+t3*d;
                const double t1=state[0]*ia+t2*h;
                double sum=t2*c+t1*f;sum+=t3*p;sum+=t4;
                const double u=((sub==0 ? static_cast<double>(input) : 0.0)-sum*feedback-state[4]*hpNegative)*denominator;
                const double shaped=(u < -3.0 ? -1.0 : u>3.0 ? 1.0 : ((u*u+27.0)*u)/((u*9.0)*u+27.0))*1.2857142857142858;
                const double v1=shaped*e+t1;
                const double v2=v1*d+t2;
                const double v3=v2*b+t3;
                const double v4=v3*p+t4;
                state[0]=((v2+shaped)-state[0])*G+v1;
                state[1]=((v3+v1)*.5-state[1])*G+v2;
                state[2]=((v4+v2)*.5-state[2])*G+v3;
                state[3]=(v3*.5-state[3])*G+v4;
                state[4]+=(v4*k-state[4])*hpB;
                output=v3;
            }
            return static_cast<float>(output*scale);
        }
        const std::array<double,5>& readState() const noexcept {return state;}
    private:
        std::array<double,5> state{};
        int lastRate{-1},oversampling{1};double lastIndex{-1};float lastResonance{};
        double a{},G{},p{},b{},c{},d{},e{},f{},h{},ib{},ic{},ia{},hpB{},hpNegative{},k{},feedback{},denominator{},scale{};
    };
}
