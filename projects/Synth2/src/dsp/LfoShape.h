#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include "LfoRange.h"
#include "OscillatorConstants.h"

namespace synth2::dsp
{
    // CLFO sine amplitude/pan/pitch integer controls, Win64 RVA0x14d10/0x15080.
    // Clock/frequency slew is deliberately separate from waveform evaluation.
    struct LfoShape
    {
        static void prepare() noexcept { (void)sine(); }
        static int amplitudeDepth(int dial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            return static_cast<int>(unit*2048.0f);
        }
        static int amplitude(int depth,int phaseIndex) noexcept
        {
            const float sample=sine()[static_cast<size_t>(phaseIndex)&2047u];
            float value=static_cast<float>(depth)*sample;
            value+=static_cast<float>(depth);
            return depth-static_cast<int>(value)/2;
        }
        static int panDepth(int dial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            return static_cast<int>(unit*64.0f);
        }
        static int pulseWidthDepth(int dial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            return static_cast<int>(unit*128.0f);
        }
        static int filterDepth(int dial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            return static_cast<int>(unit*1024.0f);
        }
        static int unipolarControl(int depth,float sample) noexcept
        {
            float value=static_cast<float>(depth)*sample;
            value+=static_cast<float>(depth);
            return static_cast<int>(value)/2;
        }
        static int filter(int depth,int phaseIndex) noexcept
        {
            return depth-amplitude(depth,phaseIndex);
        }
        static std::pair<int,int> triangleFilter(int depth,int counter,int period,int minimumStep) noexcept
        {
            if(depth==0)return {0,-1};
            if(period<2)return {0,minimumStep};
            const int half=period/2;
            int value,next;
            if(counter<half)
            {
                value=static_cast<int>(static_cast<int64_t>(depth)*counter/half);
                next=period>depth ? static_cast<int>((static_cast<int64_t>(value+1)*half+depth-1)/depth) : counter+1;
            }
            else
            {
                const int falling=period-half,position=counter-half;
                value=depth-static_cast<int>(static_cast<int64_t>(depth)*position/falling);
                next=falling>depth ? half+static_cast<int>((static_cast<int64_t>(depth-value+1)*falling+depth-1)/depth) : counter+1;
            }
            return {value,std::max(minimumStep,std::min(period,next)-counter)};
        }
        static std::pair<int,int> triangleAmplitude(int depth,int counter,int period,int minimumStep) noexcept
        {
            auto query=triangleFilter(depth,counter,period,minimumStep);
            query.first=depth-query.first;
            return query;
        }
        // CLFO square query0x14e81: the second receiver endpoint comes first,
        // including reversed amplitude ranges. Return the held-control span.
        static std::pair<int,int> square(int first,int second,int counter,int period,int minimumStep) noexcept
        {
            if(first==second)return {second,-1};
            const int half=period/2;
            return {counter<half ? second : first,std::max(minimumStep,(counter<half ? half : period)-counter)};
        }
        static std::pair<int,int> squareFilter(int depth,int counter,int period,int minimumStep) noexcept
        {
            if(depth==0)return {0,-1};
            if(period<2)return {0,minimumStep};
            const int half=period/2;
            return {counter<half ? depth : 0,std::max(minimumStep,(counter<half ? half : period)-counter)};
        }
        static std::pair<int,int> squarePitch(int depth,int counter,int period,int minimumStep) noexcept
        {
            if(depth==0)return {0,-1};
            const int half=period/2;
            return {counter<half ? depth : -depth,std::max(minimumStep,(counter<half ? half : period)-counter)};
        }
        // Native saw query0x14ed1: inclusive integer ramp, optional reversed
        // receiver range, and the next held-control boundary. IMUL/ADD/SUB wrap
        // at32 bits, including slow synchronized periods.
        static std::pair<int,int> saw(int first,int second,int counter,int period,int minimumStep) noexcept
        {
            if(first==second)return {second,-1};
            if(period<2)return {second,minimumStep};
            const auto multiply=[](int a,int b){return static_cast<int32_t>(static_cast<uint32_t>(a)*static_cast<uint32_t>(b));};
            const auto add=[](int a,int b){return static_cast<int32_t>(static_cast<uint32_t>(a)+static_cast<uint32_t>(b));};
            const int low=std::min(first,second),high=std::max(first,second),range=high-low;
            int value,next;
            if(period>range+1)
            {
                value=high-multiply(range+1,counter)/period;
                next=add(multiply(high-value+1,period),range)/(range+1);
            }
            else
            {
                value=high-multiply(range,counter)/(period-1);next=counter+1;
            }
            if(first>second)value=low+high-value;
            return {value,std::max(minimumStep,add(std::min(period,next),-counter))};
        }
        static int pitchDepth(int dial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            const float curve=static_cast<float>((std::pow(10.0f,static_cast<float>(unit))-1.0f)*(1.0f/9.0f));
            return static_cast<int>((curve*5.0f)*128.0f);
        }
        static int bipolarControl(int depth,float sample) noexcept
        {
            const float value=static_cast<float>(depth*2)*sample;
            return static_cast<int>(value)/2;
        }
        static int pan(int depth,int phaseIndex) noexcept
        {
            return bipolarControl(depth,sine()[static_cast<size_t>(phaseIndex)&2047u]);
        }
        static int pitch(int depth,int phaseIndex) noexcept
        {
            return pan(depth,phaseIndex);
        }
        // Triangle query at the native retained sample counter, not a sine-table
        // phase bin. The second word is the next constant-control span.
        static std::pair<int,int> trianglePitch(int depth,int counter,int period,int minimumStep) noexcept
        {
            if(depth==0)return {0,-1};
            if(period<2)return {0,minimumStep};
            const int half=period/2,range=depth*2;
            int value,next;
            if(counter<half)
            {
                value=-depth+static_cast<int>(static_cast<int64_t>(range)*counter/half);
                next=period>range ? static_cast<int>((static_cast<int64_t>(value+depth+1)*half+range-1)/range)
                                  : counter+1;
            }
            else
            {
                const int falling=period-half,position=counter-half;
                value=depth-static_cast<int>(static_cast<int64_t>(range)*position/falling);
                next=falling>range ? half+static_cast<int>((static_cast<int64_t>(depth-value+1)*falling+range-1)/range)
                                   : counter+1;
            }
            return {value,std::max(minimumStep,std::min(period,next)-counter)};
        }
        static int step(int rateDial) noexcept
        {
            const float unit=static_cast<float>(std::clamp(rateDial,0,127))*(1.0f/127.0f);
            const float frequency=static_cast<float>(std::pow(1500.0f,static_cast<float>(unit))*(1.0f/12.0f));
            return frequency>10.0f ? 32 : 64;
        }

