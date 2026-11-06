#include <gtest/gtest.h>
#include "dsp/LfoShape.h"
#include <cmath>

TEST(LfoPhasePrecision, AmplitudeAndPanInterpolateBetweenSineCoordinates)
{
    using synth2::dsp::LfoShape;
    for(int destination:{3,6})for(int dial:{64,127})
    {
        SCOPED_TRACE(::testing::Message()<<destination<<' '<<dial);
        const LfoShape::DepthCurve curve(destination,dial);
        const auto range=synth2::dsp::lfoDepthRange(destination+1,dial);
        const int depth=destination==3 ? range.first : range.second;
        bool fractional=false;
        for(int phase=0;phase<2048;++phase)
        {
            const auto a=curve.query(0,phase*2,4096,64);
            const auto b=curve.query(0,(phase*2+2)%4096,4096,64);
            const auto middle=curve.query(0,phase*2+1,4096,64);
            const int knot=destination==3 ? LfoShape::amplitude(depth,phase) : LfoShape::pan(depth,phase);
            EXPECT_EQ(a.first,knot);
            EXPECT_DOUBLE_EQ(middle.first,(a.first+b.first)*.5);
            EXPECT_EQ(middle.second,64);
            fractional|=middle.first!=std::trunc(middle.first);
        }
        EXPECT_TRUE(fractional);
    }
}

TEST(LfoPhasePrecision, OtherReceiversAndFallbackQueriesKeepCompatibleCoordinates)
{
    using synth2::dsp::LfoShape;
    for(int destination:{0,1,2,3,4,5,6})for(int wave:{0,4,5})
    {
        if(wave==0 && (destination==3 || destination==6))continue;
        const auto range=synth2::dsp::lfoDepthRange(destination+1,64);
        const int depth=destination==3 ? range.first : range.second;
        const LfoShape::DepthCurve curve(destination,64);
        for(int counter:{1,1235,4095})
        {
            const int phase=counter/2;
            const int expected=destination==3 ? LfoShape::amplitude(depth,phase)
                : destination==6 ? LfoShape::pan(depth,phase)
                : destination==2 || destination==5 ? LfoShape::filter(depth,phase)
                : LfoShape::pitch(depth,phase);
            const auto result=curve.query(wave,counter,4096,64);
            EXPECT_EQ(result.first,expected);EXPECT_EQ(result.second,64);
        }
    }
}

TEST(LfoPhasePrecision, FractionalDepthAndPhaseRemainIndependentCoordinates)
{
    using Curve=synth2::dsp::LfoShape::DepthCurve;
    for(int destination:{3,6})for(int counter:{1,1235,4095})
    {
        const auto low=Curve(destination,64).query(0,counter,4096,32);
        const auto high=Curve(destination,65).query(0,counter,4096,32);
        const auto fraction=Curve(destination,64.25).query(0,counter,4096,32);
        EXPECT_DOUBLE_EQ(fraction.first,low.first+(high.first-low.first)*.25);
        EXPECT_EQ(fraction.second,32);
    }
}

TEST(LfoPhasePrecision, SlowClockKeepsPhaseWordWrappingWithoutLosingTheRemainder)
{
    using Curve=synth2::dsp::LfoShape::DepthCurve;
    for(int destination:{3,6})
    {
        const Curve curve(destination,127);
        const auto first=curve.query(0,1,4000000,64);
        const auto wrapped=curve.query(0,(1<<21)+1,4000000,64);
        EXPECT_EQ(wrapped,first);
    }
    const auto amplitude=Curve(3,127).query(0,1,4000000,64);
    EXPECT_NE(amplitude.first,std::trunc(amplitude.first));
}
