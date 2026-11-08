#include <gtest/gtest.h>
#include "dsp/Lfo.h"
#include "dsp/SharedModulationRuntime.h"
#include <cmath>
#include <cstring>
#include <type_traits>
#include <vector>

TEST(ModulationBehavior, EveryWaveformIsFiniteBoundedAndResettable)
{
    for(int wave=0;wave<6;++wave)
    {
        SCOPED_TRACE(wave);
        synth2::dsp::Lfo lfo;
        lfo.prepare(48000.25);
        synth2::LfoSettings settings; settings.on=true; settings.wave=wave;
        settings.amount=.37f; settings.speed=1;
        std::vector<float> values;
        float peak=0;
        for(int i=0;i<4096;++i)
        {
            const float value=lfo.processSample(settings,120);
            ASSERT_TRUE(std::isfinite(value)); EXPECT_LE(std::abs(value),settings.amount);
            peak=std::max(peak,std::abs(value)); values.push_back(value);
        }
        EXPECT_GT(peak,.05);
        lfo.reset();
        for(float value:values) EXPECT_EQ(lfo.processSample(settings,120),value);
    }
}

TEST(ModulationBehavior, ZeroDepthKeepsPhaseAndRestoringDepthMatchesRunningSource)
{
    synth2::dsp::Lfo muted,audible;
    muted.prepare(48000); audible.prepare(48000);
    synth2::LfoSettings zero,full; zero.on=full.on=true; full.amount=1;
    for(int i=0;i<513;++i)
    { EXPECT_EQ(muted.processSample(zero,120),0); audible.processSample(full,120); }
    EXPECT_NE(muted.nativePhaseIndex(),0);
    EXPECT_EQ(muted.nativePhaseIndex(),audible.nativePhaseIndex());
    EXPECT_EQ(muted.processSample(full,120),audible.processSample(full,120));
}

TEST(ModulationBehavior, DisabledWaveformSourcePausesAndPhaseResetRestartsDeterministicWave)
{
    synth2::dsp::Lfo source,fresh;
    source.prepare(48000); fresh.prepare(48000);
    synth2::LfoSettings settings; settings.on=true; settings.amount=1;
    for(int i=0;i<257;++i) source.processSample(settings,120);
    const auto phase=source.nativePhaseIndex();
    settings.on=false;
    for(int i=0;i<257;++i) EXPECT_EQ(source.processSample(settings,120),0);
    EXPECT_EQ(source.nativePhaseIndex(),phase);
    settings.on=true; source.resetPhase();
    EXPECT_EQ(source.processSample(settings,120),fresh.processSample(settings,120));
}

TEST(ModulationBehavior, SynchronizedPeriodFollowsTempoRatio)
{
    const auto frequency=[](float bpm)
    {
        synth2::dsp::Lfo source; source.prepare(1000);
        synth2::LfoSettings settings; settings.on=true; settings.amount=1;
        settings.tempoSync=true; settings.speed=100.f/127;
        std::vector<int> crossings; int previous=source.nativePhaseIndex();
        for(int i=0;i<8000;++i)
        {
            source.processSample(settings,bpm);
            const int phase=source.nativePhaseIndex();
            if(phase<previous) crossings.push_back(i);
            previous=phase;
        }
        EXPECT_GT(crossings.size(),8u);
        return 1000.*(crossings.size()-1)/(crossings.back()-crossings.front());
    };
    EXPECT_NEAR(frequency(180)/frequency(90),2,.005);
}

namespace
{
synth2::dsp::SharedModulationSettings sharedSettings()
{
    synth2::dsp::SharedModulationSettings settings;
    for(auto& lfo:settings.lfos)
    { lfo.on=true; lfo.nativeSharedClock=true; lfo.keySync=true; lfo.amount=.5f; lfo.speed=.5f; }
    return settings;
}
void prepare(synth2::dsp::SharedModulationRuntime& runtime,const synth2::dsp::SharedModulationSettings& settings)
{ runtime.prepareSources(48000,64,settings); runtime.synchronize(settings,48000); }
void advance(synth2::dsp::SharedModulationRuntime& runtime,const synth2::dsp::SharedModulationSettings& settings)
{ runtime.beginChunk(0); runtime.renderSpan(0,64,settings); }
}

TEST(ModulationBehavior, FractionalSharedDepthChangesSignalsWithoutChangingClocksOrRandomHistory)
{
    using synth2::dsp::SharedModulationRuntime;
    for(int wave=0;wave<4;++wave)
    {
        SCOPED_TRACE(wave);
        auto a=sharedSettings(),b=a;
        a.lfos[0].dest=b.lfos[0].dest=3;
        a.lfos[0].wave=b.lfos[0].wave=wave;
        a.lfos[0].speed=b.lfos[0].speed=64.0f/127;
        a.lfos[0].amount=64.03125f/127;b.lfos[0].amount=64.0625f/127;
        SharedModulationRuntime first,second;prepare(first,a);prepare(second,b);
        bool differs=false,fractional=false;
        for(int block=0;block<512;++block)
        {
            advance(first,a);advance(second,b);
            for(int i=0;i<64;++i)
            {
                const double x=first.block().amplitudeControl[0][i],y=second.block().amplitudeControl[0][i];
                ASSERT_TRUE(std::isfinite(x));ASSERT_TRUE(std::isfinite(y));
                differs|=x!=y;fractional|=x!=std::trunc(x);
                EXPECT_GT(first.block().controlRemaining[0][i],0);
            }
            EXPECT_EQ(first.clock(0).counter,second.clock(0).counter);
            EXPECT_EQ(first.clock(0).period,second.clock(0).period);
            EXPECT_EQ(first.randomState().random.state,second.randomState().random.state);
        }
        EXPECT_TRUE(differs);EXPECT_TRUE(fractional);
    }
}

