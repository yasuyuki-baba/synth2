#pragma once
#include <algorithm>

namespace synth2::dsp
{
    // Root pitch slew RVA28300: one 7-bit step per renderer region (max32
    // samples), followed by signed integer conversion to128 units/octave.
    struct PitchBendSmoother
    {
        int current {},target {},offset {};
        bool active {};
        bool moving() const noexcept {return active;}
        void set(int value) noexcept {target=std::clamp((value-8192)/128,-63,63);active=true;}
        int advance(int range) noexcept
        {
            current+=current<target ? 1 : current>target ? -1 : 0;
            offset=(current*range*128)/63/12;
            active=current!=target;
            return offset;
        }
    };
}
