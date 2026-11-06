#include <gtest/gtest.h>
#include "editor/EditorLayout.h"
#include "editor/EditorPages.h"
#include "editor/EditorValueDisplay.h"
#include "editor/EditorPresetDisplay.h"
#include "patch/Synth1SourceRange.h"
#include <Synth1PatchIo.h>
#include <array>
#include <map>
#include <set>

using namespace synth2::editor;
TEST(EditorLayout, PresetDisplayIncludesAvailableBankAndPreservesEditMarker)
{
    EXPECT_EQ(editorPresetDisplay("Warm Pad", "User Bank", false), "Warm Pad / User Bank");
    EXPECT_EQ(editorPresetDisplay("Warm Pad", "User Bank", true), "Warm Pad / User Bank *");
    EXPECT_EQ(editorPresetDisplay("Init", "", false), "Init");
    EXPECT_EQ(editorPresetDisplay("Recalled Sound", "", true), "Recalled Sound *");
    EXPECT_EQ(editorPresetDisplay("同名パッチ", "バンク A", false), "同名パッチ / バンク A");
    EXPECT_NE(editorPresetDisplay("Pad", "Bank A", false), editorPresetDisplay("Pad", "Bank B", false));
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
        EXPECT_GE(panel.bounds.x,20); EXPECT_GE(panel.bounds.y,124);
        EXPECT_LE(panel.bounds.x+panel.bounds.width,1260); EXPECT_LE(panel.bounds.y+panel.bounds.height,790);
        for (std::size_t next=p+1;next<panels.size();++next) EXPECT_FALSE(overlaps(panel.bounds,panels[next].bounds));
        for (std::size_t cell=0;cell<panel.controls.size();++cell)
        {
            SCOPED_TRACE(panel.controls[cell].parameter);
            const auto bounds=editorControlBounds(panel,cell);
            EXPECT_GE(bounds.width,50); EXPECT_GE(bounds.height,44);
            EXPECT_GE(bounds.x,panel.bounds.x); EXPECT_GE(bounds.y,panel.bounds.y+40);
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
