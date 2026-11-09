#include <gtest/gtest.h>
#include "dsp/Oscillator.h"
#include "dsp/AmplitudeEnvelope.h"
#include "dsp/FilterEnvelope.h"
#include "dsp/VoiceEnvelopeConfigurator.h"
#include "SynthVoice.h"
#include "support/PooledTestVoice.h"
#include "SynthEngine.h"
#include "dsp/VoiceTuning.h"
#include <array>
#include <cmath>
#include <vector>

namespace
{
synth2::WaveTableBank sineBank()
{
    synth2::ShapeTables tables;
    tables.infos.push_back({0,2048,100000});
    for(int i=0;i<2048;++i)
        tables.data.push_back(static_cast<float>(std::sin(2*std::acos(-1.0)*i/2048)));
    synth2::WaveTableBank bank;
    bank.setShapeTables(3,std::move(tables));
    return bank;
}
}

TEST(OscillatorBehavior, GeneratedSineHasRequestedPitchAcrossRates)
{
    const auto bank=sineBank();
    for(double rate:{44100.,48000.25,96000.}) for(float frequency:{110.f,440.f,1760.f})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(frequency);
        synth2::dsp::Oscillator oscillator;
        oscillator.setSampleRate(rate); oscillator.updatePhaseIncrement(frequency);
        std::vector<double> crossings;
        float previous=0;
        for(int i=0;i<static_cast<int>(rate/2);++i)
        {
            const float value=oscillator.renderSample(3,.5f,0,&bank);
            ASSERT_TRUE(std::isfinite(value));
            if(previous<0 && value>=0) crossings.push_back(i-value/(value-previous));
            previous=value; oscillator.advancePhase();
        }
        ASSERT_GT(crossings.size(),10u);
        const double measured=(crossings.size()-1)*rate/(crossings.back()-crossings.front());
        EXPECT_NEAR(measured,frequency,.02); // Hz: interpolation/phase quantization tolerance.
    }
}

TEST(OscillatorBehavior, OctaveFrequencyDoublesPitchAndResetRestoresPhase)
{
    const auto bank=sineBank();
    synth2::dsp::Oscillator low,high;
    low.setSampleRate(48000); high.setSampleRate(48000);
    low.updatePhaseIncrement(220); high.updatePhaseIncrement(440);
    EXPECT_NEAR(double(high.getPhaseIncFixed27())/low.getPhaseIncFixed27(),2,1e-5);
    std::vector<float> first;
    for(int i=0;i<257;++i)
    { first.push_back(low.renderSample(3,.5f,0,&bank)); low.advancePhase(); }
    ASSERT_NE(low.getPhaseFixed27(),0u);
    low.reset();
    EXPECT_EQ(low.getPhaseFixed27(),0u);
    for(float expected:first)
    { EXPECT_EQ(low.renderSample(3,.5f,0,&bank),expected); low.advancePhase(); }
}

TEST(AmplitudeBehavior, AttackDecaySustainAndReleaseFollowTheirLifecycle)
{
    using Envelope=synth2::dsp::AmplitudeEnvelope;
    Envelope envelope;
    envelope.setParameters(128,256,768,256);
    envelope.noteOn();
    bool sawAttack=false,sawDecay=false;
    for(int i=0;i<1024;++i)
    {
        const auto phase=envelope.getPhase();
        const double before=envelope.getLevel();
        float gain{}; envelope.render(&gain,1);
        ASSERT_TRUE(std::isfinite(gain));
        EXPECT_GE(envelope.getLevel(),0); EXPECT_LE(envelope.getLevel(),2048);
        if(phase==Envelope::Phase::Attack)
        { sawAttack=true; EXPECT_GE(envelope.getLevel(),before); }
        if(phase==Envelope::Phase::Decay)
        { sawDecay=true; EXPECT_LE(envelope.getLevel(),before); }
    }
    EXPECT_TRUE(sawAttack); EXPECT_TRUE(sawDecay);
    EXPECT_EQ(envelope.getPhase(),Envelope::Phase::Sustain);
    EXPECT_EQ(envelope.getLevel(),768);
    envelope.noteOff();
    EXPECT_EQ(envelope.getPhase(),Envelope::Phase::Release);
    for(int i=0;i<1024;++i)
    {
        const double before=envelope.getLevel();
        float gain{}; envelope.render(&gain,1);
        EXPECT_LE(envelope.getLevel(),before);
        ASSERT_TRUE(std::isfinite(gain));
    }
    EXPECT_FALSE(envelope.isActive()); EXPECT_EQ(envelope.getLevel(),0);
}

