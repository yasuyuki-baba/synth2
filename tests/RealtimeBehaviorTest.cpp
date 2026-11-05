#include <gtest/gtest.h>
#include "support/HeapActivity.h"
#include "dsp/LfoHistory.h"
#include "../tools/host_scenarios.h"
#include <array>
#include <cstdlib>
#include <new>

TEST(RealtimeBehavior, TrackerDetectsAllocationsAndDeallocations)
{
    testheap::Counts counts;
    { testheap::Scope scope(counts); auto* p=::operator new(31); ::operator delete(p); }
    EXPECT_EQ(counts.allocations,1u); EXPECT_EQ(counts.deallocations,1u);
}

TEST(RealtimeBehavior, PreparedCallbacksAndSourceEditsDoNotUseCppHeap)
{
    for(int version:{99,100,101,103,104,105,108,112,113})
    {
        SCOPED_TRACE(version);
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        synth2::host::ProcessorBridge processor;
        std::string error;
        ASSERT_TRUE(processor.loadPatch(text,error))<<error;
        processor.prepare(48000,257);
        std::array<float,257> left{},right{};
        for(int block=0;block<12;++block)
        {
            SCOPED_TRACE(block);
            if(block==1) processor.setNormalizedValue(19,.75);
            if(block==2) processor.setNormalizedValue(42,1.0/127);
            if(block==3) processor.setNormalizedValue(57,1.0/127);
            if(block==4) processor.setNormalizedValue(44,.25);
            if(block==5) processor.setNormalizedValue(128,.5);
            const int frames=std::array<int,4>{1,63,64,257}[block%4];
            testheap::Counts counts;
            bool enqueued,processed;
            {
                testheap::Scope scope(counts);
                enqueued=processor.enqueueMidi(frames-1,block%2 ? 0x80 : 0x90,60,100);
                processed=processor.process(left.data(),right.data(),frames,137);
            }
            EXPECT_TRUE(enqueued); EXPECT_TRUE(processed);
            EXPECT_EQ(counts.allocations,0u); EXPECT_EQ(counts.deallocations,0u);
            EXPECT_EQ(counts.cAllocations,0u); EXPECT_EQ(counts.cDeallocations,0u);
        }
    }
}

TEST(RealtimeBehavior, EverySourceDialCanBeAutomatedWithoutCallbackHeapActivity)
{
    for(int version:{99,100,101,103,105,113})
    {
        SCOPED_TRACE(version);
        auto text=hostvalidation::patch();
        text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
        synth2::host::ProcessorBridge processor;
        std::string error; ASSERT_TRUE(processor.loadPatch(text,error)); processor.prepare(48000,64);
        std::array<float,64> left{},right{};
        for(int parameter=0;parameter<129;++parameter)
        {
            SCOPED_TRACE(parameter);
            const auto before=processor.normalizedValue(parameter);
            for(double value:{0.,.5,1.})
            {
                processor.setNormalizedValue(parameter,value);
                testheap::Counts counts; bool processed;
                { testheap::Scope scope(counts); processed=processor.process(left.data(),right.data(),64,120); }
                EXPECT_TRUE(processed); EXPECT_EQ(counts.allocations,0u); EXPECT_EQ(counts.deallocations,0u);
                EXPECT_EQ(counts.cAllocations,0u); EXPECT_EQ(counts.cDeallocations,0u);
            }
            processor.setNormalizedValue(parameter,before);
        }
    }
}

TEST(RealtimeBehavior, DoubleZeroOversizeAndOverflowCallbacksAvoidCppHeap)
{
    synth2::host::ProcessorBridge processor;
    processor.prepare(44100,64);
    std::array<double,65> left{},right{};
    for(size_t i=0;i<synth2::host::ProcessorBridge::midiCapacity;++i)
        ASSERT_TRUE(processor.enqueueMidi(0,0xb0,1,0));
    EXPECT_FALSE(processor.enqueueMidi(0,0x80,60,0));
    testheap::Counts counts;
    bool zero,overflow,oversize;
    {
        testheap::Scope scope(counts);
        zero=processor.process(static_cast<double*>(nullptr),static_cast<double*>(nullptr),0,120);
        overflow=processor.process(left.data(),right.data(),64,120);
        oversize=processor.process(left.data(),right.data(),65,120);
    }
    EXPECT_TRUE(zero); EXPECT_TRUE(overflow); EXPECT_FALSE(oversize);
    EXPECT_EQ(counts.allocations,0u); EXPECT_EQ(counts.deallocations,0u);
    EXPECT_EQ(counts.cAllocations,0u); EXPECT_EQ(counts.cDeallocations,0u);
}

