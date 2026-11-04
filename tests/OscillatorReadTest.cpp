#include <gtest/gtest.h>
#include "dsp/OscillatorAudioRenderer.h"
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

TEST(OscillatorRead, LegacyBothPitchDestinationUsesTheCommonEnvelopeAtOneOctaveRange)
{
    for(bool receiver:{false,true})
    {
        synth2::dsp::OscillatorSection actual,reference;
        for(auto* oscillator:{&actual,&reference})oscillator->prepare({44100,64,2});
        synth2::dsp::OscillatorAudioSettings config;config.pitchEnvelope=config.pitchEnvelopeBoth=true;config.pitchReceiver=receiver;
        synth2::dsp::OscillatorAudioInput input;input.firstFrequency=220;input.secondFrequency=330;input.pulseWidth=.5f;input.envelope=640;
        for(int frame=0;frame<256;++frame)
        {
            synth2::dsp::OscillatorAudioRenderer::process(actual,config,input,{},0,nullptr);
            reference.processSample(440,660,.5f,0,0,false,false,0,0,-1,nullptr);
            EXPECT_EQ(actual.readPhaseWords(),reference.readPhaseWords());
        }
    }
}
