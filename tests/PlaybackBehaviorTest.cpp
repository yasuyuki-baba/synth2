#include "SynthEngine.h"
#include <cstring>
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

TEST(PlaybackBehavior, UnchangedCallbacksMatchExplicitRefreshThroughLiveTransitions)
{
    ProcessorBridge cached,refreshed;
    setup(cached,hostvalidation::patch(4));setup(refreshed,hostvalidation::patch(4));
    const auto saved=cached.saveState();
    for(int callback=0;callback<96;++callback)
    {
        SCOPED_TRACE(callback);
        for(auto* processor:{&cached,&refreshed})
        {
            if(callback==0)ASSERT_TRUE(processor->enqueueMidi(7,0x90,60,100));
            if(callback==8 || callback==9)
            {
                // Equal visible values still publish source/history edits.
                for(int index:{41,43,67,19})
                    processor->setNormalizedValue(index,processor->normalizedValue(index));
            }
            if(callback==12)processor->setNormalizedValue(128,.37);
            if(callback==16)processor->setNormalizedValue(126,104./127.);
            if(callback==20 || callback==21)
            {
                std::string error;
                ASSERT_TRUE(processor->loadPatch(hostvalidation::patch(8),error))<<error;
            }
            if(callback==28 || callback==29)processor->restoreState(saved.data(),static_cast<int>(saved.size()));
            if(callback==36)processor->setNormalizedValue(128,.71);
            if(callback==40)ASSERT_TRUE(processor->enqueueMidi(31,0xe0,0,96));
            if(callback==44)ASSERT_TRUE(processor->enqueueMidi(13,0xb0,1,127));
            if(callback==48)ASSERT_TRUE(processor->enqueueMidi(13,0xb0,121,0));
            if(callback==52)ASSERT_TRUE(processor->enqueueMidi(17,0xb0,2,99));
            if(callback==56)processor->prepare(48000.25,64);
            if(callback==64)ASSERT_TRUE(processor->enqueueMidi(7,0x90,64,95));
            if(callback==80)ASSERT_TRUE(processor->enqueueMidi(29,0x80,64,0));
        }
        // p127 has no clock notification. An explicit same-value publication
        // refreshes the source snapshot without changing saved controls/history.
        refreshed.setNormalizedValue(127,refreshed.normalizedValue(127));
        Stereo a{},b{};
        ASSERT_TRUE(cached.process(a[0].data(),a[1].data(),64,120));
        ASSERT_TRUE(refreshed.process(b[0].data(),b[1].data(),64,120));
        EXPECT_EQ(std::memcmp(a.data(),b.data(),sizeof(a)),0);
        EXPECT_EQ(cached.saveState(),refreshed.saveState());
        std::array<int,99> controlsA{},controlsB{};
        EXPECT_EQ(cached.readNativeControls(controlsA),refreshed.readNativeControls(controlsB));
        EXPECT_EQ(controlsA,controlsB);
    }
}

TEST(PlaybackBehavior, RunningHistoryInvalidationRefreshesAdmissionWithoutSourceEdits)
{
    ProcessorBridge processor;
    processor.prepare(44100,64);
    Stereo output{};
    ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
    ASSERT_TRUE(processor.engineForInspection().nativeRandomHistoryValid());
    ASSERT_TRUE(processor.engineForInspection().getRenderHistory().nativeRandomLfoHistoryValid);
    const auto state=processor.saveState();
    ASSERT_TRUE(processor.enqueueMidi(17,0xb0,2,99));
    ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
    EXPECT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
    EXPECT_FALSE(processor.engineForInspection().getRenderHistory().nativeRandomLfoHistoryValid);
    EXPECT_EQ(processor.saveState(),state);
}

TEST(PlaybackBehavior, FilterEnvelopeVelocityTrackingAndRandomLfoAffectNormalPlayback)
{
    const auto render=[](double rate,int unison,int mode,int feature)
    {
        auto text=hostvalidation::patch(unison);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(14,mode);set(15,16);set(16,48);set(17,32);set(18,0);set(19,45);
        set(21,feature==0 ? 63 : 79);
        if(feature==2)set(24,1);
        if(feature==3)set(22,127);
        if(feature==4){set(57,1);set(41,3);set(42,5);set(43,96);set(44,90);}
        ProcessorBridge processor;
        std::string error;
        EXPECT_TRUE(processor.loadPatch(text,error))<<error;
        processor.prepare(rate,64);
        const auto state=processor.saveState();
        const int blocks=static_cast<int>(rate*.15/64)+1;
        std::vector<float> samples;
        for(int block=0;block<blocks;++block)
        {
            if(block==0)EXPECT_TRUE(processor.enqueueMidi(7,0x90,72,20));
            if(block==blocks*3/4)EXPECT_TRUE(processor.enqueueMidi(13,0x80,72,0));
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
        }
        EXPECT_EQ(processor.saveState(),state);
        return samples;
    };
    for(double rate:{8000.,48000.25})for(int unison:{1,4})for(int mode:{0,1})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(unison);
        SCOPED_TRACE(mode);
        const auto reference=render(rate,unison,mode,1);
        for(int feature:{0,2,3,4})
        {
            SCOPED_TRACE(feature);
            const auto changed=render(rate,unison,mode,feature);
            ASSERT_EQ(changed.size(),reference.size());
            float difference=0;
            for(size_t i=0;i<reference.size();++i)difference=std::max(difference,std::abs(reference[i]-changed[i]));
            EXPECT_GT(difference,1e-6f);
        }
    }
}