TEST(AmplitudeBehavior, ResetAndZeroDurationStagesDoNotLeaveAHeldEnvelope)
{
    synth2::dsp::AmplitudeEnvelope envelope;
    envelope.setParameters(0,0,1024,0);
    envelope.noteOn();
    std::array<float,256> gains{};
    envelope.render(gains.data(),gains.size());
    EXPECT_EQ(envelope.getLevel(),1024);
    envelope.noteOff(); envelope.render(gains.data(),gains.size());
    EXPECT_FALSE(envelope.isActive());
    envelope.noteOn(); envelope.render(gains.data(),gains.size()); envelope.reset();
    EXPECT_FALSE(envelope.isActive()); EXPECT_EQ(envelope.getLevel(),0);
    envelope.render(gains.data(),gains.size());
    for(float gain:gains) EXPECT_EQ(gain,0);
}

TEST(FilterEnvelopeBehavior, InversionChangesSignAndSegmentsCoverRequestedFrames)
{
    using Envelope=synth2::dsp::FilterEnvelope;
    Envelope positive,negative;
    positive.configure(128,256,1024,256,128);
    negative.configure(128,256,1024,256,128,true);
    positive.noteOn(); negative.noteOn();
    const auto levels=[](Envelope& envelope,int frames)
    {
        std::vector<int> result;
        envelope.render(frames,[&](int level,int count){
            EXPECT_GT(count,0); EXPECT_LE(std::abs(level),1024);
            result.insert(result.end(),count,level);
        });
        EXPECT_EQ(result.size(),static_cast<size_t>(frames));
        return result;
    };
    const auto a=levels(positive,512),b=levels(negative,512);
    ASSERT_EQ(a.size(),b.size());
    for(size_t i=0;i<a.size();++i) EXPECT_EQ(a[i],-b[i]);
    EXPECT_EQ(positive.getPhase(),Envelope::Phase::Sustain);
    EXPECT_EQ(positive.getLevel(),256);
    positive.noteOff(); levels(positive,512);
    EXPECT_FALSE(positive.isActive()); EXPECT_EQ(positive.getLevel(),0);
    negative.reset();
    for(int level:levels(negative,257)) EXPECT_EQ(level,0);
}

TEST(AmplitudeBehavior, ConfigurationKeepsAssignedVelocityAndRunningEnvelopeOnEdits)
{
    using namespace synth2::dsp;
    AmplitudeEnvelope envelope;
    AmplitudeEnvelopeSettings settings{.01f,.1f,.8f,.03f,1.f,1.f};
    float reference=1;
    VoiceEnvelopeConfigurator::amplitude(envelope,reference,settings,64.f/127,44100,true);
    EXPECT_GT(reference,0);EXPECT_LT(reference,1);
    const float assigned=reference;envelope.noteOn();std::array<float,64> gains{};envelope.render(gains.data(),64);
    const auto phase=envelope.getPhase();const auto level=envelope.getLevel();const auto counter=envelope.getCounter();
    settings.sensitivity=0;settings.gain=.75f;
    VoiceEnvelopeConfigurator::amplitude(envelope,reference,settings,1,44100,false);
    EXPECT_EQ(reference,assigned);EXPECT_EQ(envelope.getPhase(),phase);
    EXPECT_EQ(envelope.getLevel(),level);EXPECT_EQ(envelope.getCounter(),counter);
    VoiceEnvelopeConfigurator::amplitude(envelope,reference,settings,1,44100,true);
    EXPECT_EQ(reference,1);
}

