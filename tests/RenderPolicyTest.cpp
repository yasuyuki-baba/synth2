#include <gtest/gtest.h>
#include "RenderControls.h"
#include "render/RenderPolicy.h"
#include "render/VoiceLifecycleConfiguration.h"

TEST(RenderPolicy, NativeCompositionUsesDestinationsAcrossWaveformMixtures)
{
    for(int first=0;first<6;++first)for(int second=0;second<6;++second)
        for(int destination=0;destination<7;++destination)
        {
            synth2::RenderControls p;
            synth2::render::RenderPolicyInputs input;
            input.nativeSource=input.randomHistoryValid=true;input.stackDial=3;
            p.lfo1.on=p.lfo2.on=true;p.lfo1.wave=first;p.lfo2.wave=second;
            p.lfo1.dest=p.lfo2.dest=destination;
            synth2::render::selectRenderPolicy(input,p);
            EXPECT_TRUE(p.nativeVoicePerformance);EXPECT_TRUE(p.nativeFilterEnvelope);
            EXPECT_TRUE(p.lfo1.nativeSharedClock);EXPECT_TRUE(p.lfo2.nativeSharedClock);
            EXPECT_EQ(p.nativePitchLfo,destination==0 || destination==1);
            EXPECT_EQ(p.nativeFilterLfo,destination==2);EXPECT_EQ(p.nativeFmLfo,destination==5);
            EXPECT_EQ(p.nativePanLfo,destination==6);EXPECT_EQ(p.nativePulseWidthLfo,destination==4);
            EXPECT_EQ(p.osc1StackDial,3);
        }
}

TEST(RenderPolicy, UnavailableRandomHistoryAndFallbackSourcesKeepRestrictedSelection)
{
    for(bool native:{false,true})
    {
        synth2::RenderControls p;
        synth2::render::RenderPolicyInputs input;input.nativeSource=native;
        p.unisonVoices=4;p.osc1FmAmount=.5f;p.modEnv.on=true;
        p.lfo1.on=true;p.lfo1.wave=4;p.lfo1.dest=0;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_FALSE(p.nativeVoicePerformance);
    }
}

TEST(RenderPolicy, NativeCompositionRetainsExplicitNoiseLifecycleRules)
{
    synth2::RenderControls p;
    synth2::render::RenderPolicyInputs input;input.nativeSource=true;
    p.osc1WaveIndex=1.f/3;p.osc2WaveIndex=4.f/3;p.unisonVoices=1;p.filterType=0;p.resonanceNorm=0;
    p.filterEnvAmount=p.filterTracking=p.filterKeyTrackNorm=0;
    synth2::render::selectRenderPolicy(input,p);
    EXPECT_TRUE(p.nativeResonantNoise);EXPECT_FALSE(p.nativeIdleFilterReset);
    p.playMode=1;p.arpeggiator.enabled=true;p.arpeggiator.type=1;p.arpeggiator.gate=64;
    synth2::render::selectRenderPolicy(input,p);
    EXPECT_TRUE(p.nativeIdleFilterReset);EXPECT_FALSE(p.nativeResonantNoise);
}

TEST(RenderPolicy, CommonCompositionKeepsControlsAcrossFallbackAdmissionBoundaries)
{
    for(int wave=0;wave<6;++wave)for(int destination=0;destination<7;++destination)
        for(float resonance:{111.9999f,112.f,112.0001f,127.f})
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=input.randomHistoryValid=true;
        input.modulationAmountDial=42;input.modulationAttackDial=23;input.modulationDecayDial=91;
        input.stackDial=7;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.wave=p.lfo2.wave=wave;
        p.lfo1.dest=p.lfo2.dest=destination;p.unisonVoices=4;
        p.modEnv.on=true;p.resonanceNorm=resonance/127;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_TRUE(p.nativeFilterEnvelope);EXPECT_TRUE(p.nativeOrdinaryStateVariable);
        EXPECT_EQ(p.modEnv.amountDial,42);EXPECT_EQ(p.modEnv.attackDial,23);EXPECT_EQ(p.modEnv.decayDial,91);
        EXPECT_EQ(p.osc1StackDial,7);EXPECT_TRUE(p.nativeVoicePerformance);
    }
}