TEST(RealtimeBehavior, AlignedArrayAndNothrowCppAllocationsAreTracked)
{
    testheap::Counts counts;
    {
        testheap::Scope scope(counts);
        auto* array=::operator new[](64); ::operator delete[](array);
        auto* aligned=::operator new(64,std::align_val_t{64}); ::operator delete(aligned,std::align_val_t{64});
        auto* safe=::operator new(64,std::nothrow); ::operator delete(safe,std::nothrow);
        auto* both=::operator new[](64,std::align_val_t{64},std::nothrow);
        ::operator delete[](both,std::align_val_t{64},std::nothrow);
    }
    EXPECT_EQ(counts.allocations,4u); EXPECT_EQ(counts.deallocations,4u);
}
#if defined(SYNTH2_TEST_WRAP_C_HEAP)
TEST(RealtimeBehavior, StaticCHeapCallsAreTracked)
{
    void* (*volatile allocate)(std::size_t)=std::malloc;
    void* (*volatile zeroed)(std::size_t,std::size_t)=std::calloc;
    void* (*volatile resize)(void*,std::size_t)=std::realloc;
    void (*volatile release)(void*)=std::free;
    testheap::Counts counts;
    {
        testheap::Scope scope(counts);
        auto* p=allocate(32); p=resize(p,64); release(p);
        auto* q=zeroed(2,32); release(q);
    }
    EXPECT_EQ(counts.cAllocations,3u); EXPECT_EQ(counts.cDeallocations,2u);
}
#endif

TEST(RealtimeBehavior, ParameterUpdatesAndMidiPublicationAvoidHeapActivity)
{
    synth2::host::ProcessorBridge processor;
    processor.prepare(44100,64);
    std::array<float,64> left{},right{};
    testheap::Counts counts;
    bool enqueued=true,processed;
    {
        testheap::Scope scope(counts);
        for(int parameter=0;parameter<129;++parameter)
            for(double value:{0.,.5,1.}) processor.setNormalizedValue(parameter,value);
        for(int key=36;key<84;++key) enqueued=processor.enqueueMidi(key%64,0x90,key,100) && enqueued;
        processed=processor.process(left.data(),right.data(),64,120);
    }
    EXPECT_TRUE(enqueued); EXPECT_TRUE(processed);
    EXPECT_EQ(counts.allocations,0u); EXPECT_EQ(counts.deallocations,0u);
    EXPECT_EQ(counts.cAllocations,0u); EXPECT_EQ(counts.cDeallocations,0u);
}

TEST(RealtimeBehavior, SaturatedHistoryDrainAndSubsequentEditAvoidHeapActivity)
{
    synth2::host::ProcessorBridge processor;
    processor.prepare(44100,64);
    constexpr auto capacity=synth2::dsp::LfoHistory<>::capacity;
    for(unsigned i=0;i<capacity+8;++i) processor.setNormalizedValue(43,(i%2)/127.);
    ASSERT_EQ(processor.historyDrops(),8u);
    std::array<float,64> left{},right{};
    testheap::Counts counts;
    bool drained,recovered;
    {
        testheap::Scope scope(counts);
        drained=processor.process(left.data(),right.data(),64,120);
        processor.setNormalizedValue(43,1);
        recovered=processor.process(left.data(),right.data(),64,120);
    }
    EXPECT_TRUE(drained); EXPECT_TRUE(recovered);
    EXPECT_EQ(processor.historyDrops(),8u);
    EXPECT_EQ(counts.allocations,0u); EXPECT_EQ(counts.deallocations,0u);
    EXPECT_EQ(counts.cAllocations,0u); EXPECT_EQ(counts.cDeallocations,0u);
}


TEST(RealtimeBehavior, EditorNotesAndOverlapWithHostMidiDoNotAllocateOrLoseRelease)
{
    using namespace synth2::host;
    ProcessorBridge processor;processor.prepare(44100,64);
    std::array<float,64> left{},right{};
    for(int block=0;block<8;++block) {
        testheap::Counts counts;bool processed;
        {
            testheap::Scope scope(counts);
            if(block==0)processor.setEditorNote(60,127,EditorNoteSource::computer);
            if(block==1)processor.setEditorNote(60,80,EditorNoteSource::mouse);
            if(block==2)processor.enqueueMidi(13,0x91,60,95);
            if(block==3)processor.setEditorNote(60,0,EditorNoteSource::computer);
            if(block==4)processor.enqueueMidi(29,0x81,60,0);
            if(block==5)processor.releaseEditorNotes();
            processed=processor.process(left.data(),right.data(),64,120);
        }
        EXPECT_TRUE(processed);
        EXPECT_EQ(counts.allocations,0u);EXPECT_EQ(counts.deallocations,0u);
        EXPECT_EQ(counts.cAllocations,0u);EXPECT_EQ(counts.cDeallocations,0u);
    }
    EXPECT_FALSE(processor.editorNotePressed(60));
}
