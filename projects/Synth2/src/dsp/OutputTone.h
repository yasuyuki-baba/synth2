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
        void prepare (float rate, bool preserveState = false) noexcept
        {
            sampleRate = static_cast<int> (rate);
            if (!preserveState) { x = {}; y = {}; }
            setFractionalDial(dial);
        }
        void setDial (int value) noexcept
        {
            dial = std::clamp (value,0,127);
            const bool high = dial > 64;
            const float exponent = static_cast<float> (high ? dial-64 : dial) * (1.0f/64.0f);
            frequency = static_cast<float> (std::pow (high ? 800.0 : 110.0,static_cast<double> (exponent)) * (high ? 10.0 : 200.0));
            bypass = high ? frequency <= 1.0f : frequency >= sampleRate/2;
            const float rate2 = static_cast<float> (sampleRate*2.0);
            const float omega = static_cast<float> (static_cast<double> (frequency*2.0f) * 3.141592653589793);
            const float inverse = static_cast<float> (1.0/static_cast<double> (rate2+omega));
            b = (high ? rate2 : omega) * inverse;
            feedback = (rate2-omega) * inverse;
            b0=bypass ? 1.0f : b;
            b1=bypass ? 0.0f : high ? -b : b;
        }
        void setFractionalDial(float value) noexcept
        {
            value=std::clamp(value,0.0f,127.0f);
            const int low=static_cast<int>(value);
            if(value==low){setDial(low);return;}
            OutputTone lower,upper;
            lower.sampleRate=upper.sampleRate=sampleRate;
            lower.setDial(low);upper.setDial(low+1);
            const float fraction=value-low;
            auto mix=[&](float a,float b){return a+(b-a)*fraction;};
            b0=mix(lower.b0,upper.b0);
            b1=mix(lower.b1,upper.b1);
            b=mix(lower.b,upper.b);
            feedback=mix(lower.bypass ? 0.0f : lower.feedback,upper.bypass ? 0.0f : upper.feedback);
            frequency=mix(lower.frequency,upper.frequency);
            bypass=lower.bypass && upper.bypass;
            dial=value;
        }
        float process (float input,int channel) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if (bypass) return input;
            const auto c = static_cast<size_t> (std::clamp(channel,0,1));
            const float output=input*b0+x[c]*b1+y[c]*feedback;
            x[c]=input; y[c]=output; return output;
        }
        float getB() const noexcept { return b; }
        float getFeedback() const noexcept { return feedback; }
        float getFrequency() const noexcept { return frequency; }
        bool isBypassed() const noexcept {return bypass;}
    private:
        int sampleRate {44100};
        float dial {64};
        bool bypass {false};
        float frequency {},b {},feedback {};
        float b0 {},b1 {};
        std::array<float,2> x {},y {};
    };
}
