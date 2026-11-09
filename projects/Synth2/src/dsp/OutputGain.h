#pragma once
#include <cmath>

namespace synth2::dsp
{
    // CSynth output stage, Win64 RVAs 0x28150 / 0x28480. The recurrence
    // advances once per interleaved sample, so right follows left by one step.
    class OutputGain
    {
    public:
        static constexpr double defaultTarget=0x1.a696eep-1;
        void prepare (double sampleRate, bool preserveCurrent = false) noexcept
        {
            coefficient = static_cast<double> (1.0 - std::pow (0.5,
                1.0 / static_cast<double> (static_cast<double> (sampleRate) * 0.002)));
            if (!preserveCurrent) current = 0.0;
        }
        double next (double target) noexcept
        {
            current += (target - current) * coefficient;
            return current;
        }
        void finish (double target) noexcept
        {
            if (std::abs (target - current) < 1.0 / 65536.0) current = target;
        }
        void silence (double target) noexcept { current = target; }
        double value() const noexcept { return current; }
    private:
        double current { 0.0 }, coefficient { 0.0 };
    };
}