TEST(PlaybackBehavior, OscillatorEnvelopeDestinationsKeepSignedDepthLfoAndUnison)
{
    const auto render=[](double rate,int unison,int destination,int depth,bool lfo)
    {
        auto text=hostvalidation::patch(unison);
        text+="71,"+std::to_string(destination)+"\n";
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,2);set(1,2);set(5,destination==0 ? 127 : 0);
        set(10,1);set(11,depth);set(12,32);set(13,64);
        if(destination==1)set(45,64); // Negative depth reduces an existing FM amount.
        if(lfo){set(57,1);set(41,destination==0 ? 2 : destination==1 ? 6 : 5);set(42,5);set(43,96);set(44,48);}
        ProcessorBridge processor;
        std::string error;
        EXPECT_TRUE(processor.loadPatch(text,error))<<error;
        processor.prepare(rate,64);
        const auto state=processor.saveState();
        const int blocks=static_cast<int>(rate*.12/64)+1;
        std::vector<float> samples;
        for(int block=0;block<blocks;++block)
        {
            if(block==0)EXPECT_TRUE(processor.enqueueMidi(7,0x90,69,100));
            if(block==blocks*3/4)EXPECT_TRUE(processor.enqueueMidi(13,0x80,69,0));
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
        }
        EXPECT_EQ(processor.saveState(),state);
        return samples;
    };
    const auto difference=[](const auto& a,const auto& b)
    {
        float maximum=0;
        for(size_t i=0;i<a.size();++i)maximum=std::max(maximum,std::abs(a[i]-b[i]));
        return maximum;
    };
    for(double rate:{8000.,48000.25})for(int unison:{1,4})for(int destination:{0,1,2})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(unison);
        SCOPED_TRACE(destination);
        const auto neutral=render(rate,unison,destination,64,false);
        const auto positive=render(rate,unison,destination,110,false);
        const auto negative=render(rate,unison,destination,20,false);
        EXPECT_GT(difference(neutral,positive),1e-6f);
        EXPECT_GT(difference(neutral,negative),1e-6f);
        EXPECT_GT(difference(positive,render(rate,unison,destination,110,true)),1e-6f);
    }
}

TEST(PlaybackBehavior, LiveFmAndCoarseEditsKeepSourceStateAcrossNoteReuse)
{
    const auto render=[](double rate,int version,int unison,int mode,bool edits)
    {
        auto text=hostvalidation::patch(unison,mode);
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,2);set(1,2);set(5,0);set(45,64);set(91,1);
        ProcessorBridge processor;
        setup(processor,text,rate);
        auto state=processor.saveState();
        std::vector<float> samples;
        for(int callback=0;callback<70;++callback)
        {
            if(callback==0)EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
            if(callback==12)EXPECT_TRUE(processor.enqueueMidi(19,0x90,64,90));
            if(callback==16)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
            if(edits && (callback==8 || callback==10 || callback==24 || callback==32))
            {
                const int index=callback==24 ? 2 : 45;
                const int dial=callback==24 ? 80 : callback==32 ? 0 : 100;
                processor.setNormalizedValue(index,dial/127.);
                EXPECT_NEAR(processor.normalizedValue(index),dial/127.,1e-6);
                state=processor.saveState();
            }
            if(callback==45)EXPECT_TRUE(processor.enqueueMidi(23,0xb0,123,0));
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(int unison:{1,4})for(int mode:{0,2})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(unison);
        SCOPED_TRACE(mode);
        const auto reference=render(rate,version,unison,mode,false);
        const auto changed=render(rate,version,unison,mode,true);
        float difference=0;
        for(size_t i=0;i<reference.size();++i)difference=std::max(difference,std::abs(reference[i]-changed[i]));
        EXPECT_GT(difference,1e-6f);
    }
}

TEST(PlaybackBehavior, OscillatorTwoKeyboardTrackingKeepsFixedBaseAndOctavePitch)
{
    const auto frequency=[](double rate,int version,bool tracking,int note)
    {
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,0);set(1,0);set(4,tracking);set(5,127);set(91,1);
        ProcessorBridge processor;
        setup(processor,text,rate);
        const auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,note,100));
        std::vector<int> crossings;
        float previous=0;
        for(int callback=0;callback<80;++callback)
        {
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(int i=0;i<64;++i)
            {
                EXPECT_TRUE(std::isfinite(audio[0][i]));
                EXPECT_TRUE(std::isfinite(audio[1][i]));
                if(callback>=20 && previous<0 && audio[0][i]>=0)crossings.push_back(callback*64+i);
                previous=audio[0][i];
            }
        }
        EXPECT_EQ(processor.saveState(),state);
        if(crossings.size()<3){ADD_FAILURE()<<"Insufficient oscillator crossings";return 0.;}
        return (crossings.size()-1)*rate/(crossings.back()-crossings.front());
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool tracking:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(tracking);
        const double lower=frequency(rate,version,tracking,69);
        const double upper=frequency(rate,version,tracking,81);
        EXPECT_NEAR(lower,tracking ? 440. : 220.,3.);
        EXPECT_NEAR(upper/lower,tracking ? 2. : 1.,.02);
    }
}

TEST(PlaybackBehavior, PortamentoRespondsToOverlapAutomaticModeAndLiveDurationEdits)
{
    const auto render=[](double rate,int version,int mode,int unison,bool automatic,int dial,bool edit)
    {
        auto text=hostvalidation::patch(unison,mode);
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,2);set(1,2);set(5,0);set(39,dial);set(74,automatic);set(91,1);
        ProcessorBridge processor;
        setup(processor,text,rate);
        auto state=processor.saveState();
        std::vector<float> samples;
        for(int callback=0;callback<90;++callback)
        {
            if(callback==0)EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
            if(callback==20)EXPECT_TRUE(processor.enqueueMidi(19,0x90,72,100));
            if(callback==24)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
            if(callback==40)EXPECT_TRUE(processor.enqueueMidi(23,0x80,72,0));
            if(callback==60)EXPECT_TRUE(processor.enqueueMidi(11,0x90,67,100));
            if(callback==80)EXPECT_TRUE(processor.enqueueMidi(17,0xb0,123,0));
            if(edit && (callback==28 || callback==30 || callback==48))
            {
                const int value=callback==48 ? 0 : 21;
                processor.setNormalizedValue(39,value/127.);
                EXPECT_NEAR(processor.normalizedValue(39),value/127.,1e-6);
                state=processor.saveState();
            }
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            if(callback==0)
            {
                synth2::RenderControls controls;
                EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.portamentoAuto,automatic);
                EXPECT_EQ(controls.sourcePortamentoDial,dial);
            }
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    const auto difference=[](const auto& a,const auto& b,int first,int last)
    {
        float maximum=0;
        for(int i=first*64;i<last*64;++i)maximum=std::max(maximum,std::abs(a[i]-b[i]));
        return maximum;
    };
    for(double rate:{8000.,48000.25})for(int version:{113})for(int mode:{1,2})for(int unison:{1,4})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(mode);
        SCOPED_TRACE(unison);
        const auto disabled=render(rate,version,mode,unison,true,0,false);
        const auto automatic=render(rate,version,mode,unison,true,64,false);
        const auto manual=render(rate,version,mode,unison,false,64,false);
        EXPECT_GT(difference(automatic,disabled,20,40),1e-6f);
        EXPECT_GT(difference(automatic,manual,0,20),1e-6f);
        const auto edited=render(rate,version,mode,unison,true,64,true);
        EXPECT_GT(difference(automatic,edited,28,40),1e-6f);
    }
}

