#include <gtest/gtest.h>
#include "editor/ComputerKeyboard.h"
#include "host/EditorMidiInput.h"
#include "host/ProcessorBridge.h"
#include <array>
#include <string_view>
#include <vector>

using synth2::editor::ComputerKeyboard;
using namespace synth2::host;
using synth2::audio::MidiMessage;

TEST(EditorKeyboard, MatchesCheapSynthKeysAndIgnoresRepeatAndShortcutPresses) {
    ComputerKeyboard keyboard;std::vector<std::pair<int,int>> events;
    const auto send=[&](int note,int velocity){events.emplace_back(note,velocity);};
    constexpr std::string_view keys="awsedftgyhujkol";
    for(int i=0;i<15;++i) {
        EXPECT_TRUE(keyboard.key(keys[i],false,false,send));
        EXPECT_EQ(events.back(),(std::pair<int,int>{48+i,127}));
        const auto count=events.size();keyboard.key(keys[i],false,false,send);EXPECT_EQ(events.size(),count);
    }
    for(int i=0;i<15;++i) {
        EXPECT_TRUE(keyboard.key(keys[i],true,true,send)); // Modifier added while held.
        EXPECT_EQ(events.back(),(std::pair<int,int>{48+i,0}));
    }
    const auto count=events.size();
    EXPECT_FALSE(keyboard.key('A',false,true,send));
    EXPECT_FALSE(keyboard.key('Q',false,false,send));
    EXPECT_FALSE(keyboard.key(0x141,false,false,send));
    EXPECT_EQ(events.size(),count);
}

TEST(EditorKeyboard, OctaveChangesReleaseOriginalPitchesAndAreBoundedWithoutRepeat) {
    ComputerKeyboard keyboard;std::vector<std::pair<int,int>> events;
    const auto send=[&](int note,int velocity){events.emplace_back(note,velocity);};
    keyboard.key('A',false,false,send);keyboard.key('X',false,false,send);
    EXPECT_EQ(events.back(),(std::pair<int,int>{48,0}));EXPECT_EQ(keyboard.baseNote(),60);
    keyboard.key('X',false,false,send);EXPECT_EQ(keyboard.baseNote(),60);
    keyboard.key('A',false,false,send);EXPECT_EQ(events.size(),2u); // Held-key repeat after octave change.
    keyboard.key('A',true,false,send);EXPECT_EQ(events.size(),2u);
    keyboard.key('X',true,true,send);keyboard.key('X',false,false,send);
    EXPECT_EQ(keyboard.baseNote(),72); // Modified key-up cannot leave the octave key stuck.
    for(int i=0;i<20;++i) { keyboard.key('X',true,false,send);keyboard.key('X',false,false,send); }
    EXPECT_EQ(keyboard.baseNote(),96);
    for(int i=0;i<20;++i) { keyboard.key('Z',true,false,send);keyboard.key('Z',false,false,send); }
    EXPECT_EQ(keyboard.baseNote(),24);
    keyboard.key('L',false,false,send);keyboard.release(send);
    EXPECT_EQ(events.back(),(std::pair<int,int>{38,0}));
    const auto count=events.size();keyboard.release(send);EXPECT_EQ(events.size(),count);
}

TEST(EditorKeyboard, MouseAndComputerShareNotesUntilTheLastOwnerReleases) {
    EditorMidiInput input;std::vector<MidiMessage> events;
    const auto send=[&](const auto& message){events.push_back(message);};
    input.set(60,127,EditorNoteSource::computer);input.consume(send);
    ASSERT_EQ(events.size(),1u);EXPECT_TRUE(events.back().isNoteOn());
    input.set(60,80,EditorNoteSource::mouse);input.consume(send);EXPECT_EQ(events.size(),1u);
    input.set(60,0,EditorNoteSource::computer);input.consume(send);
    EXPECT_TRUE(input.pressed(60));EXPECT_EQ(events.size(),1u);
    input.release();input.consume(send);ASSERT_EQ(events.size(),2u);EXPECT_TRUE(events.back().isNoteOff());
    input.consume(send);EXPECT_EQ(events.size(),2u);
    // A press released before the next callback cannot leave a queued note-on.
    input.set(62,127,EditorNoteSource::computer);input.release();input.consume(send);
    EXPECT_EQ(events.size(),2u);
}