TEST(ModulationBehavior, FractionalRandomDepthPreservesSharedDrawSequenceAndKeySync)
{
    using namespace synth2::dsp;
    for(int wave:{4,5})
    {
        SCOPED_TRACE(wave);
        auto a=sharedSettings(),b=a;
        a.lfos[0].dest=b.lfos[0].dest=3;a.lfos[0].wave=b.lfos[0].wave=wave;
        a.lfos[0].speed=b.lfos[0].speed=64.0f/127;
        a.lfos[0].amount=64.03125f/127;b.lfos[0].amount=64.0625f/127;
        LfoHistoryEvent load;load.kind=1;load.randomAdmitted=true;
        load.randomControls={{{4,wave==4 ? 3 : 4,64},{1,0,0}}};
        SharedModulationRuntime first,second;
        first.applyHistory(load,120);second.applyHistory(load,120);
        prepare(first,a);prepare(second,b);
        const auto note=synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100});
        first.trackKey(note,a);second.trackKey(note,b);
        bool differs=false,fractional=false;
        for(int block=0;block<512;++block)
        {
            advance(first,a);advance(second,b);
            for(int i=0;i<64;++i)
            {
                const auto x=first.block().amplitudeControl[0][i],y=second.block().amplitudeControl[0][i];
                ASSERT_TRUE(std::isfinite(x));ASSERT_TRUE(std::isfinite(y));
                differs|=x!=y;fractional|=x!=std::trunc(x);
            }
            EXPECT_EQ(first.randomState().random.state,second.randomState().random.state);
            EXPECT_EQ(first.clock(0).counter,second.clock(0).counter);
        }
        EXPECT_TRUE(differs);EXPECT_TRUE(fractional);
    }
}

TEST(ModulationBehavior, FractionalSharedRateSurvivesBothSlotsSyncAndRepeatedUpdates)
{
    using synth2::dsp::SharedModulationRuntime;
    for(int slot:{0,1})
    {
        SCOPED_TRACE(slot);
        auto a=sharedSettings(),b=a;
        a.lfos[slot].speed=64.03125f/127;b.lfos[slot].speed=64.0625f/127;
        SharedModulationRuntime first,second;prepare(first,a);prepare(second,b);
        const float firstHz=first.clock(slot).targetHz,secondHz=second.clock(slot).targetHz;
        ASSERT_LT(firstHz,secondHz);
        bool differs=false;
        for(int block=0;block<256;++block)
        {
            advance(first,a);advance(second,b);
            const auto snapshot=first.clock(slot);
            first.synchronize(a,48000);second.synchronize(b,48000);
            EXPECT_EQ(first.clock(slot).counter,snapshot.counter);
            EXPECT_EQ(first.clock(slot).period,snapshot.period);
            EXPECT_EQ(first.clock(slot).currentHz,snapshot.currentHz);
            for(int i=0;i<64;++i)
                differs|=first.block().amplitudeControl[slot][i]!=second.block().amplitudeControl[slot][i];
        }
        EXPECT_TRUE(differs);
        for(bool sync:{true,false})
        {
            a.lfos[slot].tempoSync=b.lfos[slot].tempoSync=sync;
            first.applyHistory({3,slot,sync},120);second.applyHistory({3,slot,sync},120);
            first.synchronize(a,48000);second.synchronize(b,48000);
            EXPECT_EQ(first.clock(slot).rateDial,64.03125f);
            EXPECT_EQ(second.clock(slot).rateDial,64.0625f);
            if(sync)EXPECT_EQ(first.clock(slot).targetHz,second.clock(slot).targetHz);
            else {EXPECT_EQ(first.clock(slot).targetHz,firstHz);EXPECT_EQ(second.clock(slot).targetHz,secondHz);}
        }
    }
}

TEST(ModulationBehavior, FirstHeldKeyResetsClockButOverlappingKeysDoNot)
{
    auto settings=sharedSettings();
    synth2::dsp::SharedModulationRuntime runtime; prepare(runtime,settings);
    advance(runtime,settings); ASSERT_NE(runtime.clock(0).counter,0);
    runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),settings);
    EXPECT_EQ(runtime.clock(0).counter,0);
    advance(runtime,settings); const auto phase=runtime.clock(0).counter;
    runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,67,uint8_t{100}),settings);
    EXPECT_EQ(runtime.clock(0).counter,phase);
    runtime.trackKey(synth2::audio::MidiMessage::noteOff(1,60),settings);
    runtime.trackKey(synth2::audio::MidiMessage::noteOff(1,67),settings);
    runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,72,uint8_t{100}),settings);
    EXPECT_EQ(runtime.clock(0).counter,0);
}

TEST(ModulationBehavior, DisabledSharedClocksAdvanceAndEnablingKeepsTheirHistory)
{
    auto settings=sharedSettings(); for(auto& lfo:settings.lfos) lfo.on=false;
    synth2::dsp::SharedModulationRuntime runtime; prepare(runtime,settings);
    const auto before=runtime.clock(0).counter;
    advance(runtime,settings);
    EXPECT_NE(runtime.clock(0).counter,before);
    const auto running=runtime.clock(0).counter;
    settings.lfos[0].on=true; runtime.synchronize(settings,48000);
    EXPECT_EQ(runtime.clock(0).counter,running);
    advance(runtime,settings); EXPECT_NE(runtime.clock(0).counter,running);
}

