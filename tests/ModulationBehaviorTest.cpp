#include <gtest/gtest.h>
#include "dsp/SharedModulationRuntime.h"
#include "dsp/LfoShape.h"
#include "dsp/AmplitudeBlockRenderer.h"
#include "dsp/OscillatorControlRenderer.h"
#include "dsp/FilterControlRenderer.h"
#include "render/VoiceConfiguration.h"
#include "render/PanConfiguration.h"
#include "SynthEngine.h"
#include "../tools/host_scenarios.h"
#include <cmath>
#include <cstring>
#include <type_traits>
#include <vector>
namespace
{
synth2::dsp::SharedModulationSettings sharedSettings()
{
    synth2::dsp::SharedModulationSettings settings;
    for(auto& lfo:settings.lfos)
    { lfo.on=true;  lfo.keySync=true; lfo.amount=.5f; lfo.speed=.5f; }
    return settings;
}
void prepare(synth2::dsp::SharedModulationRuntime& runtime,const synth2::dsp::SharedModulationSettings& settings)
{ runtime.prepareBlock(64); runtime.synchronize(settings,48000); }
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
                const double x=first.block().controls[0][i],y=second.block().controls[0][i];
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
                const auto x=first.block().controls[0][i],y=second.block().controls[0][i];
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
                differs|=first.block().controls[slot][i]!=second.block().controls[slot][i];
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
        EXPECT_TRUE(std::isfinite(runtime.block().controls[0][i]));
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
            runtime->prepareBlock(64);
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

TEST(ModulationBehavior, EveryWaveformUsesBoundedPhysicalControlsAndReplaysItsHistory)
{
    using namespace synth2::dsp;
    for(int wave=0;wave<6;++wave)for(int dest=0;dest<7;++dest)
    {
        SCOPED_TRACE(::testing::Message()<<wave<<" "<<dest);
        auto settings=sharedSettings();settings.lfos[0].wave=wave;settings.lfos[0].dest=dest;
        settings.lfos[0].amount=47.f/127;settings.lfos[0].speed=1;
        LfoHistoryEvent load;load.kind=1;load.randomAdmitted=true;
        load.randomControls={{{dest+1,wave==4 ? 3 : wave==5 ? 4 : 0,47},{1,0,0}}};
        SharedModulationRuntime first,replay;
        for(auto* runtime:{&first,&replay}){runtime->applyHistory(load,120);prepare(*runtime,settings);}
        const auto range=lfoDepthRange(dest+1,47);bool changes=false;double previous=0;
        for(int block=0;block<128;++block)
        {
            for(auto* runtime:{&first,&replay})advance(*runtime,settings);
            EXPECT_EQ(first.block().controls,replay.block().controls);
            for(int i=0;i<64;++i)
            {
                const double value=first.block().controls[0][i];
                ASSERT_TRUE(std::isfinite(value));ASSERT_GE(value,std::min(range.first,range.second));ASSERT_LE(value,std::max(range.first,range.second));
                changes|=value!=previous;previous=value;
            }
        }
        EXPECT_TRUE(changes);EXPECT_EQ(first.randomState().random.state,replay.randomState().random.state);
    }
}
TEST(ModulationBehavior, UnknownHistoryKeepsTheSameClockAndCrtProcessing)
{
    using namespace synth2::dsp;
    auto settings=sharedSettings();settings.lfos[0].wave=4;
    LfoHistoryEvent load;load.kind=1;load.randomAdmitted=true;load.randomControls={{{1,3,64},{1,0,0}}};
    SharedModulationRuntime known,unknown;
    for(auto* runtime:{&known,&unknown}){runtime->applyHistory(load,120);prepare(*runtime,settings);}
    unknown.invalidateRandomHistory();
    for(int block=0;block<64;++block)
    {
        if(block==17)for(auto* runtime:{&known,&unknown})runtime->trackKey(synth2::audio::MidiMessage::noteOn(1,60,uint8_t{100}),settings);
        for(auto* runtime:{&known,&unknown})advance(*runtime,settings);
        EXPECT_EQ(known.block().controls,unknown.block().controls);
        EXPECT_EQ(known.randomState().random.state,unknown.randomState().random.state);
    }
    EXPECT_FALSE(unknown.randomState().valid);EXPECT_TRUE(known.randomState().valid);
}
TEST(PanInputs, BothPreparedSlotsUseTheirOwnDepthAtTheOutputOffset)
{
    synth2::RenderControls p;p.lfo1.on=p.lfo2.on=true;p.lfo1.dest=p.lfo2.dest=6;
    synth2::dsp::LfoBlock block;block.startSample=100;
    block.controls[0].fill(19);block.controls[1].fill(23);
    const auto view=synth2::render::outputPanControls(p,&block,103);
    EXPECT_EQ(view.values[0],block.controls[0].data()+3);
    EXPECT_EQ(view.values[1],block.controls[1].data()+3);
    EXPECT_TRUE(view.receivers[0].enabled);EXPECT_TRUE(view.receivers[1].enabled);
    EXPECT_FALSE(synth2::render::outputPanControls(p,nullptr,103).available);
}
namespace
{
    std::vector<float> renderFragmentedTone(int hostFrames,int wave,bool arp,int preparedFrames=64,bool amplitudeLfo=true)
    {
        synth2::SynthEngine engine;auto& p=engine.getParams();
        p.osc1WaveIndex=1;p.oscMix=0;p.unisonVoices=1;
        p.ampAttackSec=0;p.ampDecaySec=.001f;p.ampSustain=1;p.ampReleaseSec=.001f;
        p.filterEnvAmount=0;
        p.lfo1.on=amplitudeLfo;
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


TEST(ModulationBehavior, SilentTailFragmentsDoNotChangeRetainedPhysicalOscillatorPhases)
{
    synth2::SynthEngine first,second;
    for(auto* engine:{&first,&second})
    {
        auto& p=engine->getParams();p.osc1WaveIndex=1;p.oscMix=0;
        p.unisonVoices=2;p.polyphony=8;p.ampAttackSec=0;p.ampDecaySec=0;
        p.ampSustain=1;p.ampReleaseSec=.01f;p.filterEnvAmount=0;
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

TEST(AmplitudeInputs, CompleteVoiceDispatchUsesItsDeclaredGainHorizon)
{
    for(int mode:{0,1,2})
    {
        synth2::SynthEngine engine;auto& p=engine.getParams();
        p.unisonVoices=1;p.ampAttackSec=0;p.ampDecaySec=0;p.ampSustain=1;
        p.lfo1.on=mode!=0;p.lfo1.dest=mode==1 ? 0 : 3;
        p.lfo1.amount=mode==2 ? .8f : 0;
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
    const auto expected=renderFragmentedTone(64,0,true,64,false);
    float peak=0;for(float value:expected)peak=std::max(peak,std::abs(value));
    for(int block:{1,17,127,1000})
    {
        const auto actual=renderFragmentedTone(block,0,true,64,false);
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
