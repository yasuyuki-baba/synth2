#include <gtest/gtest.h>
#include "dsp/DelayKernel.h"
#include "dsp/EffectPair.h"
#include "dsp/EffectSettings.h"
#include <array>
#include <cmath>
#include <memory>

namespace {
using synth2::dsp::DelayKernel;
auto makeDelay(int kind=0,int tone=64)
{
    auto delay=std::make_unique<DelayKernel>();
    EXPECT_TRUE(delay->configure(44100,kind,.01f,0,50,100,tone));
    return delay;
}
void signal(std::array<float,128>& data,int frame)
{
    for(int i=0;i<64;++i)
    {
        data[2*i]=.1f*std::sin((frame+i)*.037f);
        data[2*i+1]=.1f*std::cos((frame+i)*.031f);
    }
}
}

TEST(DelayPath, FractionalEditHistoryDoesNotSelectDifferentProcessingAfterReset)
{
    for(int kind:{0,1,2})for(int tone:{63,64,65})
    {
        auto cold=makeDelay(kind,tone),edited=makeDelay(kind,tone);
        edited->setFrameTargets(cold->getLeftFrames()+.25f,cold->getRightFrames()+.75f);
        edited->setTimeSeconds(.01f);
        edited->setToneDial(64.25f);edited->setToneDial(static_cast<float>(tone));
        cold->reset();edited->reset();
        std::array<float,128>a{},b{},sa{},sb{};
        for(int frame=0;frame<44100*2;frame+=64)
        {
            if(frame==4096){cold->setTimeSeconds(.02f);edited->setTimeSeconds(.02f);}
            signal(a,frame);b=a;
            ASSERT_TRUE(cold->process(a.data(),sa.data(),64));
            ASSERT_TRUE(edited->process(b.data(),sb.data(),64));
            EXPECT_EQ(a,b);EXPECT_EQ(sa,sb);
            EXPECT_EQ(cold->getCurrentLeft(),edited->getCurrentLeft());
            EXPECT_EQ(cold->isSlewing(),edited->isSlewing());
        }
    }
}

TEST(DelayPath, ProcessingPartitionDoesNotChangeFractionalOrIntegralSlew)
{
    for(int kind:{0,1,2})for(float fraction:{0.f,.25f})
    {
        auto block=makeDelay(kind,37),single=makeDelay(kind,37);
        block->reset();single->reset();
        std::array<float,128>a{},b{},sa{},sb{};
        for(int frame=0;frame<4096;frame+=64)
        {
            if(frame==1024)
            {
                block->setFrameTargets(800+fraction,900+fraction);
                single->setFrameTargets(800+fraction,900+fraction);
            }
            signal(a,frame);b=a;
            ASSERT_TRUE(block->process(a.data(),sa.data(),64));
            for(int i=0;i<64;++i)ASSERT_TRUE(single->process(b.data()+2*i,sb.data()+2*i,1));
            EXPECT_EQ(a,b);EXPECT_EQ(sa,sb);
            EXPECT_EQ(block->getCurrentRight(),single->getCurrentRight());
        }
    }
}

TEST(DelayPath, EveryCoordinateUsesTheSameSettlingRule)
{
    for(float fraction:{0.f,.25f})
    {
        auto delay=makeDelay();
        const float base=delay->getLeftFrames()+fraction;
        delay->setFrameTargets(base,base);delay->reset();
        float dry[2]{1,0},send[2]{};
        ASSERT_TRUE(delay->process(dry,send,1));
        delay->setFrameTargets(base+.0625f,base+.0625f);
        dry[0]=0;ASSERT_TRUE(delay->process(dry,send,1));
        EXPECT_FALSE(delay->isSlewing());
        EXPECT_EQ(delay->getCurrentLeft(),base+.0625f);
    }
}

TEST(DelayPath, IntegralFrameTargetsPreserveTheExactImpulseEndpoint)
{
    auto delay=std::make_unique<DelayKernel>();
    ASSERT_TRUE(delay->configure(44100,0,.01f,0,0,127,64));
    const int endpoint=delay->getLeftFrames();
    ASSERT_TRUE(delay->setFrameTargets(endpoint,endpoint));delay->reset();
    for(int frame=0;frame<=endpoint+1;++frame)
    {
        float dry[2]{frame==0 ? 1.f : 0.f,0},send[2]{};
        ASSERT_TRUE(delay->process(dry,send,1));
        EXPECT_EQ(send[0],frame==endpoint ? 1.f : 0.f);EXPECT_EQ(send[1],0);
    }
}

TEST(DelayPath, AdjacentFloatTempoUsesAlignedFrameWords)
{
    synth2::DelaySettings settings;
    settings.enable=true;settings.time=7.0f/127;settings.spread=settings.tone=64.0f/127;
    settings.feedback=0;settings.level=1;settings.type=0;
    synth2::ChorusSettings chorus;chorus.enabled=false;
    const float tempo=std::nextafter(120.0f,121.0f);
    ASSERT_GT(tempo,120.0f);
    auto cold=std::make_unique<synth2::dsp::EffectPair>();
    auto changed=std::make_unique<synth2::dsp::EffectPair>();
    cold->prepare(48000);changed->prepare(48000);
    cold->setParams(settings,chorus,tempo);
    changed->setParams(settings,chorus,120);changed->setParams(settings,chorus,tempo);
    // Native adjacent tempo words give aligned endpoints 9000 and 8996.
    // Float32 BPM uses the adjacent native aligned frame word.
    for(int frame=0;frame<=9001;++frame)
    {
        float left=frame==0 ? 1.0f : 0.0f,right=0,editedLeft=left,editedRight=right;
        cold->process(&left,&right,1);changed->process(&editedLeft,&editedRight,1);
        EXPECT_EQ(left,editedLeft);EXPECT_EQ(right,editedRight);
        EXPECT_EQ(left,frame==8996 ? 1.0f : 0.0f);
        EXPECT_EQ(right,0.0f);
    }
}
