#include <gtest/gtest.h>
#include "editor/EditorLayout.h"
#include "editor/EditorPages.h"
#include "editor/EditorValueDisplay.h"
#include "editor/EditorPresetDisplay.h"
#include "editor/EditorBankMenu.h"
#include "editor/EditorTextFit.h"
#include "editor/EditorLibraryColumns.h"
#include "patch/Synth1SourceRange.h"
#include <Synth1PatchIo.h>
#include <array>
#include <map>
#include <set>

using namespace synth2::editor;
TEST(EditorLayout, PresetDisplaySeparatesTitleAndBankAndPreservesEditMarker)
{
    const auto display=editorPresetDisplay("Warm Pad","User Bank",true);
    EXPECT_EQ(display.title,"Warm Pad *");EXPECT_EQ(display.detail,"User Bank");
    EXPECT_EQ(display.tooltip(),"Warm Pad *\nUser Bank");
    const auto init=editorPresetDisplay("Init","",false);
    EXPECT_EQ(init.title,"Init");EXPECT_TRUE(init.detail.empty());EXPECT_EQ(init.tooltip(),"Init");
    const auto utf8=editorPresetDisplay("同名パッチ","バンク A",false);
    EXPECT_EQ(utf8.title,"同名パッチ");EXPECT_EQ(utf8.detail,"バンク A");
    EXPECT_EQ(editorPresetDisplay("Pad","Bank A",false).title,
              editorPresetDisplay("Pad","Bank B",false).title);
    EXPECT_NE(editorPresetDisplay("Pad","Bank A",false).detail,
              editorPresetDisplay("Pad","Bank B",false).detail);
}

namespace
{
bool overlaps(const EditorBounds& a,const EditorBounds& b)
{
    return a.x<b.x+b.width && b.x<a.x+a.width && a.y<b.y+b.height && b.y<a.y+a.height;
}
}

TEST(EditorLayout, SoundControlsAreExposedOnceAndSerializedMetadataStaysHidden)
{
    std::array<int,128> seen{};
    for (const auto& panel:editorPanels())
        for (const auto& control:panel.controls)
        { ASSERT_GE(control.parameter,0); ASSERT_LT(control.parameter,99); ++seen[control.parameter]; }
    for (std::size_t section=5;section<editorPages().size();++section)
        for (int index:editorPages()[section].parameters) ++seen[index];
    for (int index=0;index<128;++index) { SCOPED_TRACE(index); EXPECT_EQ(seen[index],index<99 ? 1 : 0); }
    ASSERT_EQ(editorViews.size(),3u);
    EXPECT_STREQ(editorViews[0],"Synth");EXPECT_STREQ(editorViews[1],"MIDI");
    EXPECT_STREQ(editorViews[editorLibraryView],"Library");
    for (int index : {50,51,86,87,88,89})
        for (const auto& panel:editorPanels())
            for (const auto& control:panel.controls) EXPECT_NE(control.parameter,index);
}

