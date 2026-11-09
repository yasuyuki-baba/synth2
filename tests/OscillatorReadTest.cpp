#include <gtest/gtest.h>
#include "dsp/OscillatorAudioRenderer.h"
#include "dsp/AudioOversampler.h"
#include <cmath>

namespace {
synth2::WaveTableBank generatedBank()
{
    synth2::WaveTableBank bank;
    for(int shape:{0,2,3,5})
    {
        synth2::ShapeTables table;table.infos.push_back({0,2048,100000});
        for(int i=0;i<2048;++i)
        {
            const double phase=i/2048.;
            const double value=(shape==0 || shape==5) ? 2*phase-1 : shape==2 ? (phase<.5 ? 4*phase-1 : 3-4*phase) : std::sin(phase*2*std::acos(-1.));
            table.data.push_back(static_cast<float>(value));
        }
        bank.setShapeTables(shape,std::move(table));
    }
    return bank;
}
}


TEST(OscillatorRead, EveryTableReadUsesAdjacentSamplesWithoutAdvancingPhase)
{
    for(double rate:{44100.,96000.})
    {
        const auto bank=generatedBank();
        for(int shape:{0,2,3})for(float frequency:{55.f,261.625f,4000.f})
        {
            synth2::dsp::Oscillator oscillator;oscillator.setSampleRate(rate);
            oscillator.updatePhaseIncrement(frequency);
            for(unsigned bin:{0u,31u,511u,1023u,2047u})
            {
                oscillator.setPhaseFixed27(bin<<16);
                const float first=oscillator.renderSample(shape,.5f,0,&bank);
                oscillator.setPhaseFixed27(((bin+1)&2047u)<<16);
                const float second=oscillator.renderSample(shape,.5f,0,&bank);
                for(unsigned fraction:{0u,8192u,32768u,57344u})
                {
                    const unsigned phase=(bin<<16)|fraction;oscillator.setPhaseFixed27(phase);
                    const float weight=static_cast<float>(fraction)/65536.f;
                    EXPECT_EQ(oscillator.renderSample(shape,.5f,0,&bank),first+weight*(second-first));
                    EXPECT_EQ(oscillator.getPhaseFixed27(),phase);
                }
            }
        }
    }
}

TEST(OscillatorRead, DefaultVoiceAdapterAndDirectOscillatorUseTheSameReader)
{
    const auto bank=generatedBank();
    synth2::dsp::OscillatorSection adapted,direct;
    for(auto* oscillator:{&adapted,&direct})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=0;oscillator->params.osc2Wave=2;
    }
    synth2::dsp::OscillatorAudioSettings config;
    synth2::dsp::OscillatorAudioInput input;
    input.firstFrequency=220;input.secondFrequency=330;input.pulseWidth=input.mix=.5f;
    for(int frame=0;frame<2048;++frame)
    {
        const float actual=synth2::dsp::OscillatorAudioRenderer::process(adapted,config,input,{},0,&bank);
        EXPECT_EQ(actual,direct.processSample(220,330,.5f,.5f,0,false,false,0,0,-1,&bank));
        EXPECT_EQ(adapted.readPhaseWords(),direct.readPhaseWords());
    }
}

TEST(OscillatorRead, RingAlwaysUsesProductMixAndSuppressesFmPhaseOffsets)
{
    const auto bank=generatedBank();
    for(int shape:{0,1,2,3,4})for(float mix:{0.f,.25f,1.f})for(double depth:{0.,16384.5})
    {
        synth2::dsp::OscillatorSection ring;
        ring.prepare({44100,64,2});ring.params.osc1Wave=3;ring.params.osc2Wave=shape;
        synth2::dsp::Oscillator first,second;first.setSampleRate(44100);second.setSampleRate(44100);
        first.updatePhaseIncrement(220);second.updatePhaseIncrement(330);
        for(int i=0;i<256;++i)
        {
            const float carrier=first.renderSample(3,.5f,0,&bank);
            float modulator=second.renderSample(shape,.5f,0,&bank);
            if(shape==4 && depth==0)modulator*=.65f;
            if(shape==1 && depth!=0)modulator*=2.f;
            const float expected=(modulator*mix)*carrier+carrier*(1.f-mix);
            EXPECT_EQ(ring.processSample(220,330,.5f,mix,depth,false,true,0,0,-1,&bank),expected);
            first.advancePhase();if(shape!=4)second.advancePhase();
            EXPECT_EQ(ring.osc1.getPhaseFixed27(),first.getPhaseFixed27());
            EXPECT_EQ(ring.osc2.getPhaseFixed27(),second.getPhaseFixed27());
        }
    }
}

