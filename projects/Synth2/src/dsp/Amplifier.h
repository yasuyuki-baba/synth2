#pragma once

#include "audio/AudioTypes.h"
#include "OutputGain.h"
#include "StereoBalance.h"

namespace synth2::dsp
{
    /**
        Amplifier: Voltage Controlled Amplifier module.
        Handles per-voice volume level, panning, and final gain.
    */
    class Amplifier
    {
    public:
        void prepare (const ::synth2::audio::ProcessSpec& spec)
        {
            numChannels = spec.numChannels;
        }

        void reset() {}

        /**
            Processes a single mono sample into stereo output based on gain and pan.
        */
        void processSample (float input, float gain, float pan, float& outL, float& outR)
        {
            // Linear gain for now, can be changed to exponential if needed
            float level = input * gain;
            
            // Original-DLL p90 sweeps use balance: both channels at full level
            // in the centre, fading only the opposite channel toward an edge.
            const auto balance=StereoBalance::gains(pan);
            
            // Measured unity-gain, zero-effect sine reference at default native
            // master level. It does not model the original master-volume knob.
            constexpr float outputTrim = OutputGain::defaultTarget;
            outL = level * balance.left * outputTrim;
            outR = level * balance.right * outputTrim;
        }

    private:
        uint32_t numChannels { 2 };
    };
}
