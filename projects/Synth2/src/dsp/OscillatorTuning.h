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
        // float MIDI table, then applies the summed fine controls in Float32.
        static float oscillator1(int note,int keyShift,float fine,float masterFine) noexcept
        {
            const float base=noteFrequencies()[static_cast<size_t>(std::clamp(note+keyShift,0,128))];
            const float exponent=(fine+masterFine)*(1.0f/1200.0f);
            return std::pow(2.0f,exponent)*base;
        }
        static void prepare() noexcept { (void)noteFrequencies(); }
        static float coarseExponent(float semitones) noexcept
        {
            const float scaled=semitones*5.0f;
            return scaled*(1.0f/60.0f);
        }
        static float trackedRatio(float semitones,float cents) noexcept
        {
            const float fine=cents*(1.0f/1200.0f);
            const float exponent=fine+coarseExponent(semitones);
            return std::pow(2.0f,static_cast<float>(exponent));
        }
        static float applyTrackedRatio(float oscillator1,float ratio) noexcept
        {
            return ratio*oscillator1;
        }
        static float trackedOsc2(float oscillator1,float semitones,float cents) noexcept
        {return applyTrackedRatio(oscillator1,trackedRatio(semitones,cents));}
        static float fixedOsc2(float semitones,float cents,float masterFine,float unisonFine=0) noexcept
        {
            // The track-off branch sums all fine controls before its one
            // exponentiation; there is no rounded, separately tuned220Hz base.
            float fine=masterFine+cents;fine+=unisonFine;
            return coarseFrequency(220.0f,fine,coarseExponent(semitones));
        }
        static float coarseFrequency(float base,float fine,float exponent) noexcept
        {
            const float octave=fine*(1.0f/1200.0f)+exponent;
            return std::pow(2.0f,octave)*base;
        }

    private:
        static const std::array<float,129>& noteFrequencies() noexcept
        {
            static const auto values=[] {
                std::array<float,129> result{};
                for(int note=0;note<128;++note)
                    result[static_cast<size_t>(note)]=static_cast<float>(440.0f*std::pow(2.0f,(note-69)/12.0f));
                // Native clamps to128 despite constructing only128 MIDI entries;
                // that address reads the adjacent initialized0.5f table word.
                result[128]=0.5f;
                return result;
            }();
            return values;
        }
    };
}
