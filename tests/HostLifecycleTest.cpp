#include "support/BankFiles.h"
#include "support/GeneratedPatches.h"
#include "support/StateDocument.h"
#include "host/PatchLibrary.h"
#include <gtest/gtest.h>
#include "../tools/host_scenarios.h"
#include "RenderControls.h"
#include <algorithm>
using namespace hostvalidation;
TEST(HostLifecycle, GeneratedSoundsRestoreTheSameColdStartAudio)
{
    const auto bank=testfiles::generatedSounds();ASSERT_EQ(bank.size(),128u);
    for(size_t preset=0;preset<bank.size();++preset)
    {
        SCOPED_TRACE(preset);auto a=processor(),b=processor();load(*a,bank[preset]);restore(*b,save(*a));
        a->prepare(48000,127);b->prepare(48000,127);double energy=0,difference=0;
        for(int block=0;block<12;++block)
        {
            synth2::audio::AudioBuffer<float> x(2,127),y(2,127);synth2::audio::MidiBuffer ma,mb;
            if(block==0)notes(ma,1);if(block==8)notes(ma,1,false);mb=ma;
            process(*a,x,ma);process(*b,y,mb);ASSERT_TRUE(finite(x));ASSERT_TRUE(finite(y));
            for(int ch=0;ch<2;++ch)for(int i=0;i<127;++i){const double v=x.getSample(ch,i),d=v-y.getSample(ch,i);energy+=v*v;difference+=d*d;}
        }
        EXPECT_LE(difference,1e-12*std::max(energy,1e-20));
    }
}
TEST(HostLifecycle, RepeatedGeneratedLoadsWhilePlayingStayFiniteAndReleaseTheAudibleGroup)
{
    const auto bank=testfiles::generatedSounds();ASSERT_EQ(bank.size(),128u);auto p=processor();p->prepare(44100,64);
    for(int pass=0;pass<2;++pass)for(size_t preset=0;preset<bank.size();++preset)
    {
        SCOPED_TRACE(preset);load(*p,bank[preset]);
        for(int block=0;block<4;++block)
        {
            synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;
            if(block==0)notes(midi,8);if(block==3)notes(midi,8,false,63);
            process(*p,audio,midi);ASSERT_TRUE(finite(audio));EXPECT_LE(activeCores(*p),32);
        }
    }
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;midi.addEvent(synth2::audio::MidiMessage::allSoundOff(1),0);
    process(*p,audio,midi);auto text=patch(1,1);text.replace(text.find("\n27,127\n"),8,"\n27,0\n");load(*p,text);
    midi.clear();process(*p,audio,midi);midi.addEvent(synth2::audio::MidiMessage::allNotesOff(16),0);
    process(*p,audio,midi);midi.clear();for(int block=0;block<64;++block)process(*p,audio,midi);
    std::array<int,13> primary{};ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,0,primary));
    EXPECT_EQ(primary[10],0);EXPECT_LT(audio.getMagnitude(0,0,64),1e-6f);EXPECT_LT(audio.getMagnitude(1,0,64),1e-6f);
}
TEST(HostLifecycle, ReprepareAcrossRatesAndVariableBlockSizesRemainsPlayable)
{
    auto p=processor();load(*p,patch());
    for(double rate:{22050.,44100.,48000.,96000.,192000.})for(int prepared:{1,17,64,256,1024})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(prepared);p->release();p->prepare(rate,prepared);float peak=0;
        for(int actual:{0,1,17,64,257,1024})
        {
            synth2::audio::AudioBuffer<float> audio(2,actual);synth2::audio::MidiBuffer midi;if(actual==1)notes(midi,8);
            // The bridge deliberately rejects oversize callbacks instead of
            // splitting native DSP histories. Verify both sides of the contract.
            if(actual>prepared) {
                EXPECT_FALSE(p->process(audio.getWritePointer(0),audio.getWritePointer(1),actual,120.));
                EXPECT_EQ(audio.getMagnitude(0,actual),0.f);
            } else {process(*p,audio,midi);ASSERT_TRUE(finite(audio));if(actual>0)peak=std::max(peak,audio.getMagnitude(0,actual));}
        }
        synth2::audio::AudioBuffer<float> audio(2,prepared);synth2::audio::MidiBuffer midi;
        // Even capacity1 must get enough valid samples to pass the onset ramp.
        for(int n=0;n<256;++n){process(*p,audio,midi);peak=std::max(peak,audio.getMagnitude(0,prepared));}
        EXPECT_GT(peak,1e-5f);
        midi.addEvent(synth2::audio::MidiMessage::allSoundOff(1),0);process(*p,audio,midi);midi.clear();
        for(int n=0;n<4096/prepared+1;++n)process(*p,audio,midi);EXPECT_EQ(activeCores(*p),0);
    }
}
TEST(HostLifecycle, DensePolyphonyAndUnisonStealWithinBudgetAndReleaseCompletely)
{
    for(int unison:{1,2,8})for(int count:{1,8,16,32,48})
    {
        SCOPED_TRACE(unison);
        SCOPED_TRACE(count);auto p=processor();load(*p,patch(unison));p->prepare(48000,64);
        synth2::MappedSynthSnapshot mapped;synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,count);
        process(*p,audio,midi);ASSERT_TRUE(finite(audio));ASSERT_TRUE(synth2::readMappedSynthParams(*p,mapped));
        ASSERT_EQ(mapped.unisonVoices,unison);EXPECT_EQ(activeCores(*p),std::min(32,count*unison));
        midi.clear();notes(midi,count,false);process(*p,audio,midi);midi.clear();
        for(int b=0;b<1500;++b){process(*p,audio,midi);ASSERT_TRUE(finite(audio));}
        EXPECT_EQ(activeCores(*p),0);EXPECT_LT(audio.getMagnitude(0,64),1e-6f);
    }
}

