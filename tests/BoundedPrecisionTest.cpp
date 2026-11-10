#include <gtest/gtest.h>
#include "dsp/AmplitudeEnvelope.h"
#include "dsp/ChorusKernel.h"
#include "dsp/DelayKernel.h"
#include "dsp/EffectControls.h"
#include "dsp/MultiEffect.h"
#include "dsp/Phaser.h"
#include "dsp/Saturation.h"
#include "dsp/AmplitudeBlockRenderer.h"
#include "dsp/FilterControlRenderer.h"
#include "dsp/FrequencyModulation.h"
#include "dsp/ModulationEnvelope.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/VoiceEnvelopeConfigurator.h"
#include "dsp/OscillatorControlRenderer.h"
#include "dsp/LfoShape.h"
#include "dsp/RandomLfoRuntime.h"
#include "dsp/LfoClock.h"
#include "patch/Synth1Tuning.h"
#include "patch/Synth1PatchMapper.h"
#include "render/VoiceConfiguration.h"
#include "render/VoiceLifecycleConfiguration.h"
#include "render/ControllerMapping.h"
#include "render/VoiceTuningConfiguration.h"
#include "dsp/OscillatorPhase.h"
#include "dsp/OscillatorSection.h"
#include <array>
#include <memory>
#include <type_traits>
#include <vector>

TEST(BoundedPrecision, FractionalStackDetuneSurvivesLoadingAndFrequencyCacheUpdates)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[0]=1.0f/127;controls[1]=2.0f/127;controls[76]=64.125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    ASSERT_EQ(mapped.osc1StackDial,64.125f);
    const auto first=OscillatorStack::frequencies(440,64.125f),second=OscillatorStack::frequencies(440,64.375f);
    for(int pair=0;pair<4;++pair)
    {EXPECT_LT(first[pair*2],second[pair*2]);EXPECT_GT(first[pair*2+1],second[pair*2+1]);}
    synth2::ShapeTables tables;tables.infos.push_back({0,2048,100000});
    for(int i=0;i<2048;++i)tables.data.push_back(static_cast<float>(std::sin(2*std::acos(-1.0f)*i/2048)));
    synth2::WaveTableBank bank;bank.setShapeTables(0,std::move(tables));
    OscillatorSection section;section.prepare({44100,64,2});section.stackDial=mapped.osc1StackDial;
    const auto sample=[&] {return section.processSample(440,440,.5f,0,0,false,false,0,0,-1,&bank,false);};
    EXPECT_TRUE(std::isfinite(sample()));EXPECT_EQ(section.cachedStackDial,64.125f);EXPECT_EQ(section.stackFrequencies,first);
    section.stackDial=64.375f;
    EXPECT_TRUE(std::isfinite(sample()));EXPECT_EQ(section.cachedStackDial,64.375f);EXPECT_EQ(section.stackFrequencies,second);
    OscillatorSection off;off.prepare({44100,64,2});off.stackDial=.25f;
    off.processSample(440,440,.5f,0,0,false,false,0,0,-1,&bank,false);
    EXPECT_EQ(off.cachedStackDial,-1); // Rounded-zero stack mode remains scalar.
}

TEST(BoundedPrecision, FractionalFixedPhaseReachesScalarAndLaneAccumulatorsWithoutSharedDraws)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[91]=64.125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    EXPECT_EQ(mapped.oscillatorPhaseDial,64.125f);
    OscillatorPhaseState first,second;first.setDial(mapped.oscillatorPhaseDial);second.setDial(64.375f);
    OscillatorPhaseRandom a,b,untouched;
    ASSERT_TRUE(first.start(a));ASSERT_TRUE(second.start(b));
    EXPECT_EQ(first.dial,64.125f);EXPECT_EQ(second.dial,64.375f);
    EXPECT_EQ(first.osc1,0u);EXPECT_EQ(second.osc1,0u);
    EXPECT_LT(first.osc2,second.osc2);EXPECT_NE(first.lanes,second.lanes);
    const auto expected=untouched.nextPhase();EXPECT_EQ(a.nextPhase(),expected);EXPECT_EQ(b.nextPhase(),expected);
}

TEST(BoundedPrecision, FractionalPhaseEditsPreserveRandomModeBoundariesAndPendingDraws)
{
    using namespace synth2::dsp;
    OscillatorPhaseState state;OscillatorPhaseRandom random,expected;
    const auto checkDraw=[&] {
        ASSERT_TRUE(state.start(random));EXPECT_EQ(state.osc1,expected.nextPhase());EXPECT_EQ(state.osc2,expected.nextPhase());
        for(auto phase:state.lanes)EXPECT_EQ(phase,expected.nextPhase());
    };
    state.setDial(.25f);checkDraw();
    const auto held=state.osc2;state.setDial(.375f);
    EXPECT_FALSE(state.start(random));EXPECT_EQ(state.osc2,held);
    state.setDial(.75f);ASSERT_TRUE(state.start(random));EXPECT_EQ(state.osc2,0u);
    for(auto phase:state.lanes)EXPECT_EQ(phase,0u);
    state.setDial(1.25f);ASSERT_TRUE(state.start(random));EXPECT_GT(state.osc2,0u);
    state.setDial(.25f);checkDraw();
    EXPECT_EQ(random.nextPhase(),expected.nextPhase());
}

TEST(BoundedPrecision, FractionalUnisonDetuneRetainsPitchAndFilterOffsets)
{
    using synth2::dsp::Unison;
    const auto low=Unison::layout(4,58),high=Unison::layout(4,59);
    const auto first=Unison::layout(4,58.25f),second=Unison::layout(4,58.375f);
    EXPECT_EQ(first.filterIndex[0],-.25f);EXPECT_EQ(second.filterIndex[0],-.375f);
    EXPECT_LT(high.pitchCents[0],second.pitchCents[0]);
    EXPECT_LT(second.pitchCents[0],first.pitchCents[0]);EXPECT_LT(first.pitchCents[0],low.pitchCents[0]);
    const auto single=Unison::layout(1,58.25f);
    for(size_t i=0;i<8;++i){EXPECT_EQ(single.pitchCents[i],0);EXPECT_EQ(single.filterIndex[i],0);}
    synth2::RenderControls mapped;mapped.unisonVoices=4;mapped.unisonDetune=58.25f/127;
    const auto config=synth2::render::voiceTuning(mapped,false);
    EXPECT_EQ(config.unisonDetuneDial,58.25f);
    synth2::dsp::VoiceTuning tuning(config,60);
    EXPECT_EQ(tuning.unison.filterIndex,first.filterIndex);
    EXPECT_EQ(tuning.unison.pitchCents,first.pitchCents);
}

