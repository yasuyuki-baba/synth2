#include <gtest/gtest.h>
#include "../tools/host_scenarios.h"
#include "support/StateDocument.h"
#include <array>
#include <future>
#include <barrier>
#include <limits>
#include <vector>

namespace
{
using synth2::host::ProcessorBridge;
using Stereo=std::array<std::array<float,64>,2>;
Stereo block(ProcessorBridge& p,int position)
{
    if(position%9==0) EXPECT_TRUE(p.enqueueMidi(13,0x90,60+position%12,100));
    if(position%9==5) EXPECT_TRUE(p.enqueueMidi(29,0x80,60+(position-5)%12,0));
    if(position==7) EXPECT_TRUE(p.enqueueMidi(31,0xb0,1,100));
    if(position==19) EXPECT_TRUE(p.enqueueMidi(31,0xe0,0,96));
    Stereo output{};
    EXPECT_TRUE(p.process(output[0].data(),output[1].data(),64,position<20 ? 120 : 137));
    for(const auto& channel:output) for(float value:channel) EXPECT_TRUE(std::isfinite(value));
    return output;
}
void setup(ProcessorBridge& p,const std::string& text,double rate=44100)
{
    std::string error;
    ASSERT_TRUE(p.loadPatch(text,error))<<error;
    p.prepare(rate,64);
}
}

TEST(PlaybackBehavior, EveryHostParameterCanBeEditedDuringPlaybackAndRecalled)
{
    ProcessorBridge processor;
    setup(processor,hostvalidation::patch());
    ASSERT_TRUE(processor.enqueueMidi(0,0x90,60,100));
    for(int parameter=0;parameter<ProcessorBridge::parameterCount;++parameter)
    {
        SCOPED_TRACE(parameter);
        const auto before=processor.normalizedValue(parameter);
        for(double value:{0.,.5,1.})
        {
            processor.setNormalizedValue(parameter,value);
            block(processor,1);
            const auto saved=processor.saveState();
            ProcessorBridge restored;
            restored.restoreState(saved.data(),static_cast<int>(saved.size()));
            EXPECT_EQ(restored.saveState(),saved);
            EXPECT_NEAR(restored.normalizedValue(parameter),value,1e-6);
        }
        processor.setNormalizedValue(parameter,before);
    }
}

TEST(PlaybackBehavior, NonFiniteHostEditsAreNoOpsAndFiniteEditsClampToHostBounds)
{
    ProcessorBridge processor;
    for(int parameter=0;parameter<ProcessorBridge::parameterCount;++parameter)
    {
        SCOPED_TRACE(parameter);
        const auto before=processor.saveState();
        for(double value:{std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()})
        { processor.setNormalizedValue(parameter,value); EXPECT_EQ(processor.saveState(),before); }
        processor.setNormalizedValue(parameter,-1); EXPECT_NEAR(processor.normalizedValue(parameter),0,1e-6);
        processor.setNormalizedValue(parameter,2); EXPECT_NEAR(processor.normalizedValue(parameter),1,1e-6);
    }
}

TEST(PlaybackBehavior, PulseWidthPreservesReciprocalMultiplyForIntegerAndFractionalControls)
{
    ProcessorBridge continuous,legacy;
    setup(continuous,hostvalidation::patch());
    auto state=stateinspection::document(continuous);
    state->RootElement()->SetAttribute("stateVersion","2");
    const auto bytes=stateinspection::bytes(*state);
    legacy.restoreState(bytes.data(),static_cast<int>(bytes.size()));legacy.prepare(44100,64);
    for(int dial=0;dial<128;++dial)for(auto* processor:{&continuous,&legacy})
    {
        SCOPED_TRACE(dial);
        processor->setNormalizedValue(8,static_cast<double>(dial)/127.0);
        block(*processor,1);
        synth2::RenderControls mapped;
        ASSERT_TRUE(synth2::readMappedSynthParams(*processor,mapped));
        EXPECT_EQ(mapped.pulseWidth,static_cast<float>(dial)*(1.0f/127.0f));
    }
    for(double dial:{.25,17.5,72.25,126.75})
    {
        SCOPED_TRACE(dial);
        continuous.setNormalizedValue(8,dial/127.0);block(continuous,1);
        synth2::RenderControls mapped;
        ASSERT_TRUE(synth2::readMappedSynthParams(continuous,mapped));
        EXPECT_NEAR(mapped.pulseWidth,static_cast<float>(dial)*(1.0f/127.0f),1e-7f);
    }
}

