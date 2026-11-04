#include <gtest/gtest.h>
#include "dsp/OutputPipeline.h"
#include "dsp/FractionalDial.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
using Stereo=std::array<std::vector<float>,2>;
Stereo impulse(int count=2048)
{
    Stereo result{std::vector<float>(count),std::vector<float>(count)};
    result[0][0]=.25f;
    return result;
}
void finite(const Stereo& audio)
{ for(const auto& channel:audio) for(float value:channel) ASSERT_TRUE(std::isfinite(value)); }
double tailEnergy(const std::vector<float>& audio)
{ double energy=0; for(size_t i=1;i<audio.size();++i) energy+=double(audio[i])*audio[i]; return energy; }
}

TEST(EffectsBehavior, PipelineBypassPreservesDrySamplesWithAllEffectGroupsEnabled)
{
    auto pipeline=std::make_unique<synth2::dsp::OutputPipeline>();
    pipeline->prepareEffects({48000,64,2},false,false);
    pipeline->prepareProcessors(48000,false);
    synth2::dsp::OutputPipelineSettings settings;
    settings.nativePhaser=true; settings.nativeMultiEffectType=2;
    settings.eq.tone=0; settings.delay.enable=true; settings.chorus.enabled=true;
    synth2::dsp::OutputProcessContext context;
    context.bypassEffects=true; context.sampleRate=48000;
    pipeline->prepareGain(48000,false);
    synth2::audio::AudioBuffer<float> warm(2,64);warm.clear();
    for(int i=0;i<256;++i)pipeline->process(warm,0,64,settings,context,{});
    synth2::audio::AudioBuffer<float> audio(2,64);
    for(int ch=0;ch<2;++ch) for(int i=0;i<64;++i) audio.setSample(ch,i,(ch==0 ? 1.f : -1.f)*i/128);
    pipeline->process(audio,0,64,settings,context,{});
    for(int ch=0;ch<2;++ch) for(int i=0;i<64;++i)
        EXPECT_EQ(audio.getSample(ch,i),(ch==0 ? 1.f : -1.f)*i/128);
}

TEST(EffectsBehavior, DisabledDelayAndChorusPreserveDryAudio)
{
    auto pair=std::make_unique<synth2::dsp::EffectPair>();
    pair->prepare(48000);
    synth2::DelaySettings delay; synth2::ChorusSettings chorus;
    pair->setParams(delay,chorus,120);
    auto audio=impulse(); const auto before=audio;
    pair->process(audio[0].data(),audio[1].data(),audio[0].size());
    EXPECT_EQ(audio,before);
}

TEST(EffectsBehavior, StereoDelayAndSingleVoiceChorusDoNotLeakIntoTheOtherChannel)
{
    for(bool useDelay:{false,true})
    {
        SCOPED_TRACE(useDelay);
        auto pair=std::make_unique<synth2::dsp::EffectPair>();
        pair->prepare(48000);
        synth2::DelaySettings delay; synth2::ChorusSettings chorus;
        delay.enable=useDelay; delay.type=0; delay.level=1; delay.time=0;
        delay.spread=64.f/127; delay.tone=64.f/127;
        chorus.enabled=!useDelay; chorus.type=1; chorus.depth=0;
        chorus.time=.5f; chorus.feedback=64.f/127; chorus.level=1;
        pair->setParams(delay,chorus,120);
        auto audio=impulse();
        pair->process(audio[0].data(),audio[1].data(),audio[0].size());
        finite(audio);
        EXPECT_GT(tailEnergy(audio[0]),0);
        for(float value:audio[1]) EXPECT_EQ(value,0);
    }
}

