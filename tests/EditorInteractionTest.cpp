#include "editor/EditorResize.h"
#include "editor/EditorDisplayScale.h"
#include "editor/EditorLibraryScroll.h"
#include "editor/EditorLibraryNavigation.h"
#include <gtest/gtest.h>
#include "editor/EditorInteraction.h"
#include "editor/EditorValueDisplay.h"
#include "host/ProcessorBridge.h"
#include "editor/ParameterPresentation.h"
#include <array>
#include <utility>

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

TEST(EditorLibraryScroll, RevealsOnlyTheNeededRowsAndClampsToAFullLastViewport)
{
    EXPECT_EQ(editorLibraryScrollOffset(0,16,8),0);
    EXPECT_EQ(editorLibraryScrollOffset(3,16,8),0);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,99),24);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,-2),0);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,2.25),2.25);
    EXPECT_EQ(editorLibraryReveal(40,16,0,15),0);
    EXPECT_EQ(editorLibraryReveal(40,16,0,16),1);
    EXPECT_EQ(editorLibraryReveal(40,16,8,7),7);
    EXPECT_EQ(editorLibraryReveal(40,16,0,39),24);
    EXPECT_EQ(editorLibraryReveal(40,16,8.5,8),8);
    EXPECT_EQ(editorLibraryReveal(40,16,8.5,24),9);
}

TEST(EditorResize, ParentDimensionsScaleTheWholeCanvasForDpiAndKeyboard)
{
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1050,1,false),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(3200,2100,2,false),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1170,1,true),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,840,1,false),1.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1280,1050,1,false),1.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1024,840,1,false),.8f);
    // Even a one-pixel height reduction must fit instead of retaining a scale
    // from the previous window size. Width and height are equally authoritative.
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1049,1,false),1049.f/840.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1500,984,1,false),984.f/840.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(100,100,1,false),.5f);
    EXPECT_FLOAT_EQ(*editorResizeScale(9000,9000,1,false),4.f);
    EXPECT_FALSE(editorResizeScale(0,840,1,false));
    EXPECT_FALSE(editorResizeScale(1280,0,1,false));
    EXPECT_FALSE(editorResizeScale(1280,840,0,false));
}

TEST(EditorResize, ParentOwnedBoundsKeepTheEffectsAndFooterVisible)
{
    // A wide tiled window must fit its height, rather than request a taller
    // parent that the window manager can refuse (issue #21).
    for (const bool keyboard : {false,true}) {
        const float logicalHeight=keyboard ? 936.f : 840.f;
        for (const float dpi : {1.f,1.5f,2.f}) {
            for (const auto bounds : {std::pair{1890,990},std::pair{900,1000},
                                      std::pair{1600,840},std::pair{1280,1050}}) {
                const int width=static_cast<int>(bounds.first*dpi);
                const int height=static_cast<int>(bounds.second*dpi);
                const float scale=*editorResizeScale(width,height,dpi,keyboard);
                EXPECT_LE(1280.f*scale*dpi,width+.001f);
                EXPECT_LE(logicalHeight*scale*dpi,height+.001f);
            }
        }
    }
}

TEST(EditorDisplayScale, AcceptsPercentagesWithoutDependingOnLocale)
{
    EXPECT_FLOAT_EQ(*editorScaleFromText("50"),.5f);
    EXPECT_FLOAT_EQ(*editorScaleFromText("400%"),4.f);
    EXPECT_FLOAT_EQ(*editorScaleFromText(" 125.5 % "),1.255f);
    EXPECT_EQ(editorScaleText(1.f),"100%");
    EXPECT_EQ(editorScaleText(1.255f),"125.5%");
    EXPECT_EQ(editorScaleText(990.f/840.f),"117.9%");
    for(const auto* text:{"","%","49.9","400.1","0","-100","nan","inf",
                          "1e100","125,5","100%%","100%garbage","100 200"})
        EXPECT_FALSE(editorScaleFromText(text))<<text;
    for(const int percent:editorScaleChoices)
        EXPECT_FLOAT_EQ(*editorScaleFromText(std::to_string(percent)+"%"),percent/100.f);
}

TEST(EditorDisplayScale, ActualScaleCanDifferFromTheRequestedPercentage)
{
    // A host/WM may keep a 1280x840 parent after the user requests 150%.
    const auto requested=*editorScaleFromText("150%");
    const auto accepted=*editorResizeScale(1280,840,1,false);
    EXPECT_GT(requested,accepted);
    EXPECT_EQ(editorScaleText(accepted),"100%");
    EXPECT_EQ(editorScaleText(*editorResizeScale(2560,1680,2,false)),"100%");
    EXPECT_EQ(editorScaleText(*editorResizeScale(1280,936,1,true)),"100%");
}
