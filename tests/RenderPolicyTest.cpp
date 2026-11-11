#include <gtest/gtest.h>
#include "RenderControls.h"
#include "render/RenderPolicy.h"
#include "render/VoiceConfiguration.h"

TEST(RenderPolicy, EveryWaveformAndDestinationRetainsCommonControls)
{
    for(int first=0;first<6;++first)for(int second=0;second<6;++second)
    for(int destination=0;destination<7;++destination)for(int mode=0;mode<3;++mode)
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.wave=first;p.lfo2.wave=second;
        p.lfo1.dest=p.lfo2.dest=destination;p.playMode=mode;p.unisonVoices=4;
        input.stackDial=7;input.modulationAmountDial=42;
        input.modulationAttackDial=23;input.modulationDecayDial=91;
        p.modEnv.on=true;p.modEnv.dest=2;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_EQ(p.hasPitchLfo(),destination==0 || destination==1);
        EXPECT_EQ(p.hasLfo(2),destination==2);EXPECT_EQ(p.hasLfo(5),destination==5);
        EXPECT_EQ(p.hasLfo(6),destination==6);EXPECT_EQ(p.hasLfo(4),destination==4);
        EXPECT_EQ(p.osc1StackDial,7);EXPECT_EQ(p.modEnv.amountDial,42);
        EXPECT_EQ(p.modEnv.attackDial,23);EXPECT_EQ(p.modEnv.decayDial,91);
        const auto config=synth2::render::oscillatorControls(p,44100,false);
        EXPECT_TRUE(config.envelopeEnabled);EXPECT_TRUE(config.envelopeFm);
        EXPECT_EQ(config.fmLfo,destination==5);
    }
}
TEST(RenderPolicy, ExplicitIdleResetDependsOnControls)
{
    synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
    p.unisonVoices=1;p.osc1WaveIndex=1.f/3;p.osc2WaveIndex=4.f/3;p.filterType=0;p.resonanceNorm=0;
    synth2::render::selectRenderPolicy(input,p);EXPECT_FALSE(p.resetIdleFilter);
    p.playMode=1;p.arpeggiator.enabled=true;p.arpeggiator.type=1;p.arpeggiator.gate=64;
    synth2::render::selectRenderPolicy(input,p);EXPECT_TRUE(p.resetIdleFilter);
    p.lfo1.on=true;synth2::render::selectRenderPolicy(input,p);EXPECT_FALSE(p.resetIdleFilter);
}
TEST(RenderPolicy, EffectsAndSubLevelKeepTheirMappingsAcrossRandomControls)
{
    for(int wave=0;wave<6;++wave)for(int dial:{0,1,64,127})for(int effect=0;effect<10;++effect)
    {
        synth2::RenderControls p;synth2::render::RenderPolicyInputs input;
        input.effectEnabled=true;input.effectType=effect;input.subLevelDial=dial;
        p.lfo1.on=true;p.lfo1.wave=wave;p.ringMod=1;
        synth2::render::selectRenderPolicy(input,p);
        EXPECT_EQ(p.subOscGain,float(dial*4)/127.0f);EXPECT_EQ(p.ringMod,1);
        EXPECT_EQ(p.nativeMultiEffectType,effect<=5 ? effect : -1);
        EXPECT_EQ(p.nativePhaser,effect>=6);
        EXPECT_EQ(p.nativePhaserPairs,effect==7 ? 2 : effect==8 ? 4 : effect==9 ? 6 : 1);
    }
}
