#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // CVcf's independent post-filter gain controller, Win64 RVA 0x3af70.
    class FilterLimiter
    {
    public:
        void prepare (double rate, bool preserveState = false) noexcept
        {
            coefficient=std::exp(-1.0f/(static_cast<float>(rate)*0.005000000353902578f));
            if (!preserveState) reset();
        }
        void reset() noexcept { excess=0; }
        float process (float input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const float target=std::max(std::abs(input)-1.1f,0.0f);
            excess=(excess-target)*coefficient+target;
            return (1.1f/(excess+1.1f))*input;
        }
        void finish() noexcept { if(static_cast<double>(excess)<1e-5) excess=0; }
        float getCoefficient() const noexcept { return coefficient; }
        float value() const noexcept { return excess; }
    private:
        float coefficient {},excess {};
    };
}