TEST(HostLifecycle, ChannelStopReleasesTheSharedKeyboardAndAllowsRetrigger)
{
    auto a=processor(),b=processor();load(*a,patch());load(*b,patch());
    a->prepare(48000,64);b->prepare(48000,64);
    for(int block=0;block<70;++block)
    {
        synth2::audio::AudioBuffer<float> x(2,64),y(2,64);synth2::audio::MidiBuffer ma,mb;
        if(block==0)
        {
            ma.addEvent(synth2::audio::MidiMessage::noteOn(1,60,static_cast<uint8_t>(100)),0);
            ma.addEvent(synth2::audio::MidiMessage::noteOn(2,67,static_cast<uint8_t>(100)),0);
        }
        mb=ma;
        // Product contract: both stop messages follow the release path.
        if(block==1){ma.addEvent(synth2::audio::MidiMessage::allSoundOff(1),17);mb.addEvent(synth2::audio::MidiMessage::allNotesOff(1),17);}
        process(*a,x,ma);process(*b,y,mb);ASSERT_TRUE(finite(x));ASSERT_TRUE(finite(y));
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)EXPECT_FLOAT_EQ(x.getSample(ch,i),y.getSample(ch,i));
    }
    EXPECT_EQ(activeCores(*a),0); // Native omni input stops the shared keyboard.
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);
    process(*a,audio,midi);EXPECT_EQ(activeCores(*a),1);EXPECT_TRUE(finite(audio));
}

TEST(HostLifecycle, SampleRateChangesKeepNewNotesAtConcertPitch)
{
    auto p=processor();auto text=patch();
    text.replace(text.find("\n0,0\n"),5,"\n0,1\n");
    text.replace(text.find("\n5,64\n"),6,"\n5,0\n");load(*p,text);
    for(double rate:{44100.,96000.,22050.,192000.,48000.})
    {
        SCOPED_TRACE(rate);p->release();p->prepare(rate,64);
        synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;
        midi.addEvent(synth2::audio::MidiMessage::noteOn(1,69,static_cast<uint8_t>(100)),0);
        process(*p,audio,midi);midi.clear();for(int b=0;b<100;++b)process(*p,audio,midi);
        int crossings=0,frames=0;float previous=audio.getSample(0,63);
        while(frames<static_cast<int>(rate*.2))
        {
            process(*p,audio,midi);ASSERT_TRUE(finite(audio));
            for(int i=0;i<64;++i){const float v=audio.getSample(0,i);if(previous<=0 && v>0)++crossings;previous=v;}
            frames+=64;
        }
        EXPECT_NEAR(crossings*rate/frames,440.,8.);
    }
}

