#pragma once

#include "audio/AudioTypes.h"

namespace synth2::dsp
{
    /**
        Mixer: Combines signals from Oscillator and Noise before the Filter.
    */
    class Mixer
    {
    public:
        void prepare (const ::synth2::audio::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
        }

        void reset() {}

        /**
            Mixes oscillator signal with noise based on noise balance.
        */
        float mix (float oscSignal, float noiseSignal, float noiseLevel)
        {
            // Simple linear mix for now
            return oscSignal * (1.0f - noiseLevel) + noiseSignal * noiseLevel;
        }

    private:
        double sampleRate { 44100.0 };
    };
}
