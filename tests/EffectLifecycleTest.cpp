#include <gtest/gtest.h>
#include "dsp/DelayKernel.h"
#include "dsp/OutputPipeline.h"
#include <memory>
#include <vector>

namespace
{
using synth2::dsp::DelayKernel;
auto delay(int rate=48000)
{
    auto result=std::make_unique<DelayKernel>();
    EXPECT_TRUE(result->configure(rate,0,.01f,0,64,127));
    result->reset();
    return result;
}
void impulse(DelayKernel& effect)
{
    float dry[2]{1,0},send[2]{};
    ASSERT_TRUE(effect.process(dry,send,1));
}
}

TEST(EffectLifecycle, SilentDelayInputStillProducesItsPendingEcho)
{
    auto effect=delay();
    impulse(*effect);
    std::vector<float> dry(2048),send(2048);
    int silent=1;
    ASSERT_TRUE(effect->process(dry.data(),send.data(),1024,&silent));
    EXPECT_EQ(silent,0);
    float energy=0;
    for (float sample:send) { ASSERT_TRUE(std::isfinite(sample)); energy+=float(sample)*sample; }
    EXPECT_GT(energy,0);
}

TEST(EffectLifecycle, ResetClearsDelayTailAndRestoresFreshProcessing)
{
    auto used=delay(),fresh=delay();
    impulse(*used);
    used->reset();
    std::vector<float> dry(2048),send(2048);
    int silent=1;
    EXPECT_FALSE(used->process(dry.data(),send.data(),1024,&silent));
    EXPECT_EQ(silent,1);
    for(float sample:send) EXPECT_EQ(sample,0);
    impulse(*used); impulse(*fresh);
    std::vector<float> otherDry(2048),otherSend(2048);
    ASSERT_TRUE(used->process(dry.data(),send.data(),1024));
    ASSERT_TRUE(fresh->process(otherDry.data(),otherSend.data(),1024));
    EXPECT_EQ(dry,otherDry); EXPECT_EQ(send,otherSend);
}

TEST(EffectLifecycle, ExplicitRateReconfigurationAndResetMatchAFreshDelay)
{
    auto used=delay();
    impulse(*used);
    for(int rate:{44100,96000})
    {
        SCOPED_TRACE(rate);
        ASSERT_TRUE(used->configure(rate,0,.01f,0,64,127));
        used->reset();
        auto fresh=delay(rate);
        impulse(*used); impulse(*fresh);
        std::vector<float> dry(4096),send(4096),otherDry(4096),otherSend(4096);
        ASSERT_TRUE(used->process(dry.data(),send.data(),2048));
        ASSERT_TRUE(fresh->process(otherDry.data(),otherSend.data(),2048));
        EXPECT_EQ(dry,otherDry); EXPECT_EQ(send,otherSend);
    }
}

TEST(EffectLifecycle, OutputGainAdvancesThroughSilenceAndRespectsRequestedRegion)
{
    using namespace synth2::dsp;
    auto silent=std::make_unique<OutputPipeline>(),audible=std::make_unique<OutputPipeline>();
    silent->prepareGain(48000,false); audible->prepareGain(48000,false);
    OutputPipelineSettings settings;
    OutputProcessContext context;
    context.sampleRate=48000; context.gainBlockCapacity=64; context.bypassEffects=true;
    synth2::audio::AudioBuffer<float> zero(2,80),signal(2,80);
    for (int channel=0;channel<2;++channel) for (int i=0;i<80;++i)
    {
        zero.setSample(channel,i,(i<8 || i>=72) ? 7.f : 0.f);
        signal.setSample(channel,i,1);
    }
    context.inputSilent=true;
    silent->process(zero,8,64,settings,context,{});
    context.inputSilent=false;
    audible->process(signal,8,64,settings,context,{});
    const float initialGain=signal.getSample(0,8);
    ASSERT_GT(initialGain,0);
    ASSERT_LT(initialGain,1);
    for (int channel=0;channel<2;++channel) for (int i=0;i<80;++i)
        EXPECT_EQ(zero.getSample(channel,i),(i<8 || i>=72) ? 7.f : 0.f);
    // Equal elapsed time must leave equal gain history despite different inputs.
    for (int channel=0;channel<2;++channel) for (int i=0;i<80;++i)
    { zero.setSample(channel,i,1); signal.setSample(channel,i,1); }
    silent->process(zero,8,64,settings,context,{});
    audible->process(signal,8,64,settings,context,{});
    for (int channel=0;channel<2;++channel) for (int i=0;i<80;++i)
        EXPECT_EQ(zero.getSample(channel,i),signal.getSample(channel,i));
    EXPECT_GT(zero.getSample(0,8),initialGain);
}
