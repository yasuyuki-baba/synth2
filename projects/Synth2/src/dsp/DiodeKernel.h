#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // CVcf serialized mode4, Win64 RVA0x398b0. Float32 diode recurrence,
    // zero-stuffed oversampling and an independent high-pass feedback state.
    class DiodeKernel
    {
    public:
        void reset() noexcept { state.fill(0);lastRate=-1;lastIndex=-1; }
        void configure(int rate,float index,float resonance) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            index=std::clamp(index,0.0f,1023.0f);
            if(rate==lastRate && index==lastIndex && resonance==lastResonance)return;
            lastRate=rate;lastIndex=index;lastResonance=resonance;
            oversampling=rate<=48000 ? 2 : 1;
            const float exponent=static_cast<float>(index)*(1.0f/1024.0f);
            const float frequency=static_cast<float>(std::exp(std::log(1066.6666666666667f)*static_cast<float>(exponent))*15.0f);
            const float cutoff=std::min(static_cast<float>(frequency)*1.5f,(static_cast<float>(rate)*.5f)*.9f);
            const float inverseRate=1.0f/static_cast<float>(rate*oversampling);
            const float g=std::tan((cutoff*3.141592653589793f)*inverseRate);
            a=1.0f/(g+1.0f);G=a*g;p=G*.5f;
            const float bBase=1.0f/(1.0f-p*p);
            b=bBase*p;c=b*p;
            const float dBase=1.0f/(1.0f-c);
            d=dBase*p;e=(p*2.0f)/(1.0f-(p*2.0f)*d);
            f=c*d;
            const float hBase=1.0f/(1.0f-d*p);
            h=(p*2.0f)*hBase;
            ib=bBase*a;ic=dBase*a;ia=hBase*a;
            float highpass=30.0f;
            if(cutoff>=300.0f)highpass=120.0f;
            else if(cutoff>=100.0f)
            {
                const float lo=std::log(100.0f);
                const float fraction=(std::log(cutoff)-lo)/(std::log(300.0f)-lo);
                highpass=std::exp(fraction*(std::log(120.0f)-std::log(30.0f))+std::log(30.0f));
            }
            const float hpG=std::tan((highpass*3.141592653589793f)*inverseRate);
            const float hpA=1.0f/(hpG+1.0f);
            hpB=(hpG*2.0f)*hpA;hpNegative=-hpA;
            k=static_cast<float>(resonance)*15.0f;
            feedback=hpA*k;denominator=1.0f/(1.0f+feedback*(f*e));
            scale=static_cast<float>(static_cast<float>(oversampling)*0.6499999761581421f)*(1.0f+k*0.20000000298023224f);
        }
        float process(float input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            float output=0;
            for(int sub=0;sub<oversampling;++sub)
            {
                const float t4=state[3]*a;
                const float t3=state[2]*ib+t4*b;
                const float t2=state[1]*ic+t3*d;
                const float t1=state[0]*ia+t2*h;
                float sum=t2*c+t1*f;sum+=t3*p;sum+=t4;
                const float u=((sub==0 ? static_cast<float>(input) : 0.0f)-sum*feedback-state[4]*hpNegative)*denominator;
                const float shaped=(u < -3.0f ? -1.0f : u>3.0f ? 1.0f : ((u*u+27.0f)*u)/((u*9.0f)*u+27.0f))*1.2857142857142858f;
                const float v1=shaped*e+t1;
                const float v2=v1*d+t2;
                const float v3=v2*b+t3;
                const float v4=v3*p+t4;
                state[0]=((v2+shaped)-state[0])*G+v1;
                state[1]=((v3+v1)*.5f-state[1])*G+v2;
                state[2]=((v4+v2)*.5f-state[2])*G+v3;
                state[3]=(v3*.5f-state[3])*G+v4;
                state[4]+=(v4*k-state[4])*hpB;
                output=v3;
            }
            return static_cast<float>(output*scale);
        }
        const std::array<float,5>& readState() const noexcept {return state;}
    private:
        std::array<float,5> state{};
        int lastRate{-1},oversampling{1};float lastIndex{-1};float lastResonance{};
        float a{},G{},p{},b{},c{},d{},e{},f{},h{},ib{},ic{},ia{},hpB{},hpNegative{},k{},feedback{},denominator{},scale{};
    };
}