TEST(ModulationBehavior, ConsumerViewsAndBoundaryQueriesDoNotAdvanceClocks)
{
    auto settings=sharedSettings();
    synth2::dsp::SharedModulationRuntime runtime; prepare(runtime,settings); advance(runtime,settings);
    static_assert(std::is_same_v<decltype(runtime.clock(0)),const synth2::dsp::LfoClock&>);
    static_assert(std::is_same_v<decltype(runtime.block()),const synth2::dsp::LfoBlock&>);
    const auto first=runtime.clock(0).counter,second=runtime.clock(1).counter;
    for(int i=0;i<32;++i)
    {
        EXPECT_GT(runtime.randomBoundary(64,settings),0);
        EXPECT_GE(runtime.block().samples.getSample(0,i),-1);
        EXPECT_EQ(runtime.clock(0).counter,first); EXPECT_EQ(runtime.clock(1).counter,second);
    }
}

TEST(ModulationBehavior, FirstKeyDoesNotResetAnUnsynchronizedReceiver)
{
    auto settings=sharedSettings(); settings.lfos[0].keySync=false;
    synth2::dsp::SharedModulationRuntime runtime; prepare(runtime,settings); advance(runtime,settings);
    const auto first=runtime.clock(0).counter; ASSERT_NE(first,0);
    runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),settings);
    EXPECT_EQ(runtime.clock(0).counter,first);
    EXPECT_EQ(runtime.clock(1).counter,0); // The second slot remains key synchronized.
}

TEST(ModulationBehavior, LoadRoutingRateRedispatchPrecedesTheSyncSetter)
{
    using synth2::dsp::SharedModulationRuntime;
    for(bool oldSync:{false,true})for(bool nextSync:{false,true})
        for(int destination:{43,48})for(int routes:{1,2})for(int dial:{7,37,89})
            for(bool queued:{false,true})
    {
        SCOPED_TRACE(::testing::Message()<<oldSync<<' '<<nextSync<<' '<<destination<<' '<<routes<<' '<<dial<<' '<<queued);
        auto settings=sharedSettings();
        settings.midiRoutingLoadHistory=true;
        settings.midiRoutingControls={0,0,0,0};
        for(auto& lfo:settings.lfos){lfo.speed=0;lfo.keySync=false;lfo.tempoSync=oldSync;}
        SharedModulationRuntime loaded,explicitSetters;
        for(auto* runtime:{&loaded,&explicitSetters})
        {
            runtime->prepareSources(44100,64,settings);
            runtime->synchronize(settings,44100);
            runtime->beginChunk(0);runtime->renderSpan(0,64,settings);
        }
        const int slot=destination==43 ? 0 : 1;
        settings.midiRoutingControls={45057,destination,routes==2 ? 45057 : 0,routes==2 ? destination : 0};
        settings.lfos[slot].speed=static_cast<float>(dial)/127.0f;
        settings.lfos[slot].tempoSync=nextSync;
        // Compare a full load with its explicit ordered setter protocol. MIDI
        // routes and the ordinary rate use the retained sync mode first.
        for(int call=0;call<routes+1;++call)explicitSetters.applyHistory({0,slot,dial},120);
        explicitSetters.applyHistory({3,slot,nextSync},120);
        if(queued)
        {
            synth2::dsp::LfoHistoryEvent event;
            event.kind=2;event.a=slot==0 ? dial : 0;event.b=slot==1 ? dial : 0;
            event.routes=settings.midiRoutingControls;
            event.sync={settings.lfos[0].tempoSync,settings.lfos[1].tempoSync};
            loaded.applyHistory(event,120);
        }
        else{loaded.requestReload();loaded.synchronize(settings,44100);}
        const auto& expected=explicitSetters.clock(slot);const auto& actual=loaded.clock(slot);
        EXPECT_EQ(actual.counter,expected.counter);EXPECT_EQ(actual.period,expected.period);
        EXPECT_EQ(actual.currentHz,expected.currentHz);EXPECT_EQ(actual.targetHz,expected.targetHz);
        EXPECT_EQ(actual.step,expected.step);EXPECT_EQ(actual.rate,expected.rate);
    }
}

TEST(ModulationBehavior, EveryBackendGeneratesKeySynchronizedSignalsAfterMidiDispatch)
{
    using namespace synth2::dsp;
    for(bool native:{false,true})
    {
        auto settings=sharedSettings();
        for(auto& lfo:settings.lfos)lfo.nativeSharedClock=native;
        SharedModulationRuntime runtime;prepare(runtime,settings);advance(runtime,settings);
        Lfo reference;reference.prepare(48000);reference.setPhase(0);
        // Chunk storage is established before MIDI; generation must follow it.
        runtime.beginChunk(100);
        runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),settings);
        runtime.renderSpan(100,17,settings);
        for(int i=0;i<17;++i)
            EXPECT_EQ(runtime.block().samples.getSample(0,i),reference.processSample(settings.lfos[0],settings.hostBpm));
        const auto counter=runtime.clock(0).counter;
        runtime.trackKey(synth2::audio::MidiMessage::noteOn(2,67,uint8_t{100}),settings);
        EXPECT_EQ(runtime.clock(0).counter,counter);
        runtime.renderSpan(117,47,settings);
        for(int i=17;i<64;++i)
            EXPECT_EQ(runtime.block().samples.getSample(0,i),reference.processSample(settings.lfos[0],settings.hostBpm));
        runtime.trackKey(synth2::audio::MidiMessage::noteOff(1,60),settings);
        runtime.trackKey(synth2::audio::MidiMessage::noteOff(2,67),settings);
        runtime.beginChunk(164);
        runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,72,uint8_t{100}),settings);
        EXPECT_EQ(runtime.clock(0).counter,0);
        reference.setPhase(0);runtime.renderSpan(164,64,settings);
        for(int i=0;i<64;++i)
            EXPECT_EQ(runtime.block().samples.getSample(0,i),reference.processSample(settings.lfos[0],settings.hostBpm));
    }
}