TEST(PlaybackBehavior, SharedLfoKeepsMidiOnsetsAtTheirQueuedOffsetsAndReleases)
{
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(int onset:{0,1,31,63,64,65,127,128})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(onset);
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(41,4);set(42,5);set(43,96);set(44,64);set(57,1);set(91,1);
        ProcessorBridge processor,quiet;
        setup(processor,text,rate);setup(quiet,text,rate);
        const auto state=processor.saveState();
        float audible=0,tail=0;
        for(int callback=0;callback<40;++callback)
        {
            const int start=callback*64,off=onset+192;
            if(onset>=start && onset<start+64)
            {
                EXPECT_TRUE(processor.enqueueMidi(onset-start,0x90,60,100));
                // An ignored channel event retains the same graph partition,
                // including the output graph's existing semantic-silence fill.
                EXPECT_TRUE(quiet.enqueueMidi(onset-start,0xa0,127,0));
            }
            if(off>=start && off<start+64)
            {
                EXPECT_TRUE(processor.enqueueMidi(off-start,0x80,60,0));
                EXPECT_TRUE(quiet.enqueueMidi(off-start,0xa0,127,0));
            }
            Stereo audio{},reference{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            EXPECT_TRUE(quiet.process(reference[0].data(),reference[1].data(),64,120));
            for(int i=0;i<64;++i)for(int channel=0;channel<2;++channel)
            {
                const float value=audio[channel][i];
                EXPECT_TRUE(std::isfinite(value));
                if(start+i<onset)EXPECT_FLOAT_EQ(value,reference[channel][i]);
                if(start+i>=onset && start+i<off)audible=std::max(audible,std::abs(value));
                if(callback==39)tail=std::max(tail,std::abs(value));
            }
            EXPECT_EQ(processor.saveState(),state);
        }
        EXPECT_GT(audible,1e-6f);EXPECT_LT(tail,1e-6f);
    }
}

TEST(PlaybackBehavior, PitchLfoUsesCommonDepthForLocalAndSharedSources)
{
    const auto frequency=[](double rate,int version,bool keySync,int destination,int depth)
    {
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,0);set(1,4);set(5,0);set(14,0);set(41,destination+1);
        set(42,2);set(43,0);set(44,depth);set(57,1);set(68,keySync);set(91,1);
        ProcessorBridge processor;setup(processor,text,rate);
        const auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,36,100));
        std::vector<double> crossings;float previous=0;
        for(int callback=0;callback<64;++callback)
        {
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(callback==0)
            {
                synth2::RenderControls controls;
                EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.lfo1.wave,3);EXPECT_EQ(controls.lfo1.dest,destination);
                EXPECT_EQ(controls.lfo1.keySync,keySync);EXPECT_EQ(controls.unisonVoices,1);
                EXPECT_NEAR(controls.lfo1.amount,depth/127.f,1e-6);
                if(version==1 && keySync)EXPECT_TRUE(controls.lfo1.nativeSharedClock);
            }
            for(int i=0;i<64;++i)
            {
                const float value=audio[0][i];
                EXPECT_TRUE(std::isfinite(value));EXPECT_TRUE(std::isfinite(audio[1][i]));
                if(callback>=16 && previous<0 && value>=0)
                    crossings.push_back(callback*64+i-1.0-previous/(value-previous));
                previous=value;
            }
        }
        EXPECT_EQ(processor.saveState(),state);
        if(crossings.size()<3){ADD_FAILURE()<<"Insufficient pitch crossings";return 0.;}
        return (crossings.size()-1)*rate/(crossings.back()-crossings.front());
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const double base=440.*std::pow(2.,(36-69)/12.);
        EXPECT_NEAR(frequency(rate,version,keySync,1,0),base,1.);
        EXPECT_NEAR(frequency(rate,version,keySync,0,127),base,1.);
        EXPECT_NEAR(frequency(rate,version,keySync,1,127),base*32,base*.32);
    }
}

TEST(PlaybackBehavior, PulseWidthLfoKeepsBoundsEnvelopeCompositionAndKeySync)
{
    const auto render=[](double rate,int version,bool keySync,int base,int depth,int envelope,bool edit=false)
    {
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        text+="71,2\n";
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,2);set(1,4);set(5,0);set(8,base);set(14,0);set(41,5);
        set(42,2);set(43,0);set(44,depth);set(57,1);set(68,keySync);set(91,1);
        set(10,envelope>=0);set(11,envelope>=0 ? envelope : 64);set(12,0);set(13,127);
        ProcessorBridge processor;setup(processor,text,rate);
        auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        std::vector<float> samples;
        for(int callback=0;callback<64;++callback)
        {
            if(edit && (callback==16 || callback==24 || callback==32 || callback==34))
            {
                const int value=callback==16 ? 0 : callback==24 ? 127 : 64;
                processor.setNormalizedValue(8,value/127.);
                EXPECT_NEAR(processor.normalizedValue(8),value/127.,1e-6);
                state=processor.saveState();
            }
            if(callback==56)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
            Stereo audio{};
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(callback==0)
            {
                synth2::RenderControls controls;
                EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.lfo1.wave,3);EXPECT_EQ(controls.lfo1.dest,4);
                EXPECT_TRUE(controls.lfo1.on);EXPECT_EQ(controls.lfo1.keySync,keySync);
                if(version==1 && keySync)EXPECT_TRUE(controls.lfo1.nativeSharedClock);
                if(envelope>=0){EXPECT_TRUE(controls.modEnv.on);EXPECT_EQ(controls.modEnv.dest,3);}
            }
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    const auto distance=[](const auto& a,const auto& b)
    {
        double sum=0;
        for(int i=8*64;i<48*64;++i){const double delta=a[i]-b[i];sum+=delta*delta;}
        return std::sqrt(sum/(40*64));
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const auto neutral=render(rate,version,keySync,64,0,-1);
        const auto modulated=render(rate,version,keySync,64,127,-1);
        EXPECT_GT(distance(neutral,modulated),1e-6);
        for(int base:{0,127})for(int depth:{0,127})render(rate,version,keySync,base,depth,-1);
        const auto running=render(rate,version,keySync,64,64,-1);
        EXPECT_GT(distance(running,render(rate,version,keySync,64,64,-1,true)),1e-6);
        if(version==113)
        {
            const auto center=render(rate,version,keySync,64,0,64);
            const auto positive=render(rate,version,keySync,64,127,64);
            const auto negative=render(rate,version,keySync,64,0,0);
            const auto combined=render(rate,version,keySync,64,127,0);
            EXPECT_LT(distance(center,combined),distance(center,positive));
            EXPECT_LT(distance(center,combined),distance(center,negative));
            EXPECT_GT(distance(combined,negative),1e-6);
            EXPECT_GT(distance(combined,positive),1e-6);
        }
    }
}

