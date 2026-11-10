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
        for(int i=0;i<2048;++i)table.data.push_back(static_cast<float>(std::sin(2*std::acos(-1.f)*i/2048)));
        bank.setShapeTables(shape,std::move(table));
    }
    return bank;
}
}

TEST(PwmPrecision, SourceKnotsKeepCompatibleWidthAndModulationValues)
{
    using M=synth2::dsp::ModulationEnvelope;
    for(int dial=0;dial<=127;++dial)for(float control:{-512.f,-128.f,-1.f,0.f,.125f,1.f,127.f,128.f,512.f})
    {
        const float expected=M::pulseWidthBins(dial*(1.f/127.f),control);
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
    float previous=516;
    for(float dial:{64.01f,64.05f,64.1f,64.125f,64.25f,64.75f})
    {
        const float value=M::pulseWidthForDial(dial,0);
        EXPECT_DOUBLE_EQ(value,516+8*(dial-64));EXPECT_GT(value,previous);previous=value;
    }
}

TEST(PwmPrecision, BothCoordinatesRemainContinuousAtCompatibleKnots)
{
    using M=synth2::dsp::ModulationEnvelope;
    for(float dial:{1.f,32.f,64.f,96.f,126.f})for(float control:{-128.f,-1.f,0.f,1.f,127.f,128.f})
    {
        const float exact=M::pulseWidthForDial(dial,control);
        for(float epsilon:{-.00001f,.00001f})
        {
            EXPECT_NEAR(M::pulseWidthForDial(dial+epsilon,control),exact,.001f);
            EXPECT_NEAR(M::pulseWidthForDial(dial,control+epsilon),exact,.001f);
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
        const float bins=synth2::dsp::ModulationEnvelope::pulseWidthForDial(synth2::dsp::fractionalDial(width),0);
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
    for(float bins:{10.f,10.25f,516.f,516.25f,2037.f})
    {
        const auto sample=[](uint32_t offset){return static_cast<float>(offset*offset);};
        const auto low=static_cast<uint32_t>(bins);
        const float expected=static_cast<float>(sample(low))+(sample(low+1)-sample(low))*static_cast<float>(bins-low);
        EXPECT_EQ(synth2::dsp::PulseWidthRead::interpolate(bins,sample),expected);
        const float a=oscillator.renderPhaseDiffSawNoAdvance(low<<16,&bank,0,5);
        const float b=oscillator.renderPhaseDiffSawNoAdvance((low+1)<<16,&bank,0,5);
        EXPECT_EQ(oscillator.renderSample(1,.5f,0,&bank,bins),a+(b-a)*static_cast<float>(bins-low));
        EXPECT_EQ(oscillator.getPhaseFixed27(),phase);
    }
}

TEST(PwmPrecision, WidthConversionIsIndependentOfPriorControlAndLifecycleEdits)
{
    using namespace synth2::dsp;
    OscillatorSection oscillator;
    oscillator.prepare({44100,64,2});
    oscillator.resetPhase(.375f);
    // Repeated controls, base-only edits, modulation-only edits and reversions
    // must all resolve the same fractional coordinates as a fresh conversion.
    const std::array<std::array<float,2>,9> edits{{
        {64.0f/127.0f,0},{64.0f/127.0f,0},{64.125f/127.0f,0},
        {64.125f/127.0f,28.875f},{64.125f/127.0f,-17.25f},{65.5f/127.0f,-17.25f},
        {64.0f/127.0f,0},{0,-128},{1,128}}};
    const auto phase=oscillator.readPhaseWords();
    for(const auto& edit:edits)
    {
        const float expected=ModulationEnvelope::pulseWidthForDial(fractionalDial(edit[0]),edit[1]);
        EXPECT_DOUBLE_EQ(oscillator.resolvePulseWidth(edit[0],edit[1]),expected);
        EXPECT_EQ(oscillator.readPhaseWords(),phase);
    }
    oscillator.reset();oscillator.prepare({96000,128,2});
    for(const auto& edit:edits)
        EXPECT_DOUBLE_EQ(oscillator.resolvePulseWidth(edit[0],edit[1]),
            ModulationEnvelope::pulseWidthForDial(fractionalDial(edit[0]),edit[1]));
}