TEST(PlaybackBehavior, ContinuousSubLevelPreservesLegacyIntegerAudio)
{
    for(int version:{104,110,111,112,113})
    {
        SCOPED_TRACE(version);
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        text.replace(text.find("95,0\n"),5,"95,32\n");
        ProcessorBridge continuous,legacy;
        std::string error;
        ASSERT_TRUE(continuous.loadPatch(text,error))<<error;
        auto state=stateinspection::document(continuous);
        state->RootElement()->SetAttribute("stateVersion","2");
        const auto bytes=stateinspection::bytes(*state);
        legacy.restoreState(bytes.data(),static_cast<int>(bytes.size()));
        ASSERT_EQ(stateinspection::attribute(*stateinspection::document(legacy),"stateVersion"),"2");
        continuous.prepare(44100,64);legacy.prepare(44100,64);
        double energy=0;
        for(int frame=0;frame<32;++frame)
        {
            const auto expected=block(legacy,frame),actual=block(continuous,frame);
            EXPECT_EQ(actual,expected)<<frame;
            for(const auto& channel:actual)for(float value:channel)energy+=double(value)*value;
        }
        EXPECT_GT(energy,0);
    }
}

TEST(PlaybackBehavior, FractionalSubLevelInterpolatesAndSurvivesRecall)
{
    ProcessorBridge processor,restored;
    setup(processor,hostvalidation::patch());
    auto gain=[&](double dial) {
        processor.setNormalizedValue(95,dial/127.);
        block(processor,1);
        synth2::RenderControls mapped;
        EXPECT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        return mapped.subOscGain;
    };
    for(const auto& interval:std::array<std::array<double,2>,2>{{{0,.25},{32,.5}}})
    {
        const auto [start,fraction]=interval;
        const float low=gain(start),high=gain(start+1),middle=gain(start+fraction);
        EXPECT_GT(middle,low);EXPECT_LT(middle,high);
        EXPECT_NEAR(middle,low+(high-low)*fraction,1e-7f);
    }
    const auto saved=processor.saveState();
    restored.restoreState(saved.data(),static_cast<int>(saved.size()));
    EXPECT_EQ(restored.saveState(),saved);
    EXPECT_NEAR(stateinspection::raw(restored,95),32.5f,1e-5f);
    restored.prepare(44100,64);
    // Compare two cold starts after recall, without retaining running histories.
    ProcessorBridge fresh;
    fresh.restoreState(saved.data(),static_cast<int>(saved.size()));fresh.prepare(44100,64);
    EXPECT_EQ(block(fresh,0),block(restored,0));
}

TEST(PlaybackBehavior, LiveProgramLoadsRecallAndInvalidTransactionsPreservePlayableState)
{
    ProcessorBridge processor;
    setup(processor,hostvalidation::patch());
    const auto recalled=processor.saveState();
    for(int i=0;i<60;++i)
    {
        SCOPED_TRACE(i);
        if(i%3==0)
        {
            auto text=hostvalidation::patch(1+i%4,i%3);
            text.replace(text.find("ver=113"),7,i%2 ? "ver=101" : "ver=112");
            std::string error;
            ASSERT_TRUE(processor.loadProgram(text,i,error))<<error;
        }
        if(i%3==1) processor.restoreState(recalled.data(),static_cast<int>(recalled.size()));
        const auto before=processor.saveState();
        std::string error;
        EXPECT_FALSE(processor.loadProgram("Bad\nver=113\n29,bad\n",2,error));
        EXPECT_EQ(processor.saveState(),before);
        block(processor,i);
    }
    // Return to an explicitly dry, short-release contract before checking stop.
    std::string error;
    ASSERT_TRUE(processor.loadPatch(hostvalidation::patch(),error));
    ASSERT_TRUE(processor.enqueueMidi(0,0xb0,123,0));
    for(int i=0;i<1500;++i) { Stereo audio{}; ASSERT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120)); }
    EXPECT_EQ(hostvalidation::activeCores(processor),0);
    EXPECT_GT([](const Stereo& x){double e=0;for(const auto& c:x)for(float v:c)e+=double(v)*v;return e;}(block(processor,0)),0);
}