TEST(BoundedPrecision, FractionalUnisonFilterOffsetSurvivesHeadroomAndReceiverTransport)
{
    using namespace synth2::dsp;
    const float offset=Unison::layout(4,58.25f).filterIndex[0];
    FilterEnvelopeSettings settings;settings.cutoffIndex=512;settings.amount=1;settings.sustain=1;
    FilterEnvelope envelope;float keyboard=0;
    VoiceEnvelopeConfigurator::filter(envelope,keyboard,offset,settings,48,1,44100);
    envelope.noteOn();envelope.render(1,[](float,int){});
    EXPECT_EQ(envelope.getLevel(),512.25f);
    FilterEnvelope empty;CutoffControl cutoff;cutoff.prepare(44100);VoiceControlBuffer indices{};
    FilterControlRenderer::render({true,false,512},{},empty,cutoff,0,offset,64,indices);
    for(int i=0;i<64;++i)EXPECT_EQ(indices[i],511.75f);
}

TEST(BoundedPrecision, FractionalUnisonPhaseSpreadPreservesTheSharedDrawStream)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[75]=58.25f/127;controls[92]=64.03125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    EXPECT_EQ(mapped.unisonPhaseDial,64.03125f);
    EXPECT_EQ(synth2::render::voiceTuning(mapped,false).unisonDetuneDial,58.25f);
    OscillatorPhaseRandom firstStream,secondStream,untouched;
    OscillatorPhaseState first,second;first.setDial(25);second.setDial(25);
    ASSERT_TRUE(first.start(firstStream,4,1,mapped.unisonPhaseDial));
    ASSERT_TRUE(second.start(secondStream,4,1,64.0625f));
    EXPECT_NE(first.osc1,second.osc1);EXPECT_NE(first.osc2,second.osc2);EXPECT_NE(first.lanes,second.lanes);
    const auto expected=untouched.nextPhase();
    EXPECT_EQ(firstStream.nextPhase(),expected);EXPECT_EQ(secondStream.nextPhase(),expected);
}

TEST(BoundedPrecision, FractionalCutoffReachesTheNativeReceiverAtCompatibleDialKnots)
{
    using namespace synth2;
    for(float dial:{64.03125f,64.0625f})
    {
        std::array<float,128> controls{};controls[19]=dial/127;controls[21]=79.0f/127;
        Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
        RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
        const auto config=render::filterControls(mapped,false);
        ASSERT_TRUE(config.enabled);
        const float expected=dial==64.03125f ? 515.25f : 515.5f;
        EXPECT_EQ(config.cutoffIndex,expected);
        EXPECT_EQ(render::filterEnvelope(mapped).cutoffIndex,expected);
        dsp::FilterEnvelope envelope;dsp::CutoffControl cutoff;cutoff.prepare(44100);
        dsp::VoiceControlBuffer indices{};
        dsp::FilterControlRenderer::render(config,{},envelope,cutoff,0,0,64,indices);
        for(int i=0;i<64;++i)EXPECT_EQ(indices[i],expected);
        // A discrete controller setter replaces the fractional patch adjustment.
        ASSERT_TRUE(render::applyControllerDial(mapped,19,64));
        EXPECT_EQ(mapped.filterCutoffAdjustment,0);
        EXPECT_EQ(render::filterControls(mapped,false).cutoffIndex,515);
    }
}

TEST(BoundedPrecision, FractionalKeyTrackingSurvivesEnvelopeSetupAndReceiverTransport)
{
    using namespace synth2::dsp;
    FilterEnvelopeSettings first;first.cutoffIndex=515;first.tracking=64.03125f/127;
    auto second=first;second.tracking=64.0625f/127;
    FilterEnvelope a,b;float x=0,y=0;
    VoiceEnvelopeConfigurator::filter(a,x,0,first,96,1,44100);
    VoiceEnvelopeConfigurator::filter(b,y,0,second,96,1,44100);
    ASSERT_NE(x,y);EXPECT_NE(x,std::trunc(x));EXPECT_NE(y,std::trunc(y));
    CutoffControl cutoff;cutoff.prepare(44100);VoiceControlBuffer indices{};
    FilterControlRenderer::render({true,false,515},{},a,cutoff,x,0,64,indices);
    for(int i=0;i<64;++i)EXPECT_EQ(indices[i],515+x);
}

TEST(BoundedPrecision, FractionalFilterHeadroomRetainsPeakSustainAndReleaseLevels)
{
    using namespace synth2::dsp;
    for(float base:{512.25f,512.5f})
    {
        FilterEnvelopeSettings settings;settings.cutoffIndex=base;
        settings.amount=1;settings.sustain=1;settings.release=.25f;
        FilterEnvelope envelope;float keyboard=0;
        VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,48,1,44100);
        envelope.noteOn();envelope.render(1,[](float,int){});
        EXPECT_EQ(keyboard,0);EXPECT_EQ(envelope.getLevel(),1024-base);
        envelope.noteOff();EXPECT_EQ(envelope.getLevel(),1024-base);
        bool fractional=false;
        envelope.render(64,[&](float level,int count) {
            EXPECT_GT(count,0);EXPECT_GE(level,0);EXPECT_LE(level,1024-base);
            fractional|=level!=std::trunc(level);
        });
        EXPECT_TRUE(fractional);
    }
}

TEST(BoundedPrecision, FractionalFreeLfoRateChangesThePeriodWithoutRoundingTheDial)
{
    using synth2::dsp::LfoClock;
    LfoClock low,first,second,high;
    low.setRate(64);first.setRate(64.03125f);second.setRate(64.0625f);high.setRate(65);
    EXPECT_LT(low.targetHz,first.targetHz);EXPECT_LT(first.targetHz,second.targetHz);
    EXPECT_LT(second.targetHz,high.targetHz);
    EXPECT_EQ(first.rateDial,64.03125f);EXPECT_EQ(second.rateDial,64.0625f);
    for(auto* clock:{&low,&first,&second,&high})clock->advance(44100);
    EXPECT_GT(first.period,second.period);
    // Sync chooses a musical division, but retains the free dial for restoration.
    first.setSynchronizedRate(first.rateDial,120);second.setSynchronizedRate(second.rateDial,120);
    EXPECT_EQ(first.targetHz,second.targetHz);
    first.setRate(first.rateDial);second.setRate(second.rateDial);
    EXPECT_LT(first.targetHz,second.targetHz);
}