TEST(HostLifecycle, RepeatedProgramSelectionAndStateRecallRemainPlayable)
{
    testfiles::Directory directory;const auto path=directory.path/"programs.zip";
    std::vector<std::pair<std::string,std::string>> files;
    for(int mode=0;mode<3;++mode)files.emplace_back(std::to_string(mode)+".sy1",patch(1,mode));
    ASSERT_TRUE(testfiles::zip(path,files));synth2::host::PatchLibrary bank;std::string error;
    ASSERT_TRUE(bank.openFile(path.string(),error));auto p=processor();ASSERT_EQ(bank.patches().size(),3u);
    const auto select=[&](int index){const auto& selected=bank.patches().at(static_cast<size_t>(index));ASSERT_TRUE(p->loadProgram(selected.text,index,error,selected.name))<<error;};
    std::array<std::vector<uint8_t>,3> states;
    for(int mode=0;mode<3;++mode){select(mode);states[static_cast<size_t>(mode)]=save(*p);}
    p->prepare(48000,64);
    for(int i=0;i<300;++i)
    {
        const int expected=i%3;
        if(i%2==0)select(expected);else restore(*p,states[static_cast<size_t>(expected)]);
        ASSERT_EQ(p->currentProgram(),expected);
        synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,8);
        process(*p,audio,midi);ASSERT_TRUE(finite(audio));EXPECT_GT(audio.getMagnitude(0,64),1e-6f);
        synth2::MappedSynthSnapshot mapped;ASSERT_TRUE(synth2::readMappedSynthParams(*p,mapped));EXPECT_EQ(mapped.playMode,expected);
        midi.clear();notes(midi,8,false);process(*p,audio,midi);ASSERT_TRUE(finite(audio));
    }
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;midi.addEvent(synth2::audio::MidiMessage::allSoundOff(1),0);
    process(*p,audio,midi);midi.clear();for(int b=0;b<70;++b)process(*p,audio,midi);
    std::array<int,13> primary{};ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,0,primary));
    EXPECT_EQ(primary[10],0);EXPECT_LT(audio.getMagnitude(0,0,64),1e-6f);EXPECT_LT(audio.getMagnitude(1,0,64),1e-6f);
}

TEST(HostLifecycle, CachedSourceReadsTrackLiveEditsVersionChangesAndStateRecall)
{
    auto p=processor();load(*p,patch());p->prepare(48000,64);
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);process(*p,audio,midi);midi.clear();
    const auto state=save(*p);std::array<int,99> before{},after{};
    ASSERT_TRUE(synth2::readNativePatchControls(*p,before));
    const auto edit=[&](const char* id,float raw) {
        for(int index=0;index<synth2::host::ProcessorBridge::parameterCount;++index)
            if(p->parameter(index).id==id){p->setNormalizedValue(index,stateinspection::normalized(*p,index,raw));return;}
        FAIL()<<id;
    };
    edit("p19",40);process(*p,audio,midi);ASSERT_TRUE(synth2::readNativePatchControls(*p,after));EXPECT_EQ(after[19],40);
    edit("p126",105);process(*p,audio,midi);synth2::MappedSynthSnapshot mapped;
    ASSERT_TRUE(synth2::readMappedSynthParams(*p,mapped));EXPECT_EQ(mapped.patchVersion,105);
    restore(*p,state);process(*p,audio,midi);ASSERT_TRUE(synth2::readNativePatchControls(*p,after));EXPECT_EQ(after,before);
    edit("masterVolume",.25f);process(*p,audio,midi);ASSERT_TRUE(synth2::readMappedSynthParams(*p,mapped));
    EXPECT_FLOAT_EQ(mapped.masterVolume,.25f);EXPECT_TRUE(finite(audio));
}

TEST(HostLifecycle, HostFilterEditsRestartEverySoundingAndReleasingCore)
{
    for(int index:{17,19,21,22})
    {
        SCOPED_TRACE(index);auto p=processor();
        load(*p,patch()+"15,16\n16,64\n17,95\n18,64\n19,32\n21,64\n28,80\n");
        p->prepare(44100,64);synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;
        notes(midi,2);process(*p,audio,midi);midi.clear();
        for(int i=0;i<64;++i)process(*p,audio,midi);
        midi.addEvent(synth2::audio::MidiMessage::noteOff(1,36),0);process(*p,audio,midi);midi.clear();
        std::array<std::array<int,13>,2> before{},after{};
        for(size_t slot=0;slot<2;++slot){ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,slot,before[slot]));ASSERT_NE(before[slot][10],0);ASSERT_GT(before[slot][6],64);}
        ASSERT_NE(before[0][10],before[1][10]);
        p->setNormalizedValue(index,80./127.);
        process(*p,audio,midi);
        for(size_t slot=0;slot<2;++slot){ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,slot,after[slot]));EXPECT_EQ(after[slot][6],64);EXPECT_EQ(after[slot][7],before[slot][5]);}
    }
}

