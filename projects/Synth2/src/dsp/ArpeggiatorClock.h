#pragma once
#include <algorithm>
#include <cstdint>

namespace synth2::dsp
{
    struct ArpeggiatorClock
    {
        // Native 24-ticks-per-quarter table and single-precision division.
        // Caller supplies the bounded positive rate/tempo domain.
        static int period(int rate,float tempo,int beat) noexcept
        {
            constexpr int ticks[]{96,84,72,48,42,36,32,24,21,18,16,12,9,8,6,4,3,2,1};
            const int numerator=rate*ticks[std::clamp(beat,0,18)]*60;
            return static_cast<int>(float(numerator)/(tempo*24.0f));
        }
        static float gateAmount(int gate) noexcept {return float(gate)*(1.0f/127.0f);}
        static int gateFrames(int period,int gate) noexcept
        {
            return static_cast<int>(float(period)*gateAmount(gate));
        }
        static int span(int period,int gate,int counter) noexcept
        {
            if(counter==0)return 0;
            const int on=gateFrames(period,gate);
            return counter<on ? on-counter : counter==on ? 0 : period-counter;
        }
    };
}