TEST(ModulationBehavior, UnknownRandomHistoryKeepsSharedTargetsAcrossKeyResets)
{
    using namespace synth2::dsp;
    auto settings=sharedSettings();
    for(auto& lfo:settings.lfos)
    { lfo.nativeSharedClock=false;lfo.wave=4;lfo.speed=1; }
    SharedModulationRuntime runtime;prepare(runtime,settings);runtime.invalidateRandomHistory();
    for(int block=0;block<16;++block)advance(runtime,settings);
    const float held=runtime.block().samples.getSample(0,63);ASSERT_NE(held,0);
    const auto crtState=runtime.randomState().random.state;
    runtime.trackKey(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),settings);
    EXPECT_EQ(runtime.randomState().random.state,crtState);
    advance(runtime,settings);
    EXPECT_EQ(runtime.block().samples.getSample(0,0),held);
    EXPECT_FALSE(runtime.randomState().valid);
}

#include "dsp/VoiceLfoRenderer.h"
TEST(ModulationBehavior, AllVoiceConsumersReadTheSamePreparedSignalsWithoutAdvancingThem)
{
    using namespace synth2::dsp;
    for(bool native:{false,true})for(int wave=0;wave<6;++wave)
    {
        auto settings=sharedSettings();
        for(auto& lfo:settings.lfos){lfo.nativeSharedClock=native;lfo.wave=wave;}
        SharedModulationRuntime runtime;prepare(runtime,settings);advance(runtime,settings);
        const auto counter=runtime.clock(0).counter;
        const auto random=runtime.randomState().random.state;
        VoiceLfoView view;
        view.shared={runtime.block().samples.getReadPointer(0),runtime.block().samples.getReadPointer(1)};
        const VoiceLfoSettings config{};
        for(int voices:{1,4,32})for(int voice=0;voice<voices;++voice)for(int frame=0;frame<64;++frame)
        {
            const auto values=VoiceLfoRenderer::sample(config,view,frame);
            for(int slot=0;slot<2;++slot)EXPECT_EQ(values.values[slot],runtime.block().samples.getSample(slot,frame));
        }
        EXPECT_EQ(runtime.clock(0).counter,counter);
        EXPECT_EQ(runtime.randomState().random.state,random);
    }
    EXPECT_EQ(VoiceLfoRenderer::sample({}, {},0).values,(std::array<float,2>{0,0}));
}

#include "dsp/ModulationRouter.h"
TEST(ModulationBehavior, SharedAmplitudeReceiverDoesNotDropTheOtherFallbackSlot)
{
    for(bool firstShared:{false,true})
    {
        synth2::dsp::ModulationRoutingSettings routing;
        routing.lfo1.on=routing.lfo2.on=true;routing.lfo1.dest=routing.lfo2.dest=3;
        routing.lfo1.amount=routing.lfo2.amount=.5f;
        routing.lfo1.nativeSharedClock=firstShared;routing.lfo2.nativeSharedClock=!firstShared;
        routing.consumedLfos={firstShared,!firstShared};
        const auto result=synth2::dsp::ModulationRouter::calculate(routing,.25f,-.125f);
        const std::array<double,2> expected{firstShared ? 0.0 : 255.875,firstShared ? 639.6875 : 0.0};
        EXPECT_EQ(result.amplitudeControl,expected);
        routing.consumedLfos={false,false};
        const auto direct=synth2::dsp::ModulationRouter::calculate(routing,.25f,-.125f);
        EXPECT_EQ(direct.amplitudeControl,(std::array<double,2>{255.875,639.6875}));
    }
}

#include "render/VoiceConfiguration.h"
TEST(ModulationInputs, RemainingLfosUseReceiverCoordinatesAndDisabledSourcesAreNeutral)
{
    synth2::dsp::ModulationRoutingSettings routing;auto& p=routing.lfo1;p.on=true;p.amount=1;
    for(int dest:{0,1,2,4,5})
    {
        p.dest=dest;
        const auto low=synth2::dsp::ModulationRouter::calculate(routing,-1,0);
        const auto high=synth2::dsp::ModulationRouter::calculate(routing,1,0);
        if(dest==0 || dest==1){EXPECT_EQ(low.secondPitch,-640);EXPECT_EQ(high.secondPitch,640);}
        if(dest==1){EXPECT_EQ(low.firstPitch,-640);EXPECT_EQ(high.firstPitch,640);}
        if(dest==2){EXPECT_EQ(low.filterControl,0);EXPECT_EQ(high.filterControl,1024);}
        if(dest==4){EXPECT_EQ(low.pulseControl,-128);EXPECT_EQ(high.pulseControl,128);}
        if(dest==5){EXPECT_EQ(low.fmControl,0);EXPECT_EQ(high.fmControl,512);}
        p.on=false;const auto off=synth2::dsp::ModulationRouter::calculate(routing,1,0);p.on=true;
        EXPECT_EQ(off.firstPitch,0);EXPECT_EQ(off.secondPitch,0);EXPECT_EQ(off.filterControl,0);EXPECT_EQ(off.pulseControl,0);EXPECT_EQ(off.fmControl,0);
    }
}
TEST(ModulationInputs, ConsumedSlotCannotDiscardTheOtherSlot)
{
    for(int dest:{0,1,2,4,5})for(bool first:{false,true})
    {
        synth2::RenderControls p;p.lfo1.on=p.lfo2.on=true;p.lfo1.amount=p.lfo2.amount=1;
        p.lfo1.dest=p.lfo2.dest=dest;p.lfo1.nativeSharedClock=first;p.lfo2.nativeSharedClock=!first;
        p.nativePitchLfo=p.nativeFilterLfo=p.nativePulseWidthLfo=p.nativeFmLfo=true;
        const auto routing=synth2::render::modulationRouting(p);
        EXPECT_EQ(routing.consumedLfos,(std::array<bool,2>{first,!first}));
        const auto result=synth2::dsp::ModulationRouter::calculate(routing,1,1);
        if(dest==0 || dest==1)EXPECT_EQ(result.secondPitch,640);
        if(dest==2)EXPECT_EQ(result.filterControl,1024);
        if(dest==4)EXPECT_EQ(result.pulseControl,128);
        EXPECT_EQ(result.fmControl,dest==5 ? 512 : 0);
    }
}
TEST(ModulationInputs, SharedAndRemainingFmControlsReachOnePhysicalDepthConversion)
{
    using namespace synth2::dsp;
    OscillatorControlSettings config;config.envelopeEnabled=config.envelopeFm=config.fmLfo=true;config.fmAmount=.5f;
    VoiceControlBuffer envelope{},shared{},additional{},depth{};
    envelope.fill(128);shared.fill(32);additional.fill(64);
    VoiceSpanBuffer spans{};for(int i=0;i<256;++i)spans[i]=256-i;
    ReceiverLfoView view;view.available=true;view.receivers[0]={true,5};view.values[0]=shared.data();view.remaining[0]=spans.data();
    FrequencyModulation state;
    OscillatorControlRenderer::fm(config,view,state,256,envelope,spans,depth,nullptr,nullptr,&additional);
    const double expected=FrequencyModulation::depth(63.5,224.0);
    for(int i=0;i<256;++i){EXPECT_EQ(depth[i],expected);EXPECT_EQ(shared[i],32);EXPECT_EQ(additional[i],64);}
}
TEST(ModulationInputs, AdditionalControlChangesKeepTheirOwnBoundaries)
{
    using namespace synth2::dsp;
    VoiceControlBuffer control{};control.fill(4);control[3]=8;
    EXPECT_EQ(constantControlSpan(control,0,256),3);EXPECT_EQ(constantControlSpan(control,3,253),1);
    EXPECT_EQ(constantControlSpan(control,4,252),252);EXPECT_EQ(constantControlSpan(control,255,1),1);
    FilterControlSettings config{true,false,256};FilterEnvelope envelope;
    envelope.configure(0,0,0,0,0);envelope.noteOn();CutoffControl cutoff;cutoff.prepare(44100);cutoff.settle();
    VoiceControlBuffer modulation{},indices{};modulation.fill(512);for(int i=0;i<3;++i)modulation[i]=0;
    FilterControlRenderer::render(config,{},envelope,cutoff,0,0,256,indices,&modulation);
    for(int i=0;i<256;++i)EXPECT_EQ(indices[i],i<3 ? 256 : 640);
}

