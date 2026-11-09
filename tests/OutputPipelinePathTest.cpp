#include <gtest/gtest.h>
#include "dsp/OutputPipeline.h"
#include <memory>
#include <cmath>

namespace
{
synth2::dsp::OutputGain settleGain(synth2::dsp::OutputPipeline& pipeline,double rate)
{
    pipeline.prepareGain(rate,false);
    synth2::dsp::OutputGain reference;reference.prepare(rate);
    synth2::dsp::OutputPipelineSettings settings;synth2::dsp::OutputProcessContext context;
    context.sampleRate=rate;context.bypassEffects=true;
    synth2::audio::AudioBuffer<float> zero(2,64);zero.clear();
    for(int i=0;i<512;++i)
    {
        pipeline.process(zero,0,64,settings,context,{});
        for(int word=0;word<128;++word)reference.next(synth2::dsp::OutputGain::defaultTarget);
        reference.finish(synth2::dsp::OutputGain::defaultTarget);
    }
    return reference;
}
}

TEST(OutputPipelinePath, WetProcessingAlwaysUsesTheCurrentKernelWithinItsRateDomain)
{
    for(double rate:{10000.,44100.,48000.25,384000.})for(int channels:{1,2})
    {
        auto pipeline=std::make_unique<synth2::dsp::OutputPipeline>();
        auto reference=std::make_unique<synth2::dsp::EffectPair>();
        pipeline->prepareEffects({rate,64,static_cast<uint32_t>(channels)},false,false);pipeline->prepareProcessors(rate,false);
        reference->prepare(rate);
        settleGain(*pipeline,rate);
        synth2::dsp::Equalizer eq;eq.prepare(rate);
        synth2::dsp::OutputPipelineSettings settings;
        settings.delay.enable=true;settings.delay.time=0;settings.delay.level=1;
        settings.delay.feedback=settings.delay.spread=settings.delay.tone=64.f/127;
        settings.chorus.enabled=true;settings.chorus.type=2;settings.chorus.level=.5f;
        reference->setParams(settings.delay,settings.chorus,120);
        eq.setParams(settings.eq);
        synth2::dsp::OutputProcessContext context;context.sampleRate=rate;
        synth2::audio::AudioBuffer<float> actual(channels,528),expected(channels,528);
        for(int c=0;c<channels;++c)for(int i=0;i<528;++i)
        {
            const float sample=i<8 || i>=520 ? 7.f : i==8 ? .25f : 0.f;
            actual.setSample(c,i,sample);expected.setSample(c,i,sample);
        }
        for(int i=8;i<520;i+=64)
        {
            pipeline->process(actual,i,64,settings,context,{});
            for(int c=0;c<channels;++c)for(int frame=i;frame<i+64;++frame)
                expected.setSample(c,frame,eq.processSample(expected.getSample(c,frame),c));
            reference->process(expected.getWritePointer(0,i),channels==2 ? expected.getWritePointer(1,i) : nullptr,64);
        }
        for(int c=0;c<channels;++c)for(int i=0;i<528;++i)
            EXPECT_EQ(actual.getSample(c,i),expected.getSample(c,i))<<rate<<" channels="<<channels<<" frame="<<i;
    }
}

TEST(OutputPipelinePath, UnsupportedWetRatesBypassWithoutSelectingAnotherAlgorithm)
{
    for(double rate:{8000.,9999.,384001.})
    {
        auto pipeline=std::make_unique<synth2::dsp::OutputPipeline>();
        pipeline->prepareEffects({rate,64,2},false,false);pipeline->prepareProcessors(rate,false);
        auto gain=settleGain(*pipeline,rate);
        synth2::dsp::OutputPipelineSettings settings;
        settings.delay.enable=true;settings.delay.time=0;settings.delay.level=1;
        settings.chorus.enabled=true;settings.chorus.level=1;
        synth2::dsp::OutputProcessContext context;context.sampleRate=rate;
        synth2::audio::AudioBuffer<float> audio(2,80);
        for(int c=0;c<2;++c)for(int i=0;i<80;++i)audio.setSample(c,i,static_cast<float>(i-40)/80);
        synth2::audio::AudioBuffer<float> expected(audio);
        synth2::dsp::Equalizer eq;eq.prepare(rate);eq.setParams(settings.eq);
        for(int c=0;c<2;++c)for(int i=8;i<72;++i)expected.setSample(c,i,eq.processSample(expected.getSample(c,i),c));
        for(int i=8;i<72;++i)for(int c=0;c<2;++c)
            expected.setSample(c,i,expected.getSample(c,i)*(gain.next(synth2::dsp::OutputGain::defaultTarget)/synth2::dsp::OutputGain::defaultTarget));
        pipeline->process(audio,8,64,settings,context,{});
        for(int c=0;c<2;++c)for(int i=0;i<80;++i)EXPECT_EQ(audio.getSample(c,i),expected.getSample(c,i));
    }
}