TEST(BoundedPrecision, FractionalModulationTimesReachEachEnvelopeDestination)
{
    using namespace synth2::dsp;
    for(int parameter:{12,13})for(int destination:{0,1,2})
    {
        SCOPED_TRACE(::testing::Message()<<parameter<<' '<<destination);
        const auto curve=[&](float dial) {
            std::array<float,128> controls{};
            controls[11]=96.0f/127;controls[12]=0;controls[13]=64.0f/127;
            controls[parameter]=dial/127;
            synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
            synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
            EXPECT_EQ(parameter==12 ? mapped.modEnv.attackDial : mapped.modEnv.decayDial,dial);
            ModulationEnvelopeSettings settings;
            settings.attackDial=mapped.modEnv.attackDial;settings.decayDial=mapped.modEnv.decayDial;
            settings.amountDial=mapped.modEnv.amountDial;settings.fm=destination==1;settings.pulseWidth=destination==2;
            FilterEnvelope envelope;VoiceEnvelopeConfigurator::modulation(envelope,settings,44100);envelope.noteOn();
            std::vector<float> levels;levels.reserve(44100);
            envelope.render(44100,[&](float value,int count) {
                EXPECT_TRUE(std::isfinite(value));EXPECT_GE(value,0);
                levels.insert(levels.end(),count,value);
            });
            return levels;
        };
        const auto first=curve(64.125f),second=curve(64.375f);
        ASSERT_EQ(first.size(),44100u);ASSERT_EQ(second.size(),first.size());
        EXPECT_NE(first,second);
    }
}

TEST(BoundedPrecision, LegacyTimingConversionPrecedesFractionalRenderMapping)
{
    for(int version:{105,106})for(int dial:{0,32,64,96,127})
    {
        SCOPED_TRACE(::testing::Message()<<version<<' '<<dial);
        std::array<float,128> controls{};controls[12]=controls[13]=dial/127.0f;
        synth2::Synth1PatchMapper legacy(version),precision(version);
        precision.setContinuousControls(true);
        synth2::RenderControls first,second;
        legacy.mapFromRawBuffer(controls.data(),first);precision.mapFromRawBuffer(controls.data(),second);
        EXPECT_EQ(first.modEnv.attackDial,second.modEnv.attackDial);
        EXPECT_EQ(first.modEnv.decayDial,second.modEnv.decayDial);
    }
}

TEST(BoundedPrecision, FractionalRandomTargetsUseOneSharedDrawAndRetainRealState)
{
    using synth2::dsp::RandomLfo;
    struct Draws {int calls=0;int next(){++calls;return 17;}} draws;
    RandomLfo target;target.setWave(3,0,10.25f,draws);
    EXPECT_EQ(draws.calls,1);EXPECT_DOUBLE_EQ(target.current,6.75f);
    EXPECT_DOUBLE_EQ(target.query(3,0,10.25f,0,1000,32).first,6.75f);
    int counter=0;
    target.advance(999,counter,1000,3,0,10.25f,draws);EXPECT_EQ(draws.calls,1);
    target.advance(1,counter,1000,3,0,10.25f,draws);EXPECT_EQ(draws.calls,2);
    EXPECT_EQ(target.previous,6.75f);EXPECT_EQ(target.current,6.75f);
    target.setWave(3,.25f,.25f,draws);EXPECT_EQ(draws.calls,2);
    const auto fixed=target.query(3,.25f,.25f,7,1000,32);
    EXPECT_EQ(fixed.first,.25f);EXPECT_EQ(fixed.second,-1);
}

TEST(BoundedPrecision, SmoothedRandomQueriesKeepFractionalEndpointsWithIntegerBounds)
{
    synth2::dsp::RandomLfo target{8.5f,2.25f};
    const auto first=target.query(4,0,16,0,1000,32);
    const auto last=target.query(4,0,16,999,1000,32);
    EXPECT_EQ(first.first,2.25f);EXPECT_EQ(last.first,8.5f);
    EXPECT_GE(first.second,32);EXPECT_GE(last.second,32);
    EXPECT_EQ(target.current,8.5f);EXPECT_EQ(target.previous,2.25f);
}

TEST(BoundedPrecision, FractionalRandomDepthDoesNotRedrawHeldHistory)
{
    using synth2::dsp::RandomLfoRuntime;
    RandomLfoRuntime state;
    std::array<RandomLfoRuntime::Control,2> controls{{{4,3,64},{1,0,0}}};
    state.load({},controls);
    const auto seed=state.random.state;const float held=state.targets[0].current;
    const auto range=state.effectiveRange(0,64.03125f);
    EXPECT_EQ(range.first,1032.5f);EXPECT_EQ(range.second,0);
    const auto query=state.query(0,0,1000,32,64.03125f);
    EXPECT_EQ(query.first,1032.5f-held);EXPECT_NE(query.first,std::trunc(query.first));
    EXPECT_EQ(state.random.state,seed);EXPECT_EQ(state.targets[0].current,held);
    auto expected=state.random;(void)expected.next();
    state.advance(0,1000,0,1000,64.03125f);EXPECT_EQ(state.random.state,expected.state);
}

TEST(BoundedPrecision, FractionalGlideHistorySurvivesRetriggerAndReceiverTransport)
{
    using namespace synth2::dsp;
    Portamento state;state.setDuration(Portamento::timeSeconds(64),44100);
    state.setNote(60,false);state.offset=.25f;state.setNote(72,true);
    EXPECT_EQ(state.offset,-127.75f);EXPECT_EQ(state.delta,-127.75f);
    OscillatorControlSettings config;config.portamentoDial=64;config.applyPortamento=true;
    VoiceSpanBuffer spans{};for(int i=0;i<64;++i)spans[i]=64-i;
    VoiceControlBuffer pitch{};
    OscillatorControlRenderer::glide(config,{},state,64,spans,pitch);
    EXPECT_EQ(pitch[0],-127.75f);EXPECT_NE(state.offset,std::trunc(state.offset));
    const float retained=state.offset;
    state.setNote(76,true);EXPECT_EQ(state.delta,retained-42);
}