TEST(InstanceIsolation, InterleavingNoiseRandomLfoAndArpDoesNotChangeEitherInstance)
{
    const auto aText=hostvalidation::patch()+"1,4\n41,4\n42,4\n44,71\n57,1\n59,1\n31,4\n33,14\n";
    const auto bText=hostvalidation::patch(3,1)+"41,3\n42,5\n44,90\n57,1\n";
    std::array<std::vector<Stereo>,2> expected;
    for(int instance=0;instance<2;++instance)
    {
        ProcessorBridge p;
        setup(p,instance==0 ? aText : bText,instance==0 ? 44100 : 48000);
        for(int i=0;i<96;++i) expected[instance].push_back(block(p,i));
    }
    ProcessorBridge a,b;
    setup(a,aText); setup(b,bText,48000);
    for(int i=0;i<96;++i)
    {
        SCOPED_TRACE(i);
        if(i%7==0) { ProcessorBridge unrelated; setup(unrelated,hostvalidation::patch()); block(unrelated,0); }
        if(i%2==0) { EXPECT_EQ(block(a,i),expected[0][i]); EXPECT_EQ(block(b,i),expected[1][i]); }
        else { EXPECT_EQ(block(b,i),expected[1][i]); EXPECT_EQ(block(a,i),expected[0][i]); }
    }
}

TEST(InstanceIsolation, EditingLoadingAndReleasingOneInstanceLeavesAnotherUntouched)
{
    ProcessorBridge reference,untouched,edited;
    const auto text=hostvalidation::patch()+"1,4\n42,5\n44,90\n57,1\n";
    setup(reference,text); setup(untouched,text); setup(edited,text);
    const auto state=untouched.saveState();
    for(int i=0;i<64;++i)
    {
        SCOPED_TRACE(i);
        edited.setNormalizedValue(i%99,(i%3)*.5);
        block(edited,i);
        if(i%8==0)
        {
            std::string error;
            ASSERT_TRUE(edited.loadPatch(hostvalidation::patch(2,1),error));
            edited.release(); edited.prepare(96000,64);
        }
        EXPECT_EQ(block(reference,i),block(untouched,i));
        EXPECT_EQ(untouched.saveState(),state);
    }
}

TEST(InstanceIsolation, SeparateInstancesRenderIndependentlyOnParallelThreads)
{
    const auto text=hostvalidation::patch()+"1,4\n42,5\n44,90\n57,1\n";
    std::vector<Stereo> expected;
    { ProcessorBridge p; setup(p,text); for(int i=0;i<48;++i) expected.push_back(block(p,i)); }
    std::barrier start(4);
    std::vector<std::future<std::vector<Stereo>>> workers;
    for(int instance=0;instance<4;++instance)
        workers.push_back(std::async(std::launch::async,[&] {
            ProcessorBridge p; setup(p,text); start.arrive_and_wait();
            std::vector<Stereo> actual;
            for(int i=0;i<48;++i) actual.push_back(block(p,i));
            return actual;
        }));
    for(auto& worker:workers) EXPECT_EQ(worker.get(),expected);
}