TEST(PlaybackBehavior, FmLfoUsesCommonDepthForLocalAndSharedSources)
{
    const auto render=[](double rate,int version,bool keySync,int base,int depth,bool edit=false)
    {
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,0);set(1,2);set(5,0);set(14,0);set(41,6);set(42,2);
        set(43,0);set(44,depth);set(45,base);set(57,1);set(68,keySync);set(91,1);
        ProcessorBridge processor;setup(processor,text,rate);auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        std::vector<float> samples;
        for(int callback=0;callback<64;++callback)
        {
            if(edit && (callback==16 || callback==24 || callback==32 || callback==34))
            {
                const int value=callback==16 ? 0 : callback==24 ? 127 : 64;
                processor.setNormalizedValue(45,value/127.);state=processor.saveState();
                EXPECT_NEAR(processor.normalizedValue(45),value/127.,1e-6);
            }
            if(callback==56)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
            Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(callback==0)
            {
                synth2::RenderControls controls;EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.lfo1.wave,3);EXPECT_EQ(controls.lfo1.dest,5);
                EXPECT_TRUE(controls.lfo1.on);EXPECT_EQ(controls.lfo1.keySync,keySync);
                if(version==1 && keySync)EXPECT_TRUE(controls.lfo1.nativeSharedClock);
            }
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    const auto distance=[](const auto& a,const auto& b)
    {
        double sum=0;
        for(int i=8*64;i<48*64;++i){const double delta=a[i]-b[i];sum+=delta*delta;}
        return std::sqrt(sum/(40*64));
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const auto dry=render(rate,version,keySync,0,0);
        const auto maximum=render(rate,version,keySync,127,0);
        const auto modulated=render(rate,version,keySync,0,127);
        EXPECT_GT(distance(dry,maximum),1e-6);
        EXPECT_LT(distance(modulated,maximum),1e-6);
        const auto mid=render(rate,version,keySync,64,64);
        EXPECT_GT(distance(mid,render(rate,version,keySync,64,64,true)),1e-6);
    }
}

TEST(PlaybackBehavior, FilterLfoUsesCommonCutoffAndEnvelopeComposition)
{
    const auto render=[](double rate,int version,bool keySync,int base,int depth,int envelope=63,bool edit=false)
    {
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,0);set(1,4);set(5,0);set(14,0);set(19,base);set(21,envelope);
        set(41,3);set(42,2);set(43,0);set(44,depth);set(57,1);set(68,keySync);set(91,1);
        ProcessorBridge processor;setup(processor,text,rate);auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        std::vector<float> samples;
        for(int callback=0;callback<64;++callback)
        {
            if(edit && (callback==16 || callback==24 || callback==32 || callback==34))
            {
                const int value=callback==16 ? 0 : callback==24 ? 127 : 64;
                processor.setNormalizedValue(19,value/127.);state=processor.saveState();
                EXPECT_NEAR(processor.normalizedValue(19),value/127.,1e-6);
            }
            if(callback==56)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
            Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(callback==0)
            {
                synth2::RenderControls controls;EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.lfo1.wave,3);EXPECT_EQ(controls.lfo1.dest,2);
                EXPECT_TRUE(controls.lfo1.on);EXPECT_EQ(controls.lfo1.keySync,keySync);
                if(version==1 && keySync)EXPECT_TRUE(controls.lfo1.nativeSharedClock);
            }
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
            samples.insert(samples.end(),audio[0].begin(),audio[0].end());
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    const auto distance=[](const auto& a,const auto& b)
    {
        double sum=0;
        for(int i=8*64;i<48*64;++i){const double delta=a[i]-b[i];sum+=delta*delta;}
        return std::sqrt(sum/(40*64));
    };
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const auto closed=render(rate,version,keySync,48,0);
        const auto maximum=render(rate,version,keySync,127,0);
        const auto modulated=render(rate,version,keySync,48,127);
        EXPECT_GT(distance(closed,maximum),1e-6);
        EXPECT_LT(distance(modulated,maximum),1e-6);
        for(int envelope:{0,127})
            EXPECT_LT(distance(render(rate,version,keySync,48,127,envelope),maximum),1e-6);
        const auto mid=render(rate,version,keySync,64,64);
        EXPECT_GT(distance(mid,render(rate,version,keySync,64,64,63,true)),1e-6);
        render(rate,version,keySync,0,0);
    }
}

namespace
{
Stereo lfoReceiverRms(double rate,int version,bool keySync,int destination,int depth,int speed)
{
    auto text=hostvalidation::patch();
    text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
    const auto set=[&](int field,int value)
    {
        const auto start=text.find("\n"+std::to_string(field)+",");
        if(start==std::string::npos)throw std::runtime_error("Missing source field");
        const auto end=text.find('\n',start+1);
        text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
    };
    set(0,0);set(1,4);set(5,0);set(14,0);set(41,destination+1);set(42,2);
    set(43,speed);set(44,depth);set(57,1);set(68,keySync);set(91,1);
    ProcessorBridge processor;setup(processor,text,rate);const auto state=processor.saveState();
    EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
    Stereo powers{};
    for(int callback=0;callback<64;++callback)
    {
        if(callback==56)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
        Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        if(callback==0)
        {
            synth2::RenderControls controls;EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
            EXPECT_EQ(controls.lfo1.dest,destination);EXPECT_EQ(controls.lfo1.wave,3);
            EXPECT_TRUE(controls.lfo1.on);EXPECT_EQ(controls.lfo1.keySync,keySync);
            if(version==1 && keySync)EXPECT_TRUE(controls.lfo1.nativeSharedClock);
        }
        for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
        {
            EXPECT_TRUE(std::isfinite(audio[channel][i]));
            if(callback>=16 && callback<48)powers[channel][i]+=audio[channel][i]*audio[channel][i];
        }
        EXPECT_EQ(processor.saveState(),state);
    }
    for(auto& channel:powers)for(auto& value:channel)value=std::sqrt(value/32);
    return powers;
}
double receiverEnergy(const std::array<float,64>& rms)
{
    double sum=0;for(float value:rms)sum+=value*value;
    return std::sqrt(sum/64);
}
}