TEST(BoundedPrecision, LfoDepthCurvesKeepSubUnitValuesAcrossFourWaveforms)
{
    using Curve=synth2::dsp::LfoShape::DepthCurve;
    const auto sine=Curve(4,.125f).query(0,250,1000,32);
    EXPECT_DOUBLE_EQ(sine.first,.125f);EXPECT_EQ(sine.second,32);
    const auto triangle=Curve(5,64.125f).query(1,333,1000,32);
    EXPECT_DOUBLE_EQ(triangle.first,171.375f);EXPECT_EQ(triangle.second,32);
    const auto saw=Curve(2,64.125f).query(2,1200,4096,32);
    EXPECT_DOUBLE_EQ(saw.first,365.75f);EXPECT_EQ(saw.second,32);
    const auto square=Curve(3,.03125f).query(3,750,1000,32);
    EXPECT_DOUBLE_EQ(square.first,.5f);EXPECT_EQ(square.second,250);
    for(int wave=0;wave<4;++wave)
    {
        const auto silent=Curve(2,0).query(wave,123,1000,32);
        EXPECT_EQ(silent.first,0);EXPECT_EQ(silent.second,-1);
    }
}

TEST(BoundedPrecision, FractionalPortamentoTimeReachesTheSampleClock)
{
    using namespace synth2::dsp;
    const float first=Portamento::timeSeconds(64.125f),second=Portamento::timeSeconds(64.375f);
    EXPECT_GT(first,Portamento::timeSeconds(64));EXPECT_LT(first,second);
    std::array<float,128> controls{};controls[39]=64.125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    EXPECT_EQ(mapped.sourcePortamentoDial,64.125f);EXPECT_TRUE(synth2::render::oscillatorControls(mapped,44100,false).applyPortamento);
    Portamento state;state.setDuration(Portamento::timeSeconds(64),44100);state.setNote(60,false);state.setNote(72,true);
    OscillatorControlSettings config;config.portamentoDial=mapped.sourcePortamentoDial;config.applyPortamento=true;
    VoiceSpanBuffer spans{};for(int i=0;i<64;++i)spans[i]=64-i;
    VoiceControlBuffer pitch{};
    OscillatorControlRenderer::glide(config,{},state,64,spans,pitch);
    EXPECT_EQ(state.seconds,first);EXPECT_EQ(state.duration,static_cast<int>(first*44100.0f));
    EXPECT_NE(state.duration,static_cast<int>(second*44100.0f));
}

TEST(BoundedPrecision, FractionalEnvelopeLevelsReachTheVoiceControlBuffers)
{
    using namespace synth2::dsp;
    FilterEnvelope envelope;
    envelope.configure(0,0,512.25f,256.125f,1000);
    envelope.noteOn();
    OscillatorControlSettings config;config.envelopeEnabled=true;
    VoiceControlBuffer signal{};VoiceSpanBuffer spans{};
    OscillatorControlRenderer::envelope(config,{},envelope,32,signal,spans);
    for(int i=0;i<32;++i){EXPECT_EQ(signal[i],256.125f);EXPECT_EQ(spans[i],32-i);}
    EXPECT_EQ(envelope.getLevel(),256.125f);
    envelope.noteOff();
    bool fractional=false;
    envelope.render(32,[&](float value,int count){
        EXPECT_GE(value,0);EXPECT_LE(value,256.125f);EXPECT_GT(count,0);
        fractional=fractional || value!=std::trunc(value);
    });
    EXPECT_TRUE(fractional);
}

TEST(BoundedPrecision, AmplitudeSustainAndReleaseRetainSubLevelValues)
{
    using namespace synth2::dsp;
    AmplitudeEnvelope amplitude;
    amplitude.setParameters(0,0,1024.25f,4096);amplitude.noteOn();
    float gain=0;amplitude.render(&gain,1);
    EXPECT_EQ(amplitude.getLevel(),1024.25f);
    EXPECT_EQ(amplitude.getCachedLevel(),1024.25f);
    EXPECT_EQ(gain,1024.25f/2048);
    amplitude.noteOff();amplitude.render(&gain,1);
    EXPECT_EQ(amplitude.getPhase(),AmplitudeEnvelope::Phase::Release);
    EXPECT_NE(amplitude.getLevel(),std::trunc(amplitude.getLevel()));
    ReleaseRamp release;release.start(4096,1024.25f);release.advance(1);
    EXPECT_EQ(release.getLevel(),amplitude.getLevel());
    EXPECT_EQ(release.getCounter(),1);
}

TEST(BoundedPrecision, FractionalModulationDepthSurvivesConfiguration)
{
    using namespace synth2::dsp;
    FilterEnvelope envelope;
    ModulationEnvelopeSettings settings;settings.amountDial=64.0625f;settings.fm=true;
    settings.attackDial=0;settings.decayDial=127;
    VoiceEnvelopeConfigurator::modulation(envelope,settings,44100);
    envelope.noteOn();
    float maximum=0;bool fractional=false;
    envelope.render(64,[&](float value,int){maximum=std::max(maximum,value);fractional|=value!=std::trunc(value);});
    EXPECT_GT(maximum,0);EXPECT_LT(maximum,1.0f);EXPECT_TRUE(fractional);
    std::array<float,128> controls{};controls[11]=64.0625f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    EXPECT_EQ(mapped.modEnv.amountDial,64.0625f);
}

TEST(BoundedPrecision, FilterEnvelopeConfigurationKeepsFractionalDepthAndSustain)
{
    using namespace synth2::dsp;
    FilterEnvelope envelope;FilterEnvelopeSettings settings;
    settings.cutoffIndex=512;settings.amount=(70.03125f-63)/64;
    settings.sustain=64.125f/127;
    float keyboard=0;
    VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,60,1,44100);
    envelope.noteOn();
    envelope.render(1,[](float value,int count){EXPECT_DOUBLE_EQ(value,56.375f);EXPECT_EQ(count,1);});
    EXPECT_DOUBLE_EQ(envelope.getLevel(),56.375f);
}