TEST(ModulationInputs, MissingPreparedSignalsDoNotCreateUnipolarModulation)
{
    synth2::RenderControls p;p.lfo1.on=p.lfo2.on=true;p.lfo1.amount=p.lfo2.amount=1;
    p.lfo1.dest=2;p.lfo2.dest=5;
    const auto routing=synth2::render::modulationRouting(p,false,false);
    const auto result=synth2::dsp::ModulationRouter::calculate(routing,0,0);
    EXPECT_EQ(result.filterControl,0);EXPECT_EQ(result.fmControl,0);
}
#include "dsp/AmplitudeBlockRenderer.h"
TEST(AmplitudeInputs, SharedAndRemainingSlotsUseOneVcaWithTheOriginalAlignment)
{
    using namespace synth2::dsp;
    for(int alignment=0;alignment<4;++alignment)for(bool firstShared:{false,true})
    {
        AmplitudeEnvelope actual(true),reference(true);
        for(auto* state:{&actual,&reference}){state->setParameters(0,0,2048,64);state->noteOn();}
        VoiceControlBuffer shared{};shared.fill(128.25);VoiceSpanBuffer spans{};
        for(int i=0;i<256;++i)spans[i]=256-i;
        ReceiverLfoView view;view.available=true;const size_t slot=firstShared ? 0 : 1;
        view.receivers[slot]={true,3};view.values[slot]=shared.data();view.remaining[slot]=spans.data();
        AmplitudeControls additional{};additional[1-slot].fill(512.125);
        alignas(16) std::array<float,68> got{},expected{};got.fill(-9);expected.fill(-9);
        AmplitudeBlockRenderer::render({alignment},view,actual,64,got.data(),&additional);
        reference.setModulation(firstShared ? 128.25 : 512.125,firstShared ? 512.125 : 128.25);
        reference.render(expected.data()+alignment,64);
        EXPECT_EQ(got,expected);EXPECT_EQ(actual.getLevel(),reference.getLevel());
        for(double control:shared)EXPECT_EQ(control,128.25);
    }
}
TEST(AmplitudeInputs, AdditionalChangesRetainVcaPartitionsAndReleaseState)
{
    using namespace synth2::dsp;
    AmplitudeEnvelope actual(true),reference(true);
    for(auto* state:{&actual,&reference}){state->setParameters(0,0,2048,64);state->noteOn();}
    AmplitudeControls controls{};controls[0].fill(1024);
    for(int i=0;i<8;++i)controls[0][i]=i<4 ? 0 : 512;
    alignas(16) std::array<float,16> got{},expected{};
    AmplitudeBlockRenderer::render({}, {},actual,16,got.data(),&controls);
    reference.setModulation(0,0);reference.render(expected.data(),4);
    reference.setModulation(512,0);reference.render(expected.data()+4,4);
    reference.setModulation(1024,0);reference.render(expected.data()+8,8);
    EXPECT_EQ(got,expected);
    actual.noteOff();reference.noteOff();
    AmplitudeBlockRenderer::render({}, {},actual,16,got.data());
    reference.setModulation(0,0);reference.render(expected.data(),16);
    EXPECT_EQ(got,expected);EXPECT_EQ(actual.getPhase(),reference.getPhase());
    EXPECT_EQ(actual.getCounter(),reference.getCounter());EXPECT_EQ(actual.getLevel(),reference.getLevel());
}
TEST(AmplitudeInputs, NormalizedSourcesReachRealAttenuationKnotsWithoutCrossingSlots)
{
    synth2::dsp::ModulationRoutingSettings routing;routing.lfo1.on=true;routing.lfo1.dest=3;routing.lfo1.amount=1;
    for(const auto pair:std::array<std::pair<float,double>,3>{{{-1,2048},{0,1024},{1,0}}})
    {
        const auto controls=synth2::dsp::ModulationRouter::calculate(routing,pair.first,0);
        EXPECT_EQ(controls.amplitudeControl[0],pair.second);EXPECT_EQ(controls.amplitudeControl[1],0);
    }
    routing.lfo1.amount=.5f;
    EXPECT_EQ(synth2::dsp::ModulationRouter::calculate(routing,-.5f,0).amplitudeControl[0],1023.5);
    routing.lfo1.on=false;
    EXPECT_EQ(synth2::dsp::ModulationRouter::calculate(routing,-.5f,0).amplitudeControl,(std::array<double,2>{0,0}));
}
#include "render/PanConfiguration.h"
#include "render/OutputConfiguration.h"
TEST(PanInputs, ProjectionKeepsCompatibleRealEndpointsAndDisabledSourcesNeutral)
{
    synth2::LfoSettings p;p.on=true;p.dest=6;p.amount=1;
    EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,-1),-64);
    EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,0),0);
    EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,1),64);
    p.amount=.5f;
    EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,-.5f),-31.5);
    EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,.5f),31.5);
    p.on=false;EXPECT_EQ(synth2::dsp::LfoControlProjection::fromNormalized(p,.5f),0);
}
TEST(PanInputs, SharedAndProjectedSlotsAreConsumedOnceAtTheOutputOffset)
{
    for(bool firstShared:{false,true})
    {
        synth2::RenderControls p;p.nativePanLfo=true;p.pan=.5f;
        p.lfo1.on=p.lfo2.on=true;p.lfo1.dest=p.lfo2.dest=6;
        p.lfo1.nativeSharedClock=firstShared;p.lfo2.nativeSharedClock=!firstShared;
        synth2::dsp::LfoBlock block;block.startSample=100;
        block.amplitudeControl[0].fill(7);block.amplitudeControl[1].fill(11);
        block.projectedPan[0].fill(19);block.projectedPan[1].fill(23);
        const auto controls=synth2::render::outputPanControls(p,&block,103);
        ASSERT_TRUE(controls.available);EXPECT_TRUE(controls.receivers[0].enabled);EXPECT_TRUE(controls.receivers[1].enabled);
        EXPECT_EQ(controls.values[0][0],firstShared ? 7 : 19);EXPECT_EQ(controls.values[1][0],firstShared ? 23 : 11);
        EXPECT_EQ(controls.values[0],firstShared ? block.amplitudeControl[0].data()+3 : block.projectedPan[0].data()+3);
        EXPECT_EQ(synth2::render::modulationRouting(p).consumedLfos,(std::array<bool,2>{true,true}));
        EXPECT_TRUE(synth2::render::outputSettings(p).panLfo);
        p.nativePanLfo=false;
        const auto projected=synth2::render::outputPanControls(p,&block,103);
        EXPECT_EQ(projected.values[0][0],19);EXPECT_EQ(projected.values[1][0],23);
        EXPECT_FALSE(synth2::render::outputPanControls(p,nullptr,103).available);
        p.lfo1.on=p.lfo2.on=false;
        EXPECT_FALSE(synth2::render::outputPanLfo(p));EXPECT_EQ(synth2::render::outputSettings(p).pan,.5f);
    }
}
TEST(PanInputs, DepthProjectionRetainsEachGeneratedMidiSubregion)
{
    using namespace synth2::dsp;
    SharedModulationSettings settings;
    auto& p=settings.lfos[0];p.on=true;p.dest=6;p.wave=3;p.speed=1;p.amount=1;
    SharedModulationRuntime runtime;runtime.prepareSources(44100,64,settings);runtime.synchronize(settings,44100);
    runtime.beginChunk(100);runtime.renderSpan(100,7,settings);
    p.amount=.5f;runtime.renderSpan(107,5,settings);
    p.on=false;runtime.renderSpan(112,3,settings);
    for(int frame=0;frame<15;++frame)
        EXPECT_EQ(runtime.block().projectedPan[0][frame],frame<7 ? 64 : frame<12 ? 31.5 : 0);
    const auto counter=runtime.clock(0).counter;const auto random=runtime.randomState().random.state;
    synth2::RenderControls controls;controls.lfo1=settings.lfos[0];controls.lfo1.on=true;
    for(int i=0;i<64;++i)EXPECT_EQ(synth2::render::outputPanControls(controls,&runtime.block(),107).values[0][0],31.5);
    EXPECT_EQ(runtime.clock(0).counter,counter);EXPECT_EQ(runtime.randomState().random.state,random);
}

