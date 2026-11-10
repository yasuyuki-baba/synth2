#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "CompatibleControlCurve.h"
#include "FmTables.h"
namespace synth2::dsp
{
    // CVco dispatch0x3bf50 and the kernels'32-frame coefficient updates.
    // Preserve receiver calls even when consecutive envelope targets are equal.
    struct FrequencyModulation
    {
        float current=0,target=-1,anchor=0;
        int remaining=0,duration=0,elapsed=0;
        bool settle=true;
        // CVco note-on0x3db00 invalidates the target without settling a live slew.
        void noteOn() noexcept { target=-1;elapsed=0; }
        // Native base-dial setter RVA3e050 starts a rate/10 slew horizon.
        void setBase(int rate) noexcept {target=-1;elapsed=rate/10;}
        static void prepare() noexcept { (void)slewTable(); }
        static int depth(int dial,int control) noexcept
        {
            float value=static_cast<float>(dial);
            if(control>0)value+=static_cast<float>(std::min(control,512))*static_cast<float>(127-dial)*(1.0f/512.0f);
            else value-=static_cast<float>(std::min(-control,512))*value*(1.0f/512.0f);
            // Integer source/control knots land exactly on this 1/512 dial
            // grid. Retain their calibrated integer phase words in one table.
            return fmDepthWords[static_cast<size_t>(value*512.0f)];
        }
        static float depth(float dial,float control) noexcept
        {
            dial=std::clamp(dial,0.0f,127.0f);control=std::clamp(control,-512.0f,512.0f);
            return compatibleControlCurve(dial,[&](int source){
                return compatibleControlCurve(control,[&](int modulation){return depth(source,modulation);});
            });
        }
        template<class Segment> int render(float desired,int frames,int rate,Segment&& segment,bool oneRegion=false,bool advanceSlew=true) noexcept
        {
            const int requested=frames;
            while(frames>0)
            {
                if(target!=desired)
                {
                    if(settle || std::abs(current-desired)>67108864){settle=false;duration=0;current=desired;}
                    else if(duration<=elapsed){duration=remaining=elapsed;anchor=current;}
                    if(remaining==0)current=desired;
                    elapsed=0;target=desired;
                }
                int count=current!=target ? std::min(frames,remaining) : frames;
                if(count<=0){current=target;count=frames;}
                // Dispatch advances this clock by the requested span before
                // shortening the kernel call to its remaining slew length.
                elapsed=std::min(elapsed+frames,rate/1000);
                const bool slewing=current!=target;
                for(int i=0;i<count;++i)
                {
                    segment(current);
                    // Ring dispatch retains the receiver clock and target, but its
                    // audio kernel does not consume the FM slew.
                    if(advanceSlew && slewing && remaining>0)
                    {
                        if((remaining&31)==0)
                        {
                            const int index=(remaining<<8)/duration;
                            const float weight=slewTable()[static_cast<size_t>(index)];
                            // Keep the measured slew knots at whole phase-unit deltas.
                            // Interpolate fractional deltas without narrowing the state.
                            current=anchor+compatibleControlCurve(target-anchor,[&](int delta){
                                return std::trunc(static_cast<float>(static_cast<float>(delta))*weight);
                            });
                        }
                        --remaining;
                    }
                }
                if(slewing && remaining==0)current=target;
                frames-=count;
                if(oneRegion)break;
            }
            return requested-frames;
        }
    private:
        static const std::array<float,257>& slewTable() noexcept
        {
            // Native init0x3d8c4 fills256 entries with divisor257. The kernel
            // can also read the following zero word at index256.
            static const auto values=[] {
                std::array<float,257> result{};
                for(int i=0;i<256;++i)
                    result[static_cast<size_t>(i)]=(std::cos((static_cast<float>(i)*3.141592653589793f)*(1.0f/257.0f))+1.0f)*0.5f;
                return result;
            }();
            return values;
        }
    };
}
