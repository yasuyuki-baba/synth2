#pragma once
#include "FloatExp2.h"
#include "OscillatorConstants.h"
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
            return scaledExp2(exponent,base);
        }
        static void prepare() noexcept { (void)noteFrequencies(); }
        static float coarseExponent(float semitones) noexcept
        {
            const float scaled=semitones*5.0f;
            return scaled*(1.0f/60.0f);
        }
        static float trackedExponent(float semitones,float cents) noexcept
        {return cents*(1.0f/1200.0f)+coarseExponent(semitones);}
        static float trackedOsc2(float oscillator1,float semitones,float cents) noexcept
        {return scaledExp2(trackedExponent(semitones,cents),oscillator1);}
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
            return scaledExp2(octave,base);
        }

    private:
        static const std::array<float,129>& noteFrequencies() noexcept
        {
            return midiFrequencyWords;
        }
    };
}