TEST(AmplitudeBehavior, DirectVoiceUsesPhysicalVcaAndRetiresAfterItsRelease)
{
    synth2::RenderControls params;params.ampAttackSec=.01f;params.ampReleaseSec=.01f;
    auto voice=std::make_unique<synth2::test::PooledTestVoice>(params);voice->prepare(44100);
    voice->startNote(60,1,nullptr,8192);
    std::array<uint32_t,16> words{};ASSERT_TRUE(voice->readRenderingCoreWords(0,words));
    EXPECT_EQ(words[6],static_cast<uint32_t>(synth2::dsp::AmplitudeEnvelope::Phase::Attack));
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();voice->renderNextBlock(audio,0,64);
    ASSERT_TRUE(voice->readRenderingCoreWords(0,words));EXPECT_GT(words[7],0u);
    voice->stopNote(0,true);ASSERT_TRUE(voice->readRenderingCoreWords(0,words));
    EXPECT_EQ(words[6],static_cast<uint32_t>(synth2::dsp::AmplitudeEnvelope::Phase::Release));
    const auto releaseWords=words;voice->stopNote(0,true);ASSERT_TRUE(voice->readRenderingCoreWords(0,words));
    EXPECT_EQ(words,releaseWords);
    for(int block=0;block<200;++block){audio.clear();voice->renderNextBlock(audio,0,64);}
    EXPECT_EQ(voice->getLifecyclePhase(),synth2::dsp::VoiceLifecycle::Phase::Idle);
}

TEST(AmplitudeBehavior, FullPoolReusesAReleasingCoreBeforeStealingAHeldNote)
{
    synth2::RenderControls params;params.polyphony=2;params.ampReleaseSec=1;
    synth2::dsp::OscillatorPhaseRandom phase;
    auto pool=std::make_unique<synth2::VoiceCorePool>();pool->prepare({44100,64,2},false);
    auto held=std::make_unique<synth2::SynthVoice>(params,phase,*pool);
    auto released=std::make_unique<synth2::SynthVoice>(params,phase,*pool);
    auto replacement=std::make_unique<synth2::SynthVoice>(params,phase,*pool);
    for(auto* voice:{held.get(),released.get(),replacement.get()})voice->prepare(44100);
    held->startNote(60,1,nullptr,8192);released->startNote(67,1,nullptr,8192);
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();
    held->renderNextBlock(audio,0,64);released->renderNextBlock(audio,0,64);
    std::array<uint32_t,16> heldWords{},releasedWords{},nextWords{};
    ASSERT_TRUE(held->readRenderingCoreWords(0,heldWords));ASSERT_TRUE(released->readRenderingCoreWords(0,releasedWords));
    released->stopNote(0,true);replacement->startNote(72,1,nullptr,8192);
    ASSERT_TRUE(replacement->readRenderingCoreWords(0,nextWords));EXPECT_EQ(nextWords[15],releasedWords[15]);
    EXPECT_EQ(held->getLifecyclePhase(),synth2::dsp::VoiceLifecycle::Phase::Playing);
    EXPECT_EQ(released->getLifecyclePhase(),synth2::dsp::VoiceLifecycle::Phase::Idle);
    ASSERT_TRUE(held->readRenderingCoreWords(0,nextWords));EXPECT_EQ(nextWords[15],heldWords[15]);
}

TEST(EnvelopeBehavior, DirectFilterEnvelopeUsesPhysicalStateWithoutHistoryAdmission)
{
    synth2::RenderControls p;p.cutoffIndex=256;p.filterEnvAmount=.5f;p.filterEnv.attackSec=.01f;
    auto voice=std::make_unique<synth2::test::PooledTestVoice>(p);voice->prepare(44100);voice->startNote(60,1,nullptr,8192);
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();voice->renderNextBlock(audio,0,64);
    std::array<uint32_t,16> words{};ASSERT_TRUE(voice->readRenderingCoreWords(0,words));
    EXPECT_GT(words[10],0u);EXPECT_GT(words[11],0u);EXPECT_FALSE(p.nativeFilterEnvelope);
}

TEST(EnvelopeBehavior, DirectOscillatorEnvelopeUsesResolvedDialsWithoutReceiverAdmission)
{
    synth2::RenderControls on;on.modEnv.on=true;on.modEnv.dest=0;
    on.modEnv.amountDial=127;on.modEnv.attackDial=0;on.modEnv.decayDial=127;
    auto off=on;off.modEnv.on=false;
    auto a=std::make_unique<synth2::test::PooledTestVoice>(on),b=std::make_unique<synth2::test::PooledTestVoice>(off);
    for(auto* voice:{a.get(),b.get()}){voice->prepare(44100);voice->startNote(60,1,nullptr,8192);}
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();a->renderNextBlock(audio,0,64);b->renderNextBlock(audio,0,64);
    std::array<uint32_t,16> first{},second{};ASSERT_TRUE(a->readRenderingCoreWords(0,first));ASSERT_TRUE(b->readRenderingCoreWords(0,second));
    EXPECT_GT(first[5],second[5]*4);EXPECT_FALSE(on.modEnv.nativePitch);
}