TEST(BoundedPrecision, LiveDepthChangesPreserveSubUnitEnvelopePosition)
{
    using synth2::dsp::FilterEnvelope;
    FilterEnvelope envelope;envelope.configure(1,1000,.5f,0,1000);
    envelope.noteOn();envelope.render(1,[](float,int){});
    ASSERT_EQ(envelope.getLevel(),.5f);
    envelope.configure(1,1000,1.25f,0,1000);
    EXPECT_EQ(envelope.getLevel(),1.25f);
    envelope.configure(1,1000,.25f,0,1000);
    EXPECT_EQ(envelope.getLevel(),.25f);
}

TEST(BoundedPrecision, SignalStorageKeepsFractionalPhaseUnitsAndDiscreteSpans)
{
    using namespace synth2::dsp;
    static_assert(std::is_same_v<VoiceSpanBuffer::value_type,int>);
    VoiceControlBuffer values{};
    values[0]=67108863.25f;
    values[1]=.125f;
    ReceiverLfoView view;
    view.values[0]=values.data();
    EXPECT_DOUBLE_EQ(view.values[0][0],67108863.25f);
    EXPECT_DOUBLE_EQ(view.values[0][1],.125f);
}

TEST(BoundedPrecision, FmKeepsFractionalDepthThroughDispatchAndSlew)
{
    using synth2::dsp::FrequencyModulation;
    FrequencyModulation state;
    int samples=0;
    state.render(1.25f,64,44100,[&](float value){EXPECT_EQ(value,1.25f);++samples;});
    EXPECT_EQ(samples,64);
    EXPECT_EQ(state.current,1.25f);
    state.render(12.75f,64,44100,[](float){});
    EXPECT_EQ(state.target,12.75f);
    EXPECT_EQ(state.anchor,1.25f);
    EXPECT_NE(state.current,std::trunc(state.current));
    const float depth=FrequencyModulation::depth(64.25f,.125f);
    EXPECT_GT(depth,FrequencyModulation::depth(64,0));
    EXPECT_LT(depth,FrequencyModulation::depth(65,1));
}

TEST(BoundedPrecision, FractionalFilterModulationReachesCoefficients)
{
    using namespace synth2::dsp;
    EXPECT_EQ(CutoffControl::target(512.25f,0,0,0,0,0),512.25f);
    EXPECT_EQ(CutoffControl::target(512,0,0,0,1.5f,0),512.5f);
    VoiceControlBuffer modulation{},indices{};
    modulation.fill(1.5f);
    VoiceSpanBuffer spans{};
    for(int i=0;i<64;++i)spans[i]=64-i;
    ReceiverLfoView view;view.available=true;view.receivers[0]={true,2};
    view.values[0]=modulation.data();view.remaining[0]=spans.data();
    FilterEnvelope envelope;CutoffControl cutoff;cutoff.prepare(44100);
    FilterControlRenderer::render({true,true,512},view,envelope,cutoff,0,0,64,indices);
    for(int i=0;i<64;++i)EXPECT_EQ(indices[i],512.5f);
    StateVariableFilter filter;
    filter.configure(44100,indices[0],.5f);
    const float fractional=filter.getCoefficient();
    filter.configure(44100,512,.5f);
    EXPECT_NE(filter.getCoefficient(),fractional);
    filter.configure(44100,513,.5f);
    EXPECT_GT(filter.getCoefficient(),fractional);
}

TEST(BoundedPrecision, FractionalAmplitudeModulationSurvivesTheReceiver)
{
    using namespace synth2::dsp;
    VoiceControlBuffer first{},second{};first.fill(.25f);second.fill(.5f);
    VoiceSpanBuffer spans{};spans.fill(1);
    ReceiverLfoView view;view.available=true;
    view.receivers[0]={true,3};view.receivers[1]={true,3};
    view.values={first.data(),second.data()};view.remaining={spans.data(),spans.data()};
    AmplitudeEnvelope amplitude;amplitude.setParameters(0,0,2048,0);amplitude.noteOn();
    float gain=0;
    AmplitudeBlockSettings config;
    ASSERT_TRUE(AmplitudeBlockRenderer::render(config,view,amplitude,1,&gain));
    const float expected=((2048-.25f)*(2048-.5f))/4194304.0f;
    EXPECT_EQ(gain,expected);EXPECT_LT(gain,1);
}

TEST(BoundedPrecision, PitchAndPulseModulationRetainFractionalCoordinates)
{
    using synth2::dsp::ModulationEnvelope;
    EXPECT_DOUBLE_EQ(ModulationEnvelope::pulseWidthBins(.5f,.125f),513.5f);
    EXPECT_GT(ModulationEnvelope::pitchFrequency(440.0f,.125f),440);
    EXPECT_LT(ModulationEnvelope::pitchFrequency(440.0f,.125f),ModulationEnvelope::pitchFrequency(440.0f,1));
}

TEST(BoundedPrecision, GainAndVelocityInterpolateCompatibleIntegerAnchors)
{
    using Envelope=synth2::dsp::AmplitudeEnvelope;
    for(int dial=0;dial<127;++dial)
    {
        EXPECT_EQ(Envelope::gainForDial(static_cast<float>(dial)),Envelope::gainForDial(dial));
        const float a=Envelope::gainForDial(dial),b=Envelope::gainForDial(dial+1);
        EXPECT_EQ(Envelope::gainForDial(dial+.25f),a+(b-a)*.25f);
        const float first=Envelope::velocityForDials(100,dial),second=Envelope::velocityForDials(100,dial+1);
        EXPECT_EQ(Envelope::velocityForDials(100,dial+.25f),first+(second-first)*.25f);
    }
    EXPECT_GT(Envelope::gainForDial(.25f),0);
    EXPECT_LT(Envelope::gainForDial(.25f),Envelope::gainForDial(1));
}

