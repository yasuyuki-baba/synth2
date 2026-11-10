#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // Win64 CEq mode 0: raw callers 0x29f06/45/70 and coefficients 0xd4a0.
    class PeakEq
    {
    public:
        void prepare(float rate,bool preserve=false) noexcept
        {
            sampleRate=static_cast<int>(rate);
            if(!preserve)states={};
            setDials(frequencyDial,levelDial,qDial);
        }
        void reset() noexcept {states={};}
        void setDials(float frequency,float level,float q) noexcept
        {
            frequencyDial=std::clamp(frequency,0.0f,127.0f);levelDial=std::clamp(level,0.0f,127.0f);qDial=std::clamp(q,0.0f,127.0f);
            const float inverse127=1.0f/127.0f;
            frequencyHz=static_cast<float>(std::pow(320.0f,static_cast<float>(frequencyDial*inverse127))*50.0f);
            gainDb=static_cast<float>(levelDial-64)*inverse127*50.0f;
            quality=static_cast<float>(std::pow(10.0f,static_cast<float>(qDial*inverse127)));
            const float amplitude=static_cast<float>(std::pow(10.0f,static_cast<float>(gainDb*.05f)));
            const float omega=static_cast<float>(static_cast<float>(frequencyHz)*6.283185307179586f);
            const float inverseRate=1.0f/static_cast<float>(sampleRate);
            float pole,zero;
            if(amplitude>=1.0f){pole=1.0f/(quality*2.0f);zero=pole*amplitude;}
            else {zero=1.0f/(quality*2.0f);pole=zero/amplitude;}
            // The DLL stores float(e) in a double constant, rather than using exp.
            constexpr float exponentialBase=2.7182817459106445f;
            const float rz=static_cast<float>(std::pow(exponentialBase,static_cast<float>(-((inverseRate*omega)*zero))));
            const float rp=static_cast<float>(std::pow(exponentialBase,static_cast<float>(-((omega*inverseRate)*pole))));
            auto dampedAngular=[&](float damping,float radius,float decay) {
                const float square=static_cast<float>(std::pow(static_cast<float>(damping),2.0f));
                float argument=std::sqrt(std::abs(1.0f-square));argument*=inverseRate;argument*=omega;
                const float angular=std::abs(damping)>1.0f ? static_cast<float>(std::cosh(static_cast<float>(argument))) : static_cast<float>(std::cos(static_cast<float>(argument)));
                if(std::isfinite(angular))return angular*(radius*-2.0f);
                // At low rates an overdamped section can form 0 * inf. Evaluate
                // -2 * base^decay * cosh(argument) as a sum of decaying exponentials.
                // Keep the factored expression whenever its intermediate is finite.
                const float exponent=static_cast<float>(decay)*std::log(exponentialBase);
                return static_cast<float>(-(std::exp(exponent+static_cast<float>(argument))
                    +std::exp(exponent-static_cast<float>(argument))));
            };
            const float z1=dampedAngular(zero,rz,-((inverseRate*omega)*zero)),z2=static_cast<float>(std::pow(static_cast<float>(rz),2.0f));
            a1=dampedAngular(pole,rp,-((omega*inverseRate)*pole));a2=static_cast<float>(std::pow(static_cast<float>(rp),2.0f));
            const float angle=omega*inverseRate,c=static_cast<float>(std::cos(static_cast<float>(angle))),s=static_cast<float>(std::sin(static_cast<float>(angle)));
            const float poleReal=(a2+1.0f)*c+a1,poleImag=-((a2-1.0f)*s);
            const float poleReal2=static_cast<float>(std::pow(static_cast<float>(poleReal),2.0f));
            const float poleImag2=static_cast<float>(std::pow(static_cast<float>(poleImag),2.0f));
            const float zeroReal=(z2+1.0f)*c+z1,zeroImag=-((z2-1.0f)*s);
            const float zeroReal2=static_cast<float>(std::pow(static_cast<float>(zeroReal),2.0f));
            const float zeroImag2=static_cast<float>(std::pow(static_cast<float>(zeroImag),2.0f));
            b0=(std::sqrt(poleImag2+poleReal2)*amplitude)/std::sqrt(zeroImag2+zeroReal2);
            b1=b0*z1;b2=b0*z2;
        }
        float process(float input,int channel) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(levelDial==64)return input;
            auto& state=states[static_cast<size_t>(std::clamp(channel,0,1))];
            float output=input*b0;output+=state.x1*b1;output+=state.x2*b2;output-=state.y1*a1;output-=state.y2*a2;
            state.x2=state.x1;state.x1=input;state.y2=state.y1;state.y1=output;return output;
        }
        std::array<float,8> snapshot() const noexcept {return {frequencyHz,gainDb,quality,b0,b1,b2,a1,a2};}
    private:
        struct State {float x1=0,x2=0,y1=0,y2=0;};
        std::array<State,2> states {};
        int sampleRate=44100;
        float frequencyDial=64,levelDial=64,qDial=0;
        float frequencyHz=1000,gainDb=0,quality=1,b0=1,b1=0,b2=0,a1=0,a2=0;
    };
}