TEST(EffectsBehavior, WetPairResetAndPrepareRestoreFreshImpulseResponse)
{
    for(double rate:{44100.,48000.,96000.}) for(int voices:{1,2,4}) for(bool fractional:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(voices);
        synth2::DelaySettings delay; synth2::ChorusSettings chorus;
        delay.enable=true; delay.level=.5f; delay.feedback=.4f;
        chorus.enabled=true; chorus.type=voices; chorus.level=.6f;
        if(!fractional)
        {
            delay.level=64.f/127;delay.feedback=51.f/127;delay.spread=delay.tone=64.f/127;
            chorus.time=chorus.depth=chorus.rate=64.f/127;
            chorus.feedback=25.f/127;chorus.level=76.f/127;
        }
        auto used=std::make_unique<synth2::dsp::EffectPair>();
        used->prepare(48000); used->setParams(delay,chorus,120);
        auto history=impulse(); used->process(history[0].data(),history[1].data(),history[0].size());
        used->prepare(rate); used->setParams(delay,chorus,120);
        for(bool reset:{false,true})
        {
            if(reset) { used->reset(); used->setParams(delay,chorus,120); }
            auto fresh=std::make_unique<synth2::dsp::EffectPair>();
            fresh->prepare(rate); fresh->setParams(delay,chorus,120);
            auto a=impulse(),b=a;
            used->process(a[0].data(),a[1].data(),a[0].size());
            fresh->process(b[0].data(),b[1].data(),b[0].size());
            finite(a); EXPECT_EQ(a,b);
        }
    }
}

TEST(EffectsBehavior, PhaserKeepsChannelHistorySeparateAndColdPrepareClearsIt)
{
    for(double rate:{44100.,96000.}) for(int pairs:{1,6})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(pairs);
        synth2::dsp::Phaser used,fresh;
        used.prepare(48000); used.setDials(64,64,96,pairs);
        auto history=impulse(); used.process(history[0].data(),history[1].data(),history[0].size());
        used.prepare(rate,false); fresh.prepare(rate,false);
        used.setDials(64,64,96,pairs); fresh.setDials(64,64,96,pairs);
        auto a=impulse(),b=a;
        used.process(a[0].data(),a[1].data(),a[0].size());
        fresh.process(b[0].data(),b[1].data(),b[0].size());
        finite(a); EXPECT_EQ(a,b);
        EXPECT_GT(tailEnergy(a[0]),0);
        for(float value:a[1]) EXPECT_EQ(value,0);
    }
}

TEST(EffectsBehavior, AllMultiEffectsRemainFiniteAndColdPrepareRestoresFreshState)
{
    for(int type=0;type<=5;++type) for(int dial:{0,64,127}) for(double rate:{44100.,96000.})
    {
        SCOPED_TRACE(type);
        SCOPED_TRACE(dial);
        SCOPED_TRACE(rate);
        synth2::dsp::MultiEffect used,fresh;
        used.prepare(rate); used.setDials(dial,dial,dial);
        auto history=impulse(); used.process(type,history[0].data(),history[1].data(),history[0].size(),false);
        used.prepare(rate,false); fresh.prepare(rate,false);
        used.setDials(dial,dial,dial); fresh.setDials(dial,dial,dial);
        auto a=impulse(),b=a;
        used.process(type,a[0].data(),a[1].data(),a[0].size(),false);
        fresh.process(type,b[0].data(),b[1].data(),b[0].size(),false);
        finite(a); EXPECT_EQ(a,b);
        // Biased waveshapers can produce their own zero-input transient. Channel
        // isolation means the right output is unchanged by a left impulse.
        synth2::dsp::MultiEffect zeroInput;
        zeroInput.prepare(rate); zeroInput.setDials(dial,dial,dial);
        Stereo zeros{std::vector<float>(2048),std::vector<float>(2048)};
        zeroInput.process(type,zeros[0].data(),zeros[1].data(),zeros[0].size(),false);
        EXPECT_EQ(a[1],zeros[1]);
    }
}

namespace
{
Stereo wetRender(synth2::DelaySettings delay,synth2::ChorusSettings chorus,double rate=44100)
{
    auto pair=std::make_unique<synth2::dsp::EffectPair>();
    pair->prepare(rate);pair->setParams(delay,chorus,120);
    auto audio=impulse(4096);
    for(int at=0;at<4096;at+=64)pair->process(audio[0].data()+at,audio[1].data()+at,64);
    return audio;
}
}