TEST(BoundedPrecision, SaturationKeepsIntegerResponseAndFractionalDrive)
{
    for(int dial=0;dial<=127;++dial)
    {
        const float exponent=dial*(1.0f/127.0f)-1.0f;
        const float curve=std::exp(static_cast<float>(exponent)*5.8f)*dial*(1.0f/127.0f);
        const float expected=curve*100.0f;
        EXPECT_EQ(synth2::dsp::saturationStrength(dial/127.0f),expected);
    }
    EXPECT_LT(synth2::dsp::saturationStrength(64.125f/127),synth2::dsp::saturationStrength(64.375f/127));
    EXPECT_EQ(synth2::fineTuneCents(67.125f),1.125f);
    EXPECT_EQ(synth2::fineTuneCents(61.875f),-.125f);
}

TEST(BoundedPrecision, DelayRetainsFractionalFrameCoordinatesAcrossReset)
{
    auto delay=std::make_unique<synth2::dsp::DelayKernel>();
    ASSERT_TRUE(delay->configure(44100,0,10.0f/44100,0,0,127,64));
    ASSERT_TRUE(delay->setFrameTargets(10.25f,10.25f));delay->reset();
    EXPECT_DOUBLE_EQ(delay->getCurrentLeft(),10.25f);
    EXPECT_DOUBLE_EQ(delay->getLeftFramesContinuous(),10.25f);
    std::array<float,32> result{};
    for(int i=0;i<32;++i)
    {
        float dry[2]{i==0 ? 1.0f : 0,0},send[2]{};
        ASSERT_TRUE(delay->process(dry,send,1));result[i]=send[0];EXPECT_EQ(send[1],0);
    }
    EXPECT_EQ(result[10],.75f);EXPECT_EQ(result[11],.25f);
    for(int i=0;i<32;++i)if(i!=10 && i!=11)EXPECT_EQ(result[i],0);
}

TEST(BoundedPrecision, ChorusInterpolatesTheCompatibleDelayFrameAnchors)
{
    auto chorus=std::make_unique<synth2::dsp::ChorusKernel>();
    ASSERT_TRUE(chorus->configure(44100,1,64,0,64,64,127));
    chorus->setTimeDial(64.125f);chorus->reset();
    // Native time dials64/65 resolve to666/677 frames at44.1kHz.
    EXPECT_DOUBLE_EQ(chorus->getDelayFramesContinuous(),667.375f);
    std::array<float,700> result{};
    for(int i=0;i<700;++i)
    {
        float dry[2]{i==0 ? 1.0f : 0,0};chorus->process(dry,nullptr,1);result[i]=dry[0];
    }
    EXPECT_EQ(result[667],.625f);EXPECT_EQ(result[668],.375f);
    for(int i=1;i<700;++i)if(i!=667 && i!=668)EXPECT_EQ(result[i],0);
}

TEST(BoundedPrecision, PhaserAndEffectControlsRetainSubIntegerValues)
{
    synth2::dsp::Phaser phaser;phaser.prepare(44100);
    phaser.setDials(64.125f,64.125f,64.125f,2);const auto first=phaser.snapshot();
    phaser.setDials(64.375f,64.375f,64.375f,2);const auto second=phaser.snapshot();
    EXPECT_NE(first[0],second[0]);EXPECT_NE(first[1],second[1]);
    EXPECT_NE(first[2],second[2]);EXPECT_NE(first[3],second[3]);
    synth2::dsp::MultiEffect effect;effect.prepare(44100);effect.setDials(64.125f,64.125f,64.125f);
    EXPECT_EQ(effect.snapshot()[1],64.125f);EXPECT_EQ(effect.snapshot()[0],64.125f*(1.0f/127.0f));
}

TEST(BoundedPrecision, FractionalDecimationPeriodKeepsItsSampleClockRemainder)
{
    synth2::dsp::MultiEffect effect;effect.prepare(44100);effect.setDials(.5f,0,127);
    std::array<float,8> audio{0,.25f,.5f,.75f,1,1.25f,1.5f,1.75f};
    effect.process(3,audio.data(),nullptr,audio.size(),false);
    const std::array<float,8> expected{0,0,.5f,.75f,.75f,1.25f,1.5f,1.5f};
    EXPECT_EQ(audio,expected);
}

TEST(BoundedPrecision, FractionalHostDialsReachTheActualRenderControls)
{
    std::array<float,128> values{};
    values[3]=67.125f/127;values[72]=67.375f/127;
    values[79]=35.125f/127;values[80]=64.125f/127;values[81]=64.375f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(values.data(),mapped);
    EXPECT_EQ(mapped.osc2FineCents,1.125f);EXPECT_EQ(mapped.globalFineTune,1.375f);
    EXPECT_EQ(mapped.nativeMultiEffectDials[0],35.125f);
    EXPECT_EQ(mapped.nativeMultiEffectDials[1],64.125f);
    EXPECT_EQ(mapped.nativeMultiEffectDials[2],64.375f);
    EXPECT_EQ(mapped.nativePhaserDepth,35.125f);
    EXPECT_EQ(mapped.nativePhaserRate,64.125f);
    EXPECT_EQ(mapped.nativePhaserFeedback,64.375f);
}

TEST(BoundedPrecision, TinySubLevelsKeepNativeScalingWithAndWithoutStack)
{
    for(float stack:{0.f,7.f})for(float dial:{0.f,.125f,.25f,.49f,.5f,.51f,.75f,1.f})
    {
        std::array<float,128> values{};values[76]=stack/127;values[95]=dial/127;
        synth2::Synth1PatchMapper mapper(113);
        mapper.setContinuousControls(true);mapper.setDirectControls(true);
        synth2::RenderControls mapped;mapper.mapFromRawBuffer(values.data(),mapped);
        EXPECT_NEAR(mapped.subOscGain,(dial*4)*(1.f/127),1e-7f)<<"stack="<<stack<<" dial="<<dial;
        EXPECT_EQ(mapped.osc1StackDial,stack);
    }
}

TEST(BoundedPrecision, FilterRecurrenceRetainsRepresentableFloatInputDifferences)
{
    synth2::dsp::StateVariableFilter first,second;
    first.configureCoefficients(.25f,0);second.configureCoefficients(.25f,0);
    const auto base=first.process(1.0f);
    const auto changed=second.process(std::nextafter(1.0f,2.0f));
    static_assert(std::is_same_v<decltype(first.process(0)),float>);
    EXPECT_GT(changed,base);
}

