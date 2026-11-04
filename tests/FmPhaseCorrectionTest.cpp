#include <gtest/gtest.h>
#include "dsp/FmPhaseCorrection.h"
#include "dsp/NoiseGenerator.h"
#include "render/VoiceAudioConfiguration.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace {
constexpr uint32_t mask=0x07ffffffu;
int32_t offset(uint32_t word) {return static_cast<int32_t>(word);}
}

TEST(FmPhaseCorrection, DisabledPathPreservesCompatiblePhaseAdvancement)
{
    synth2::dsp::FmPhaseCorrection correction;
    uint32_t phase=12345;
    for(int i=0;i<1000;++i)
    {
        const float depth=123456.75f,source=i%3==0 ? -.65f : .65f;
        const auto expected=static_cast<uint32_t>(static_cast<int32_t>(depth*source));
        const auto result=correction.advance(depth,source,400000,phase,false);
        EXPECT_EQ(result.audibleOffset,expected);EXPECT_EQ(result.referenceOffset,expected);
        EXPECT_EQ(result.referenceBefore,phase);
        phase=(phase+400000+expected)&mask;EXPECT_EQ(result.referenceAfter,phase);
    }
}

TEST(FmPhaseCorrection, ConstantCoefficientDoesNotAccumulateQuantizationDrift)
{
    synth2::dsp::FmPhaseCorrection correction;
    uint32_t phase=0;int64_t difference=0;
    constexpr float depth=123456.75f,source=.3f;
    const int32_t native=static_cast<int32_t>(depth*source);
    const double bound=31*std::abs(static_cast<double>(depth)*source-native)+1;
    for(int i=0;i<20000;++i)
    {
        const auto result=correction.advance(depth,source,400000,phase,true);
        difference+=offset(result.audibleOffset)-offset(result.referenceOffset);
        EXPECT_LE(std::abs(static_cast<double>(difference)),bound);
        phase=(phase+400000+result.audibleOffset)&mask;
    }
}

TEST(FmPhaseCorrection, ReferenceClockPreservesSynchronizedNoiseHistory)
{
    synth2::dsp::FmPhaseCorrection correction;
    synth2::dsp::NoiseGenerator nativeNoise,correctedNoise;
    uint32_t nativePhase=0,phase=0;bool differs=false;
    for(int i=0;i<20000;++i)
    {
        const float nativeSource=nativeNoise.sample()*.65f;
        const float source=correctedNoise.sample()*.65f;
        ASSERT_EQ(source,nativeSource);
        const float depth=static_cast<float>((i/64)%16)*100000;
        const auto result=correction.advance(depth,source,400000,phase,true);
        const uint32_t next=(nativePhase+400000+static_cast<uint32_t>(static_cast<int32_t>(depth*nativeSource)))&mask;
        EXPECT_EQ(result.referenceBefore,nativePhase);EXPECT_EQ(result.referenceAfter,next);
        nativeNoise.synchronize(nativePhase,next);
        correctedNoise.synchronize(result.referenceBefore,result.referenceAfter);
        EXPECT_EQ(nativeNoise.state,correctedNoise.state);
        nativePhase=next;phase=(phase+400000+result.audibleOffset)&mask;
        differs|=phase!=nativePhase;
    }
    EXPECT_TRUE(differs);
}

TEST(FmPhaseCorrection, OscillatorUsesReferencePhaseForNoiseSync)
{
    synth2::dsp::OscillatorSection native,corrected;
    for(auto* oscillator:{&native,&corrected})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    bool phaseDiffers=false;
    for(int frame=0;frame<20000;++frame)
    {
        const float depth=static_cast<float>((frame/64)%16)*100000;
        const auto render=[&](auto& oscillator,bool correcting) {
            return oscillator.processSample(261.625f,261.625f,.5f,1,depth,true,false,
                0,0,0,nullptr,-1,0,0,false,correcting);
        };
        // With no wavetable and full OSC2 mix, output is the synchronized noise.
        EXPECT_EQ(render(native,false),render(corrected,true));
        phaseDiffers|=native.getOsc1Phase()!=corrected.getOsc1Phase();
    }
    EXPECT_TRUE(phaseDiffers);
}