TEST(OscillatorRead, SubKeepsSharedPhaseAcrossMuteAndFractionalLevelEdits)
{
    const auto bank=generatedBank();
    for(int octave:{-1,0})for(float stack:{0.f,7.f})for(bool ring:{false,true})
    for(float level:{.00001f,.25f,1.f})
    {
        synth2::dsp::OscillatorSection audible,muted;
        for(auto* oscillator:{&audible,&muted})
        {
            oscillator->prepare({44100,64,2});oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=2;
            oscillator->stackDial=stack;
        }
        for(int frame=0;frame<4096;++frame)
        {
            const float a=audible.processSample(440,330,.5f,.25f,16384.5,false,ring,level,3,octave,&bank);
            const float b=muted.processSample(440,330,.5f,.25f,16384.5,false,ring,frame<2048 ? 0.f : level,3,octave,&bank);
            EXPECT_EQ(audible.subSharedPhase,muted.subSharedPhase);
            EXPECT_EQ(audible.subSharedPhase&synth2::dsp::kPhaseMask27,audible.getOsc1Phase());
            if(frame>=2048)EXPECT_EQ(a,b);
        }
    }
}

TEST(OscillatorRead, PulseReceiverComposesSharedEnvelopeAndRemainingControlOnce)
{
    const auto bank=generatedBank();
    synth2::dsp::OscillatorSection actual,reference;
    for(auto* oscillator:{&actual,&reference}){oscillator->prepare({44100,64,2});oscillator->params.osc1Wave=oscillator->params.osc2Wave=1;}
    synth2::dsp::OscillatorAudioSettings config;config.pulseReceiver=config.pulseEnvelope=true;config.pulseWidth=.5f;
    synth2::dsp::OscillatorAudioInput input;input.firstFrequency=input.secondFrequency=440;input.pulseWidth=.5f;
    input.fmDepth=1234.5;input.envelope=8.25;input.pulseControl=4.5;
    synth2::dsp::VoiceControlBuffer control{};control.fill(16.125);
    synth2::dsp::ReceiverLfoView view;view.available=true;view.receivers[0]={true,4};view.values[0]=control.data();
    const double bins=synth2::dsp::ModulationEnvelope::pulseWidthForDial(synth2::dsp::fractionalDial(.5f),28.875);
    for(int frame=0;frame<256;++frame)
    {
        EXPECT_EQ(synth2::dsp::OscillatorAudioRenderer::process(actual,config,input,view,frame,&bank),
            reference.processSample(440,440,.5f,0,input.fmDepth,false,false,0,0,-1,&bank,bins));
        EXPECT_EQ(actual.readPhaseWords(),reference.readPhaseWords());
    }
}

TEST(OscillatorRead, OscillatorTwoPitchEnvelopeUsesTheCommonOneOctaveControl)
{
    for(bool receiver:{false,true})
    {
        synth2::dsp::OscillatorSection actual,reference;
        for(auto* oscillator:{&actual,&reference})oscillator->prepare({44100,64,2});
        synth2::dsp::OscillatorAudioSettings config;config.pitchEnvelope=true;config.pitchReceiver=receiver;
        synth2::dsp::OscillatorAudioInput input;input.firstFrequency=220;input.secondFrequency=330;input.pulseWidth=.5f;input.envelope=128;
        for(int frame=0;frame<256;++frame)
        {
            synth2::dsp::OscillatorAudioRenderer::process(actual,config,input,{},0,nullptr);
            reference.processSample(220,660,.5f,0,0,false,false,0,0,-1,nullptr);
            EXPECT_EQ(actual.readPhaseWords(),reference.readPhaseWords());
        }
    }
}

