#pragma once

#include "CompatibleControlCurve.h"
#include "EnvelopeCurve.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace synth2::dsp
{
    // Win64 CEnvelopedObject release control (RVA 0xccc0, resolution 0xc780).
    // Tracks the amplitude envelope's real target and callback lifetime;
    // audio gain interpolation remains the voice's existing envelope path.
    class ReleaseRamp
    {
    public:
        static int durationFrames (double seconds,double sampleRate) noexcept
        {
            // Frame counts retain their compatible source-word rounding.
            const double frames = static_cast<float>(static_cast<float>(seconds)*static_cast<float>(sampleRate));
            if (!std::isfinite (frames) || frames <= 0) return 0;
            if (frames >= static_cast<double> (std::numeric_limits<int>::max()))
                return std::numeric_limits<int>::max();
            return static_cast<int> (frames);
        }

        void start (int durationFrames, double initialLevel) noexcept
        {
            duration = std::max (0, durationFrames);
            initial = level = std::clamp (initialLevel, 0.0, 2048.0);
            counter = boundary = pending = 0;
            active = true;
            updateResolution();
        }

        void setDuration (int frames) noexcept
        {
            frames = std::max (0,frames);
            if (active && frames != duration)
            {
                counter = duration > 0 ? static_cast<int> (static_cast<int64_t> (counter)*frames/duration) : 0;
                pending=0;
            }
            duration = frames;
        }

        void reset() noexcept { active = false;pending=0; }
        int segmentHorizon() const noexcept { return pending; }
        bool isActive() const noexcept { return active; }
        int getCounter() const noexcept { return counter; }
        double getLevel() const noexcept { return level; }

        void advance (int frames) noexcept
        {
            advance (frames, [] (double,int) {});
        }

        template <typename Segment>
        void advance (int frames,Segment&& segment,int horizon = 0) noexcept
        {
            if (!active) return;
            while (frames > 0 && counter < duration && (level > 0 || pending > 0))
            {
                if(pending==0)
                {
                    pending=std::min({std::max(frames,horizon),step,duration-counter});
                    level = compatibleControlCurve(initial,[&](int start) {
                        return exponentialEnvelopeKnot(start,duration-counter-pending,duration);
                    });
                }
                const int count=std::min(frames,pending);
                segment (level,count);
                pending-=count;
                horizon=std::max(0,horizon-count);
                counter += count;
                frames -= count;
                if (pending==0 && counter >= boundary) updateResolution();
            }
            if (pending==0 && (counter >= duration || level <= 0
                || counter >= static_cast<int> (static_cast<float> (duration)*0.9f)))
            {
                level = 0;
                active = false;
            }
        }

    private:
        void updateResolution() noexcept
        {
            // Advance from the previous boundary, even when a callback has
            // overshot it. The DLL reads +0x68 here, not counter at +0x60.
            if      (boundary >= duration/2)  boundary = duration;
            else if (boundary >= duration/3)  boundary = duration/2;
            else if (boundary >= duration/4)  boundary = duration/3;
            else if (boundary >= duration/10) boundary = duration/4;
            else if (boundary >= duration/20) boundary = duration/10;
            else                            boundary = duration/20;
            // Native amplitude flag at +0x34 overrides the level-based step.
            step = std::max (1,boundary/2);
        }

        int duration {},counter {},boundary {},step {1},pending {};
        double initial {},level {};
        bool active {};
    };
}
