#include <gtest/gtest.h>
#include "dsp/LfoHistory.h"
#include "SynthEngine.h"
#include "../tools/host_scenarios.h"
#include <array>
#include <thread>
#include <barrier>

TEST(HistoryQueue, FullQueueRejectsNewEventsPreservesFifoAndReportsEveryDrop)
{
    synth2::dsp::LfoHistory<8> queue;
    synth2::dsp::LfoHistoryEvent event;
    EXPECT_FALSE(queue.pop(event));
    for(int i=0;i<8;++i) ASSERT_TRUE(queue.push({0,i,i+17,{}}));
    for(int i=0;i<3;++i) EXPECT_FALSE(queue.push({3,99,i,{}}));
    EXPECT_EQ(queue.droppedEvents(),3u);
    for(int i=0;i<8;++i)
    {
        ASSERT_TRUE(queue.pop(event));
        EXPECT_EQ(event.kind,0); EXPECT_EQ(event.a,i); EXPECT_EQ(event.b,i+17);
    }
    EXPECT_FALSE(queue.pop(event));
    EXPECT_EQ(queue.droppedEvents(),3u);
}

TEST(HistoryQueue, WrappedSlotsRemainReusableAfterRepeatedSaturation)
{
    synth2::dsp::LfoHistory<8> queue;
    for(int round=0;round<128;++round)
    {
        SCOPED_TRACE(round);
        for(int i=0;i<8;++i) ASSERT_TRUE(queue.push({4,round,i,{}, {1,0},{{{1,2,3},{4,5,6}}},true,false}));
        EXPECT_FALSE(queue.push({}));
        for(int i=0;i<8;++i)
        {
            synth2::dsp::LfoHistoryEvent event;
            ASSERT_TRUE(queue.pop(event));
            EXPECT_EQ(event.a,round); EXPECT_EQ(event.b,i);
            EXPECT_EQ(event.sync,(std::array<int,2>{1,0}));
            EXPECT_EQ(event.randomControls[1],(std::array<int,3>{4,5,6}));
            EXPECT_TRUE(event.randomAdmitted); EXPECT_FALSE(event.randomArpeggio);
        }
        EXPECT_EQ(queue.droppedEvents(),static_cast<unsigned>(round+1));
    }
}

TEST(HistoryQueue, BridgeReportsLostHistoryAndAcceptsNewEditsAfterDrain)
{
    synth2::host::ProcessorBridge processor;
    std::string error;
    ASSERT_TRUE(processor.loadPatch(hostvalidation::patch(),error));
    processor.prepare(44100,64);
    std::array<float,64> left{},right{};
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    ASSERT_EQ(processor.historyDrops(),0u);
    constexpr auto capacity=synth2::dsp::LfoHistory<>::capacity;
    for(unsigned i=0;i<capacity+7;++i) processor.setNormalizedValue(43,(i%2)/127.);
    EXPECT_EQ(processor.historyDrops(),7u);
    const auto source=processor.saveState();
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    EXPECT_EQ(processor.saveState(),source);
    EXPECT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
    processor.setNormalizedValue(43,1);
    ASSERT_TRUE(processor.enqueueMidi(7,0x90,60,100));
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    EXPECT_EQ(processor.historyDrops(),7u);
    EXPECT_EQ(processor.engineForInspection().getNativeLfoClock(0).rate,127);
    EXPECT_NEAR(processor.normalizedValue(43),1,1e-6);
    EXPECT_FALSE(processor.engineForInspection().nativeRandomHistoryValid());
    double energy=0;
    for(int i=0;i<64;++i)
    {
        ASSERT_TRUE(std::isfinite(left[i])); ASSERT_TRUE(std::isfinite(right[i]));
        energy+=double(left[i])*left[i]+double(right[i])*right[i];
    }
    EXPECT_GT(energy,0);
}

TEST(HistoryQueue, ConcurrentPublishersPreserveEveryAcceptedEventAndPerProducerOrder)
{
    synth2::dsp::LfoHistory<16> queue;
    constexpr int events=512;
    std::array<std::array<bool,events>,2> accepted{},seen{};
    std::atomic<int> finished{};
    std::barrier start(3);
    std::array<std::thread,2> writers;
    for(int producer=0;producer<2;++producer)
        writers[producer]=std::thread([&,producer] {
            start.arrive_and_wait();
            for(int i=0;i<events;++i) accepted[producer][i]=queue.push({4,producer,i,{}});
            finished.fetch_add(1,std::memory_order_release);
        });
    start.arrive_and_wait();
    std::array<int,2> last{-1,-1};
    int received=0;
    const auto consume=[&](const synth2::dsp::LfoHistoryEvent& event) {
        EXPECT_GE(event.a,0); EXPECT_LT(event.a,2); EXPECT_GE(event.b,0); EXPECT_LT(event.b,events);
        if(event.a<0 || event.a>=2 || event.b<0 || event.b>=events)return;
        EXPECT_FALSE(seen[event.a][event.b]); EXPECT_GT(event.b,last[event.a]);
        seen[event.a][event.b]=true; last[event.a]=event.b; ++received;
    };
    for(;;)
    {
        synth2::dsp::LfoHistoryEvent event;
        while(queue.pop(event)) consume(event);
        if(finished.load(std::memory_order_acquire)==2)
        {
            // A producer may have published between the empty query and done.
            while(queue.pop(event)) consume(event);
            break;
        }
        std::this_thread::yield();
    }
    for(auto& writer:writers)writer.join();
    EXPECT_EQ(seen,accepted);
    EXPECT_GT(received,0);
    EXPECT_EQ(queue.droppedEvents(),static_cast<unsigned>(2*events-received));
}