TEST(OscillatorRead, ScalarFrequenciesUseFixedCeilingAtEverySampleRate)
{
    const auto bank=generatedBank();
    const std::array<std::pair<float,float>,5> frequencies{{
        {21999.f,21999.f},{22000.f,22000.f},
        {std::nextafter(22000.f,INFINITY),22000.f},{34689.f,22000.f},{80000.f,22000.f}}};
    for(double rate:{32000.,44100.,48000.25,96000.})
        for(const auto& [requested,expected]:frequencies)
            for(int index:{0,1})for(bool tables:{false,true})
            {
                SCOPED_TRACE(rate);
                SCOPED_TRACE(requested);
                SCOPED_TRACE(index);
                SCOPED_TRACE(tables);
                synth2::dsp::OscillatorSection actual,reference;
                synth2::dsp::Oscillator phaseReference;
                phaseReference.setSampleRate(rate);phaseReference.updatePhaseIncrement(expected);
                for(auto* section:{&actual,&reference}) {
                    section->prepare({rate,64,2});section->params={0,2};
                }
                for(int frame=0;frame<128;++frame) {
                    const auto* selected=tables ? &bank : nullptr;
                    const auto sample=actual.processSample(index==0 ? requested : 440.f,index==1 ? requested : 660.f,
                        .5f,.5f,0,false,false,0,0,-1,selected);
                    const auto match=reference.processSample(index==0 ? expected : 440.f,index==1 ? expected : 660.f,
                        .5f,.5f,0,false,false,0,0,-1,selected);
                    ASSERT_EQ(actual.readPhaseWords()[2+index],phaseReference.getPhaseIncFixed27());
                    ASSERT_EQ(actual.readPhaseWords(),reference.readPhaseWords());
                    ASSERT_EQ(sample,match);
                }
            }
}

TEST(OscillatorRead, CeilingPrecedesFmRingSyncStackAndSubDispatch)
{
    auto bank=generatedBank();
    // Different bands make an uncapped table lookup observable even if only the
    // phase increment were capped. Inputs are self-authored, not reference PCM.
    synth2::ShapeTables bands;
    for(float border:{22050.f,100000.f}) {
        bands.infos.push_back({bands.data.size(),2048,border});
        for(int i=0;i<2048;++i)
            bands.data.push_back(static_cast<float>(std::sin(i*2*std::acos(-1.)/2048))*(border==22050.f ? 1.f : .25f));
    }
    bank.setShapeTables(2,std::move(bands));
    for(int mode=0;mode<6;++mode) {
        SCOPED_TRACE(mode);
        synth2::dsp::OscillatorSection actual,reference;
        for(auto* section:{&actual,&reference}) {
            section->prepare({44100,64,2});section->params={0,2};
            section->stackDial=mode==4 ? 32.f : 0.f;
        }
        const bool sync=mode==3,ring=mode==2;
        const double depth=mode>0 && mode<5 ? 1048576.25 : 0.;
        const float sub=mode==5 ? .25f : 0.f;
        for(int frame=0;frame<1024;++frame) {
            const auto a=actual.processSample(440,34689,.5f,.3f,depth,sync,ring,sub,2,0,&bank,-1,0,0,false,true);
            const auto b=reference.processSample(440,22000,.5f,.3f,depth,sync,ring,sub,2,0,&bank,-1,0,0,false,true);
            ASSERT_EQ(actual.readPhaseWords(),reference.readPhaseWords());ASSERT_EQ(a,b);
            ASSERT_EQ(actual.stackPhases,reference.stackPhases);ASSERT_EQ(actual.subSharedPhase,reference.subSharedPhase);
        }
    }
}

TEST(OscillatorRead, CeilingAppliesAfterPitchModulation)
{
    const auto bank=generatedBank();
    struct Input {float base;int control;float expected;};
    for(const auto input:{Input{40000,-128,20000},Input{20000,128,22000},Input{30000,0,22000}}) {
        synth2::dsp::OscillatorSection actual,reference;
        for(auto* section:{&actual,&reference}){section->prepare({44100,64,2});section->params={0,2};}
        for(int frame=0;frame<128;++frame) {
            const auto a=actual.processSample(440,input.base,.5f,.5f,0,false,false,0,0,-1,&bank,-1,0,input.control,true);
            const auto b=reference.processSample(440,input.expected,.5f,.5f,0,false,false,0,0,-1,&bank);
            ASSERT_EQ(actual.readPhaseWords(),reference.readPhaseWords());ASSERT_EQ(a,b);
        }
    }
}