TEST(PlaybackBehavior, PanLfoUsesCommonCurveForSharedAndNoteSynchronizedSources)
{
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const auto center=lfoReceiverRms(rate,version,keySync,6,0,0);
        const auto positive=lfoReceiverRms(rate,version,keySync,6,127,0);
        const double base=receiverEnergy(center[0]),left=receiverEnergy(positive[0]),right=receiverEnergy(positive[1]);
        EXPECT_GT(base,1e-6);EXPECT_GT(left,base*1.4);EXPECT_LT(left,base*1.6);
        EXPECT_LT(right,left*.01);
        EXPECT_NEAR(receiverEnergy(center[1]),base,base*.01);
    }
}

TEST(PlaybackBehavior, TremoloUsesVcaForSharedAndNoteSynchronizedSources)
{
    for(double rate:{8000.,48000.25})for(int version:{1,113})for(bool keySync:{false,true})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(version);
        SCOPED_TRACE(keySync);
        const auto base=lfoReceiverRms(rate,version,keySync,3,0,0);
        const auto positive=lfoReceiverRms(rate,version,keySync,3,127,0);
        const auto changing=lfoReceiverRms(rate,version,keySync,3,127,127);
        for(int channel=0;channel<2;++channel)
        {
            const double reference=receiverEnergy(base[channel]);
            EXPECT_GT(reference,1e-6);
            EXPECT_NEAR(receiverEnergy(positive[channel]),reference,reference*.01);
            EXPECT_LT(receiverEnergy(changing[channel]),reference*.95);
        }
    }
}

TEST(PlaybackBehavior, ZeroDepthRandomLfoDoesNotChangeSteadyOscillatorReadout)
{
    const auto render=[](double rate,bool enabled)
    {
        auto text=hostvalidation::patch();
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(0,0);set(1,0);set(5,0);set(14,0);set(29,127);
        set(41,4);set(42,3);set(43,64);set(44,0);set(57,enabled);set(91,1);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);const auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        std::vector<float> samples;
        for(int callback=0;callback<64;++callback)
        {
            Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(callback==0)
            {
                synth2::RenderControls controls;EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
                EXPECT_EQ(controls.lfo1.on,enabled);EXPECT_EQ(controls.lfo1.wave,4);
                EXPECT_EQ(controls.lfo1.amount,0);EXPECT_EQ(controls.lfo1.dest,3);
            }
            for(const auto& channel:audio)for(float value:channel)
            {
                EXPECT_TRUE(std::isfinite(value));if(callback>=16)samples.push_back(value);
            }
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    for(double rate:{8000.,48000.25})
    {
        SCOPED_TRACE(rate);
        const auto dry=render(rate,false),zero=render(rate,true);
        ASSERT_EQ(dry.size(),zero.size());double difference=0,energy=0;
        for(size_t i=0;i<dry.size();++i){const double delta=dry[i]-zero[i];difference+=delta*delta;energy+=dry[i]*dry[i];}
        EXPECT_GT(std::sqrt(energy/dry.size()),1e-6);
        EXPECT_LT(std::sqrt(difference/dry.size()),1e-6);
    }
}

namespace
{
std::vector<float> renderOscillatorComposition(double rate,bool ring,int fm,int sub,int stack,int subOctave=0)
{
    auto text=hostvalidation::patch();
    const auto set=[&](int field,int value)
    {
        const auto start=text.find("\n"+std::to_string(field)+",");
        if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
        const auto end=text.find('\n',start+1);
        text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
    };
    set(0,ring?1:0);set(1,3);set(5,ring?64:0);set(7,ring);set(14,0);set(29,127);
    set(41,4);set(42,3);set(43,64);set(44,0);set(45,fm);set(57,1);
    set(76,stack);set(91,ring?1:0);set(95,sub);set(96,0);set(97,subOctave);
    ProcessorBridge processor;
    for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
    setup(processor,text,rate);const auto state=processor.saveState();
    EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));std::vector<float> samples;
    for(int callback=0;callback<64;++callback)
    {
        if(callback==4)EXPECT_TRUE(processor.enqueueMidi(19,0x90,64,90));
        if(callback==32)EXPECT_TRUE(processor.enqueueMidi(13,0x80,60,0));
        if(callback==56)EXPECT_TRUE(processor.enqueueMidi(23,0x80,64,0));
        Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        if(callback==0)
        {
            synth2::MappedSynthSnapshot controls;EXPECT_TRUE(synth2::readMappedSynthParams(processor,controls));
            EXPECT_FALSE(controls.nativeRandomLfoHistoryValid);EXPECT_TRUE(controls.lfo1.on);
            EXPECT_EQ(controls.lfo1.wave,4);EXPECT_EQ(controls.lfo1.amount,0);
            EXPECT_EQ(controls.osc1StackDial,stack);EXPECT_NEAR(controls.subOscGain,sub*4.f/127.f,1e-6);
            EXPECT_EQ(controls.subOscWave,3);EXPECT_EQ(controls.subOscOctave,-subOctave);
            if(ring)EXPECT_NEAR(controls.osc2WaveIndex,2.f/3,1e-6);
            else EXPECT_EQ(controls.osc1WaveIndex,1.f);
        }
        for(int i=0;i<64;++i)for(int channel=0;channel<2;++channel)
        {
            EXPECT_TRUE(std::isfinite(audio[channel][i]));
            if(callback>=16 && callback<48)samples.push_back(audio[channel][i]);
        }
        EXPECT_EQ(processor.saveState(),state);
    }
    return samples;
}
double compositionDifference(const std::vector<float>& first,const std::vector<float>& second)
{
    double sum=0;for(size_t i=0;i<first.size();++i){const double delta=first[i]-second[i];sum+=delta*delta;}
    return std::sqrt(sum/first.size());
}
}