TEST(HostLifecycle, FilterSetterSuppressionKeepsFractionalAndInterveningEdits)
{
    auto p=processor();load(*p,patch()+"19,32\n21,95\n16,64\n28,80\n");p->prepare(44100,64);
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);process(*p,audio,midi);midi.clear();
    const auto edit=[&](int index,double value){p->setNormalizedValue(index,value/127.);};
    const auto elapsed=[&](){std::array<int,13> words{};if(!synth2::readPhysicalCoreEnvelopeWords(*p,0,words))throw std::runtime_error("Missing core");return words[6];};
    edit(21,64.125);process(*p,audio,midi);EXPECT_EQ(elapsed(),64);
    edit(21,64.125);process(*p,audio,midi);EXPECT_EQ(elapsed(),128);
    // Two fractional edits within the same MIDI/integer dial still restart.
    edit(21,64.25);process(*p,audio,midi);EXPECT_EQ(elapsed(),64);
    edit(20,32);edit(21,64.25);process(*p,audio,midi);EXPECT_EQ(elapsed(),64);
    // Coalesced pending controls and zero-frame callbacks preserve the request.
    edit(17,90);edit(19,40);edit(22,64);edit(21,80);
    EXPECT_TRUE(p->process(static_cast<float*>(nullptr),static_cast<float*>(nullptr),0,120));
    process(*p,audio,midi);EXPECT_EQ(elapsed(),64);
    process(*p,audio,midi);EXPECT_EQ(elapsed(),128);
}

TEST(HostLifecycle, FilterDepthRoundTripRetainsItsZeroBeforeRendering)
{
    auto p=processor();load(*p,patch()+"15,0\n16,0\n17,127\n18,64\n19,32\n21,95\n28,80\n");p->prepare(44100,64);
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);process(*p,audio,midi);midi.clear();
    midi.addEvent(synth2::audio::MidiMessage::noteOff(1,36),0);process(*p,audio,midi);midi.clear();
    std::array<int,13> before{},after{};ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,0,before));ASSERT_EQ(before[0],4);ASSERT_GT(before[1],0);
    const auto original=p->normalizedValue(21);
    p->setNormalizedValue(21,63./127.);p->setNormalizedValue(21,95./127.);
    EXPECT_EQ(p->normalizedValue(21),original);
    EXPECT_TRUE(p->process(static_cast<float*>(nullptr),static_cast<float*>(nullptr),0,120));
    process(*p,audio,midi);ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,0,after));
    EXPECT_EQ(after[0],0);EXPECT_EQ(after[1],0);EXPECT_EQ(p->droppedFilterControlEdits(),0u);
}

TEST(HostLifecycle, FilterEditsPrecedeTheSameCallbackNoteOff)
{
    for(bool cutoffToo:{false,true})
    {
        auto p=processor();load(*p,patch()+"15,0\n16,0\n17,127\n18,64\n19,32\n21,95\n28,80\n");p->prepare(44100,64);
        synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);process(*p,audio,midi);midi.clear();
        p->setNormalizedValue(21,64./127.);if(cutoffToo)p->setNormalizedValue(19,80./127.);
        midi.addEvent(synth2::audio::MidiMessage::noteOff(1,36),0);process(*p,audio,midi);
        std::array<int,13> words{};ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*p,0,words));
        // Depth64 is 1/64 of the 1024-level range. Release must capture the
        // edited level at most16, rather than the preceding depth95 level512.
        EXPECT_EQ(words[0],4);EXPECT_GT(words[1],0);EXPECT_LE(words[1],16);
    }
}

TEST(HostLifecycle, FilterHistoryReportsCapacityAndRecoversAfterDrain)
{
    auto p=processor();load(*p,patch()+"19,32\n21,95\n");p->prepare(44100,64);
    for(int i=0;i<4200;++i)p->setNormalizedValue(21,(i%2 ? 95. : 64.)/127.);
    EXPECT_GT(p->droppedFilterControlEdits(),0u);const auto dropped=p->droppedFilterControlEdits();
    synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;notes(midi,1);process(*p,audio,midi);EXPECT_TRUE(finite(audio));midi.clear();
    p->setNormalizedValue(21,80./127.);process(*p,audio,midi);
    EXPECT_EQ(p->droppedFilterControlEdits(),dropped);EXPECT_NEAR(p->normalizedValue(21),80./127.,1e-6);
}

TEST(HostLifecycle, StateReplacementDoesNotReplayPendingFilterEdits)
{
    auto edited=processor(),clean=processor();const auto sound=patch()+"19,32\n21,95\n";
    load(*edited,sound);load(*clean,sound);edited->prepare(44100,64);clean->prepare(44100,64);
    edited->setNormalizedValue(21,63./127.);edited->setNormalizedValue(21,95./127.);load(*edited,sound);
    synth2::audio::AudioBuffer<float> a(2,64),b(2,64);synth2::audio::MidiBuffer ma,mb;notes(ma,1);mb=ma;
    process(*edited,a,ma);process(*clean,b,mb);
    for(int channel:{0,1})for(int i=0;i<64;++i)EXPECT_EQ(a.getSample(channel,i),b.getSample(channel,i));
}
