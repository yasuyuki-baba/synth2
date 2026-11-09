#pragma once
#include "OscillatorTuning.h"
#include "Unison.h"

namespace synth2::dsp
{
    struct VoiceTuningSettings
    {
        bool secondTracksKeyboard {};
        double octave {},keyShift {},fine {},secondPitch {},secondFine {};
        int unisonCount {1};
        double unisonDetuneDial {};
        int unisonPitchDial {24};
    };
    // One compatible note/coarse/fine calculation for every source. Running
    // pitch controls and physical coarse history are composed by the caller.
    class VoiceTuning
    {
    public:
        struct Core { double first {},second {}; };
        VoiceTuning(const VoiceTuningSettings& value,int note) : config(value),midiNote(note)
        {
            secondRatio=OscillatorTuning::trackedRatio(config.secondPitch,config.secondFine);
            config.unisonCount=std::clamp(config.unisonCount,1,8);
            unison=Unison::layout(config.unisonCount,config.unisonDetuneDial,config.unisonPitchDial);
        }
        int unisonCount() const noexcept {return config.unisonCount;}
        Core prepareCore(double fine) const
        {
            Core result;
            result.first=OscillatorTuning::oscillator1(midiNote,
                static_cast<int>(config.octave*12+config.keyShift),fine,config.fine);
            result.second=config.secondTracksKeyboard
                ? static_cast<double>(static_cast<double>(result.first)*secondRatio)
                : OscillatorTuning::fixedOsc2(config.secondPitch,config.secondFine,config.fine,fine);
            return result;
        }
        double coarseSecond(const Core& value,double exponent,double unisonFine) const
        {
            double fine=config.secondFine;
            double base=static_cast<double>(value.first);
            if(!config.secondTracksKeyboard){fine=config.fine+fine;fine+=unisonFine;base=220.0;}
            const double octave=fine*(1.0/1200.0)+exponent;
            return static_cast<double>(std::pow(2.0,static_cast<double>(octave))*base);
        }
        Unison unison;
    private:
        VoiceTuningSettings config;
        int midiNote;
        double secondRatio {};
    };
}
