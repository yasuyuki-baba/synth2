#pragma once
#include "OscillatorTuning.h"
#include "Unison.h"

namespace synth2::dsp
{
    struct VoiceTuningSettings
    {
        bool secondTracksKeyboard {};
        float octave {},keyShift {},fine {},secondPitch {},secondFine {};
        int unisonCount {1};
        float unisonDetuneDial {};
        int unisonPitchDial {24};
    };
    // One compatible note/coarse/fine calculation for every source. Running
    // pitch controls and physical coarse history are composed by the caller.
    class VoiceTuning
    {
    public:
        struct Core { float first {},second {}; };
        VoiceTuning(const VoiceTuningSettings& value,int note) : config(value),midiNote(note)
        {
            secondExponent=OscillatorTuning::trackedExponent(config.secondPitch,config.secondFine);
            config.unisonCount=std::clamp(config.unisonCount,1,8);
            unison=Unison::layout(config.unisonCount,config.unisonDetuneDial,config.unisonPitchDial);
        }
        int unisonCount() const noexcept {return config.unisonCount;}
        Core prepareCore(float fine) const
        {
            Core result;
            result.first=OscillatorTuning::oscillator1(midiNote,
                static_cast<int>(config.octave*12+config.keyShift),fine,config.fine);
            result.second=config.secondTracksKeyboard
                ? scaledExp2(secondExponent,result.first)
                : OscillatorTuning::fixedOsc2(config.secondPitch,config.secondFine,config.fine,fine);
            return result;
        }
        float coarseSecond(const Core& value,float exponent,float unisonFine) const
        {
            float fine=config.secondFine;
            float base=value.first;
            if(!config.secondTracksKeyboard){fine=config.fine+fine;fine+=unisonFine;base=220.0f;}
            return OscillatorTuning::coarseFrequency(base,fine,exponent);
        }
        Unison unison;
    private:
        VoiceTuningSettings config;
        int midiNote;
        float secondExponent {};
    };
}