TEST(EditorLayout, ControlsFitWithoutCollidingWithPanelsOrTheBrowserAndKeyboard)
{
    const auto& panels=editorPanels();
    for (std::size_t p=0;p<panels.size();++p)
    {
        const auto& panel=panels[p]; SCOPED_TRACE(panel.title);
        ASSERT_FALSE(panel.controls.empty()); ASSERT_GT(panel.columns,0);
        if(std::string(panel.title)=="DELAY")
        {
            EXPECT_GE(editorDelayTimingBounds.x,panel.bounds.x+8);
            EXPECT_GE(editorDelayTimingBounds.y,panel.bounds.y+38);
            EXPECT_LE(editorDelayTimingBounds.x+editorDelayTimingBounds.width,panel.bounds.x+panel.bounds.width-8);
            EXPECT_LE(editorDelayTimingBounds.y+editorDelayTimingBounds.height,panel.bounds.y+panel.bounds.height-6);
        }
        for(const auto& divider:editorPanelDividers(panel))
        {
            EXPECT_GE(divider.width,1); EXPECT_GE(divider.height,1);
            EXPECT_GE(divider.x,panel.bounds.x+8); EXPECT_GE(divider.y,panel.bounds.y+38);
            EXPECT_LE(divider.x+divider.width,panel.bounds.x+panel.bounds.width-8);
            EXPECT_LE(divider.y+divider.height,panel.bounds.y+panel.bounds.height-6);
        }
        EXPECT_GE(panel.bounds.x,20); EXPECT_GE(panel.bounds.y,124);
        EXPECT_LE(panel.bounds.x+panel.bounds.width,1260); EXPECT_LE(panel.bounds.y+panel.bounds.height,790);
        for (std::size_t next=p+1;next<panels.size();++next) EXPECT_FALSE(overlaps(panel.bounds,panels[next].bounds));
        for (std::size_t cell=0;cell<panel.controls.size();++cell)
        {
            SCOPED_TRACE(panel.controls[cell].parameter);
            const auto bounds=editorControlBounds(panel,cell);
            EXPECT_FALSE(overlaps(bounds,editorDelayTimingBounds));
            EXPECT_GE(bounds.width,50); EXPECT_GE(bounds.height,44);
            EXPECT_GE(bounds.x,panel.bounds.x); EXPECT_GE(bounds.y,panel.bounds.y+38);
            EXPECT_LE(bounds.x+bounds.width,panel.bounds.x+panel.bounds.width);
            EXPECT_LE(bounds.y+bounds.height,panel.bounds.y+panel.bounds.height);
            for (std::size_t next=cell+1;next<panel.controls.size();++next)
                EXPECT_FALSE(overlaps(bounds,editorControlBounds(panel,next)));
        }
    }
}

TEST(EditorLayout, ChoiceLabelsFollowCanonicalPatchWaveformAndFilterCodecs)
{
    const std::map<std::string,Synth1OscillatorWaveform> waves {
        {"Sine",Synth1OscillatorWaveform::sine},{"Saw",Synth1OscillatorWaveform::saw},
        {"Pulse",Synth1OscillatorWaveform::pulse},{"Triangle",Synth1OscillatorWaveform::triangle},
        {"Noise",Synth1OscillatorWaveform::noise}
    };
    for (int index : {0,1})
        for (const auto& choice:editorChoices(index))
        {
            const auto model=importSynth1Patch(Synth1PatchDocument::makeParsed("GUI",113,{{index,choice.value}}));
            const auto actual=index==0 ? model.oscillators.osc1.shape : model.oscillators.osc2.shape;
            ASSERT_TRUE(actual.has_value()); EXPECT_EQ(*actual,waves.at(choice.label));
        }
    const std::map<std::string,Synth1FilterType> filters {
        {"LP12",Synth1FilterType::lp12},{"LP24",Synth1FilterType::lp24},{"HP12",Synth1FilterType::hp12},
        {"BP12",Synth1FilterType::bp12},{"Diode",Synth1FilterType::diode}
    };
    for (const auto& choice:editorChoices(14))
    {
        const auto model=importSynth1Patch(Synth1PatchDocument::makeParsed("GUI",113,{{14,choice.value}}));
        ASSERT_TRUE(model.filter.type.has_value()); EXPECT_EQ(*model.filter.type,filters.at(choice.label));
    }
}

TEST(EditorLayout, ChoicesRetainSourceWordsIncludingNonconsecutiveChorusValues)
{
    for (int index=0;index<128;++index)
    {
        std::set<int> seen;
        for (const auto& choice:editorChoices(index))
        {
            EXPECT_GE(choice.value,synth2::sourceMinimum(index)); EXPECT_LE(choice.value,synth2::sourceMaximum(index));
            EXPECT_TRUE(seen.insert(choice.value).second);
        }
    }
    const auto chorus=editorChoices(64); ASSERT_EQ(chorus.size(),3u);
    EXPECT_EQ(chorus[0].value,1); EXPECT_EQ(chorus[1].value,2); EXPECT_EQ(chorus[2].value,4);
    const auto osc2=editorChoices(1); ASSERT_EQ(osc2.front().value,1);
}

