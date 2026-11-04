#include <gtest/gtest.h>
#include "support/StateDocument.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include <clocale>
#include <array>

namespace
{
struct NumericLocaleGuard
{
    std::string previous=std::setlocale(LC_NUMERIC,nullptr);
    ~NumericLocaleGuard(){std::setlocale(LC_NUMERIC,previous.c_str());}
    bool useComma()
    {
        for(const auto* name:{"de_DE.UTF-8","de_DE.utf8","de_DE","German_Germany.1252","de-DE"})
            if(std::setlocale(LC_NUMERIC,name) && std::string(std::localeconv()->decimal_point)==",")return true;
        return false;
    }
};
}

TEST(StateRepresentation, RecallUsesDecimalPointIndependentlyOfTheHostCLocale)
{
    NumericLocaleGuard locale;
    std::setlocale(LC_NUMERIC,"C");
    std::array<std::vector<uint8_t>,3> states;
    for(int version:{1,2,3})
    {
        synth2::host::ProcessorBridge source,canonical;std::string error;
        ASSERT_TRUE(source.loadProgram("Owned locale state\nver=113\n19,91\n29,73\n",7,error));
        source.setNormalizedValue(19,.375);source.setNormalizedValue(128,.3125);
        auto xml=stateinspection::document(source);
        xml->RootElement()->SetAttribute("stateVersion",version);
        if(version!=3)xml->RootElement()->DeleteAttribute("nativeSource");
        const auto input=stateinspection::bytes(*xml);
        canonical.restoreState(input.data(),static_cast<int>(input.size()));
        ASSERT_EQ(canonical.currentProgram(),7);
        states[version-1]=canonical.saveState();
    }
    if(!locale.useComma())GTEST_SKIP()<<"A comma-decimal C locale is unavailable";
    for(const auto& state:states)
    {
        synth2::host::ProcessorBridge restored;
        restored.restoreState(state.data(),static_cast<int>(state.size()));
        EXPECT_EQ(restored.saveState(),state);
        EXPECT_EQ(restored.currentProgram(),7);
        EXPECT_STREQ(std::localeconv()->decimal_point,",");
    }
}

TEST(StateRepresentation, LegacyNumberSpellingsRemainReadableAndInvalidNumbersAreAtomic)
{
    synth2::host::ProcessorBridge source;source.setNormalizedValue(128,.3125);
    const auto expected=source.saveState();
    for(const auto* value:{"+0.3125"," \t0.3125","0x1.4p-2","3.125e-1"})
    {
        auto xml=stateinspection::document(expected);
        xml->RootElement()->LastChildElement()->SetAttribute("value",value);
        const auto bytes=stateinspection::bytes(*xml);
        synth2::host::ProcessorBridge restored;
        restored.restoreState(bytes.data(),static_cast<int>(bytes.size()));
        EXPECT_EQ(restored.saveState(),expected)<<value;
    }
    for(const auto* value:{"0.625junk","nan","inf","0,625","0.625 "})
    {
        auto xml=stateinspection::document(expected);
        xml->RootElement()->LastChildElement()->SetAttribute("value",value);
        const auto bytes=stateinspection::bytes(*xml);
        source.restoreState(bytes.data(),static_cast<int>(bytes.size()));
        EXPECT_EQ(source.saveState(),expected)<<value;
    }
}

TEST(StateRepresentation, ParameterOrderAndEquivalentNumbersPreserveNativeAndLegacyRecall)
{
    for(int version:{101,113}) for(bool scientific:{false,true})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(scientific);
        synth2::host::ProcessorBridge source,restored;
        std::string error;
        ASSERT_TRUE(source.loadProgram("Owned state\nver="+std::to_string(version)+"\n9,-12\n19,91\n29,73\n",7,error));
        source.setNormalizedValue(128,.375);
        const auto expected=source.saveState();
        auto xml=stateinspection::document(expected);
        auto* root=xml->RootElement();
        // Reversing children must not change the stable-ID binding.
        auto reversed=stateinspection::document(expected);
        auto* result=reversed->RootElement();
        result->DeleteChildren();
        for(auto* child=root->LastChildElement();child;child=child->PreviousSiblingElement())
        {
            auto* copy=child->DeepClone(reversed.get())->ToElement();
            std::ostringstream value; value.imbue(std::locale::classic());
            if(scientific) value<<std::scientific<<std::setprecision(17);
            else value<<std::fixed<<std::setprecision(17);
            value<<child->DoubleAttribute("value");
            copy->SetAttribute("value",value.str().c_str());
            result->InsertEndChild(copy);
        }
        const auto incoming=stateinspection::bytes(*reversed);
        restored.restoreState(incoming.data(),static_cast<int>(incoming.size()));
        EXPECT_EQ(restored.saveState(),expected);
        EXPECT_EQ(restored.currentProgram(),7);
        for(int i=0;i<synth2::host::ProcessorBridge::parameterCount;++i)
            EXPECT_EQ(restored.normalizedValue(i),source.normalizedValue(i))<<i;
    }
}

TEST(StateRepresentation, ReorderedUnknownOrDuplicateIdsStillRejectAtomically)
{
    synth2::host::ProcessorBridge source;
    source.setNormalizedValue(128,.375);
    const auto expected=source.saveState();
    for(bool duplicate:{false,true})
    {
        auto xml=stateinspection::document(expected);
        auto* root=xml->RootElement();
        root->LastChildElement()->SetAttribute("id",duplicate ? root->FirstChildElement()->Attribute("id") : "unknownParameter");
        root->InsertEndChild(root->FirstChildElement());
        const auto incoming=stateinspection::bytes(*xml);
        source.restoreState(incoming.data(),static_cast<int>(incoming.size()));
        EXPECT_EQ(source.saveState(),expected);
    }
}
