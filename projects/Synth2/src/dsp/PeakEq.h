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
        void prepare(double rate,bool preserve=false) noexcept
        {
            sampleRate=static_cast<int>(rate);
            if(!preserve)states={};
            setDials(frequencyDial,levelDial,qDial);
        }
        void reset() noexcept {states={};}
        void setDials(double frequency,double level,double q) noexcept
        {
            frequencyDial=std::clamp(frequency,0.0,127.0);levelDial=std::clamp(level,0.0,127.0);qDial=std::clamp(q,0.0,127.0);
            const double inverse127=1.0/127.0;
            frequencyHz=static_cast<double>(std::pow(320.0,static_cast<double>(frequencyDial*inverse127))*50.0);
            gainDb=static_cast<double>(levelDial-64)*inverse127*50.0;
            quality=static_cast<double>(std::pow(10.0,static_cast<double>(qDial*inverse127)));
            const double amplitude=static_cast<double>(std::pow(10.0,static_cast<double>(gainDb*.05)));
            const double omega=static_cast<double>(static_cast<double>(frequencyHz)*6.283185307179586);
            const double inverseRate=1.0/static_cast<double>(sampleRate);
            double pole,zero;
            if(amplitude>=1.0){pole=1.0/(quality*2.0);zero=pole*amplitude;}
            else {zero=1.0/(quality*2.0);pole=zero/amplitude;}
            // The DLL stores float(e) in a double constant, rather than using exp.
            constexpr double exponentialBase=2.7182817459106445;
            const double rz=static_cast<double>(std::pow(exponentialBase,static_cast<double>(-((inverseRate*omega)*zero))));
            const double rp=static_cast<double>(std::pow(exponentialBase,static_cast<double>(-((omega*inverseRate)*pole))));
            auto dampedAngular=[&](double damping,double radius,double decay) {
                const double square=static_cast<double>(std::pow(static_cast<double>(damping),2.0));
                double argument=std::sqrt(std::abs(1.0-square));argument*=inverseRate;argument*=omega;
                const double angular=std::abs(damping)>1.0 ? static_cast<double>(std::cosh(static_cast<double>(argument))) : static_cast<double>(std::cos(static_cast<double>(argument)));
                if(std::isfinite(angular))return angular*(radius*-2.0);
                // At low rates an overdamped section can form 0 * inf. Evaluate
                // -2 * base^decay * cosh(argument) as a sum of decaying exponentials.
                // Keep the factored expression whenever its intermediate is finite.
                const double exponent=static_cast<double>(decay)*std::log(exponentialBase);
                return static_cast<double>(-(std::exp(exponent+static_cast<double>(argument))
                    +std::exp(exponent-static_cast<double>(argument))));
            };
            const double z1=dampedAngular(zero,rz,-((inverseRate*omega)*zero)),z2=static_cast<double>(std::pow(static_cast<double>(rz),2.0));
            a1=dampedAngular(pole,rp,-((omega*inverseRate)*pole));a2=static_cast<double>(std::pow(static_cast<double>(rp),2.0));
            const double angle=omega*inverseRate,c=static_cast<double>(std::cos(static_cast<double>(angle))),s=static_cast<double>(std::sin(static_cast<double>(angle)));
            const double poleReal=(a2+1.0)*c+a1,poleImag=-((a2-1.0)*s);
            const double poleReal2=static_cast<double>(std::pow(static_cast<double>(poleReal),2.0));
            const double poleImag2=static_cast<double>(std::pow(static_cast<double>(poleImag),2.0));
            const double zeroReal=(z2+1.0)*c+z1,zeroImag=-((z2-1.0)*s);
            const double zeroReal2=static_cast<double>(std::pow(static_cast<double>(zeroReal),2.0));
            const double zeroImag2=static_cast<double>(std::pow(static_cast<double>(zeroImag),2.0));
            b0=(std::sqrt(poleImag2+poleReal2)*amplitude)/std::sqrt(zeroImag2+zeroReal2);
            b1=b0*z1;b2=b0*z2;
        }
        double process(double input,int channel) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(levelDial==64)return input;
            auto& state=states[static_cast<size_t>(std::clamp(channel,0,1))];
            double output=input*b0;output+=state.x1*b1;output+=state.x2*b2;output-=state.y1*a1;output-=state.y2*a2;
            state.x2=state.x1;state.x1=input;state.y2=state.y1;state.y1=output;return output;
        }
        std::array<double,8> snapshot() const noexcept {return {frequencyHz,gainDb,quality,b0,b1,b2,a1,a2};}
    private:
        struct State {double x1=0,x2=0,y1=0,y2=0;};
        std::array<State,2> states {};
        int sampleRate=44100;
        double frequencyDial=64,levelDial=64,qDial=0;
        double frequencyHz=1000,gainDb=0,quality=1,b0=1,b1=0,b2=0,a1=0,a2=0;
    };
}