TEST(FmBehavior, DirectStaticFmUsesTheSamePhysicalSlewWithoutHistoryAdmission)
{
    synth2::RenderControls direct;direct.osc1FmAmount=32.25f/127.f;
    auto admitted=direct;admitted.modEnv.nativeFm=true;
    auto a=std::make_unique<synth2::test::PooledTestVoice>(direct),b=std::make_unique<synth2::test::PooledTestVoice>(admitted);
    for(auto* voice:{a.get(),b.get()}){voice->prepare(44100);voice->startNote(60,1,nullptr,8192);}
    synth2::audio::AudioBuffer<float> audio(2,64);
    std::array<uint32_t,7> first{},second{};
    for(int block=0;block<80;++block)
    {
        if(block==2)
        {
            direct.osc1FmAmount=admitted.osc1FmAmount=64.25f/127.f;
            a->setControllerDial(45);b->setControllerDial(45);
        }
        audio.clear();a->renderNextBlock(audio,0,64);audio.clear();b->renderNextBlock(audio,0,64);
        ASSERT_TRUE(a->readRenderingFmWords(0,first));ASSERT_TRUE(b->readRenderingFmWords(0,second));
        EXPECT_EQ(first,second);
        if(block==2){EXPECT_EQ(first[5],4410u);EXPECT_GT(first[2],0u);EXPECT_NE(first[0],first[3]);}
        std::array<uint32_t,16> aCore{},bCore{};
        ASSERT_TRUE(a->readRenderingCoreWords(0,aCore));ASSERT_TRUE(b->readRenderingCoreWords(0,bCore));EXPECT_EQ(aCore,bCore);
    }
    EXPECT_EQ(first[2],0u);EXPECT_EQ(first[0],first[3]);
}

TEST(FmBehavior, SoundingRetriggerRetainsPhysicalFmSlewAndInvalidatesItsTarget)
{
    synth2::RenderControls controls;controls.osc1FmAmount=.1f;controls.playMode=1;
    auto voice=std::make_unique<synth2::test::PooledTestVoice>(controls);voice->prepare(44100);voice->startNote(60,1,nullptr,8192);
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();voice->renderNextBlock(audio,0,64);
    controls.osc1FmAmount=.75f;voice->setControllerDial(45);audio.clear();voice->renderNextBlock(audio,0,64);
    std::array<uint32_t,7> before{},after{};ASSERT_TRUE(voice->readRenderingFmWords(0,before));ASSERT_GT(before[2],0u);
    voice->startNote(67,1,nullptr,8192);ASSERT_TRUE(voice->readRenderingFmWords(0,after));
    for(size_t field:{0u,1u,2u,4u,5u})EXPECT_EQ(after[field],before[field]);
    EXPECT_EQ(after[3],uint32_t(-1));EXPECT_EQ(after[6],0u);
}