TEST(BoundedPrecision, DelayHistoryRetainsRepresentableFloatInputDifferences)
{
    auto first=std::make_unique<synth2::dsp::DelayKernel>();
    auto second=std::make_unique<synth2::dsp::DelayKernel>();
    for(auto* delay:{first.get(),second.get()})
    {
        ASSERT_TRUE(delay->configure(44100,0,10.0f/44100,0,0,127,64));
        ASSERT_TRUE(delay->setFrameTargets(10,10));delay->reset();
    }
    bool retained=false;
    for(int frame=0;frame<32;++frame)
    {
        float a[2]{frame==0 ? 1.0f : 0.0f,0},b[2]{frame==0 ? std::nextafter(1.0f,2.0f) : 0.0f,0};
        float sendA[2]{},sendB[2]{};
        ASSERT_TRUE(first->process(a,sendA,1));ASSERT_TRUE(second->process(b,sendB,1));
        if(frame>0)retained|=sendA[0]!=sendB[0];
    }
    EXPECT_TRUE(retained);
}

TEST(BoundedPrecision, DurationWordsRetainTheCompatibleSampleCount)
{
    EXPECT_EQ(synth2::dsp::ReleaseRamp::durationFrames(.01f,44100),441);
    EXPECT_EQ(synth2::dsp::ReleaseRamp::durationFrames(.0005f,44100),22);
    EXPECT_EQ(synth2::dsp::ReleaseRamp::durationFrames(0,44100),0);
}

TEST(BoundedPrecision, TempoKnotsKeepDiscretePeriodsAndFractionalBpm)
{
    using synth2::dsp::LfoClock;
    LfoClock first,fractional,last;
    first.setSynchronizedRate(92,120);
    fractional.setSynchronizedRate(92,120.125f);
    last.setSynchronizedRate(92,121);
    first.advance(44100);
    EXPECT_EQ(first.period,14699);
    EXPECT_GT(fractional.targetHz,first.targetHz);
    EXPECT_LT(fractional.targetHz,last.targetHz);
    EXPECT_DOUBLE_EQ(fractional.targetHz,first.targetHz+(last.targetHz-first.targetHz)*.125f);
    static_assert(std::is_same_v<decltype(fractional.targetHz),float>);
}

TEST(BoundedPrecision, PitchModulationUsesFloatFrequencyWords)
{
    using synth2::dsp::ModulationEnvelope;
    const float frequency=std::nextafter(440.0f,441.0f);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(frequency,128),frequency*2);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(frequency,-128),frequency*.5f);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(22000,128),22000);
}

TEST(BoundedPrecision, DurationUsesFloatProductBeforeIntegerFrameConversion)
{
    using synth2::dsp::ReleaseRamp;
    constexpr float anchor=.01f;
    EXPECT_EQ(ReleaseRamp::durationFrames(anchor,44100),441);
    EXPECT_EQ(ReleaseRamp::durationFrames(std::nextafter(anchor,0.0f),44100),440);
    EXPECT_EQ(ReleaseRamp::durationFrames(anchor,44100.25f),441);
}

TEST(BoundedPrecision, AdjacentFloatPhaseEditsReachTheFinalPhaseWord)
{
    using namespace synth2::dsp;
    const float edited=std::nextafter(64.0f,65.0f);
    ASSERT_GT(edited,64.0f);
    OscillatorPhaseState base,fine;base.setDial(64);fine.setDial(edited);
    EXPECT_EQ(fine.dial,edited);
    OscillatorPhaseRandom a,b,untouched;
    ASSERT_TRUE(base.start(a));ASSERT_TRUE(fine.start(b));
    EXPECT_GT(fine.osc2,base.osc2);
    const auto next=untouched.nextPhase();EXPECT_EQ(a.nextPhase(),next);EXPECT_EQ(b.nextPhase(),next);
    // Selection is a discrete compatible word, separate from phase interpolation.
    OscillatorPhaseState boundary;boundary.setDial(.5f);
    OscillatorPhaseRandom random,expected;ASSERT_TRUE(boundary.start(random));
    EXPECT_EQ(boundary.osc1,0u);EXPECT_EQ(boundary.osc2,0u);
    EXPECT_EQ(random.nextPhase(),expected.nextPhase());
}

TEST(BoundedPrecision, AdjacentFloatSpreadEditsRetainLocalDrawOrder)
{
    using namespace synth2::dsp;
    const float edited=std::nextafter(64.0f,65.0f);ASSERT_GT(edited,64.0f);
    bool changed=false;
    OscillatorPhaseRandom a,b,untouched;
    for(int slot=0;slot<8;++slot)
    {
        OscillatorPhaseState base,fine;base.setDial(64);fine.setDial(64);
        ASSERT_TRUE(base.start(a,8,slot,64));ASSERT_TRUE(fine.start(b,8,slot,edited));
        changed=changed || base.osc1!=fine.osc1 || base.osc2!=fine.osc2 || base.lanes!=fine.lanes;
    }
    EXPECT_TRUE(changed);
    const auto next=untouched.nextPhase();EXPECT_EQ(a.nextPhase(),next);EXPECT_EQ(b.nextPhase(),next);
}

TEST(BoundedPrecision, DelayTempoRespondsToAdjacentFloatCoordinates)
{
    const float tempo=std::nextafter(120.0f,121.0f);
    const auto native=synth2::dsp::EffectControls::delaySeconds(7,120);
    const auto edited=synth2::dsp::EffectControls::delaySeconds(7,tempo);
    static_assert(std::is_same_v<decltype(synth2::dsp::EffectControls::delaySeconds(7,tempo)),float>);
    EXPECT_EQ(native,.1875f);EXPECT_LT(edited,native);
}

TEST(BoundedPrecision, LoadedFractionalPhaseInterpolatesCompatiblePhaseWords)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[91]=64.125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    ASSERT_EQ(mapped.oscillatorPhaseDial,64.125f);
    OscillatorPhaseState low,high,edited;low.setDial(64);high.setDial(65);edited.setDial(mapped.oscillatorPhaseDial);
    OscillatorPhaseRandom random;
    ASSERT_TRUE(low.start(random));ASSERT_TRUE(high.start(random));ASSERT_TRUE(edited.start(random));
    const auto expected=[](uint32_t a,uint32_t b){return static_cast<uint32_t>(a+(static_cast<float>(b)-a)*.125f);};
    EXPECT_EQ(edited.osc2,expected(low.osc2,high.osc2));
    for(size_t i=0;i<edited.lanes.size();++i)
        EXPECT_EQ(edited.lanes[i],expected(low.lanes[i],high.lanes[i]));
}

