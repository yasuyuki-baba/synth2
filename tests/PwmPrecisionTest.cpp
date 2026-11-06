#include <gtest/gtest.h>
#include "dsp/OscillatorAudioRenderer.h"
#include <cmath>

namespace {
synth2::WaveTableBank pulseBank()
{
    synth2::WaveTableBank bank;
    for(int shape:{0,5})
    {
        synth2::ShapeTables table;table.infos.push_back({0,2048,100000});
        for(int i=0;i<2048;++i)table.data.push_back(static_cast<float>(std::sin(2*std::acos(-1.)*i/2048)));
        bank.setShapeTables(shape,std::move(table));
    }
    return bank;
}
}

TEST(PwmPrecision, SourceKnotsKeepCompatibleWidthAndModulationValues)
{
    using M=synth2::dsp::ModulationEnvelope;
    for(int dial=0;dial<=127;++dial)for(double control:{-512.,-128.,-1.,0.,.125,1.,127.,128.,512.})
    {
        const double expected=M::pulseWidthBins(dial*(1.f/127.f),control);
        EXPECT_DOUBLE_EQ(M::pulseWidthForDial(synth2::dsp::fractionalDial(dial/127.f),control),expected);
        EXPECT_DOUBLE_EQ(M::pulseWidthForDial(synth2::dsp::fractionalDial(dial*(1.f/127.f)),control),expected);
    }
    EXPECT_EQ(M::pulseWidthForDial(64,0),516);
    EXPECT_EQ(M::pulseWidthForDial(65,0),524);
    EXPECT_EQ(M::pulseWidthForDial(0,-128),10);
    EXPECT_EQ(M::pulseWidthForDial(127,128),2037);
}

TEST(PwmPrecision, BaseWidthEditsNoLongerPlateauBetweenSourceDials)
{
    using M=synth2::dsp::ModulationEnvelope;
    double previous=516;
    for(double dial:{64.01,64.05,64.1,64.125,64.25,64.75})
    {
        const double value=M::pulseWidthForDial(dial,0);
        EXPECT_DOUBLE_EQ(value,516+8*(dial-64));EXPECT_GT(value,previous);previous=value;
    }
}

TEST(PwmPrecision, BothCoordinatesRemainContinuousAtCompatibleKnots)
{
    using M=synth2::dsp::ModulationEnvelope;
    for(double dial:{1.,32.,64.,96.,126.})for(double control:{-128.,-1.,0.,1.,127.,128.})
    {
        const double exact=M::pulseWidthForDial(dial,control);
        for(double epsilon:{-.00001,.00001})
        {
            EXPECT_NEAR(M::pulseWidthForDial(dial+epsilon,control),exact,.001);
            EXPECT_NEAR(M::pulseWidthForDial(dial,control+epsilon),exact,.001);
        }
    }
}

TEST(PwmPrecision, OrdinaryAndStackAudioAdaptersRetainSmallBaseEditsWithoutClockChanges)
{
    const auto bank=pulseBank();
    for(float stack:{0.f,32.f})for(bool receiver:{false,true})
    {
        synth2::dsp::OscillatorSection first,second;
        for(auto* oscillator:{&first,&second})
        {
            oscillator->prepare({44100,64,2});oscillator->stackDial=stack;
            oscillator->params.osc1Wave=1;oscillator->params.osc2Wave=0;
        }
        synth2::dsp::OscillatorAudioSettings config{};
        config.pulseReceiver=receiver;
        synth2::dsp::OscillatorAudioInput input{};
        input.firstFrequency=input.secondFrequency=261.625f;
        bool audioDiffers=false;
        for(int frame=0;frame<512;++frame)
        {
            config.pulseWidth=input.pulseWidth=64.01f*(1.f/127.f);
            const float a=synth2::dsp::OscillatorAudioRenderer::process(first,config,input,{},0,&bank);
            config.pulseWidth=input.pulseWidth=64.1f*(1.f/127.f);
            const float b=synth2::dsp::OscillatorAudioRenderer::process(second,config,input,{},0,&bank);
            EXPECT_TRUE(std::isfinite(a));EXPECT_TRUE(std::isfinite(b));audioDiffers|=a!=b;
            EXPECT_EQ(first.getOsc1Phase(),second.getOsc1Phase());
        }
        EXPECT_TRUE(audioDiffers)<<"stack="<<stack<<" receiver="<<receiver;
    }
}

TEST(PwmPrecision, DirectAndExplicitCoordinateEntryPointsAgree)
{
    const auto bank=pulseBank();
    for(float stack:{0.f,32.f})for(float dial:{0.f,64.f,64.125f,127.f})
    {
        synth2::dsp::OscillatorSection direct,explicitCoordinate;
        for(auto* oscillator:{&direct,&explicitCoordinate})
        {
            oscillator->prepare({44100,64,2});oscillator->stackDial=stack;
            oscillator->params.osc1Wave=1;oscillator->params.osc2Wave=1;
        }
        const float width=dial*(1.f/127.f);
        const double bins=synth2::dsp::ModulationEnvelope::pulseWidthForDial(synth2::dsp::fractionalDial(width),0);
        for(int frame=0;frame<256;++frame)
        {
            EXPECT_EQ(direct.processSample(220,330,width,.25f,0,false,false,0,0,-1,&bank),
                explicitCoordinate.processSample(220,330,width,.25f,0,false,false,0,0,-1,&bank,bins));
            EXPECT_EQ(direct.getOsc1Phase(),explicitCoordinate.getOsc1Phase());
        }
    }
}

TEST(PwmPrecision, CommonReadPreservesIntegralOffsetsAndWrapsWithoutAdvancingPhase)
{
    const auto bank=pulseBank();synth2::dsp::Oscillator oscillator;
    oscillator.setSampleRate(44100);oscillator.updatePhaseIncrement(220);
    oscillator.setPhaseFixed27(0x07ff8000u);
    const auto phase=oscillator.getPhaseFixed27();
    for(double bins:{10.,10.25,516.,516.25,2037.})
    {
        const auto sample=[](uint32_t offset){return static_cast<float>(offset*offset);};
        const auto low=static_cast<uint32_t>(bins);
        const float expected=sample(low)+(sample(low+1)-sample(low))*static_cast<float>(bins-low);
        EXPECT_EQ(synth2::dsp::PulseWidthRead::interpolate(bins,sample),expected);
        const float a=oscillator.renderPhaseDiffSawNoAdvance(low<<16,&bank,0,5);
        const float b=oscillator.renderPhaseDiffSawNoAdvance((low+1)<<16,&bank,0,5);
        EXPECT_EQ(oscillator.renderSample(1,.5f,0,&bank,bins),a+(b-a)*static_cast<float>(bins-low));
        EXPECT_EQ(oscillator.getPhaseFixed27(),phase);
    }
}