TEST(PlaybackBehavior, FractionalHostModeSurvivesSourceVersionChangesAndRecall)
{
    ProcessorBridge processor;
    std::string error;
    processor.prepare(44100,64);
    for(int version:{105,113,107})
    {
        SCOPED_TRACE(version);
        ASSERT_TRUE(processor.loadPatch("Fractional host controls\nver="+std::to_string(version)+"\n19,37\n",error))<<error;
        processor.setNormalizedValue(19,37.25/127.0);block(processor,1);
        synth2::RenderControls mapped;
        ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        EXPECT_NEAR(mapped.cutoffIndex,(37.25f/127.0f)*1023.0f,1e-4f);
        const auto state=processor.saveState();
        EXPECT_EQ(stateinspection::attribute(*stateinspection::document(state),"stateVersion"),"3");
        ProcessorBridge restored;restored.restoreState(state.data(),static_cast<int>(state.size()));
        restored.prepare(44100,64);block(restored,1);
        synth2::RenderControls recalled;
        ASSERT_TRUE(synth2::readMappedSynthParams(restored,recalled));
        EXPECT_EQ(recalled.cutoffIndex,mapped.cutoffIndex);
        EXPECT_EQ(restored.saveState(),state);
    }
}

TEST(PlaybackBehavior, FractionalEffectPolicyFollowsStateModeAndKeepsNearNeutralEqActive)
{
    ProcessorBridge processor;
    setup(processor,hostvalidation::patch());
    for(int index:{35,36,37,52,53,54,55,56,61,63})
        processor.setNormalizedValue(index,64.125/127);
    processor.setNormalizedValue(60,64.0/127);
    processor.setNormalizedValue(62,64.125/127);
    block(processor,1);
    synth2::RenderControls mapped;
    ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
    EXPECT_NEAR(mapped.eq.level*127,64.125,1e-4);
    const auto expectDials=[&](float expected) {
        for(float value:{mapped.delay.time,mapped.delay.feedback,mapped.delay.level,
                         mapped.chorus.time,mapped.chorus.depth,mapped.chorus.rate,
                         mapped.chorus.feedback,mapped.chorus.level,mapped.eq.freq,
                         mapped.eq.level,mapped.eq.q})EXPECT_NEAR(value*127,expected,1e-4);
    };
    expectDials(64.125f);
    const auto continuous=processor.saveState();
    for(int version:{1,2})
    {
        auto old=stateinspection::document(continuous);
        old->RootElement()->SetAttribute("stateVersion",version);
        const auto legacy=stateinspection::bytes(*old);
        processor.restoreState(legacy.data(),static_cast<int>(legacy.size()));
        block(processor,1);
        ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        expectDials(64.f);
        processor.restoreState(continuous.data(),static_cast<int>(continuous.size()));
        block(processor,1);
        ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        expectDials(64.125f);
    }
}

TEST(PlaybackBehavior, LegacyRecallResolvesEveryOutputDialBeforeDsp)
{
    for(float dial:{64.25f,64.75f})
    {
        ProcessorBridge processor;setup(processor,hostvalidation::patch());
        for(int index:{35,36,37,83,98,52,53,54,55,56,60,61,62,63})
            processor.setNormalizedValue(index,static_cast<double>(dial)/127);
        block(processor,1);
        const auto continuous=processor.saveState();
        const auto expectDials=[&](float expected) {
            synth2::RenderControls mapped;
            ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
            for(float value:{mapped.delay.time,mapped.delay.feedback,mapped.delay.level,
                             mapped.delay.spread,mapped.delay.tone,mapped.chorus.time,
                             mapped.chorus.depth,mapped.chorus.rate,mapped.chorus.feedback,
                             mapped.chorus.level,mapped.eq.tone,mapped.eq.freq,mapped.eq.level,mapped.eq.q})
                EXPECT_NEAR(value*127,expected,1e-4);
        };
        expectDials(dial);
        for(int version:{1,2})
        {
            auto old=stateinspection::document(continuous);
            old->RootElement()->SetAttribute("stateVersion",version);
            const auto legacy=stateinspection::bytes(*old);
            processor.restoreState(legacy.data(),static_cast<int>(legacy.size()));block(processor,1);
            expectDials(dial<64.5f ? 64.f : 65.f);
            processor.restoreState(continuous.data(),static_cast<int>(continuous.size()));block(processor,1);
            expectDials(dial);
        }
    }
}