TEST(BoundedPrecision, FineTuningUsesCompatibleFloatFrequencyWords)
{
    using synth2::dsp::OscillatorTuning;
    // Adjacent cents coordinates straddle a compatible output-word transition.
    constexpr float low=0x1.00000ep+6f,high=0x1.000010p+6f;
    constexpr float first=0x1.c891fcp+8f,last=0x1.c891fep+8f;
    const float edited=low+(high-low)*.25f;
    ASSERT_EQ(static_cast<float>(edited),static_cast<float>(low));
    EXPECT_EQ(OscillatorTuning::oscillator1(69,0,low,0),first);
    EXPECT_EQ(OscillatorTuning::oscillator1(69,0,high,0),last);
    EXPECT_EQ(OscillatorTuning::oscillator1(69,0,edited,0),first);
    EXPECT_EQ(OscillatorTuning::oscillator1(69,0,0,edited),first);
    synth2::dsp::VoiceTuningSettings settings;settings.fine=edited;
    settings.secondTracksKeyboard=true;settings.secondPitch=12;
    const auto core=synth2::dsp::VoiceTuning(settings,69).prepareCore(0);
    EXPECT_EQ(core.first,first);EXPECT_EQ(core.second,core.first*2);
    for(int input=0;input<3;++input)
    {
        const float cents=input==0 ? edited : 0,master=input==1 ? edited : 0,unison=input==2 ? edited : 0;
        EXPECT_EQ(OscillatorTuning::fixedOsc2(0,cents,master,unison),(first)*.5f);
    }
}

TEST(BoundedPrecision, UnisonGroupOffsetsResolveSourceWordsBeforeFractionalEdits)
{
    using synth2::dsp::Unison;
    const auto low=Unison::layout(8,58,25),high=Unison::layout(8,59,25);
    const auto edited=Unison::layout(8,58.25f,25);
    for(size_t i=0;i<8;++i)
    {
        EXPECT_EQ(low.pitchCents[i],static_cast<float>(static_cast<float>(low.pitchCents[i])));
        EXPECT_EQ(high.pitchCents[i],static_cast<float>(static_cast<float>(high.pitchCents[i])));
        EXPECT_EQ(edited.pitchCents[i],low.pitchCents[i]+(high.pitchCents[i]-low.pitchCents[i])*.25f);
    }
}

TEST(BoundedPrecision, TrackedTuningRetainsRepresentableFloatFrequency)
{
    using namespace synth2::dsp;
    const float base=std::nextafter(440.0f,441.0f);
    ASSERT_GT(base,440.0f);
    EXPECT_EQ(OscillatorTuning::trackedOsc2(base,12,0),base*2);
    VoiceTuningSettings settings;settings.secondTracksKeyboard=true;settings.secondPitch=12;
    VoiceTuning tuning(settings,69);
    EXPECT_EQ(tuning.coarseSecond({base,0},1,0),base*2);
    EXPECT_EQ(tuning.prepareCore(0).first,440);
    EXPECT_EQ(tuning.prepareCore(0).second,880);
}

TEST(BoundedPrecision, CoarseReceiverUsesFloatExponentWords)
{
    using namespace synth2::dsp;
    VoiceTuningSettings settings;settings.secondTracksKeyboard=true;
    VoiceTuning tuning(settings,69);
    const auto core=tuning.prepareCore(0);
    const float low=1,high=std::nextafter(1.0f,2.0f),edited=low+(high-low)*.25f;
    ASSERT_EQ(static_cast<float>(edited),1.0f);
    const float first=tuning.coarseSecond(core,low,0),last=tuning.coarseSecond(core,high,0);
    ASSERT_GT(last,first);
    EXPECT_EQ(tuning.coarseSecond(core,edited,0),first);
    settings.secondTracksKeyboard=false;VoiceTuning fixed(settings,69);
    EXPECT_EQ(fixed.coarseSecond(core,edited,0),(first)*.5f);
}

TEST(BoundedPrecision, StackFrequencyCacheUsesFloatBaseWords)
{
    using namespace synth2::dsp;
    const float low=440,high=std::nextafter(440.0f,441.0f),edited=low+(high-low)*.25f;
    ASSERT_EQ(static_cast<float>(edited),440.0f);
    const auto first=OscillatorStack::frequencies(low,64.25f),last=OscillatorStack::frequencies(high,64.25f);
    const auto fractional=OscillatorStack::frequencies(edited,64.25f);
    for(size_t i=0;i<first.size();++i)
        EXPECT_EQ(fractional[i],first[i]);
    synth2::ShapeTables tables;tables.infos.push_back({0,2048,100000});
    tables.data.resize(2048);
    synth2::WaveTableBank bank;bank.setShapeTables(0,std::move(tables));
    OscillatorSection section;section.prepare({44100,64,2});section.stackDial=64.25f;
    const auto render=[&](float base) {section.processSample(base,440,.5f,0,0,false,false,0,0,-1,&bank);};
    render(low);EXPECT_EQ(section.stackFrequencies,first);
    render(edited);EXPECT_EQ(section.stackFrequencies,fractional);EXPECT_EQ(first,fractional);
    render(high);EXPECT_EQ(section.stackFrequencies,last);
    render(edited);EXPECT_EQ(section.stackFrequencies,fractional);
}

TEST(BoundedPrecision, ModulationStepsRetainCompatibleFloatWordsAcrossRates)
{
    static_assert(std::is_same_v<decltype(synth2::dsp::modulationStep(1.0f,44100.0f)),float>);
    for(float rate:{2000.f,8000.f,22050.f,44100.f,48000.25f,96000.f,192000.f})
        for(int dial=0;dial<128;++dial)
            for(float frequency:{synth2::dsp::ringFrequencyWords[dial],synth2::dsp::chorusRateWords[dial]})
            {
                const float expected=static_cast<float>(std::sin(static_cast<double>(frequency)
                    *3.141592653589793/static_cast<double>(rate))*2.0);
                EXPECT_EQ(synth2::dsp::modulationStep(frequency,rate),expected)<<rate<<" "<<dial;
            }
}