#include "dsp/StereoBalance.h"
#include "dsp/PanLfo.h"
TEST(StereoBalance, CenterEdgesAndOutOfRangeKeepTheNearChannelAtUnity)
{
    for(const auto values:std::array<std::array<float,3>,7>{{{-2,1,0},{-1,1,0},{-.5f,1,.5f},{0,1,1},{.5f,.5f,1},{1,0,1},{2,0,1}}})
    {
        const auto gains=synth2::dsp::StereoBalance::gains(values[0]);
        EXPECT_EQ(gains.left,values[1]);EXPECT_EQ(gains.right,values[2]);
    }
}
TEST(StereoBalance, VoiceAndSmoothedPanUseTheSameStaticLawAtTheirOwnStages)
{
    for(float position:{-.5f,0.f,.5f})
    {
        synth2::dsp::Amplifier voice;float voiceLeft=0,voiceRight=0;
        voice.processSample(1,1,position,voiceLeft,voiceRight);
        const float left=position==.5f ? .5f : 1.f,right=position==-.5f ? .5f : 1.f;
        EXPECT_EQ(voiceLeft,left*synth2::dsp::OutputGain::defaultTarget);
        EXPECT_EQ(voiceRight,right*synth2::dsp::OutputGain::defaultTarget);
        synth2::dsp::PanLfo pan;pan.prepare(44100);float l=1,r=1;pan.process(l,r,0,position);
        EXPECT_EQ(l,left*pan.coefficient);EXPECT_EQ(r,right*pan.coefficient);
    }
}
TEST(PanInputs, VoiceRetainsUnisonPlacementWithoutApplyingTheGlobalPanLfo)
{
    synth2::RenderControls p;p.pan=.5f;p.osc1WaveIndex=1.1f;p.unisonVoices=3;p.unisonPan=.75f;
    p.lfo1.on=true;p.lfo1.dest=6;p.lfo1.amount=1;
    auto reference=p;reference.lfo1.on=false;reference.pan=0;
    auto actual=std::make_unique<synth2::test::PooledTestVoice>(p),expected=std::make_unique<synth2::test::PooledTestVoice>(reference);
    synth2::dsp::LfoBlock lfos;lfos.samples.setSize(2,64);lfos.samples.clear();
    for(int frame=0;frame<64;++frame)lfos.samples.setSample(0,frame,1);
    actual->setGlobalLfoBlock(&lfos);
    for(auto* voice:{actual.get(),expected.get()}){voice->prepare(44100);voice->startNote(60,1,nullptr,8192);}
    synth2::audio::AudioBuffer<float> first(2,64),second(2,64);first.clear();second.clear();
    actual->renderNextBlock(first,0,64);expected->renderNextBlock(second,0,64);
    double energy=0;
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<64;++frame)
    {EXPECT_EQ(first.getSample(channel,frame),second.getSample(channel,frame));energy+=first.getSample(channel,frame)*first.getSample(channel,frame);}
    EXPECT_GT(energy,1e-8);
}
TEST(PanInputs, OversizeUnknownHistoryCallbacksKeepEveryChunkAndSurroundingSamples)
{
    auto engine=std::make_unique<synth2::SynthEngine>();auto& p=engine->getParams();
    p.osc1WaveIndex=1.1f;p.lfo1.on=true;p.lfo1.dest=6;p.lfo1.amount=.5f;
    ASSERT_FALSE(p.nativePanLfo);ASSERT_FALSE(p.nativeVoicePerformance);
    engine->prepareToPlay(44100,64);
    synth2::audio::AudioBuffer<float> audio(2,1040);audio.clear();
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<1040;++frame)
        if(frame<8 || frame>=1032)audio.setSample(channel,frame,7);
    synth2::audio::MidiBuffer midi;midi.addEvent(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),8);
    engine->render(audio,midi,8,1024);
    for(int chunk=0;chunk<4;++chunk)
    {
        double energy=0;
        for(int channel=0;channel<2;++channel)for(int frame=8+chunk*256;frame<8+(chunk+1)*256;++frame)
        {EXPECT_TRUE(std::isfinite(audio.getSample(channel,frame)));energy+=audio.getSample(channel,frame)*audio.getSample(channel,frame);}
        EXPECT_GT(energy,1e-8);
    }
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<1040;++frame)
        if(frame<8 || frame>=1032)EXPECT_EQ(audio.getSample(channel,frame),7);
}
TEST(StaticPanInputs, VoiceCorePlacementIsIndependentOfGlobalBalance)
{
    for(int cores:{1,3})for(float pan:{-1.f,.5f,1.f})
    {
        synth2::RenderControls p;p.pan=pan;p.osc1WaveIndex=1.1f;p.unisonVoices=cores;p.unisonPan=.75f;
        auto centered=p;centered.pan=0;
        auto actual=std::make_unique<synth2::test::PooledTestVoice>(p),reference=std::make_unique<synth2::test::PooledTestVoice>(centered);
        for(auto* voice:{actual.get(),reference.get()}){voice->prepare(44100);voice->startNote(60,1,nullptr,8192);}
        synth2::audio::AudioBuffer<float> got(2,64),expected(2,64);got.clear();expected.clear();
        actual->renderNextBlock(got,0,64);reference->renderNextBlock(expected,0,64);
        double energy=0;
        for(int channel=0;channel<2;++channel)for(int frame=0;frame<64;++frame)
        {EXPECT_EQ(got.getSample(channel,frame),expected.getSample(channel,frame));energy+=got.getSample(channel,frame)*got.getSample(channel,frame);}
        EXPECT_GT(energy,1e-8);
    }
}