TEST(EditorKeyboard, EditorReleaseKeepsExternalNotesAndExternalReleaseKeepsEditorNotes) {
    for(bool editorFirst:{false,true}) {
        EditorMidiInput input;std::vector<MidiMessage> events;
        const auto send=[&](const auto& message){events.push_back(message);};
        if(editorFirst) { input.set(60,127,EditorNoteSource::computer);input.consume(send); }
        input.host(MidiMessage(0x91,60,95),send);
        if(!editorFirst) { input.set(60,127,EditorNoteSource::computer);input.consume(send); }
        const auto count=events.size();
        if(editorFirst) {
            input.host(MidiMessage(0x81,60,0),send);EXPECT_EQ(events.size(),count);
            input.release();input.consume(send);ASSERT_EQ(events.size(),count+1);EXPECT_TRUE(events.back().isNoteOff());
        } else {
            input.release();input.consume(send);EXPECT_EQ(events.size(),count);
            input.host(MidiMessage(0x81,60,0),send);ASSERT_EQ(events.size(),count+1);EXPECT_EQ(events.back().getChannel(),2);
        }
    }
}

TEST(EditorKeyboard, ExternalMidiWithoutEditorOwnersKeepsEveryMessageAndChannel) {
    EditorMidiInput input;std::vector<MidiMessage> events;
    const std::array<MidiMessage,7> messages{{{0x90,60,100},{0x91,60,80},{0x80,60,0},
        {0x81,60,0},{0x81,60,0},{0xb1,64,127},{0xb1,123,0}}};
    for(const auto& message:messages)input.host(message,[&](const auto& value){events.push_back(value);});
    ASSERT_EQ(events.size(),messages.size());
    for(std::size_t i=0;i<events.size();++i) {
        EXPECT_EQ(events[i].getChannel(),messages[i].getChannel());
        EXPECT_EQ(events[i].getNoteNumber(),messages[i].getNoteNumber());
        EXPECT_EQ(events[i].getVelocity(),messages[i].getVelocity());
        EXPECT_EQ(events[i].isNoteOff(),messages[i].isNoteOff());
    }
}

TEST(EditorKeyboard, BridgeMergesEditorNotesWithTimestampedExternalMidiWithoutChangingAudio) {
    ProcessorBridge actual,reference;std::string error;
    for(auto* processor:{&actual,&reference}) {
        ASSERT_TRUE(processor->loadPatch("Keyboard test\nver=113\n19,97\n29,87\n",error))<<error;
        processor->prepare(44100,64);
    }
    actual.setEditorNote(60,127,EditorNoteSource::computer);
    reference.enqueueMidi(0,0x90,60,127);
    std::array<float,64> left{},right{},expectedLeft{},expectedRight{};
    for(int block=0;block<20;++block) {
        if(block==0) { actual.enqueueMidi(13,0x91,60,95);reference.enqueueMidi(13,0x91,60,95); }
        if(block==2)actual.releaseEditorNotes(); // External key still held: no note-off.
        if(block==4) { actual.enqueueMidi(29,0x81,60,0);reference.enqueueMidi(29,0x81,60,0); }
        ASSERT_TRUE(actual.process(left.data(),right.data(),64,120));
        ASSERT_TRUE(reference.process(expectedLeft.data(),expectedRight.data(),64,120));
        EXPECT_EQ(left,expectedLeft)<<block;EXPECT_EQ(right,expectedRight)<<block;
    }
}

TEST(EditorKeyboard, ZeroFrameCallbacksDeferEditorTransitionsAndOverflowReleasesSafely) {
    ProcessorBridge processor;processor.prepare(44100,64);
    processor.setEditorNote(60,127,EditorNoteSource::computer);
    ASSERT_TRUE(processor.process(static_cast<float*>(nullptr),nullptr,0,120));
    processor.releaseEditorNotes();
    std::array<float,64> left{},right{};
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    EXPECT_FALSE(processor.editorNotePressed(60));
    processor.setEditorNote(60,127,EditorNoteSource::computer);
    for(std::size_t i=0;i<ProcessorBridge::midiCapacity;++i)ASSERT_TRUE(processor.enqueueMidi(0,0x90,61,80));
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    EXPECT_EQ(processor.droppedMidiMessages(),1u);
    processor.releaseEditorNotes();
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    EXPECT_FALSE(processor.editorNotePressed(60));
}