#include "SynthEngine.h"
namespace
{
    std::vector<float> renderFragmentedTone(int hostFrames,bool sharedClock,int wave,bool arp,int preparedFrames=64,bool amplitudeLfo=true)
    {
        synth2::SynthEngine engine;auto& p=engine.getParams();
        p.osc1WaveIndex=1;p.oscMix=0;p.unisonVoices=1;
        p.ampAttackSec=0;p.ampDecaySec=.001f;p.ampSustain=1;p.ampReleaseSec=.001f;
        p.filterEnvAmount=0;p.filterTracking=0;
        p.lfo1.on=amplitudeLfo;p.lfo1.nativeSharedClock=sharedClock;
        p.lfo1.dest=3;p.lfo1.wave=wave;p.lfo1.amount=.7f;p.lfo1.speed=1;
        p.arpeggiator.enabled=arp;p.arpeggiator.type=2;
        p.arpeggiator.range=1;p.arpeggiator.beat=14;p.arpeggiator.gate=64;
        engine.prepareToPlay(48000,preparedFrames);
        std::vector<float> result;
        for(int position=0;position<9000;position+=hostFrames)
        {
            const int count=std::min(hostFrames,9000-position);
            synth2::audio::AudioBuffer<float> audio(2,count+5);audio.clear();
            synth2::audio::MidiBuffer midi;
            if(position<=119 && 119<position+count)
                midi.addEvent(synth2::audio::MidiMessage::noteOn(1,62,uint8_t{91}),5+119-position);
            if(position<=7601 && 7601<position+count)
                midi.addEvent(synth2::audio::MidiMessage::noteOff(1,62),5+7601-position);
            engine.render(audio,midi,5,count);
            for(int i=0;i<count;++i)for(int channel=0;channel<2;++channel)
                result.push_back(audio.getSample(channel,i+5));
            EXPECT_EQ(audio.getMagnitude(0,5),0);
        }
        return result;
    }
}


