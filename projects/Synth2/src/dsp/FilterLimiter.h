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
            coefficient=std::exp(-1.0/(static_cast<double>(rate)*0.005000000353902578));
            if (!preserveState) reset();
        }
        void reset() noexcept { excess=0; }
        double process (double input) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const double target=std::max(std::abs(input)-1.1,0.0);
            excess=(excess-target)*coefficient+target;
            return (1.1/(excess+1.1))*input;
        }
        void finish() noexcept { if(static_cast<double>(excess)<1e-5) excess=0; }
        double getCoefficient() const noexcept { return coefficient; }
        double value() const noexcept { return excess; }
    private:
        double coefficient {},excess {};
    };
}