        // Depth knots retain the compatible scalar response. The two queries
        // borrow one clock; neither owns history or advances a random stream.
        class DepthCurve
        {
            struct Knot
            {
                int destination {},depth {};
                std::pair<int,int> range {};
                std::pair<float,int> query(int wave,int counter,int period,int minimumStep) const noexcept
                {
                    if(wave==2)return saw(range.first,range.second,counter,period,minimumStep);
                    if(wave==3)return square(range.first,range.second,counter,period,minimumStep);
                    if(wave==1)
                    {
                        if(destination==3)return triangleAmplitude(depth,counter,period,minimumStep);
                        if(destination==2 || destination==5)return triangleFilter(depth,counter,period,minimumStep);
                        return trianglePitch(depth,counter,period,minimumStep);
                    }
                    // Preserve the clock's native word wrapping and held span.
                    // Only sine amplitude/pan reads extend the table coordinates.
                    const uint32_t position=static_cast<uint32_t>(counter)<<11;
                    const int phase=static_cast<int>(position/static_cast<uint32_t>(period));
                    const auto atPhase=[&](int index) {
                        return destination==0 || destination==1 || destination==4 ? pitch(depth,index)
                            : destination==2 || destination==5 ? filter(depth,index)
                            : destination==6 ? pan(depth,index) : amplitude(depth,index);
                    };
                    const float first=atPhase(phase);
                    if(wave!=0 || (destination!=3 && destination!=6))
                        return {first,depth==0 ? -1 : minimumStep};
                    const float fraction=static_cast<float>(position%static_cast<uint32_t>(period))/period;
                    const float value=first+(static_cast<float>(atPhase(phase+1))-first)*fraction;
                    return {value,depth==0 ? -1 : minimumStep};
                }
            };
            static Knot knot(int destination,int dial) noexcept
            {
                const auto range=lfoDepthRange(destination+1,dial);
                return {destination,destination==3 ? range.first : range.second,range};
            }
            Knot first,second;
            float fraction {};
        public:
            DepthCurve(int destination,float dial) noexcept
            {
                dial=std::clamp(dial,0.0f,127.0f);
                const int low=static_cast<int>(dial);
                fraction=dial-low;first=knot(destination,low);
                second=fraction==0 ? first : knot(destination,low+1);
            }
            std::pair<float,int> query(int wave,int counter,int period,int minimumStep) const noexcept
            {
                const auto a=first.query(wave,counter,period,minimumStep);
                if(fraction==0)return {a.first,a.second};
                const auto b=second.query(wave,counter,period,minimumStep);
                const int frames=a.second<0 ? b.second : b.second<0 ? a.second : std::min(a.second,b.second);
                return {a.first+(static_cast<float>(b.first)-a.first)*fraction,frames};
            }
        };
    private:
        static const std::array<float,2048>& sine() noexcept
        {
            return oscillatorSine;
        }
    };
}