TEST(PlaybackBehavior, RingTakesPriorityOverFmWithTriangleModulator)
{
    for(double rate:{8000.,48000.25})
    {
        SCOPED_TRACE(rate);
        const auto ring=renderOscillatorComposition(rate,true,0,0,0);
        const auto configuredFm=renderOscillatorComposition(rate,true,127,0,0);
        EXPECT_LT(compositionDifference(ring,configuredFm),1e-6);
        EXPECT_GT(compositionDifference(ring,std::vector<float>(ring.size())),1e-6);
    }
}

TEST(PlaybackBehavior, SubRemainsActiveWithRandomLfoAndRetainsOctaveControl)
{
    for(double rate:{8000.,48000.25})
    {
        SCOPED_TRACE(rate);
        const auto carrier=renderOscillatorComposition(rate,false,0,0,0);
        const auto same=renderOscillatorComposition(rate,false,0,127,0);
        const auto down=renderOscillatorComposition(rate,false,0,127,0,1);
        EXPECT_GT(compositionDifference(carrier,down),1e-6);
        EXPECT_GT(compositionDifference(same,down),1e-6);
    }
}

TEST(PlaybackBehavior, StackRemainsActiveWithRandomLfoAndOtherOscillatorControls)
{
    for(double rate:{8000.,48000.25})
    {
        SCOPED_TRACE(rate);
        const auto carrier=renderOscillatorComposition(rate,false,0,0,0);
        EXPECT_GT(compositionDifference(carrier,renderOscillatorComposition(rate,false,0,0,64)),1e-6);
        renderOscillatorComposition(rate,false,75,64,64);
    }
}

TEST(PlaybackBehavior, RandomArpKeepsSharedStreamAfterHistoryInvalidation)
{
    for(double rate:{8000.,44100.,48000.25,384000.})
    {
        SCOPED_TRACE(rate);
        auto text=hostvalidation::patch();
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(31,4);set(33,18);set(34,127);set(42,0);set(47,0);set(57,0);set(58,0);set(59,0);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);Stereo output{};
        ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        ASSERT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
        const auto before=processor.engineForInspection().getNativeRandomLfos().random.state;
        const auto& info=processor.parameter(59);
        processor.setNormalizedValue(59,(1-info.minimum)/(info.maximum-info.minimum));
        const auto state=processor.saveState();
        ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        const auto advanced=processor.engineForInspection().getNativeRandomLfos().random.state;
        EXPECT_NE(advanced,before);
        EXPECT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
        EXPECT_EQ(processor.saveState(),state);
        processor.setNormalizedValue(59,0);
        ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        EXPECT_EQ(processor.engineForInspection().getNativeRandomLfos().random.state,advanced);
    }
}

TEST(PlaybackBehavior, OrdinaryBendSlewsAndResetsForLegacySources)
{
    for(double rate:{8000.,44100.,48000.25,384000.})for(int channel:{1,2})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<channel);
        auto text=hostvalidation::patch();text.replace(text.find("ver=113"),7,"ver=1");
        text.replace(text.find("\n57,0"),5,"\n57,1");
        text.replace(text.find("\n42,1"),5,"\n42,3");text+="40,12\n";
        ProcessorBridge processor;setup(processor,text,rate);Stereo output{};
        ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        synth2::RenderControls mapped;ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        ASSERT_EQ(mapped.pitchBendRange,12);
        const auto state=processor.saveState();
        ASSERT_TRUE(processor.enqueueMidi(7,static_cast<uint8_t>(0xe0+channel-1),127,127));
        for(int n=0;n<40;++n)ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        auto words=processor.engineForInspection().pitchBendWords(channel);
        EXPECT_EQ(words[0],63);EXPECT_EQ(words[1],63);EXPECT_EQ(words[2],0);EXPECT_GT(words[3],0);
        ASSERT_TRUE(processor.enqueueMidi(19,static_cast<uint8_t>(0xe0+channel-1),0,0));
        ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        words=processor.engineForInspection().pitchBendWords(channel);
        EXPECT_LT(words[0],63);EXPECT_GT(words[0],-63);EXPECT_EQ(words[1],-63);EXPECT_EQ(words[2],1);
        for(int n=0;n<80;++n)ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        words=processor.engineForInspection().pitchBendWords(channel);EXPECT_EQ(words[0],-63);EXPECT_LT(words[3],0);
        ASSERT_TRUE(processor.enqueueMidi(23,static_cast<uint8_t>(0xb0+channel-1),121,0));
        for(int n=0;n<40;++n)ASSERT_TRUE(processor.process(output[0].data(),output[1].data(),64,120));
        EXPECT_EQ(processor.engineForInspection().pitchBendWords(channel),(std::array<int,4>{0,0,0,0}));
        EXPECT_EQ(processor.saveState(),state);
    }
}

TEST(PlaybackBehavior, SoundingMonoReassignmentsRetainEnvelopeWithRandomLfo)
{
    for(double rate:{8000.,48000.25})for(int mode:{1,2})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<mode);
        auto text=hostvalidation::patch(1,mode);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos)throw std::runtime_error("Missing source field");
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(15,80);set(17,127);set(41,4);set(42,3);set(44,0);set(57,1);set(91,1);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);Stereo audio{};
        ASSERT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        for(int n=0;n<32;++n)ASSERT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        synth2::RenderControls mapped;ASSERT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        ASSERT_EQ(mapped.playMode,mode);
        const auto state=processor.saveState();
        const auto current=[&](int note)
        {
            std::array<uint32_t,16> words{};
            for(size_t voice=0;voice<32;++voice)
                if(synth2::readRenderingCoreWords(processor,voice,0,words) && words[0] && words[1]==static_cast<uint32_t>(note))return words;
            ADD_FAILURE()<<"Missing active note "<<note;return std::array<uint32_t,16>{};
        };
        const auto first=current(60);ASSERT_GT(first[7],2u);
        ASSERT_TRUE(processor.enqueueMidi(63,0x90,64,90));
        ASSERT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        const auto reassigned=current(64);EXPECT_GT(reassigned[7],first[7]/2);
        ASSERT_TRUE(processor.enqueueMidi(63,0x80,64,0));
        ASSERT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        const auto returned=current(60);EXPECT_GT(returned[7],reassigned[7]/2);
        EXPECT_EQ(processor.saveState(),state);
    }
}

