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
    for(int i=0;i<2048;++i)tables.data.push_back(static_cast<float>(std::sin(2*std::acos(-1.0)*i/2048)));
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
    EXPECT_EQ(first.filterIndex[0],-.25);EXPECT_EQ(second.filterIndex[0],-.375);
    EXPECT_LT(high.pitchCents[0],second.pitchCents[0]);
    EXPECT_LT(second.pitchCents[0],first.pitchCents[0]);EXPECT_LT(first.pitchCents[0],low.pitchCents[0]);
    const auto single=Unison::layout(1,58.25f);
    for(size_t i=0;i<8;++i){EXPECT_EQ(single.pitchCents[i],0);EXPECT_EQ(single.filterIndex[i],0);}
    synth2::RenderControls mapped;mapped.unisonVoices=4;mapped.unisonDetune=58.25/127;
    const auto config=synth2::render::voiceTuning(mapped,false);
    EXPECT_EQ(config.unisonDetuneDial,58.25f);
    synth2::dsp::VoiceTuning tuning(config,60);
    EXPECT_EQ(tuning.unison.filterIndex,first.filterIndex);
    EXPECT_EQ(tuning.unison.pitchCents,first.pitchCents);
}

TEST(BoundedPrecision, FractionalUnisonFilterOffsetSurvivesHeadroomAndReceiverTransport)
{
    using namespace synth2::dsp;
    const double offset=Unison::layout(4,58.25f).filterIndex[0];
    FilterEnvelopeSettings settings;settings.cutoffIndex=512;settings.amount=1;settings.sustain=1;
    FilterEnvelope envelope;double keyboard=0;
    VoiceEnvelopeConfigurator::filter(envelope,keyboard,offset,settings,48,1,44100);
    envelope.noteOn();envelope.render(1,[](double,int){});
    EXPECT_EQ(envelope.getLevel(),512.25);
    FilterEnvelope empty;CutoffControl cutoff;cutoff.prepare(44100);VoiceControlBuffer indices{};
    FilterControlRenderer::render({true,false,512},{},empty,cutoff,0,offset,64,indices);
    for(int i=0;i<64;++i)EXPECT_EQ(indices[i],511.75);
}