TEST(FmPhaseCorrection, CoefficientStepsTurnsAndJumpsRemainUnfiltered)
{
    // Ramps, triangle turns, square jumps and held irregular steps all enter
    // the same phase boundary. Integral products must keep every native knot.
    constexpr std::array<float,16> depths {0,100000,200000,300000,200000,100000,0,0,
        600000,600000,0,0,120000,540000,30000,210000};
    for(float source:{-.5f,.5f})
    {
        synth2::dsp::FmPhaseCorrection correction;uint32_t phase=0,reference=0;
        for(int frame=0;frame<8192;++frame)
        {
            const float depth=depths[static_cast<size_t>(frame/64)%depths.size()];
            const uint32_t expected=static_cast<uint32_t>(static_cast<int32_t>(depth*source));
            const auto result=correction.advance(depth,source,400000,phase,true);
            EXPECT_EQ(result.audibleOffset,expected);
            EXPECT_EQ(result.referenceBefore,reference);
            reference=(reference+400000+expected)&mask;
            EXPECT_EQ(result.referenceAfter,reference);
            phase=(phase+400000+result.audibleOffset)&mask;
            EXPECT_EQ(phase,reference);
        }
    }
}

TEST(FmPhaseCorrection, FractionalProductsHaveCompatibilityBoundAndReturnAfterSettling)
{
    for(float sign:{-1.f,1.f})
    {
        synth2::dsp::FmPhaseCorrection correction;uint32_t phase=0;
        int64_t difference=0;bool fractionalProductRetained=false;
        for(int frame=0;frame<8192;++frame)
        {
            const float depth=frame<4096 ? sign*.25f : 0.f;
            const auto result=correction.advance(depth,1,400000,phase,true);
            difference+=offset(result.audibleOffset)-offset(result.referenceOffset);
            EXPECT_LE(std::abs(difference),8);
            fractionalProductRetained|=result.audibleOffset!=result.referenceOffset;
            phase=(phase+400000+result.audibleOffset)&mask;
            // This intentionally follows the compatible trajectory rather than
            // accumulating a quarter-unit pitch difference for the entire note.
            if(frame==4095)EXPECT_NE(difference,static_cast<int64_t>(sign*1024));
        }
        EXPECT_TRUE(fractionalProductRetained);
        EXPECT_LE(std::abs(difference),1);
    }
}

TEST(FmPhaseCorrection, ControlChangesRetainTheReferenceAndOutstandingCorrection)
{
    synth2::dsp::FmPhaseCorrection correction;uint32_t phase=0;
    auto step=[&](float depth) {
        const auto result=correction.advance(depth,1,400000,phase,true);
        phase=(phase+400000+result.audibleOffset)&mask;return result;
    };
    auto previous=step(.25f);
    for(int i=0;i<64;++i)previous=step(.25f);
    ASSERT_NE(phase,previous.referenceAfter);
    const auto changed=step(100000.25f);
    EXPECT_EQ(changed.referenceBefore,previous.referenceAfter);
    EXPECT_EQ(changed.referenceAfter,(previous.referenceAfter+500000)&mask);
    EXPECT_NE(phase,changed.referenceAfter);
    // A new LFO/envelope coefficient cannot discard the existing phase debt.
    for(int i=0;i<2048;++i)previous=step(100000);
    const int32_t distance=static_cast<int32_t>((phase-previous.referenceAfter+0x04000000u)&mask)-0x04000000;
    EXPECT_LE(std::abs(distance),1);
}

TEST(FmPhaseCorrection, ResetExternalPhaseAndDisableDiscardStaleCorrection)
{
    synth2::dsp::FmPhaseCorrection correction;uint32_t phase=0;
    auto step=[&](float depth,bool enabled=true) {
        const auto result=correction.advance(depth,1,400000,phase,enabled);
        phase=(phase+400000+result.audibleOffset)&mask;return result;
    };
    for(int i=0;i<64;++i)step(.25f);
    phase=1234;auto result=step(0);
    EXPECT_EQ(result.referenceBefore,1234u);EXPECT_EQ(result.audibleOffset,0u);
    for(int i=0;i<64;++i)step(.25f);
    correction.reset();const auto resetPhase=phase;result=step(0);
    EXPECT_EQ(result.referenceBefore,resetPhase);EXPECT_EQ(result.audibleOffset,0u);
    for(int i=0;i<64;++i)step(.25f);
    const auto disabledPhase=phase;result=step(0,false);
    EXPECT_EQ(result.referenceBefore,disabledPhase);EXPECT_EQ(result.audibleOffset,0u);
    const auto enabledPhase=phase;result=step(0);
    EXPECT_EQ(result.referenceBefore,enabledPhase);EXPECT_EQ(result.audibleOffset,0u);
}

