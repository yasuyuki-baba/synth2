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
                ? OscillatorTuning::applyTrackedRatio(result.first,secondRatio)
                : OscillatorTuning::fixedOsc2(config.secondPitch,config.secondFine,config.fine,fine);
            return result;
        }
        double coarseSecond(const Core& value,double exponent,double unisonFine) const
        {
            return compatibleFloatCurve(config.secondFine,[&](float second) {
                if(config.secondTracksKeyboard)
                    return OscillatorTuning::coarseFrequency(value.first,second,exponent);
                return compatibleFloatCurve(config.fine,[&](float master) {
                    return compatibleFloatCurve(unisonFine,[&](float local) {
                        float fine=master+second;fine+=local;
                        return OscillatorTuning::coarseFrequency(220.0,fine,exponent);
                    });
                });
            });
        }
        Unison unison;
    private:
        VoiceTuningSettings config;
        int midiNote;
        double secondRatio {};
    };
}