TEST(ModulationBehavior, ProjectedAmplitudeAndArpGainRampsSurviveHostFragments)
{
    constexpr int prepared=64;
    for(const auto mode:std::array<std::array<int,3>,2>{{{0,4,0},{0,0,1}}})
    {
        const auto expected=renderFragmentedTone(64,mode[0],mode[1],mode[2],prepared);
        for(int block:{1,17,127,1000})
        {
            const auto actual=renderFragmentedTone(block,mode[0],mode[1],mode[2],prepared);
            ASSERT_EQ(actual.size(),expected.size());
            float peak=0,difference=0;
            for(size_t i=0;i<expected.size();++i)
            {peak=std::max(peak,std::abs(expected[i]));difference=std::max(difference,std::abs(expected[i]-actual[i]));}
            EXPECT_GT(peak,.001f);
            EXPECT_LT(difference,std::max(2e-6f,peak*1e-4f))<<mode[0]<<'/'<<mode[1]<<'/'<<mode[2]<<" block="<<block<<" prepared="<<prepared;
        }
    }
}
TEST(ModulationBehavior, SilentTailFragmentsDoNotChangeRetainedPhysicalOscillatorPhases)
{
    synth2::SynthEngine first,second;
    for(auto* engine:{&first,&second})
    {
        auto& p=engine->getParams();p.osc1WaveIndex=1;p.oscMix=0;
        p.unisonVoices=2;p.polyphony=8;p.ampAttackSec=0;p.ampDecaySec=0;
        p.ampSustain=1;p.ampReleaseSec=.01f;p.filterEnvAmount=0;p.filterTracking=0;
        engine->prepareToPlay(44100,64);
    }
    const auto play=[](synth2::SynthEngine& engine,int total)
    {
        for(int position=0;position<total;position+=64)
        {
            const int count=std::min(64,total-position);
            synth2::audio::AudioBuffer<float> audio(2,count);audio.clear();
            synth2::audio::MidiBuffer midi;
            if(position==0)midi.addEvent(synth2::audio::MidiMessage::noteOn(1,65,uint8_t{93}),0);
            if(position==256)midi.addEvent(synth2::audio::MidiMessage::noteOff(1,65),0);
            engine.render(audio,midi,0,count);
        }
    };
    for(int repeat=0;repeat<19;++repeat)
    {
        play(first,1536);play(second,1547);
        for(size_t core=0;core<8;++core)
        {
            std::array<uint32_t,9> a{},b{};
            ASSERT_TRUE(first.readCoreWords(core,a));ASSERT_TRUE(second.readCoreWords(core,b));
            for(size_t word=0;word<4;++word)EXPECT_EQ(a[word],b[word])<<repeat<<'/'<<core<<'/'<<word;
        }
    }
}

TEST(AmplitudeInputs, GainHorizonRetainsProgressAndCanBeInterruptedByANewTarget)
{
    const auto render=[](int block)
    {
        synth2::dsp::AmplitudeEnvelope state(true);
        state.setParameters(0,0,2048,64);state.setGainScale(.35f);state.noteOn();
        std::vector<float> gains(512);
        const std::array<int,5> edges{{0,113,197,259,512}};
        for(size_t section=0;section+1<edges.size();++section)
        {
            state.setModulation(section==0 ? 384 : section==1 ? 1024 : 0,section==2 ? 512.5 : 0);
            for(int position=edges[section];position<edges[section+1];)
            {
                const int count=std::min(block,edges[section+1]-position);
                state.render(gains.data()+position,count,64);position+=count;
            }
        }
        EXPECT_GT(gains[31],gains[0]);
        EXPECT_NEAR(gains[63],.35f*(2048-384)/2048,1e-6f);
        EXPECT_NEAR(gains.back(),.35f,1e-6f);
        return gains;
    };
    const auto expected=render(64);
    for(int block:{1,3,17,127})
    {
        const auto actual=render(block);float difference=0;
        for(size_t i=0;i<actual.size();++i)difference=std::max(difference,std::abs(actual[i]-expected[i]));
        EXPECT_LT(difference,2e-6f)<<block;
    }
}

TEST(AmplitudeInputs, CompleteVoiceDispatchDoesNotBorrowAProjectedSignalHorizon)
{
    for(int mode:{0,1,2})
    {
        synth2::SynthEngine engine;auto& p=engine.getParams();
        p.unisonVoices=1;p.ampAttackSec=0;p.ampDecaySec=0;p.ampSustain=1;
        p.lfo1.on=mode!=0;p.lfo1.dest=mode==1 ? 0 : 3;
        p.lfo1.nativeSharedClock=mode==2;p.lfo1.amount=mode==2 ? .8f : 0;
        engine.prepareToPlay(44100,64);
        synth2::audio::AudioBuffer<float> audio(2,1);audio.clear();
        synth2::audio::MidiBuffer midi;
        midi.addEvent(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),0);
        engine.render(audio,midi,0,1);
        std::array<uint32_t,22> words{};ASSERT_TRUE(engine.readCoreFloatWords(0,words));
        float current{},target{};std::memcpy(&current,&words[0],sizeof(current));
        std::memcpy(&target,&words[1],sizeof(target));
        EXPECT_GT(target,0);EXPECT_FLOAT_EQ(current,target)<<mode;
    }
}