TEST(FmPhaseCorrection, FmPolicyIsIndependentOfWaveformEnvelopeAndHistoryAdmission)
{
    synth2::RenderControls settings;settings.nativeFmLfo=true;
    settings.lfo1.on=settings.lfo2.on=true;
    settings.lfo1.dest=settings.lfo2.dest=5;
    settings.lfo1.nativeSharedClock=settings.lfo2.nativeSharedClock=true;
    for(int first=0;first<6;++first)for(int second=0;second<6;++second)
        for(bool envelope:{false,true})
        {
            settings.lfo1.wave=first;settings.lfo2.wave=second;
            settings.modEnv.on=envelope;
            const auto config=synth2::render::oscillatorAudio(settings,false);
            EXPECT_TRUE(config.correctFmPhase);
            EXPECT_FALSE(synth2::render::oscillatorAudio(settings,true).correctFmPhase);
        }
    settings.nativeFmLfo=false;settings.modEnv.nativeFm=true;
    EXPECT_TRUE(synth2::render::oscillatorAudio(settings,false).correctFmPhase);
    settings.modEnv.nativeFm=false;settings.nativeVoicePerformance=true;
    EXPECT_TRUE(synth2::render::oscillatorAudio(settings,false).correctFmPhase);
    settings.nativeVoicePerformance=false;
    EXPECT_TRUE(synth2::render::oscillatorAudio(settings,false).correctFmPhase);
}

TEST(FmPhaseCorrection, NativeRingDispatchBypassesFmCorrection)
{
    synth2::dsp::OscillatorSection native,corrected;
    for(auto* oscillator:{&native,&corrected})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    for(int frame=0;frame<4096;++frame)
    {
        const float depth=frame%64<32 ? 16384.25f : 65536.75f;
        const auto render=[&](auto& oscillator,bool correcting) {
            return oscillator.processSample(261.625f,261.625f,.5f,.5f,depth,true,true,
                0,0,0,nullptr,-1,0,0,false,correcting);
        };
        EXPECT_EQ(render(native,false),render(corrected,true));
        EXPECT_EQ(native.readPhaseWords(),corrected.readPhaseWords());
    }
}

TEST(FmPhaseCorrection, StaticMappingPreservesKnotsAndFractionalEdits)
{
    const auto knot=[](int dial) {
        const float normalized=static_cast<float>(dial)*(1.f/127.f);
        return static_cast<int32_t>((std::exp(std::log(1000.)*normalized)-1.)/999.*67108864.);
    };
    for(int dial=0;dial<=127;++dial)
    {
        EXPECT_DOUBLE_EQ(synth2::float01ToFmTarget(dial/127.f),knot(dial));
        EXPECT_DOUBLE_EQ(synth2::float01ToFmTarget(dial*(1.f/127.f)),knot(dial));
    }
    double previous=synth2::float01ToFmTarget(64.f/127.f);
    for(float dial:{64.1f,64.25f,64.49f,64.51f,64.75f})
    {
        const double value=synth2::float01ToFmTarget(dial/127.f);
        EXPECT_GT(value,previous);EXPECT_LT(value,knot(65));previous=value;
    }
}

TEST(FmPhaseCorrection, AudioAdapterRetainsWideDepthWithCompatibleSyncReference)
{
    synth2::dsp::OscillatorSection adapted,wide,narrow;
    for(auto* oscillator:{&adapted,&wide,&narrow})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    synth2::dsp::OscillatorAudioInput input{};
    input.firstFrequency=input.secondFrequency=261.625f;
    input.pulseWidth=.5f;input.mix=1;input.sync=true;input.fmDepth=67108863.25;
    synth2::dsp::OscillatorAudioSettings config{};config.correctFmPhase=true;
    config.subOctave=0;
    bool phaseDiffers=false;
    for(int frame=0;frame<4096;++frame)
    {
        const auto render=[&](auto& oscillator,double depth) {
            return oscillator.processSample(261.625f,261.625f,.5f,1,depth,true,false,
                0,0,0,nullptr,-1,0,0,false,true);
        };
        const float output=synth2::dsp::OscillatorAudioRenderer::process(adapted,config,input,{},0,nullptr);
        EXPECT_EQ(output,render(wide,input.fmDepth));
        EXPECT_EQ(output,render(narrow,static_cast<float>(input.fmDepth)));
        EXPECT_EQ(adapted.getOsc1Phase(),wide.getOsc1Phase());
        phaseDiffers|=wide.getOsc1Phase()!=narrow.getOsc1Phase();
    }
    EXPECT_TRUE(phaseDiffers);
}