// Verify meaningful units against the synthesis mappings, including tuning's
// neutral band and envelope nominal endpoints. These are display-only values.
TEST(EditorLayout, UnitReadoutsFollowTimingTuningAndEqMappings)
{
    EXPECT_EQ(editorValueDisplay(25,0),"0.50 ms");
    EXPECT_EQ(editorValueDisplay(25,127),"28.00 s");
    EXPECT_EQ(editorValueDisplay(28,0),"10.00 ms");
    EXPECT_EQ(editorValueDisplay(28,127),"60.00 s");
    EXPECT_EQ(editorValueDisplay(2,0),"-60 st");
    EXPECT_EQ(editorValueDisplay(2,127),"60 st");
    for (int raw=62;raw<=66;++raw) EXPECT_EQ(editorValueDisplay(3,raw),"0 ct");
    EXPECT_EQ(editorValueDisplay(72,67),"1 ct");
    EXPECT_EQ(editorValueDisplay(61,0),"50 Hz");
    EXPECT_EQ(editorValueDisplay(61,127),"16.00 kHz");
    EXPECT_EQ(editorValueDisplay(62,64),"0.0 dB");
    EXPECT_EQ(editorValueDisplay(63,127),"10.00 Q");
    // Tempo-dependent controls and modulated cutoff must not imply fixed units.
    for (int index:{19,35,43,48,79,80})
    { EXPECT_FALSE(editorHasUnit(index)); EXPECT_EQ(editorValueDisplay(index,64),"64"); }
    EXPECT_EQ(editorValueDisplay(19,64.25),"64.25");
    EXPECT_EQ(editorValueDisplay(19,64.0000000001),"64");
    EXPECT_EQ(editorValueDisplay(9,-12.0000000001),"-12");
}

TEST(EditorBankMenu, GroupsParentsAndKeepsOnlyRealBanksSelectable)
{
    const auto tree=editorBankMenu({{"Collection.zip/Analog/Bass","Collection.zip/Analog/Bass"},
        {"Collection.zip/Analog/Lead","Collection.zip/Analog/Lead"},
        {"Collection.zip/Pads","Collection.zip/Pads"},
        {"Collection.zip/Analog","Collection.zip/Analog"},{".","Library root"}});
    ASSERT_EQ(tree.size(),2u);
    const auto& collection=tree[0];EXPECT_EQ(collection.label,"Collection.zip");
    EXPECT_TRUE(collection.bankId.empty());ASSERT_EQ(collection.children.size(),2u);
    const auto& analog=collection.children[0];EXPECT_EQ(analog.label,"Analog");
    EXPECT_EQ(analog.bankId,"Collection.zip/Analog");ASSERT_EQ(analog.children.size(),2u);
    EXPECT_EQ(analog.children[0].bankId,"Collection.zip/Analog/Bass");
    EXPECT_EQ(analog.children[1].label,"Lead");
    EXPECT_EQ(tree[1].bankId,".");
    EXPECT_TRUE(editorBankMenu({}).empty());
    const auto duplicates=editorBankMenu({{"bank-a","Same"},{"bank-b","Same"}});
    ASSERT_EQ(duplicates.size(),2u);EXPECT_NE(duplicates[0].bankId,duplicates[1].bankId);
}

TEST(EditorBankMenu, FlatRowsKeepHierarchyAndSelectableFolderTargets)
{
    const auto rows=editorBankMenuRows(editorBankMenu({{"Set/Pads","Set/Pads"},
        {"Set/Pads/Deep","Set/Pads/Deep"},{"Set/Lead","Set/Lead"}}));
    ASSERT_EQ(rows.size(),6u);
    EXPECT_EQ(rows[0].label,"All banks");EXPECT_TRUE(rows[0].selectable);EXPECT_TRUE(rows[0].bankId.empty());
    EXPECT_EQ(rows[1].label,"Set");EXPECT_FALSE(rows[1].selectable);
    EXPECT_EQ(rows[2].label,"Lead");EXPECT_EQ(rows[2].depth,1);EXPECT_EQ(rows[2].bankId,"Set/Lead");
    EXPECT_EQ(rows[3].label,"Pads");EXPECT_FALSE(rows[3].selectable);
    EXPECT_EQ(rows[4].label,"This folder");EXPECT_TRUE(rows[4].selectable);
    EXPECT_EQ(rows[4].bankId,"Set/Pads");EXPECT_EQ(rows[4].depth,2);
    EXPECT_EQ(rows[5].label,"Deep");EXPECT_EQ(rows[5].depth,2);EXPECT_TRUE(rows[5].selectable);
    const auto empty=editorBankMenuRows({});ASSERT_EQ(empty.size(),1u);EXPECT_TRUE(empty[0].selectable);
}