TEST(ModulationBehavior, ArpGainRampsSurviveHostFragmentsWithoutAmplitudeModulation)
{
    const auto expected=renderFragmentedTone(64,false,0,true,64,false);
    float peak=0;for(float value:expected)peak=std::max(peak,std::abs(value));
    for(int block:{1,17,127,1000})
    {
        const auto actual=renderFragmentedTone(block,false,0,true,64,false);
        float difference=0;
        for(size_t i=0;i<expected.size();++i)
            difference=std::max(difference,std::abs(expected[i]-actual[i]));
        EXPECT_LT(difference,std::max(2e-6f,peak*1e-4f))<<block;
    }
}

TEST(AmplitudeInputs, RepeatedGainSpansStopAtTheDeclaredEventBoundary)
{
    synth2::dsp::AmplitudeEnvelope actual(true);
    actual.setParameters(0,0,2048,10000);actual.setGainScale(.4f);actual.noteOn();
    alignas(16) std::array<float,64> warm{};actual.render(warm.data(),64);actual.noteOff();
    auto expected=actual;
    alignas(16) std::array<float,100> got{},want{};
    int offset=0;
    for(int count:{10,64,26})
    {
        expected.render(want.data()+offset,count);
        offset+=count;
    }
    synth2::dsp::AmplitudeBlockRenderer::render({0,10,64,100},{},actual,100,got.data());
    for(size_t i=0;i<got.size();++i)EXPECT_FLOAT_EQ(got[i],want[i])<<i;
    EXPECT_EQ(actual.getPhase(),expected.getPhase());
    EXPECT_EQ(actual.getCounter(),expected.getCounter());
    EXPECT_EQ(actual.getLevel(),expected.getLevel());
}

TEST(ModulationInputs, RingRetainsPhysicalFmSlewAndResumesWhenDisabled)
{
    using namespace synth2::dsp;
    synth2::RenderControls controls;controls.ringMod=1;controls.osc1FmAmount=0;
    auto config=synth2::render::oscillatorControls(controls,44100,false);
    FrequencyModulation state;state.settle=false;state.current=state.target=4096.25;
    state.elapsed=44;
    VoiceControlBuffer envelope{},depth{};VoiceSpanBuffer spans{};
    for(int i=0;i<256;++i)spans[i]=256-i;
    OscillatorControlRenderer::fm(config,{},state,256,envelope,spans,depth);
    for(double value:depth)EXPECT_EQ(value,4096.25);
    EXPECT_EQ(state.target,0);EXPECT_EQ(state.current,4096.25);
    EXPECT_EQ(state.remaining,44);EXPECT_EQ(state.elapsed,44);
    // A different physical core must still advance while this core uses Ring.
    auto independent=state;
    controls.ringMod=0;config=synth2::render::oscillatorControls(controls,44100,false);
    OscillatorControlRenderer::fm(config,{},independent,256,envelope,spans,depth);
    EXPECT_EQ(independent.current,0);EXPECT_EQ(independent.remaining,0);
    EXPECT_EQ(state.current,4096.25);EXPECT_EQ(state.remaining,44);
    OscillatorControlRenderer::fm(config,{},state,256,envelope,spans,depth);
    EXPECT_EQ(depth.front(),4096.25);EXPECT_EQ(depth.back(),0);
    EXPECT_EQ(state.current,0);EXPECT_EQ(state.remaining,0);
}

TEST(ModulationInputs, GlidePublishesNestedFmDispatchSpansWithoutChangingPitch)
{
    using namespace synth2::dsp;
    OscillatorControlSettings config;config.portamentoDial=40;config.applyPortamento=true;
    Portamento state;state.setDuration(Portamento::timeSeconds(40),44100);
    state.note=60;state.setNote(72,true);
    auto reference=state;
    VoiceSpanBuffer spans{};VoiceControlBuffer pitch{},expected{};
    // Two upstream regions: glide must retain their boundary while refining it.
    for(int i=0;i<64;++i)spans[i]=(i<45 ? 45 : 64)-i;
    std::vector<int> ends;int position=0;
    for(int frames:{45,19})reference.render(frames,[&](double value,int count){
        std::fill_n(expected.data()+position,count,value);position+=count;ends.push_back(position);
    });
    OscillatorControlRenderer::glide(config,{},state,64,spans,pitch);
    int begin=0;for(int end:ends){for(int i=begin;i<end;++i){EXPECT_EQ(spans[i],end-i);EXPECT_EQ(pitch[i],expected[i]);}begin=end;}
    ASSERT_GT(ends.size(),2u);EXPECT_EQ(state.elapsed,reference.elapsed);
    // A pending FM transition must see the first glide request horizon, not 64.
    config.fmAmount=0;FrequencyModulation fm;fm.settle=false;fm.current=fm.target=1024.5;
    fm.elapsed=44;VoiceControlBuffer envelope{},actual{},manual{};
    auto expectedFm=fm;position=0;begin=0;
    for(int end:ends){expectedFm.render(0,end-begin,44100,[&](double value){manual[position++]=value;});begin=end;}
    OscillatorControlRenderer::fm(config,{},fm,64,envelope,spans,actual);
    for(int i=0;i<64;++i)EXPECT_EQ(actual[i],manual[i]);
    EXPECT_EQ(fm.elapsed,expectedFm.elapsed);EXPECT_EQ(fm.remaining,expectedFm.remaining);
}
