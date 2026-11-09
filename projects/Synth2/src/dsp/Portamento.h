#pragma once

#include "FractionalDial.h"
#include "CompatibleControlCurve.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synth2::dsp
{
    // Win64 CVCO 0x3e0c0 / 0x3e3d0 and controller loop 0x3dd20.
    // Pitch offsets use128 units per octave, including endpoint quantization.
    struct Portamento
    {
        double seconds {};
        int duration {},elapsed {},segment {},span {},note {},threshold {};
        double offset {},delta {};

        static double timeSeconds(int dial) noexcept
        {
            dial=std::clamp(dial,0,127);
            const double exponent=(static_cast<double>(dial)*(1.0/127.0)-1.0)*2.5;
            return static_cast<double>(std::exp(static_cast<double>(exponent))
                *static_cast<double>(dial)*(1.0/127.0)*2.5);
        }

        static double timeSeconds(double dial) noexcept
        {return interpolateDial(dial,[](int value){return timeSeconds(value);});}

        void setDuration(double value,int sampleRate) noexcept
        {
            const int previous=duration;
            seconds=value;
            duration=static_cast<int>(static_cast<double>(sampleRate)*value);
            if(previous!=0)
            {
                const auto rescale=[&](int n) {return static_cast<int>(static_cast<int64_t>(n)*duration/previous);};
                elapsed=std::min(duration,rescale(elapsed));
                span=std::max(32,rescale(span));
                threshold=rescale(threshold);
            }
            else elapsed=duration;
        }

        void setNote(int value,bool glide,int transpose=0) noexcept
        {
            const int old=std::clamp(note+transpose,0,128)*128/12;
            note=value;
            const int target=std::clamp(note+transpose,0,128)*128/12;
            elapsed=0;
            if(!glide) {offset=delta=0;return;}
            offset+=old-target;
            delta=offset;
            segment=1;
            span=std::max(32,static_cast<int>(duration/std::max(1.0,std::abs(delta)))/10);
            threshold=duration/10;
        }

        template<class Render>
        void render(int frames,Render&& consume)
        {
            while(frames>0)
            {
                if(elapsed>=duration || delta==0)
                {
                    offset=0;
                    consume(offset,frames);
                    return;
                }
                int index=elapsed/span;
                if(index!=segment)
                {
                    const double t=static_cast<double>(elapsed)/duration;
                    offset=compatibleControlCurve(delta,[&](int distance) {
                        return std::trunc((std::exp(-4.0*t)*(1.0-t))*distance);
                    });
                    if(elapsed>=threshold)
                    {
                        const double magnitude=std::max(1.0,std::abs(delta));
                        const int tenth=duration/10,quarter=duration*25/100,later=duration*55/100;
                        if(threshold>=later) {threshold=duration;span=static_cast<int>(duration*9/magnitude);}
                        else if(threshold>=quarter) {threshold=later;span=static_cast<int>(duration/magnitude);}
                        else if(threshold>=tenth) {threshold=quarter;span=static_cast<int>(duration*2/magnitude)/5;}
                        else {threshold=tenth;span=static_cast<int>(duration/magnitude)/4;}
                        span=std::max(1,std::min(std::max(32,span),threshold-elapsed));
                        index=elapsed/span;
                    }
                    segment=index;
                }
                const int count=std::min(frames,(index+1)*span-elapsed);
                consume(offset,count);
                elapsed+=count;
                frames-=count;
            }
        }
    };
}
