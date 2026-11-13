#include <gtest/gtest.h>
#include "editor/ComputerKeyboard.h"
#include <string_view>
#include <vector>

using synth2::editor::ComputerKeyboard;

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
