#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // Ordinary CVCO source-coarse dispatch 0x2955b and tracked OSC2 base 0x3bba0.
    // The two float coarse multiplications are deliberately not replaced by /12.
    struct OscillatorTuning
    {
        // CVCO 0x3bb00 clamps the shifted integer note before reading the
        // float MIDI table, then applies the summed fine controls in double.
        static float oscillator1(int note,int keyShift,float fine,float masterFine) noexcept
        {
            const float exponent=(fine+masterFine)*(1.0f/1200.0f);
            return static_cast<float>(std::pow(2.0,static_cast<double>(exponent))
                *static_cast<double>(noteFrequencies()[static_cast<size_t>(std::clamp(note+keyShift,0,128))]));
        }
        static void prepare() noexcept { (void)noteFrequencies(); }
        static float coarseExponent(float semitones) noexcept
        {
            const float scaled=semitones*5.0f;
            return scaled*(1.0f/60.0f);
        }
        static double trackedRatio(float semitones,float cents) noexcept
        {
            const float fine=cents*(1.0f/1200.0f);
            const float exponent=fine+coarseExponent(semitones);
            return std::pow(2.0,static_cast<double>(exponent));
        }
        static float trackedOsc2(float oscillator1,float semitones,float cents) noexcept
        {
            return static_cast<float>(trackedRatio(semitones,cents)*static_cast<double>(oscillator1));
        }
        static float fixedOsc2(float semitones,float cents,float masterFine,float unisonFine=0) noexcept
        {
            // The track-off branch sums all fine controls before its one
            // exponentiation; there is no rounded, separately tuned220Hz base.
            float fine=masterFine+cents;fine+=unisonFine;
            const float exponent=fine*(1.0f/1200.0f)+coarseExponent(semitones);
            return static_cast<float>(std::pow(2.0,static_cast<double>(exponent))*220.0);
        }
    private:
        static const std::array<float,129>& noteFrequencies() noexcept
        {
            static const auto values=[] {
                std::array<float,129> result{};
                for(int note=0;note<128;++note)
                    result[static_cast<size_t>(note)]=static_cast<float>(440.0*std::pow(2.0,(note-69)/12.0));
                // Native clamps to128 despite constructing only128 MIDI entries;
                // that address reads the adjacent initialized0.5f table word.
                result[128]=0.5f;
                return result;
            }();
            return values;
        }
    };
}