TEST(TuningBehavior, OneNoteCoarseAndFineCalculationPreservesFractionalControls)
{
    synth2::dsp::VoiceTuningSettings config;
    config.secondTracksKeyboard=true;config.secondPitch=12;
    synth2::dsp::VoiceTuning integral(config,69);
    const auto base=integral.prepareCore(0);
    EXPECT_EQ(base.first,440);EXPECT_EQ(base.second,880);
    config.fine=.375f;config.secondFine=.25f;
    synth2::dsp::VoiceTuning fractional(config,69);
    const auto tuned=fractional.prepareCore(.125f);
    EXPECT_NEAR(tuned.first,440*std::pow(2.,.5/1200),.0001);
    EXPECT_NEAR(tuned.second,tuned.first*std::pow(2.,12./12+.25/1200),.0002);
    config.secondTracksKeyboard=false;
    synth2::dsp::VoiceTuning fixedLow(config,36),fixedHigh(config,96);
    EXPECT_EQ(fixedLow.prepareCore(.125f).second,fixedHigh.prepareCore(.125f).second);
    EXPECT_NE(fixedLow.prepareCore(.125f).first,fixedHigh.prepareCore(.125f).first);
    EXPECT_NEAR(fixedLow.prepareCore(.125f).second,440*std::pow(2.,.75/1200),.0001);
}

TEST(TuningBehavior, DirectBendUsesTheSameThirtyTwoFrameReceiverPartitions)
{
    for(int target:{8320,16383})
    {
        synth2::RenderControls p;p.pitchBendRange=12;
        auto whole=std::make_unique<synth2::test::PooledTestVoice>(p),split=std::make_unique<synth2::test::PooledTestVoice>(p);
        whole->prepare(44100);split->prepare(44100);
        whole->startNote(69,1,nullptr,8192);split->startNote(69,1,nullptr,8192);
        whole->pitchWheelMoved(target);split->pitchWheelMoved(target);
        synth2::audio::AudioBuffer<float> a(2,128),b(2,128);a.clear();b.clear();
        whole->renderNextBlock(a,0,128);
        split->renderNextBlock(b,0,32);
        if(target==8320)split->renderNextBlock(b,32,96); // Slew has finished: no residual partitions.
        else for(int start=32;start<128;start+=32)split->renderNextBlock(b,start,32);
        for(int channel=0;channel<2;++channel)for(int i=0;i<128;++i)
            EXPECT_EQ(a.getSample(channel,i),b.getSample(channel,i));
        EXPECT_NEAR(whole->getCurrentFrequency(),440*std::pow(2.,(target==8320 ? 2. : 8.)/128),.001);
    }
}

TEST(TuningBehavior, GlideUsesThePhysicalReceiverWithoutPerformanceAdmission)
{
    synth2::RenderControls p;p.sourcePortamentoDial=64;p.portamentoAuto=true;
    auto voice=std::make_unique<synth2::test::PooledTestVoice>(p);voice->prepare(44100);
    synth2::audio::AudioBuffer<float> audio(2,32);audio.clear();
    voice->startNote(69,1,nullptr,8192);voice->renderNextBlock(audio,0,32);
    EXPECT_NEAR(voice->getCurrentFrequency(),440,.001);
    voice->setNextTransition({synth2::dsp::NoteTransitionKind::Legato,true});
    voice->startNote(81,1,nullptr,8192);audio.clear();voice->renderNextBlock(audio,0,32);
    EXPECT_NEAR(voice->getCurrentFrequency(),440,.001); // Initial held receiver span.
    for(int i=0;i<1000;++i){audio.clear();voice->renderNextBlock(audio,0,32);}
    EXPECT_NEAR(voice->getCurrentFrequency(),880,.001);
    EXPECT_FALSE(p.nativeVoicePerformance);
}

TEST(AmplitudeBehavior, AttackDurationEditsRetainProgressAndFractionalLevel)
{
    synth2::dsp::AmplitudeEnvelope envelope;
    envelope.setParameters(100,441,496,480);envelope.noteOn();
    std::array<float,64> gain{};envelope.render(gain.data(),25);
    EXPECT_EQ(envelope.getCounter(),25);EXPECT_EQ(envelope.getLevel(),512);
    envelope.setParameters(200,441,496,480);
    EXPECT_EQ(envelope.getCounter(),50);EXPECT_EQ(envelope.getLevel(),512);
    envelope.render(gain.data(),10);
    EXPECT_EQ(envelope.getCounter(),60);
    EXPECT_DOUBLE_EQ(envelope.getLevel(),2048.0*60/200);
    EXPECT_NE(envelope.getLevel(),std::trunc(envelope.getLevel()));
}