TEST(OutputPipelinePath, ReleaseRetainsOrClearsTheCurrentEqualizerHistory)
{
    auto used=std::make_unique<synth2::dsp::OutputPipeline>();
    auto held=std::make_unique<synth2::dsp::OutputPipeline>();
    for(auto* pipeline:{used.get(),held.get()})
    {pipeline->prepareEffects({44100,64,2},false,false);pipeline->prepareProcessors(44100,false);settleGain(*pipeline,44100);}
    synth2::dsp::OutputPipelineSettings settings;
    settings.eq.tone=32.f/127;settings.eq.level=64.f/127;
    synth2::dsp::OutputProcessContext context;context.sampleRate=44100;
    synth2::audio::AudioBuffer<float> first(2,64),second(2,64);
    first.clear();second.clear();first.setSample(0,0,.25f);second.setSample(0,0,.25f);
    used->process(first,0,64,settings,context,{});held->process(second,0,64,settings,context,{});
    used->release(true);first.clear();second.clear();
    used->process(first,0,64,settings,context,{});held->process(second,0,64,settings,context,{});
    double energy=0;
    for(int c=0;c<2;++c)for(int i=0;i<64;++i)
    {EXPECT_EQ(first.getSample(c,i),second.getSample(c,i));energy+=first.getSample(c,i)*first.getSample(c,i);}
    EXPECT_GT(energy,0);
    used->release(false);first.clear();used->process(first,0,64,settings,context,{});
    for(int c=0;c<2;++c)for(int i=0;i<64;++i)EXPECT_EQ(first.getSample(c,i),0);
}
TEST(PanInputs, CubicPanRunsAfterNonlinearEffectsForEverySourceHistory)
{
    using namespace synth2::dsp;
    auto actual=std::make_unique<OutputPipeline>(),reference=std::make_unique<OutputPipeline>(),wrongOrder=std::make_unique<OutputPipeline>();
    for(auto* pipeline:{actual.get(),reference.get(),wrongOrder.get()})
    {pipeline->prepareEffects({44100,64,2},false,false);pipeline->prepareProcessors(44100,false);settleGain(*pipeline,44100);}
    OutputPipelineSettings settings;settings.panLfo=true;settings.pan=.25f;settings.nativeMultiEffectType=0;
    settings.nativeMultiEffectDials={100,64,64};settings.chorus.enabled=false;settings.delay.enable=false;
    OutputPipelineSettings withoutPan=settings;withoutPan.panLfo=false;withoutPan.pan=0;
    OutputProcessContext context;context.sampleRate=44100;
    synth2::audio::AudioBuffer<float> got(2,64),expected(2,64),wrong(2,64);
    for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
    {const float sample=static_cast<float>((i*17+channel*7)%31-15)/16;got.setSample(channel,i,sample);expected.setSample(channel,i,sample);wrong.setSample(channel,i,sample);}
    VoiceControlBuffer control{};control.fill(32);
    ReceiverLfoView view;view.available=true;view.receivers[0]={true,6};view.values[0]=control.data();
    actual->process(got,0,64,settings,context,view);reference->process(expected,0,64,withoutPan,context,{});
    PanLfo post,pre;post.prepare(44100);pre.prepare(44100);
    for(int i=0;i<64;++i)
    {
        float l=expected.getSample(0,i),r=expected.getSample(1,i);post.process(l,r,32,.25f);expected.setSample(0,i,l);expected.setSample(1,i,r);
        l=wrong.getSample(0,i);r=wrong.getSample(1,i);pre.process(l,r,32,.25f);wrong.setSample(0,i,l);wrong.setSample(1,i,r);
    }
    wrongOrder->process(wrong,0,64,withoutPan,context,{});
    double difference=0;
    for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
    {EXPECT_EQ(got.getSample(channel,i),expected.getSample(channel,i));difference+=std::abs(got.getSample(channel,i)-wrong.getSample(channel,i));}
    EXPECT_GT(difference,.001);
}
#include "render/OutputConfiguration.h"
TEST(StaticPanInputs, GlobalBalanceFollowsDriveForMonoStereoAndBothHistoryStates)
{
    using namespace synth2::dsp;
    for(int channels:{1,2})for(bool admitted:{false,true})
    {
        auto actual=std::make_unique<OutputPipeline>(),reference=std::make_unique<OutputPipeline>(),wrongOrder=std::make_unique<OutputPipeline>();
        for(auto* pipeline:{actual.get(),reference.get(),wrongOrder.get()})
        {pipeline->prepareEffects({44100,64,static_cast<uint32_t>(channels)},false,false);pipeline->prepareProcessors(44100,false);settleGain(*pipeline,44100);}
        synth2::RenderControls p;p.pan=.5f;p.nativeMultiEffectType=0;
        p.nativeMultiEffectDials={100,64,64};p.chorus.enabled=false;p.delay.enable=false;
        const auto settings=synth2::render::outputSettings(p);
        auto withoutPan=settings;withoutPan.pan=0;
        OutputProcessContext context;context.sampleRate=44100;
        synth2::audio::AudioBuffer<float> got(channels,80),expected(channels,80),wrong(channels,80);
        for(int channel=0;channel<channels;++channel)for(int i=0;i<80;++i)
        {
            const float sample=i<8 || i>=72 ? 7.f : static_cast<float>((i*17+channel*7)%31-15)/16;
            got.setSample(channel,i,sample);expected.setSample(channel,i,sample);wrong.setSample(channel,i,sample);
        }
        actual->process(got,8,64,settings,context,{});reference->process(expected,8,64,withoutPan,context,{});
        const float left=channels==1 ? .75f : .5f;
        expected.applyGain(0,8,64,left);wrong.applyGain(0,8,64,left);
        wrongOrder->process(wrong,8,64,withoutPan,context,{});
        double difference=0;
        for(int channel=0;channel<channels;++channel)for(int i=0;i<80;++i)
        {EXPECT_EQ(got.getSample(channel,i),expected.getSample(channel,i));difference+=std::abs(got.getSample(channel,i)-wrong.getSample(channel,i));}
        EXPECT_GT(difference,.001);
    }
}