TEST(PlaybackBehavior, StaticPanFollowsNonlinearEffectsWithRandomLfo)
{
    const auto render=[](double rate,int unison,int pan)
    {
        auto text=hostvalidation::patch(unison);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(14,0);set(41,4);set(42,3);set(44,0);set(57,1);
        set(55,0);set(65,0);set(77,1);set(78,2);set(79,96);set(80,64);set(81,64);set(90,pan);set(91,1);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);const auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        std::vector<float> samples;
        for(int n=0;n<48;++n)
        {
            if(n==4)EXPECT_TRUE(processor.enqueueMidi(19,0x90,64,90));
            Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            if(n==0)
            {
                synth2::RenderControls mapped;EXPECT_TRUE(synth2::readMappedSynthParams(processor,mapped));
                EXPECT_FALSE(((mapped.lfo1.on && mapped.lfo1.dest==6) || (mapped.lfo2.on && mapped.lfo2.dest==6)));
                EXPECT_EQ(mapped.nativeMultiEffectType,2);EXPECT_FLOAT_EQ(mapped.pan,pan==64 ? 0.f : -.5f);
            }
            for(int i=0;i<64;++i)for(int channel=0;channel<2;++channel)
            {EXPECT_TRUE(std::isfinite(audio[channel][i]));if(n>=16)samples.push_back(audio[channel][i]);}
            EXPECT_EQ(processor.saveState(),state);
        }
        return samples;
    };
    for(double rate:{8000.,48000.25})for(int unison:{1,4})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<unison);
        const auto center=render(rate,unison,64),half=render(rate,unison,32);
        ASSERT_EQ(center.size(),half.size());double energy=0;
        for(size_t i=0;i<center.size();i+=2)
        {
            EXPECT_FLOAT_EQ(half[i],center[i]);EXPECT_FLOAT_EQ(half[i+1],center[i+1]*.5f);
            energy+=static_cast<double>(center[i])*center[i];
        }
        EXPECT_GT(energy,1e-6);
    }
}

TEST(PlaybackBehavior, MidCallbackToneControlCannotChangeEarlierOutput)
{
    const auto render=[](double rate,int unison,bool edited)
    {
        auto text=hostvalidation::patch(unison);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(41,4);set(42,3);set(44,0);set(57,1);set(65,0);set(66,0);set(91,1);
        set(50,127);set(86,45057);set(87,60);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);const auto state=processor.saveState();
        EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,100));
        Stereo audio{};
        for(int n=0;n<32;++n)EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        synth2::RenderControls mapped;EXPECT_TRUE(synth2::readMappedSynthParams(processor,mapped));
        EXPECT_FALSE(((mapped.lfo1.on && mapped.lfo1.dest==6) || (mapped.lfo2.on && mapped.lfo2.dest==6)));
        const auto& routes=processor.engineForInspection().getRenderHistory().controllerRoutes;
        EXPECT_EQ(routes[0].source,45057);EXPECT_EQ(routes[0].destination,60);EXPECT_EQ(routes[0].sensitivity,1);
        if(edited)EXPECT_TRUE(processor.enqueueMidi(32,0xb0,1,127));
        EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
        EXPECT_EQ(processor.saveState(),state);
        return audio;
    };
    for(double rate:{8000.,48000.25})for(int unison:{1,4})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<unison);
        const auto steady=render(rate,unison,false),edited=render(rate,unison,true);
        double difference=0;
        for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
        {
            EXPECT_TRUE(std::isfinite(edited[channel][i]));
            if(i<32)EXPECT_EQ(edited[channel][i],steady[channel][i]);
            else {const double delta=edited[channel][i]-steady[channel][i];difference+=delta*delta;}
        }
        EXPECT_GT(difference,1e-10);
    }
}

TEST(PlaybackBehavior, VelocitySensitivityIsHeldUntilTheNextNoteAssignment)
{
    struct Result { std::vector<float> held;float assignedReference {}; };
    const auto render=[](double rate,int unison,bool edited)
    {
        auto text=hostvalidation::patch(unison);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(29,127);set(30,127);set(41,4);set(42,3);set(44,0);set(57,1);set(65,0);set(66,0);set(91,1);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);auto state=processor.saveState();Result result;
        for(int n=0;n<49;++n)
        {
            if(n==0)EXPECT_TRUE(processor.enqueueMidi(7,0x90,60,64));
            if(n==4)EXPECT_TRUE(processor.enqueueMidi(19,0x90,64,31));
            if(n==16 && edited){processor.setNormalizedValue(30,0);state=processor.saveState();}
            if(n==48)EXPECT_TRUE(processor.enqueueMidi(13,0x90,72,64));
            Stereo audio{};EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            for(int i=0;i<64;++i)for(int channel=0;channel<2;++channel)
            {EXPECT_TRUE(std::isfinite(audio[channel][i]));if(n>=16 && n<48)result.held.push_back(audio[channel][i]);}
            EXPECT_EQ(processor.saveState(),state);
        }
        EXPECT_EQ(processor.normalizedValue(30),edited ? 0 : 1);
        bool found=false;
        for(size_t voice=0;voice<32;++voice)
        {
            std::array<uint32_t,16> words{};std::array<uint32_t,22> floats{};
            if(synth2::readRenderingCoreWords(processor,voice,0,words) && words[0] && words[1]==72)
            {
                EXPECT_TRUE(synth2::readRenderingCoreFloatWords(processor,voice,0,floats));
                std::memcpy(&result.assignedReference,&floats[3],sizeof(float));found=true;break;
            }
        }
        EXPECT_TRUE(found);return result;
    };
    for(double rate:{8000.,48000.25})for(int unison:{1,4})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<unison);
        const auto steady=render(rate,unison,false),edited=render(rate,unison,true);
        EXPECT_EQ(edited.held,steady.held);
        EXPECT_GT(steady.assignedReference,0);EXPECT_LT(steady.assignedReference,1);
        EXPECT_EQ(edited.assignedReference,1);
    }
}

