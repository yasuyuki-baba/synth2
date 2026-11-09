#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // Win64 CEq one-pole tone stage: 0x2a5c0, 0xd864, 0xdde0/0xda80.
    class OutputTone
    {
    public:
        void prepare (double rate, bool preserveState = false) noexcept
        {
            sampleRate = static_cast<int> (rate);
            if (!preserveState) { x = {}; y = {}; }
            setFractionalDial(dial);
        }
        void setDial (int value) noexcept
        {
            dial = std::clamp (value,0,127);
            const bool high = dial > 64;
            const double exponent = static_cast<double> (high ? dial-64 : dial) * (1.0/64.0);
            frequency = static_cast<double> (std::pow (high ? 800.0 : 110.0,static_cast<double> (exponent)) * (high ? 10.0 : 200.0));
            bypass = high ? frequency <= 1.0 : frequency >= sampleRate/2;
            const double rate2 = static_cast<double> (sampleRate*2.0);
            const double omega = static_cast<double> (static_cast<double> (frequency*2.0) * 3.141592653589793);
            const double inverse = static_cast<double> (1.0/static_cast<double> (rate2+omega));
            b = (high ? rate2 : omega) * inverse;
            feedback = (rate2-omega) * inverse;
            b0=bypass ? 1.0 : b;
            b1=bypass ? 0.0 : high ? -b : b;
        }
        void setFractionalDial(double value) noexcept
        {
            value=std::clamp(value,0.0,127.0);
            const int low=static_cast<int>(value);
            if(value==low){setDial(low);return;}
            OutputTone lower,upper;
            lower.sampleRate=upper.sampleRate=sampleRate;
            lower.setDial(low);upper.setDial(low+1);
            const double fraction=value-low;
            auto mix=[&](double a,double b){return a+(b-a)*fraction;};
            b0=mix(lower.b0,upper.b0);
            b1=mix(lower.b1,upper.b1);
            b=mix(lower.b,upper.b);
            feedback=mix(lower.bypass ? 0.0 : lower.feedback,upper.bypass ? 0.0 : upper.feedback);
            frequency=mix(lower.frequency,upper.frequency);
            bypass=lower.bypass && upper.bypass;
            dial=value;
        }
        double process (double input,int channel) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if (bypass) return input;
            const auto c = static_cast<size_t> (std::clamp(channel,0,1));
            const double output=input*b0+x[c]*b1+y[c]*feedback;
            x[c]=input; y[c]=output; return output;
        }
        double getB() const noexcept { return b; }
        double getFeedback() const noexcept { return feedback; }
        double getFrequency() const noexcept { return frequency; }
        bool isBypassed() const noexcept {return bypass;}
    private:
        int sampleRate {44100};
        double dial {64};
        bool bypass {false};
        double frequency {},b {},feedback {};
        double b0 {},b1 {};
        std::array<double,2> x {},y {};
    };
}