TEST(BoundedPrecision, FractionalUnisonPhaseSpreadPreservesTheSharedDrawStream)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[75]=58.25/127;controls[92]=64.03125f/127;
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
        const double expected=dial==64.03125f ? 515.25 : 515.5;
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
    FilterEnvelope a,b;double x=0,y=0;
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
    for(double base:{512.25,512.5})
    {
        FilterEnvelopeSettings settings;settings.cutoffIndex=base;
        settings.amount=1;settings.sustain=1;settings.release=.25f;
        FilterEnvelope envelope;double keyboard=0;
        VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,48,1,44100);
        envelope.noteOn();envelope.render(1,[](double,int){});
        EXPECT_EQ(keyboard,0);EXPECT_EQ(envelope.getLevel(),1024-base);
        envelope.noteOff();EXPECT_EQ(envelope.getLevel(),1024-base);
        bool fractional=false;
        envelope.render(64,[&](double level,int count) {
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
            std::vector<double> levels;levels.reserve(44100);
            envelope.render(44100,[&](double value,int count) {
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
    RandomLfo target;target.setWave(3,0,10.25,draws);
    EXPECT_EQ(draws.calls,1);EXPECT_DOUBLE_EQ(target.current,6.75);
    EXPECT_DOUBLE_EQ(target.query(3,0,10.25,0,1000,32).first,6.75);
    int counter=0;
    target.advance(999,counter,1000,3,0,10.25,draws);EXPECT_EQ(draws.calls,1);
    target.advance(1,counter,1000,3,0,10.25,draws);EXPECT_EQ(draws.calls,2);
    EXPECT_EQ(target.previous,6.75);EXPECT_EQ(target.current,6.75);
    target.setWave(3,.25,.25,draws);EXPECT_EQ(draws.calls,2);
    const auto fixed=target.query(3,.25,.25,7,1000,32);
    EXPECT_EQ(fixed.first,.25);EXPECT_EQ(fixed.second,-1);
}

TEST(BoundedPrecision, SmoothedRandomQueriesKeepFractionalEndpointsWithIntegerBounds)
{
    synth2::dsp::RandomLfo target{8.5,2.25};
    const auto first=target.query(4,0,16,0,1000,32);
    const auto last=target.query(4,0,16,999,1000,32);
    EXPECT_EQ(first.first,2.25);EXPECT_EQ(last.first,8.5);
    EXPECT_GE(first.second,32);EXPECT_GE(last.second,32);
    EXPECT_EQ(target.current,8.5);EXPECT_EQ(target.previous,2.25);
}

TEST(BoundedPrecision, FractionalRandomDepthDoesNotRedrawHeldHistory)
{
    using synth2::dsp::RandomLfoRuntime;
    RandomLfoRuntime state;
    std::array<RandomLfoRuntime::Control,2> controls{{{4,3,64},{1,0,0}}};
    state.load({},controls);
    const auto seed=state.random.state;const double held=state.targets[0].current;
    const auto range=state.effectiveRange(0,64.03125f);
    EXPECT_EQ(range.first,1032.5);EXPECT_EQ(range.second,0);
    const auto query=state.query(0,0,1000,32,64.03125f);
    EXPECT_EQ(query.first,1032.5-held);EXPECT_NE(query.first,std::trunc(query.first));
    EXPECT_EQ(state.random.state,seed);EXPECT_EQ(state.targets[0].current,held);
    auto expected=state.random;(void)expected.next();
    state.advance(0,1000,0,1000,64.03125f);EXPECT_EQ(state.random.state,expected.state);
}

TEST(BoundedPrecision, FractionalGlideHistorySurvivesRetriggerAndReceiverTransport)
{
    using namespace synth2::dsp;
    Portamento state;state.setDuration(Portamento::timeSeconds(64),44100);
    state.setNote(60,false);state.offset=.25;state.setNote(72,true);
    EXPECT_EQ(state.offset,-127.75);EXPECT_EQ(state.delta,-127.75);
    OscillatorControlSettings config;config.portamentoDial=64;config.applyPortamento=true;
    VoiceSpanBuffer spans{};for(int i=0;i<64;++i)spans[i]=64-i;
    VoiceControlBuffer pitch{};
    OscillatorControlRenderer::glide(config,{},state,64,spans,pitch);
    EXPECT_EQ(pitch[0],-127.75);EXPECT_NE(state.offset,std::trunc(state.offset));
    const double retained=state.offset;
    state.setNote(76,true);EXPECT_EQ(state.delta,retained-42);
}

TEST(BoundedPrecision, LfoDepthCurvesKeepSubUnitValuesAcrossFourWaveforms)
{
    using Curve=synth2::dsp::LfoShape::DepthCurve;
    const auto sine=Curve(4,.125).query(0,250,1000,32);
    EXPECT_DOUBLE_EQ(sine.first,.125);EXPECT_EQ(sine.second,32);
    const auto triangle=Curve(5,64.125).query(1,333,1000,32);
    EXPECT_DOUBLE_EQ(triangle.first,171.375);EXPECT_EQ(triangle.second,32);
    const auto saw=Curve(2,64.125).query(2,1200,4096,32);
    EXPECT_DOUBLE_EQ(saw.first,365.75);EXPECT_EQ(saw.second,32);
    const auto square=Curve(3,.03125).query(3,750,1000,32);
    EXPECT_DOUBLE_EQ(square.first,.5);EXPECT_EQ(square.second,250);
    for(int wave=0;wave<4;++wave)
    {
        const auto silent=Curve(2,0).query(wave,123,1000,32);
        EXPECT_EQ(silent.first,0);EXPECT_EQ(silent.second,-1);
    }
}

TEST(BoundedPrecision, FractionalPortamentoTimeReachesTheSampleClock)
{
    using namespace synth2::dsp;
    const double first=Portamento::timeSeconds(64.125f),second=Portamento::timeSeconds(64.375f);
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
    envelope.configure(0,0,512.25,256.125,1000);
    envelope.noteOn();
    OscillatorControlSettings config;config.envelopeEnabled=true;
    VoiceControlBuffer signal{};VoiceSpanBuffer spans{};
    OscillatorControlRenderer::envelope(config,{},envelope,32,signal,spans);
    for(int i=0;i<32;++i){EXPECT_EQ(signal[i],256.125);EXPECT_EQ(spans[i],32-i);}
    EXPECT_EQ(envelope.getLevel(),256.125);
    envelope.noteOff();
    bool fractional=false;
    envelope.render(32,[&](double value,int count){
        EXPECT_GE(value,0);EXPECT_LE(value,256.125);EXPECT_GT(count,0);
        fractional=fractional || value!=std::trunc(value);
    });
    EXPECT_TRUE(fractional);
}

TEST(BoundedPrecision, AmplitudeSustainAndReleaseRetainSubLevelValues)
{
    using namespace synth2::dsp;
    AmplitudeEnvelope amplitude;
    amplitude.setParameters(0,0,1024.25,4096);amplitude.noteOn();
    double gain=0;amplitude.render(&gain,1);
    EXPECT_EQ(amplitude.getLevel(),1024.25);
    EXPECT_EQ(amplitude.getCachedLevel(),1024.25);
    EXPECT_EQ(gain,1024.25f/2048);
    amplitude.noteOff();amplitude.render(&gain,1);
    EXPECT_EQ(amplitude.getPhase(),AmplitudeEnvelope::Phase::Release);
    EXPECT_NE(amplitude.getLevel(),std::trunc(amplitude.getLevel()));
    ReleaseRamp release;release.start(4096,1024.25);release.advance(1);
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
    double maximum=0;bool fractional=false;
    envelope.render(64,[&](double value,int){maximum=std::max(maximum,value);fractional|=value!=std::trunc(value);});
    EXPECT_GT(maximum,0);EXPECT_LT(maximum,1.0);EXPECT_TRUE(fractional);
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
    settings.sustain=64.125/127;
    double keyboard=0;
    VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,60,1,44100);
    envelope.noteOn();
    envelope.render(1,[](double value,int count){EXPECT_DOUBLE_EQ(value,56.375);EXPECT_EQ(count,1);});
    EXPECT_DOUBLE_EQ(envelope.getLevel(),56.375);
}

TEST(BoundedPrecision, LiveDepthChangesPreserveSubUnitEnvelopePosition)
{
    using synth2::dsp::FilterEnvelope;
    FilterEnvelope envelope;envelope.configure(1,1000,.5,0,1000);
    envelope.noteOn();envelope.render(1,[](double,int){});
    ASSERT_EQ(envelope.getLevel(),.5);
    envelope.configure(1,1000,1.25,0,1000);
    EXPECT_EQ(envelope.getLevel(),1.25);
    envelope.configure(1,1000,.25,0,1000);
    EXPECT_EQ(envelope.getLevel(),.25);
}

TEST(BoundedPrecision, SignalStorageKeepsFractionalPhaseUnitsAndDiscreteSpans)
{
    using namespace synth2::dsp;
    static_assert(std::is_same_v<VoiceSpanBuffer::value_type,int>);
    VoiceControlBuffer values{};
    values[0]=67108863.25;
    values[1]=.125;
    ReceiverLfoView view;
    view.values[0]=values.data();
    EXPECT_DOUBLE_EQ(view.values[0][0],67108863.25);
    EXPECT_DOUBLE_EQ(view.values[0][1],.125);
}

TEST(BoundedPrecision, FmKeepsFractionalDepthThroughDispatchAndSlew)
{
    using synth2::dsp::FrequencyModulation;
    FrequencyModulation state;
    int samples=0;
    state.render(1.25,64,44100,[&](double value){EXPECT_EQ(value,1.25);++samples;});
    EXPECT_EQ(samples,64);
    EXPECT_EQ(state.current,1.25);
    state.render(12.75,64,44100,[](double){});
    EXPECT_EQ(state.target,12.75);
    EXPECT_EQ(state.anchor,1.25);
    EXPECT_NE(state.current,std::trunc(state.current));
    const double depth=FrequencyModulation::depth(64.25,.125);
    EXPECT_GT(depth,FrequencyModulation::depth(64,0));
    EXPECT_LT(depth,FrequencyModulation::depth(65,1));
}

TEST(BoundedPrecision, FractionalFilterModulationReachesCoefficients)
{
    using namespace synth2::dsp;
    EXPECT_EQ(CutoffControl::target(512.25,0,0,0,0,0),512.25);
    EXPECT_EQ(CutoffControl::target(512,0,0,0,1.5,0),512.5);
    VoiceControlBuffer modulation{},indices{};
    modulation.fill(1.5);
    VoiceSpanBuffer spans{};
    for(int i=0;i<64;++i)spans[i]=64-i;
    ReceiverLfoView view;view.available=true;view.receivers[0]={true,2};
    view.values[0]=modulation.data();view.remaining[0]=spans.data();
    FilterEnvelope envelope;CutoffControl cutoff;cutoff.prepare(44100);
    FilterControlRenderer::render({true,true,512},view,envelope,cutoff,0,0,64,indices);
    for(int i=0;i<64;++i)EXPECT_EQ(indices[i],512.5);
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
    VoiceControlBuffer first{},second{};first.fill(.25);second.fill(.5);
    VoiceSpanBuffer spans{};spans.fill(1);
    ReceiverLfoView view;view.available=true;
    view.receivers[0]={true,3};view.receivers[1]={true,3};
    view.values={first.data(),second.data()};view.remaining={spans.data(),spans.data()};
    AmplitudeEnvelope amplitude;amplitude.setParameters(0,0,2048,0);amplitude.noteOn();
    double gain=0;
    AmplitudeBlockSettings config;
    ASSERT_TRUE(AmplitudeBlockRenderer::render(config,view,amplitude,1,&gain));
    const double expected=((2048-.25)*(2048-.5))/4194304.0;
    EXPECT_EQ(gain,expected);EXPECT_LT(gain,1);
}

TEST(BoundedPrecision, PitchAndPulseModulationRetainFractionalCoordinates)
{
    using synth2::dsp::ModulationEnvelope;
    EXPECT_DOUBLE_EQ(ModulationEnvelope::pulseWidthBins(.5f,.125),513.5);
    EXPECT_GT(ModulationEnvelope::pitchFrequency(440.0f,.125),440);
    EXPECT_LT(ModulationEnvelope::pitchFrequency(440.0f,.125),ModulationEnvelope::pitchFrequency(440.0f,1));
}

TEST(BoundedPrecision, GainAndVelocityInterpolateCompatibleIntegerAnchors)
{
    using Envelope=synth2::dsp::AmplitudeEnvelope;
    for(int dial=0;dial<127;++dial)
    {
        EXPECT_EQ(Envelope::gainForDial(static_cast<float>(dial)),Envelope::gainForDial(dial));
        const double a=Envelope::gainForDial(dial),b=Envelope::gainForDial(dial+1);
        EXPECT_EQ(Envelope::gainForDial(dial+.25f),a+(b-a)*.25f);
        const double first=Envelope::velocityForDials(100,dial),second=Envelope::velocityForDials(100,dial+1);
        EXPECT_EQ(Envelope::velocityForDials(100,dial+.25f),first+(second-first)*.25f);
    }
    EXPECT_GT(Envelope::gainForDial(.25f),0);
    EXPECT_LT(Envelope::gainForDial(.25f),Envelope::gainForDial(1));
}

TEST(BoundedPrecision, SaturationKeepsIntegerResponseAndFractionalDrive)
{
    for(int dial=0;dial<=127;++dial)
    {
        const double exponent=dial*(1.0/127.0)-1.0;
        const double curve=std::exp(static_cast<double>(exponent)*5.8)*dial*(1.0/127.0);
        const double expected=curve*100.0;
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
    ASSERT_TRUE(delay->setFrameTargets(10.25,10.25));delay->reset();
    EXPECT_DOUBLE_EQ(delay->getCurrentLeft(),10.25);
    EXPECT_DOUBLE_EQ(delay->getLeftFramesContinuous(),10.25);
    std::array<double,32> result{};
    for(int i=0;i<32;++i)
    {
        double dry[2]{i==0 ? 1.0f : 0,0},send[2]{};
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
    EXPECT_DOUBLE_EQ(chorus->getDelayFramesContinuous(),667.375);
    std::array<double,700> result{};
    for(int i=0;i<700;++i)
    {
        double dry[2]{i==0 ? 1.0f : 0,0};chorus->process(dry,nullptr,1);result[i]=dry[0];
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
    EXPECT_EQ(effect.snapshot()[1],64.125f);EXPECT_EQ(effect.snapshot()[0],64.125*(1.0/127.0));
}

TEST(BoundedPrecision, FractionalDecimationPeriodKeepsItsSampleClockRemainder)
{
    synth2::dsp::MultiEffect effect;effect.prepare(44100);effect.setDials(.5f,0,127);
    std::array<double,8> audio{0,.25f,.5f,.75f,1,1.25f,1.5f,1.75f};
    effect.process(3,audio.data(),nullptr,audio.size(),false);
    const std::array<double,8> expected{0,0,.5f,.75f,.75f,1.25f,1.5f,1.5f};
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

TEST(BoundedPrecision, FilterRecurrenceRetainsInputDifferencesBelowFloatResolution)
{
    synth2::dsp::StateVariableFilter first,second;
    first.configureCoefficients(.25,0);second.configureCoefficients(.25,0);
    const double base=first.process(1.0);
    const double changed=second.process(1.0+0x1p-40);
    EXPECT_GT(changed,base);
    EXPECT_LT(changed-base,0x1p-35);
}

TEST(BoundedPrecision, DelayHistoryRetainsInputDifferencesBelowFloatResolution)
{
    auto first=std::make_unique<synth2::dsp::DelayKernel>();
    auto second=std::make_unique<synth2::dsp::DelayKernel>();
    for(auto* delay:{first.get(),second.get()})
    {
        ASSERT_TRUE(delay->configure(44100,0,10.0/44100,0,0,127,64));
        ASSERT_TRUE(delay->setFrameTargets(10,10));delay->reset();
    }
    bool retained=false;
    for(int frame=0;frame<32;++frame)
    {
        double a[2]{frame==0 ? 1.0 : 0.0,0},b[2]{frame==0 ? 1.0+0x1p-40 : 0.0,0};
        double sendA[2]{},sendB[2]{};
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
    fractional.setSynchronizedRate(92,120.125);
    last.setSynchronizedRate(92,121);
    first.advance(44100);
    EXPECT_EQ(first.period,14699);
    EXPECT_GT(fractional.targetHz,first.targetHz);
    EXPECT_LT(fractional.targetHz,last.targetHz);
    EXPECT_DOUBLE_EQ(fractional.targetHz,first.targetHz+(last.targetHz-first.targetHz)*.125);
    EXPECT_NE(fractional.targetHz,static_cast<double>(static_cast<float>(fractional.targetHz)));
}

TEST(BoundedPrecision, PitchModulationPreservesSubFloatBaseFrequency)
{
    using synth2::dsp::ModulationEnvelope;
    constexpr double delta=0x1p-20;
    const double frequency=440.0+delta;
    ASSERT_EQ(static_cast<float>(frequency),440.0f);
    EXPECT_DOUBLE_EQ(ModulationEnvelope::pitchFrequency(frequency,128),880.0+delta*2);
    EXPECT_DOUBLE_EQ(ModulationEnvelope::pitchFrequency(frequency,-128),220.0+delta*.5);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(440.0,128),880.0);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(440.0,-128),220.0);
    EXPECT_EQ(ModulationEnvelope::pitchFrequency(22000.0+delta,128),22000.0);
}

TEST(BoundedPrecision, FractionalDurationsDoNotSnapToTheNextFrameEarly)
{
    using synth2::dsp::ReleaseRamp;
    constexpr float anchor=.01f;
    const double lower=std::nextafter(anchor,0.0f);
    const double edited=lower+(static_cast<double>(anchor)-lower)*.75;
    ASSERT_EQ(static_cast<float>(edited),anchor);
    EXPECT_EQ(ReleaseRamp::durationFrames(anchor,44100),441);
    EXPECT_EQ(ReleaseRamp::durationFrames(edited,44100),440);
    const double rate=44100.0-0x1p-10;
    ASSERT_EQ(static_cast<float>(rate),44100.f);
    EXPECT_EQ(ReleaseRamp::durationFrames(anchor,rate),440);
    EXPECT_EQ(ReleaseRamp::durationFrames(anchor,44100),441);
}

TEST(BoundedPrecision, SubFloatFixedPhaseEditsReachTheFinalPhaseWord)
{
    using namespace synth2::dsp;
    const double edited=64.0+0x1p-19;
    ASSERT_EQ(static_cast<float>(edited),64.0f);
    OscillatorPhaseState base,fine;base.setDial(64);fine.setDial(edited);
    EXPECT_EQ(fine.dial,edited);
    OscillatorPhaseRandom a,b,untouched;
    ASSERT_TRUE(base.start(a));ASSERT_TRUE(fine.start(b));
    EXPECT_EQ(fine.osc2,base.osc2+1);
    const auto next=untouched.nextPhase();EXPECT_EQ(a.nextPhase(),next);EXPECT_EQ(b.nextPhase(),next);
    // Selection is a discrete compatible word, separate from phase interpolation.
    OscillatorPhaseState boundary;boundary.setDial(std::nextafter(.5,0.0));
    OscillatorPhaseRandom random,expected;ASSERT_TRUE(boundary.start(random));
    EXPECT_EQ(boundary.osc1,0u);EXPECT_EQ(boundary.osc2,0u);
    EXPECT_EQ(random.nextPhase(),expected.nextPhase());
}

TEST(BoundedPrecision, SubFloatSpreadEditsRetainLocalDrawOrder)
{
    using namespace synth2::dsp;
    const double edited=64.0+0x1p-19;ASSERT_EQ(static_cast<float>(edited),64.0f);
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

TEST(BoundedPrecision, DelayTempoRetainsSubFloatCoordinates)
{
    const double tempo=120.0+0x1p-20;
    ASSERT_EQ(static_cast<float>(tempo),120.0f);
    const double native=synth2::dsp::EffectControls::delaySeconds(7,120);
    const double edited=synth2::dsp::EffectControls::delaySeconds(7,tempo);
    EXPECT_EQ(native,.1875);EXPECT_LT(edited,native);
    EXPECT_EQ(static_cast<float>(edited),static_cast<float>(native));
}

TEST(BoundedPrecision, LoadedFractionalPhaseInterpolatesCompatiblePhaseWords)
{
    using namespace synth2::dsp;
    std::array<float,128> controls{};controls[91]=64.125f/127;
    synth2::Synth1PatchMapper mapper(113);mapper.setContinuousControls(true);mapper.setDirectControls(true);
    synth2::RenderControls mapped;mapper.mapFromRawBuffer(controls.data(),mapped);
    ASSERT_EQ(mapped.oscillatorPhaseDial,64.125);
    OscillatorPhaseState low,high,edited;low.setDial(64);high.setDial(65);edited.setDial(mapped.oscillatorPhaseDial);
    OscillatorPhaseRandom random;
    ASSERT_TRUE(low.start(random));ASSERT_TRUE(high.start(random));ASSERT_TRUE(edited.start(random));
    const auto expected=[](uint32_t a,uint32_t b){return static_cast<uint32_t>(a+(static_cast<double>(b)-a)*.125);};
    EXPECT_EQ(edited.osc2,expected(low.osc2,high.osc2));
    for(size_t i=0;i<edited.lanes.size();++i)
        EXPECT_EQ(edited.lanes[i],expected(low.lanes[i],high.lanes[i]));
}