TEST(OscillatorRead, FineFramesPreserveBaseTrajectoryAcrossWaveformsAndControlChanges)
{
    const auto bank=generatedBank();
    for(double rate:{44100.,48000.})for(int factor:{2,4,8,16})
    for(int first:{0,1,2,3})for(int second:{0,1,2,3,4})for(int mode:{0,1,2,3})
    {
        synth2::dsp::OscillatorSection base,fine;
        for(auto* o:{&base,&fine}){o->prepare({rate,64,2});o->params={first,second};o->resetPhase(.87f);}
        for(int frame=0;frame<96;++frame)
        {
            if(frame==48){base.resetPhase(.13f);fine.resetPhase(.13f);}
            const bool sync=(mode&1)!=0,ring=(mode&2)!=0;
            const float stack=frame<32 ? 0.f : 37.5f,sub=frame<16 ? 0.f : .7f;
            base.stackDial=fine.stackDial=stack;
            const float f1=frame<64 ? 1046.5f : 35000.f,f2=frame<32 ? 2489.f : 10000.f;
            const float width=frame%2 ? .23f : .77f;
            const double depth=frame<8 ? 0 : 15400286.75;
            const float expected=base.processSample(f1,f2,width,.35f,depth,sync,ring,sub,2,-1,&bank,-1,.1,-.2,true,true);
            std::array<float,16> actual{};
            fine.processFrame(actual.data(),factor,f1,f2,width,.35f,depth,sync,ring,sub,2,-1,&bank,-1,.1,-.2,true,true);
            ASSERT_EQ(actual[0],expected);
            ASSERT_EQ(fine.readPhaseWords(),base.readPhaseWords());
            ASSERT_EQ(fine.subSharedPhase,base.subSharedPhase);
            ASSERT_EQ(fine.stackPhases,base.stackPhases);
            for(int i=0;i<factor;++i)ASSERT_TRUE(std::isfinite(actual[i]));
        }
    }
}

TEST(OscillatorRead, FineFrameFollowsSignedFmAdvanceIncludingMoreThanHalfACycle)
{
    // Constant owned modulator tables make the signed FM advance independent
    // of feedback. Check analytic intermediate sine values, not another renderer.
    for(float source:{-1.f,1.f})for(int factor:{2,4,8,16})
    {
        auto bank=generatedBank();synth2::ShapeTables constant;
        constant.infos.push_back({0,2048,100000});constant.data.assign(2048,source);
        bank.setShapeTables(2,std::move(constant));
        synth2::dsp::OscillatorSection o;o.prepare({48000,64,2});o.params={3,2};
        o.osc1.setPhaseFixed27(0x07000000);o.osc2.setPhaseFixed27(0);
        std::array<float,16> values{};
        o.processFrame(values.data(),factor,22000,1000,.5f,0,67108864,true,false,0,0,-1,&bank);
        const int64_t increment=o.osc1.getPhaseIncFixed27();
        const int64_t delta=increment+(source<0 ? -67108864 : 67108864);
        for(int i=0;i<factor;++i)
        {
            const uint32_t phase=(0x07000000u+static_cast<uint32_t>(delta*i/factor))&0x07ffffffu;
            EXPECT_NEAR(values[i],std::sin(2*std::acos(-1.)*phase/134217728.),2e-6);
        }
    }
}

TEST(OscillatorRead, FineFmTrajectorySuppressesAnAboveNyquistCarrierWithoutRetuning)
{
    auto bank=generatedBank();synth2::ShapeTables constant;
    constant.infos.push_back({0,2048,100000});constant.data.assign(2048,1.f);
    bank.setShapeTables(2,std::move(constant));
    double reference=0;
    for(int factor:{1,2,4,8,16})
    {
        synth2::dsp::OscillatorSection o;o.prepare({48000,64,2});o.params={3,2};
        synth2::dsp::AudioDownsampler down;down.setFactor(factor);
        double re=0,im=0;
        for(int i=0;i<12288;++i)
        {
            std::array<float,16> samples{};
            // 12 kHz base + 20 kHz constant FM = 32 kHz. At 48 kHz it
            // folds to 16 kHz. A finer read must remove, not retune, this tone.
            o.processFrame(samples.data(),factor,12000,1000,.5f,0,20000.*134217728./48000.,false,false,0,0,-1,&bank);
            const float value=down.process(samples);
            if(i>=4096){const double w=.5-.5*std::cos(2*std::acos(-1.)*(i-4096)/8191.);re+=w*value*std::cos(2*std::acos(-1.)*16000*i/48000.);im+=w*value*std::sin(2*std::acos(-1.)*16000*i/48000.);}
        }
        const double magnitude=std::hypot(re,im);
        if(factor==1){reference=magnitude;ASSERT_GT(reference,100);}
        else EXPECT_LT(magnitude,reference*.02);
    }
}
