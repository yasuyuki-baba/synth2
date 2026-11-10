#pragma once
#include "AmplitudeEnvelope.h"
#include "FilterEnvelope.h"
#include "FilterPerformance.h"
#include "ModulationEnvelope.h"

namespace synth2::dsp
{
    struct AmplitudeEnvelopeSettings
    {
        double attack {},decay {},sustain {},release {},gain {},sensitivity {};
    };
    struct ModulationEnvelopeSettings
    {
        double attackDial {},decayDial {};
        double amountDial {64};
        bool pulseWidth {},fm {};
    };
    struct FilterEnvelopeSettings
    {
        double attack {},decay {},sustain {},release {};
        double cutoffIndex {};
        double tracking {},amount {};
        bool velocity {};
    };
    struct VoiceEnvelopeConfigurator
    {
        static void amplitude(AmplitudeEnvelope& envelope,double& referenceVelocity,
            const AmplitudeEnvelopeSettings& settings,double level,double sampleRate,bool noteAssignment=false) noexcept
        {
            const double velocity=AmplitudeEnvelope::velocityForDials(
                static_cast<int>(std::lround(level*127.0)),fractionalDial(settings.sensitivity));
            const double gain=AmplitudeEnvelope::gainForDial(fractionalDial(settings.gain));
            // Sensitivity is sampled at note assignment. Running edits preserve
            // the sounding physical core's velocity reference.
            if(noteAssignment)referenceVelocity=velocity;
            envelope.setGainFactors(gain,referenceVelocity);
            envelope.setParameters(ReleaseRamp::durationFrames(settings.attack,sampleRate),
                ReleaseRamp::durationFrames(settings.decay,sampleRate),
                static_cast<double>(std::clamp(std::isfinite(settings.sustain) ? settings.sustain : 0.0,0.0,1.0)*2048.0),
                ReleaseRamp::durationFrames(settings.release,sampleRate));
        }
        static void modulation(FilterEnvelope& envelope,const ModulationEnvelopeSettings& settings,double sampleRate) noexcept
        {
            envelope.configure(
                ReleaseRamp::durationFrames(ModulationEnvelope::timeSeconds(settings.attackDial,true),sampleRate),
                ReleaseRamp::durationFrames(ModulationEnvelope::timeSeconds(settings.decayDial,false),sampleRate),
                ModulationEnvelope::peak(settings.amountDial,settings.pulseWidth ? 2 : settings.fm ? 1 : 0),
                0,0,settings.amountDial<64,settings.fm);
        }
        static void filter(FilterEnvelope& envelope,double& keyboardOffset,double unisonOffset,
            const FilterEnvelopeSettings& settings,int note,double level,double sampleRate) noexcept
        {
            const double baseIndex=settings.cutoffIndex;
            keyboardOffset=FilterPerformance::keyboardOffset(baseIndex,settings.tracking,note);
            const double bounded=FilterPerformance::amount(baseIndex,keyboardOffset,settings.amount,
                settings.velocity,static_cast<int>(std::lround(level*127.0)),unisonOffset);
            const double amountDial=settings.amount*64.0+63.0;
            const double sustainDial=fractionalDial(settings.sustain);
            const int velocity=static_cast<int>(std::lround(level*127.0));
            const auto response=[&](bool sustainLevel) {
                return compatibleControlCurve(baseIndex,[&](int base) {
                    return compatibleControlCurve(keyboardOffset,[&](int keyboard) {
                        return compatibleControlCurve(unisonOffset,[&](int offset) {
                            return compatibleControlCurve(amountDial,[&](int dial) {
                                const float depth=static_cast<float>(std::abs(FilterPerformance::amount(base,keyboard,
                                    (dial-63.0)/64.0,settings.velocity,velocity,offset)))*1024.0f;
                                if(!sustainLevel)return std::trunc(static_cast<double>(depth));
                                return compatibleControlCurve(sustainDial,[&](int value) {
                                    // Keep the original normalized source word at exact knots.
                                    const float unit=value==sustainDial ? static_cast<float>(settings.sustain) : static_cast<float>(value)/127.0f;
                                    return std::trunc(depth*unit);
                                });
                            });
                        });
                    });
                });
            };
            const double peak=response(false),sustain=response(true);
            envelope.configure(ReleaseRamp::durationFrames(settings.attack,sampleRate),
                ReleaseRamp::durationFrames(settings.decay,sampleRate),peak,
                sustain,ReleaseRamp::durationFrames(settings.release,sampleRate),bounded<0);
        }
    };
}