TEST(RenderPolicy, AmplitudeReceiversAndGainDoNotDependOnSourceFormat)
{
    for(bool source:{false,true})for(bool history:{false,true})
    for(int first=0;first<4;++first)for(int second=0;second<4;++second)
    for(bool sync:{false,true})for(bool key:{false,true})for(int mode=0;mode<3;++mode)
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=source;input.randomHistoryValid=history;p.playMode=mode;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.dest=p.lfo2.dest=3;
        p.lfo1.wave=first;p.lfo2.wave=second;
        p.lfo1.tempoSync=p.lfo2.tempoSync=sync;p.lfo1.keySync=p.lfo2.keySync=key;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_TRUE(p.lfo1.nativeSharedClock);EXPECT_TRUE(p.lfo2.nativeSharedClock);
    }
}

TEST(RenderPolicy, DeterministicAmplitudeDoesNotAdmitUnknownRandomHistory)
{
    for(int wave:{4,5})
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=true;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.dest=p.lfo2.dest=3;
        p.lfo1.wave=0;p.lfo2.wave=wave;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_TRUE(p.lfo1.nativeSharedClock);EXPECT_FALSE(p.lfo2.nativeSharedClock);
        EXPECT_FALSE(input.randomHistoryValid);
    }
}

TEST(RenderPolicy, DeterministicReceiversAndEnvelopesDoNotDependOnSourceAdmission)
{
    for(bool source:{false,true})for(bool history:{false,true})
    for(int wave=0;wave<4;++wave)for(int destination=0;destination<7;++destination)
    for(int envelope:{0,2,3})for(int mode=0;mode<3;++mode)
    for(bool sync:{false,true})for(bool key:{false,true})
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=source;input.randomHistoryValid=history;
        input.modulationAmountDial=91;input.modulationAttackDial=26;input.modulationDecayDial=100;
        p.playMode=mode;p.modEnv.on=true;p.modEnv.dest=envelope;p.unisonVoices=2;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.wave=wave;p.lfo2.wave=(wave+1)%4;
        p.lfo1.dest=p.lfo2.dest=destination;
        p.lfo1.tempoSync=p.lfo2.tempoSync=sync;p.lfo1.keySync=p.lfo2.keySync=key;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_TRUE(p.lfo1.nativeSharedClock);EXPECT_TRUE(p.lfo2.nativeSharedClock);
        EXPECT_EQ(p.nativePitchLfo,destination==0 || destination==1);
        EXPECT_EQ(p.nativeFilterLfo,destination==2);EXPECT_EQ(p.nativePulseWidthLfo,destination==4);
        EXPECT_EQ(p.nativeFmLfo,destination==5);EXPECT_EQ(p.nativePanLfo,destination==6);
        EXPECT_EQ(p.modEnv.nativePitch,envelope==0);EXPECT_EQ(p.modEnv.nativeFm,envelope==2);
        EXPECT_EQ(p.modEnv.nativePulseWidth,envelope==3);
        EXPECT_EQ(p.modEnv.amountDial,91);EXPECT_EQ(p.modEnv.attackDial,26);EXPECT_EQ(p.modEnv.decayDial,100);
    }
}

TEST(RenderPolicy, UnverifiedRandomSourcesStillNeedBothSourceAndHistoryAdmission)
{
    for(bool source:{false,true})for(bool history:{false,true})for(int wave:{4,5})
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=source;input.randomHistoryValid=history;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.wave=0;p.lfo1.dest=3;p.lfo2.wave=wave;p.lfo2.dest=3;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_TRUE(p.lfo1.nativeSharedClock);
        EXPECT_EQ(p.lfo2.nativeSharedClock,source && history);
    }
}

TEST(RenderPolicy, LegacyRandomArpKeepsRestrictedCompositionWithDeterministicLfos)
{
    for(bool history:{false,true})for(bool lfo:{false,true})for(int type=1;type<=4;++type)
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.randomHistoryValid=history;p.arpeggiator.enabled=true;p.arpeggiator.type=type;
        p.lfo1.on=lfo;p.lfo1.dest=3;p.lfo1.wave=0;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_EQ(p.nativeVoicePerformance,type!=4);
        if(lfo)EXPECT_TRUE(p.lfo1.nativeSharedClock);
    }
}

TEST(RenderPolicy, SubLevelMappingDoesNotSelectAnAlternateScaleForRandomHistories)
{
    for(bool source:{false,true})for(bool history:{false,true})for(int dial:{0,1,64,127})
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.nativeSource=source;input.randomHistoryValid=history;input.subLevelDial=dial;
        p.lfo1.on=true;p.lfo1.wave=4;p.ringMod=1;p.subOscGain=dial/127.f;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_EQ(p.subOscGain,float(dial*4)*(1.f/127.f));
        EXPECT_EQ(p.ringMod,1);
    }
}
