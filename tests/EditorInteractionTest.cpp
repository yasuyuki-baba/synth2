#include "editor/EditorLibraryNavigation.h"
#include <gtest/gtest.h>
#include "editor/EditorInteraction.h"
#include "editor/EditorValueDisplay.h"
#include "host/ProcessorBridge.h"
#include "editor/ParameterPresentation.h"
#include <array>

using namespace synth2::editor;

namespace
{
using synth2::host::ProcessorBridge;
void submitSource(ProcessorBridge& processor,int parameter,double source)
{
    const auto& range=processor.parameter(parameter);
    processor.setNormalizedValue(parameter,(source-range.minimum)/(range.maximum-range.minimum));
}
double sourceValue(ProcessorBridge& processor,int parameter)
{
    const auto& range=processor.parameter(parameter);
    return range.minimum+processor.normalizedValue(parameter)*(range.maximum-range.minimum);
}
}

TEST(EditorInteraction, EveryPopupSelectionWritesItsSourceWordToOnlyTheBoundParameter)
{
    using namespace synth2::host;
    ProcessorBridge processor;
    for(int parameter=0;parameter<99;++parameter)
    {
        const auto choices=editorChoices(parameter);
        for(int selection=0;selection<static_cast<int>(choices.size());++selection)
        {
            SCOPED_TRACE(parameter);
            SCOPED_TRACE(selection);
            std::array<double,129> before{};
            for(int i=0;i<129;++i) before[i]=processor.normalizedValue(i);
            const auto source=editorSelectedSourceValue(choices,selection);
            ASSERT_TRUE(source); ASSERT_EQ(*source,choices[selection].value);
            submitSource(processor,parameter,*source);
            EXPECT_NEAR(sourceValue(processor,parameter),*source,1e-4);
            for(int i=0;i<129;++i) if(i!=parameter) EXPECT_EQ(processor.normalizedValue(i),before[i]);
            EXPECT_EQ(editorChoiceDisplay(false,sourceValue(processor,parameter),choices),choices[selection].label);
        }
    }
}

TEST(EditorInteraction, CancelledOrInvalidPopupSelectionsHaveNoSourceValue)
{
    using namespace synth2::host;
    for(int parameter=0;parameter<99;++parameter)
    {
        const auto choices=editorChoices(parameter);
        EXPECT_FALSE(editorSelectedSourceValue(choices,-1));
        EXPECT_FALSE(editorSelectedSourceValue(choices,static_cast<int>(choices.size())));
        EXPECT_FALSE(editorSelectedSourceValue(choices,999));
    }
}

TEST(EditorInteraction, EveryToggleHandlesHostCustomValuesAndCyclesOffOnOff)
{
    using namespace synth2::host;
    ProcessorBridge processor;
    for(int parameter=0;parameter<99;++parameter) if(editorToggle(parameter))
    {
        SCOPED_TRACE(parameter);
        submitSource(processor,parameter,63);
        for(int expected:{0,1,0})
        {
            submitSource(processor,parameter,editorToggledSourceValue(sourceValue(processor,parameter)));
            EXPECT_NEAR(sourceValue(processor,parameter),expected,1e-5);
            EXPECT_EQ(editorChoiceDisplay(true,sourceValue(processor,parameter),{}),expected ? "ON" : "OFF");
        }
    }
}

TEST(EditorInteraction, HostAutomationAndStateRecallReachChoiceAndNumericPresentation)
{
    using namespace synth2::host;
    ProcessorBridge processor,restored;
    submitSource(processor,64,4); // Chorus menu ordinal 2 is source word 4.
    submitSource(processor,9,-12);
    const auto saved=processor.saveState();
    restored.restoreState(saved.data(),static_cast<int>(saved.size()));
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"4x");
    EXPECT_EQ(editorValueDisplay(9,sourceValue(restored,9)),"-12");
    submitSource(restored,64,2);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"2x");
    EXPECT_EQ(editorChoiceDisplay(false,127,editorChoices(64)),"Custom");
}

TEST(EditorInteraction, EditingStepsDistinguishSelectorsCountsAndContinuousDials)
{
    using namespace synth2::host;
    for(int parameter : {0,1,4,14,31,33,38,40,64,86,87,88,89,93,94,96,126})
    { SCOPED_TRACE(parameter); EXPECT_TRUE(parameterUsesIntegerSteps(parameter)); }
    for(int parameter : {5,8,15,19,20,25,29,35,43,52,61,62,63,75,50,51,90,128})
    { SCOPED_TRACE(parameter); EXPECT_FALSE(parameterUsesIntegerSteps(parameter)); }
    // Sparse source words and the full legacy range are not replaced by ordinals.
    EXPECT_EQ(editorSelectedSourceValue(editorChoices(64),2),4);
    ProcessorBridge processor;
    EXPECT_EQ(processor.parameter(64).maximum,127);
    EXPECT_EQ(processor.parameter(86).minimum,-1);
    EXPECT_EQ(processor.parameter(86).maximum,65535);
    // Editing policy must not become a loading/state quantizer.
    submitSource(processor,64,3.25);
    const auto before=processor.saveState();
    ProcessorBridge restored;
    restored.restoreState(before.data(),static_cast<int>(before.size()));
    EXPECT_NEAR(sourceValue(restored,64),3.25,1e-4);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"Custom");
}

TEST(EditorInteraction, ContinuousUnitReadoutsKeepFractionalPositions)
{
    using namespace synth2::host;
    // Midpoint positions give geometric means for the logarithmic scales.
    EXPECT_EQ(editorValueDisplay(25,63.5),"118.3 ms");
    EXPECT_EQ(editorValueDisplay(28,63.5),"774.6 ms");
    EXPECT_EQ(editorValueDisplay(61,63.5),"894 Hz");
    EXPECT_EQ(editorValueDisplay(62,64.5),"0.2 dB");
    EXPECT_EQ(editorValueDisplay(63,63.5),"3.16 Q");
    EXPECT_EQ(editorValueDisplay(19,64.25),"64.25");
}

TEST(EditorLibraryNavigation, EmptyClampedAndPagedMovementNeverWraps)
{
    using namespace synth2::editor;using A=LibraryNavigation;
    EXPECT_FALSE(editorLibraryNavigation(0,16,{},0,A::next));
    EXPECT_EQ(editorLibraryNavigation(40,16,{},0,A::next),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,{},16,A::previous),16u);
    EXPECT_EQ(editorLibraryNavigation(40,16,0,0,A::previous),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,39,32,A::next),39u);
    EXPECT_EQ(editorLibraryNavigation(40,16,5,0,A::pageNext),21u);
    EXPECT_EQ(editorLibraryNavigation(40,16,5,0,A::pagePrevious),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,30,16,A::pageNext),39u);
    EXPECT_EQ(editorLibraryNavigation(40,16,30,16,A::first),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,0,0,A::last),39u);
    EXPECT_EQ(editorLibraryNavigation(3,16,12,99,A::next),2u);
}
