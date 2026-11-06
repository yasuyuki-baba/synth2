#pragma once
#include "CompatibleControlCurve.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace synth2::dsp
{
    // Isolated CRT recurrence and CLFO random state, Win64 0x55b98/0x15450.
    // Callers own the shared draw stream; the bounded runtime composes it with loading.
    struct CrtRandom
    {
        uint32_t state {1};
        int next() noexcept
        {
            state=state*214013u+2531011u;
            return static_cast<int>((state>>16)&32767u);
        }
    };

    struct RandomLfo
    {
        double current {},previous {};

        template <typename Random>
        void setWave(int nativeWave,double first,double second,Random& random) noexcept
        {
            if(nativeWave!=3 && nativeWave!=4)return;
            previous=first==second ? first : compatibleControlCurve(first,[&](int a) {
                return compatibleControlCurve(second,[&](int b) {return (a+b)/2;});
            });
            draw(first,second,random);
        }

        template <typename Random>
        void resetPhase(int& counter,int nativeWave,double first,double second,Random& random) noexcept
        {
            counter=0;
            if(nativeWave==3 || nativeWave==4)
            {
                previous=current;
                draw(first,second,random);
            }
        }

        // Stable-period portion of 0x14f60, before the separately recovered slew.
        // A call crossing several periods still draws once; enable is not tested.
        template <typename Random>
        void advance(int samples,int& counter,int period,int nativeWave,double first,double second,Random& random) noexcept
        {
            counter+=samples;
            if(counter<period)return;
            counter%=period;
            if(nativeWave==3 || nativeWave==4)
            {
                previous=current;
                draw(first,second,random);
            }
        }

        // Whole-value knots retain the native held target / inclusive ramp.
        // Fractional state and bounds interpolate those queries and their spans.
        // Domain: positive period, 0<=counter<period; varying ramps require period>=2.
        std::pair<double,int> query(int nativeWave,double first,double second,int counter,int period,int minimumStep) const noexcept
        {
            if(first==second)return {second,-1};
            return compatibleControlQuery(current,[&](int value) {
                return compatibleControlQuery(previous,[&](int prior) {
                    return compatibleControlQuery(first,[&](int a) {
                        return compatibleControlQuery(second,[&](int b) {
                            return integerQuery(value,prior,nativeWave,a,b,counter,period,minimumStep);
                        });
                    });
                });
            });
        }

    private:
        static std::pair<int,int> integerQuery(int current,int previous,int nativeWave,int first,int second,
            int counter,int period,int minimumStep) noexcept
        {
            if(first==second)return {second,-1};
            bool reverse=second<first;
            int low=std::min(first,second),high=std::max(first,second),value=current,next=period;
            if(nativeWave==4 && current!=previous)
            {
                if(previous>current){high=previous;low=current;}
                else {high=current;low=previous;reverse=!reverse;}
                const int range=high-low+1;
                if(period>range)
                {
                    value=high-range*counter/period;
                    next=((high-value+1)*period-low+high)/range;
                }
                else
                {
                    value=high-(range-1)*counter/(period-1);
                    next=counter+1;
                }
            }
            if(reverse)value=low+high-value;
            return {value,std::max(minimumStep,std::min(period,next)-counter)};
        }

        template <typename Random>
        void draw(double first,double second,Random& random) noexcept
        {
            if(first==second){current=second;return;}
            const int word=random.next();
            current=compatibleControlCurve(first,[&](int a) {
                return compatibleControlCurve(second,[&](int b) {
                    const int low=std::min(a,b),high=std::max(a,b);
                    return low==high ? b : low+word%(high-low);
                });
            });
        }
    };
}
