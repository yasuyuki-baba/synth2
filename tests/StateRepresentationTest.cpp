#include <gtest/gtest.h>
#include "support/StateDocument.h"
#include <iomanip>
#include <locale>
#include <sstream>

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
