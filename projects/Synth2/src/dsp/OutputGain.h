#pragma once
#include <cmath>

namespace synth2::dsp
{
    // CSynth output stage, Win64 RVAs 0x28150 / 0x28480. The recurrence
    // advances once per interleaved sample, so right follows left by one step.
    class OutputGain
    {
    public:
        static constexpr float defaultTarget=0x1.a696eep-1f;
        void prepare (float sampleRate, bool preserveCurrent = false) noexcept
        {
            coefficient = static_cast<float> (1.0 - std::pow (0.5,
                1.0 / static_cast<float> (static_cast<float> (sampleRate) * 0.002f)));
            if (!preserveCurrent) current = 0.0f;
        }
        float next (float target) noexcept
        {
            current += (target - current) * coefficient;
            return current;
        }
        void finish (float target) noexcept
        {
            if (std::abs (target - current) < 1.0f / 65536.0f) current = target;
        }
        void silence (float target) noexcept { current = target; }
        float value() const noexcept { return current; }
    private:
        float current { 0.0f }, coefficient { 0.0f };
    };
}