TEST(PlaybackBehavior, FirstKeyResetsRequestedClocksAndRandomTargetsAfterHistoryInvalidation)
{
    for(double rate:{8000.,44100.,48000.25,384000.})for(int mode:{0,1,2})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<mode);
        auto text=hostvalidation::patch(1,mode);
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(41,4);set(42,3);set(43,42);set(44,64);set(57,1);set(68,1);set(91,1);
        ProcessorBridge processor;
        for(int i=0;i<128;++i)processor.setNormalizedValue(i,processor.parameter(i).defaultNormalized);
        setup(processor,text,rate);const auto state=processor.saveState();Stereo audio{};
        const auto process=[&]
        {
            EXPECT_TRUE(processor.process(audio[0].data(),audio[1].data(),64,120));
            EXPECT_EQ(processor.saveState(),state);
            for(const auto& channel:audio)for(float value:channel)EXPECT_TRUE(std::isfinite(value));
        };
        const auto counter=[&]{return processor.engineForInspection().getNativeLfoClock(0).counter;};
        const auto random=[&]{return processor.engineForInspection().getNativeRandomLfos().random.state;};
        for(int n=0;n<8;++n)process();
        synth2::MappedSynthSnapshot controls;ASSERT_TRUE(synth2::readMappedSynthParams(processor,controls));
        ASSERT_FALSE(controls.nativeRandomLfoHistoryValid);ASSERT_TRUE(controls.lfo1.keySync);
        ASSERT_FALSE(controls.lfo1.nativeSharedClock);ASSERT_GT(counter(),1);
        std::array<uint32_t,9> words{};ASSERT_TRUE(synth2::readNativeRandomLfoWords(processor,0,words));
        ASSERT_NE(words[3],words[4]);const auto before=random();
        ASSERT_TRUE(processor.enqueueMidi(63,0x90,60,100));process();
        EXPECT_LE(counter(),1);EXPECT_NE(random(),before);const auto firstDraw=random();
        ASSERT_TRUE(processor.enqueueMidi(63,0x90,64,90));process();
        EXPECT_GT(counter(),1);EXPECT_EQ(random(),firstDraw);
        ASSERT_TRUE(processor.enqueueMidi(0,0xb0,64,127));
        ASSERT_TRUE(processor.enqueueMidi(7,0x80,60,0));ASSERT_TRUE(processor.enqueueMidi(19,0x80,64,0));process();
        ASSERT_TRUE(processor.enqueueMidi(63,0x90,67,95));process();
        EXPECT_GT(counter(),1);EXPECT_EQ(random(),firstDraw);
        ASSERT_TRUE(processor.enqueueMidi(7,0x80,67,0));ASSERT_TRUE(processor.enqueueMidi(31,0xb0,64,0));
        ASSERT_TRUE(processor.enqueueMidi(63,0x90,72,100));process();
        EXPECT_LE(counter(),1);EXPECT_NE(random(),firstDraw);
        EXPECT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
    }
}

TEST(PlaybackBehavior, ActiveRandomPeriodsKeepDrawsAcrossCallbackSizesAfterHistoryInvalidation)
{
    for(double rate:{8000.,48000.25})
    {
        SCOPED_TRACE(rate);
        auto text=hostvalidation::patch();
        const auto set=[&](int field,int value)
        {
            const auto start=text.find("\n"+std::to_string(field)+",");
            if(start==std::string::npos){text+=std::to_string(field)+","+std::to_string(value)+"\n";return;}
            const auto end=text.find('\n',start+1);
            text.replace(start+1,end-start-1,std::to_string(field)+","+std::to_string(value));
        };
        set(41,4);set(42,3);set(43,127);set(44,64);set(57,1);set(67,1);set(68,0);
        ProcessorBridge whole,split;
        for(auto* p:{&whole,&split})
        {
            for(int i=0;i<128;++i)p->setNormalizedValue(i,p->parameter(i).defaultNormalized);
            std::string error;ASSERT_TRUE(p->loadPatch(text,error))<<error;p->prepare(rate,512);
        }
        const auto state=whole.saveState();ASSERT_EQ(split.saveState(),state);
        const auto process=[&](ProcessorBridge& p,int frames)
        {
            std::array<float,512> left{},right{};
            EXPECT_TRUE(p.process(left.data(),right.data(),frames,1000));
            for(int i=0;i<frames;++i){EXPECT_TRUE(std::isfinite(left[i]));EXPECT_TRUE(std::isfinite(right[i]));}
            EXPECT_EQ(p.saveState(),state);
        };
        for(int n=0;n<128;++n)for(auto* p:{&whole,&split})process(*p,256);
        synth2::MappedSynthSnapshot controls;ASSERT_TRUE(synth2::readMappedSynthParams(whole,controls));
        ASSERT_FALSE(controls.nativeRandomLfoHistoryValid);ASSERT_FALSE(controls.lfo1.nativeSharedClock);
        ASSERT_TRUE(controls.lfo1.on);ASSERT_EQ(controls.lfo1.wave,4);
        const auto& clock=whole.engineForInspection().getNativeLfoClock(0);
        ASSERT_EQ(clock.currentHz,clock.targetHz);
        const int period=clock.period,remaining=period-clock.counter;
        ASSERT_GT(period,0);ASSERT_LE(period*3,512);
        for(auto* p:{&whole,&split})process(*p,remaining);
        ASSERT_EQ(whole.engineForInspection().getNativeLfoClock(0).counter,0);
        ASSERT_EQ(split.engineForInspection().getNativeLfoClock(0).counter,0);
        process(whole,period*3);
        for(int n=0;n<3;++n)process(split,period);
        std::array<uint32_t,9> a{},b{};
        ASSERT_TRUE(synth2::readNativeRandomLfoWords(whole,0,a));ASSERT_TRUE(synth2::readNativeRandomLfoWords(split,0,b));
        EXPECT_EQ(a,b);
        EXPECT_EQ(whole.engineForInspection().getNativeLfoClock(0).counter,split.engineForInspection().getNativeLfoClock(0).counter);
        EXPECT_FALSE(whole.engineForInspection().nativeRandomHistoryValid());
        EXPECT_FALSE(split.engineForInspection().nativeRandomHistoryValid());
    }
}