TEST(EditorLayout, LongLabelsFitColumnsAndKeepUtf8AndBankSuffixes)
{
    const auto measure=[](std::string_view s) {float width=0;for(unsigned char c:s)if((c&0xc0)!=0x80)width+=1;return width;};
    EXPECT_EQ(editorFitText("Short",10,measure),"Short");
    EXPECT_EQ(editorFitText("Long patch title",9,measure),"Long p...");
    EXPECT_EQ(editorFitText("Archive.zip/Folder/Bank",7,measure,true),"...Bank");
    EXPECT_EQ(editorFitText("あいうえお",5,measure),"あいうえお");
    EXPECT_EQ(editorFitText("あいうえお",4,measure),"あ...");
    EXPECT_EQ(editorFitText("あいうえお",4,measure,true),"...お");
    EXPECT_TRUE(editorFitText("Long",2,measure).empty());
    EXPECT_TRUE(editorFitText("Long",0,measure).empty());
}

TEST(EditorLayout, LibraryColumnsLeaveGuttersAndRoomForTags)
{
    using C=EditorLibraryColumns;
    EXPECT_LT(C::nameLeft,C::nameRight);
    EXPECT_LE(C::nameRight+12,C::bankLeft);
    EXPECT_LE(C::bankRight+12,C::tagsLeft);
    EXPECT_GE(C::bankRight-C::bankLeft,250);
    EXPECT_GE(1232-C::tagsLeft,500);
}

TEST(EditorLayout, HeaderTagsPreserveManualAutomaticOriginAndCompleteTooltip)
{
    const auto display=editorPresetDisplay("Warm Pad","Folder/Bank",true,{"Choice"},{"Pad","Unison"});
    ASSERT_EQ(display.tags.size(),3u);
    EXPECT_EQ(display.tags[0].text,"Choice");EXPECT_FALSE(display.tags[0].automatic);
    EXPECT_EQ(display.tags[1].text,"Pad");EXPECT_TRUE(display.tags[1].automatic);
    EXPECT_EQ(display.tags[2].text,"Unison");EXPECT_TRUE(display.tags[2].automatic);
    EXPECT_EQ(display.tooltip(),"Warm Pad *\nFolder/Bank\nChoice, Pad, Unison");
    EXPECT_TRUE(editorPresetDisplay("Init","",false).tags.empty());
    EXPECT_EQ(editorPresetDisplay("Owned","",false,{}, {"FM"}).tooltip(),"Owned\nFM");
}

TEST(EditorBankMenu, AdjacentBanksSkipHeadingsAndStopAtEitherEnd)
{
    const auto rows=editorBankMenuRows(editorBankMenu({{"Set/Pads","Pads"},
        {"Set/Pads/Deep","Deep"},{"Set/Lead","Lead"}}));
    EXPECT_FALSE(editorAdjacentBank(rows,"",-1));
    EXPECT_EQ(editorAdjacentBank(rows,"",1),"Set/Lead");
    EXPECT_EQ(editorAdjacentBank(rows,"Set/Lead",-1),"");
    EXPECT_EQ(editorAdjacentBank(rows,"Set/Lead",1),"Set/Pads");
    EXPECT_EQ(editorAdjacentBank(rows,"Set/Pads",1),"Set/Pads/Deep");
    EXPECT_EQ(editorAdjacentBank(rows,"Set/Pads/Deep",-1),"Set/Pads");
    EXPECT_FALSE(editorAdjacentBank(rows,"Set/Pads/Deep",1));
    EXPECT_FALSE(editorAdjacentBank(editorBankMenuRows({}),"",1));
    EXPECT_EQ(editorAdjacentBank(rows,"deleted",1),"Set/Lead");
}
