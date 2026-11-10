#pragma once

#include <algorithm>
#include <cmath>
#include "CompatibleControlCurve.h"

namespace synth2::dsp
{
    // Common CVcf control dispatch, reconstructed from Win64 RVA 0x3ad50.
    class CutoffControl
    {
    public:
        void prepare (int rate) noexcept
        {
            horizon = std::max(1,rate/20);
            current = anchor = 0;
            elapsed = horizon;
        }

        void setSampleRate (int rate) noexcept { horizon = std::max(1,rate/20); }

        // Native cutoff/amount/tracking/sustain setters restart the same clock.
        void restart() noexcept { anchor = current; elapsed = 0; }
        // Native reset/note-on skips the live-edit slew on the next dispatch.
        void settle() noexcept { elapsed = horizon; }

        static float target(float base,float keyboard,float offset,float envelope,
                             float modulation1,float modulation2) noexcept
        {
            const float combined=base+keyboard+offset+envelope;
            const float modulation=modulation1+modulation2;
            return compatibleControlCurve(combined,[&](int position){
                return compatibleControlCurve(modulation,[&](int amount){
                    const float product=static_cast<float>(amount>0 ? 1024-position : position)*amount;
                    // Quantization belongs to the compatible transfer-curve knots,
                    // not to signal transport or storage.
                    return std::clamp(position+std::trunc(product/1024.0f),0.0f,1023.0f);
                });
            });
        }

        // A segment uses its starting coefficient throughout. Native
        // elapsed advances by the segment length; target changes alone do not
        // restart the clock. Preserve the caller's callback boundaries.
        template <typename Segment>
        void render (float desired,int frames,Segment&& segment) noexcept
        {
            while (frames>0)
            {
                int count = frames;
                if (desired!=current && elapsed<horizon)
                {
                    const float distance = desired-anchor;
                    current=anchor+compatibleControlCurve(distance,[&](int delta){
                        return std::trunc(static_cast<float>(delta)*elapsed/horizon);
                    });
                    count=std::min(frames,std::max(1,static_cast<int>(horizon/std::max(1.0f,std::abs(distance)))));
                }
                else current = desired;
                segment(current,count);
                elapsed += count;
                frames -= count;
            }
        }

        float getCurrent() const noexcept { return current; }
        int getElapsed() const noexcept { return elapsed; }
        float getAnchor() const noexcept { return anchor; }

    private:
        int horizon {2205},elapsed {2205};
        float current {},anchor {};
    };
}
