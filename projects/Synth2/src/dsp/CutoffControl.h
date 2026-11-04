#pragma once

#include <algorithm>
#include <cmath>
#include "CompatibleControlCurve.h"

namespace synth2::dsp
{
    // CVcf control dispatch, Win64 RVA 0x3ad50. SynthVoice selects this with
    // the native filter envelope on the verified single-core routing domain.
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

        static double target(double base,double keyboard,double offset,double envelope,
                             double modulation1,double modulation2) noexcept
        {
            const double combined=base+keyboard+offset+envelope;
            const double modulation=modulation1+modulation2;
            return compatibleControlCurve(combined,[&](int position){
                return compatibleControlCurve(modulation,[&](int amount){
                    const double product=static_cast<double>(amount>0 ? 1024-position : position)*amount;
                    // Quantization belongs to the compatible transfer-curve knots,
                    // not to signal transport or storage.
                    return std::clamp(position+std::trunc(product/1024.0),0.0,1023.0);
                });
            });
        }

        // A segment uses its starting coefficient throughout. Native
        // elapsed advances by the segment length; target changes alone do not
        // restart the clock. Preserve the caller's callback boundaries.
        template <typename Segment>
        void render (double desired,int frames,Segment&& segment) noexcept
        {
            while (frames>0)
            {
                int count = frames;
                if (desired!=current && elapsed<horizon)
                {
                    const double distance = desired-anchor;
                    current=anchor+compatibleControlCurve(distance,[&](int delta){
                        return std::trunc(static_cast<double>(delta)*elapsed/horizon);
                    });
                    count=std::min(frames,std::max(1,static_cast<int>(horizon/std::max(1.0,std::abs(distance)))));
                }
                else current = desired;
                segment(current,count);
                elapsed += count;
                frames -= count;
            }
        }

        double getCurrent() const noexcept { return current; }
        int getElapsed() const noexcept { return elapsed; }
        double getAnchor() const noexcept { return anchor; }

    private:
        int horizon {2205},elapsed {2205};
        double current {},anchor {};
    };
}