TEST(EffectsBehavior, FractionalDialRecoversEveryIntegerNormalizationWithoutDroppingFractions)
{
    for(int dial=0;dial<=127;++dial)
    {
        EXPECT_EQ(synth2::dsp::fractionalDial(dial/127.0f),dial);
        EXPECT_EQ(synth2::dsp::fractionalDial(dial*(1.0f/127.0f)),dial);
    }
    EXPECT_NEAR(synth2::dsp::fractionalDial(64.25f/127),64.25f,1e-5f);
}

TEST(EffectsBehavior, FractionalWetEditingPreservesAllIntegerDialImpulseResponses)
{
    for(double rate:{44100.,96000.})for(int dial=0;dial<=127;++dial)
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(dial);
        synth2::DelaySettings d; synth2::ChorusSettings c;
        d.enable=true;d.time=1.0f/127;d.type=dial%3;
        d.feedback=d.level=d.tone=d.spread=dial/127.0f;
        c.enabled=true;c.type=1;c.time=c.depth=c.rate=c.feedback=c.level=dial/127.0f;
        const auto native=wetRender(d,c,rate);
        EXPECT_EQ(native,wetRender(d,c,rate));
    }
}

TEST(EffectsBehavior, EveryContinuousWetControlChangesAudioWithinOneIntegerBin)
{
    using D=synth2::DelaySettings;using C=synth2::ChorusSettings;
    for(float D::* field:{&D::time,&D::feedback,&D::level,&D::spread,&D::tone})
    {
        SCOPED_TRACE(field==&D::time ? "delay time" : field==&D::feedback ? "delay feedback" : field==&D::level ? "delay mix" : field==&D::spread ? "delay spread" : "delay tone");
        D d;C c;d.enable=true;d.time=1.0f/127;
        d.feedback=80.0f/127;d.level=96.0f/127;d.spread=d.tone=64.0f/127;
        const float start=field==&D::time ? 1.0f : 64.0f;
        d.*field=(start+.125f)/127;const auto a=wetRender(d,c);
        d.*field=(start+.375f)/127;const auto b=wetRender(d,c);
        finite(a);finite(b);EXPECT_NE(a,b);
    }
    for(float C::* field:{&C::time,&C::depth,&C::rate,&C::feedback,&C::level})
    {
        SCOPED_TRACE(field==&C::time ? "chorus time" : field==&C::depth ? "chorus depth" : field==&C::rate ? "chorus rate" : field==&C::feedback ? "chorus feedback" : "chorus level");
        D d;C c;c.enabled=true;c.type=2;
        c.time=c.depth=c.rate=64.0f/127;c.feedback=80.0f/127;c.level=96.0f/127;
        c.*field=64.125f/127;const auto a=wetRender(d,c);
        c.*field=64.375f/127;const auto b=wetRender(d,c);
        finite(a);finite(b);EXPECT_NE(a,b);
    }
}

TEST(EffectsBehavior, LiveSubIntegerWetEditsAreNotSuppressedByTheParameterCache)
{
    synth2::DelaySettings d;synth2::ChorusSettings c;
    d.enable=true;d.time=1.0f/127;
    d.level=64.125f/127;d.feedback=80.0f/127;d.tone=d.spread=64.0f/127;
    auto edited=std::make_unique<synth2::dsp::EffectPair>();
    auto held=std::make_unique<synth2::dsp::EffectPair>();
    for(auto* pair:{edited.get(),held.get()}){pair->prepare(44100);pair->setParams(d,c,120);}
    auto history=impulse(),same=history;
    edited->process(history[0].data(),history[1].data(),2048);
    held->process(same[0].data(),same[1].data(),2048);
    ASSERT_EQ(history,same);
    d.level=64.375f/127;edited->setParams(d,c,120);
    auto a=impulse(),b=a;
    edited->process(a[0].data(),a[1].data(),2048);
    held->process(b[0].data(),b[1].data(),2048);
    finite(a);finite(b);EXPECT_NE(a,b);
}
