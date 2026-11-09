#include <gtest/gtest.h>
#include "dsp/AudioOversampler.h"
#include "host/ProcessorBridge.h"
#include "SynthEngine.h"
#include "support/StateDocument.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

TEST(Oversampling, ImpulseDelayMatchesReportedLatencyAndDCGainStaysUnity)
{
    for(int factor:synth2::dsp::OversamplingSettings::factors)
    {
        SCOPED_TRACE(factor);
        synth2::dsp::AudioDownsampler voice;
        synth2::dsp::StereoOversampler output;
        voice.setFactor(factor);output.setFactor(factor);
        std::vector<double> impulse(512);
        for(int frame=0;frame<512;++frame)
        {
            std::array<double,16> input{};
            if(frame==0)input[0]=static_cast<float>(factor);
            const auto value=output.process(voice.process(input),0,[](double*,double*,int){});
            impulse[frame]=value[0];
        }
        const auto peak=std::max_element(impulse.begin(),impulse.end());
        EXPECT_EQ(std::distance(impulse.begin(),peak),synth2::dsp::OversamplingSettings::latency(factor));
        float steady=0;
        for(int frame=0;frame<1024;++frame)
        {
            std::array<double,16> input{};input.fill(1.f);
            steady=output.process(voice.process(input),1,[](double*,double*,int){})[0];
        }
        EXPECT_NEAR(steady,1.f,2e-5f);
    }
}

TEST(Oversampling, CubicShapingSuppressesFoldedThirdHarmonicAtEveryHigherFactor)
{
    constexpr double rate=44100,frequency=12000,alias=8100,pi=3.14159265358979323846;
    double reference=0;
    for(int factor:synth2::dsp::OversamplingSettings::factors)
    {
        SCOPED_TRACE(factor);
        synth2::dsp::StereoOversampler sampler;sampler.setFactor(factor);
        double re=0,im=0;
        for(int frame=0;frame<12288;++frame)
        {
            const float input=static_cast<float>(std::sin(2*pi*frequency*frame/rate));
            const float value=sampler.process(input,input,[](double* l,double* r,int n) {
                for(int i=0;i<n;++i){l[i]=l[i]*l[i]*l[i];r[i]=r[i]*r[i]*r[i];}
            })[0];
            if(frame<4096)continue;
            const double window=.5-.5*std::cos(2*pi*(frame-4096)/8191);
            re+=window*value*std::cos(2*pi*alias*frame/rate);
            im+=window*value*std::sin(2*pi*alias*frame/rate);
        }
        const double magnitude=std::hypot(re,im);
        if(factor==1){reference=magnitude;ASSERT_GT(reference,100);}
        else EXPECT_LT(magnitude,reference*.02);
    }
}

TEST(Oversampling, StateRetainsQualityWithoutChangingPatchOrParameterIdentities)
{
    auto p=std::make_unique<synth2::host::ProcessorBridge>();
    const auto patch=p->savePatch();const auto old=p->saveState();
    EXPECT_EQ(p->oversamplingFactor(),1);EXPECT_EQ(p->latencySamples(),0);
    EXPECT_EQ(stateinspection::attribute(*stateinspection::document(*p),"oversamplingFactor"),"");
    for(int factor:synth2::dsp::OversamplingSettings::factors)
    {
        ASSERT_TRUE(p->setOversamplingFactor(factor));
        const auto bytes=p->saveState();auto restored=std::make_unique<synth2::host::ProcessorBridge>();
        ASSERT_TRUE(restored->tryRestoreState(bytes.data(),static_cast<int>(bytes.size())));
        EXPECT_EQ(restored->oversamplingFactor(),factor);
        EXPECT_EQ(restored->savePatch(),patch);EXPECT_EQ(restored->parameterCount,129);
        std::string error;ASSERT_TRUE(restored->loadPatch(patch,error));
        EXPECT_EQ(restored->oversamplingFactor(),factor);
    }
    ASSERT_TRUE(p->tryRestoreState(old.data(),static_cast<int>(old.size())));EXPECT_EQ(p->oversamplingFactor(),1);
    ASSERT_TRUE(p->setOversamplingFactor(4));const auto before=p->saveState();
    for(const char* invalid:{"0","3","32","nan","2.5","-2"})
    {
        auto xml=stateinspection::document(*p);xml->RootElement()->SetAttribute("oversamplingFactor",invalid);
        const auto bytes=stateinspection::bytes(*xml);
        EXPECT_FALSE(p->tryRestoreState(bytes.data(),static_cast<int>(bytes.size())));EXPECT_EQ(p->saveState(),before);
    }
    EXPECT_FALSE(p->setOversamplingFactor(3));EXPECT_EQ(p->oversamplingFactor(),4);
}

TEST(Oversampling, EveryFactorKeepsControlClocksAndEnvelopeTimeAtTheHostRate)
{
    for(int factor:{2,4,8,16})
    {
        SCOPED_TRACE(factor);
        auto base=std::make_unique<synth2::host::ProcessorBridge>();
        auto high=std::make_unique<synth2::host::ProcessorBridge>();
        const std::string patch="Quality clocks\nver=113\n0,0\n1,4\n25,60\n28,50\n57,1\n41,4\n42,5\n43,60\n44,90\n59,1\n31,2\n33,11\n34,90\n";
        std::string error;ASSERT_TRUE(base->loadPatch(patch,error));ASSERT_TRUE(high->loadPatch(patch,error));
        ASSERT_TRUE(high->setOversamplingFactor(factor));base->prepare(44100,64);high->prepare(44100,64);
        std::array<double,64> a{},b{},c{},d{};
        ASSERT_TRUE(base->enqueueMidi(7,0x90,60,100));ASSERT_TRUE(high->enqueueMidi(7,0x90,60,100));
        double energy=0;
        for(int block=0;block<80;++block)
        {
            ASSERT_TRUE(base->process(a.data(),b.data(),64,120));ASSERT_TRUE(high->process(c.data(),d.data(),64,120));
            for(float value:c){ASSERT_TRUE(std::isfinite(value));energy+=double(value)*value;}
            for(int slot=0;slot<2;++slot)
            {
                const auto& x=base->engineForInspection().getNativeLfoClock(slot);
                const auto& y=high->engineForInspection().getNativeLfoClock(slot);
                EXPECT_EQ(x.counter,y.counter);EXPECT_EQ(x.period,y.period);
            }
            for(size_t core=0;core<32;++core)
            {
                std::array<int,13> x{},y{};
                ASSERT_TRUE(base->engineForInspection().readCoreEnvelopeWords(core,x));
                ASSERT_TRUE(high->engineForInspection().readCoreEnvelopeWords(core,y));EXPECT_EQ(x,y);
            }
        }
        EXPECT_GT(energy,1e-8);
    }
}

TEST(Oversampling, AllFactorsKeepTheAudibleFundamentalAndDrainNoteOff)
{
    double referenceFrequency=0;
    for(int factor:synth2::dsp::OversamplingSettings::factors)
    {
        SCOPED_TRACE(factor);
        auto processor=std::make_unique<synth2::host::ProcessorBridge>();
        std::string error;
        ASSERT_TRUE(processor->loadPatch("Quality tone\nver=113\n0,0\n5,0\n25,0\n26,0\n27,127\n28,0\n19,127\n21,64\n20,0\n23,0\n57,0\n58,0\n59,0\n65,0\n66,0\n77,0\n73,0\n9,0\n",error));
        // Exercise the continuous format as well as the legacy loading adapter.
        const auto continuous=processor->savePatch();ASSERT_TRUE(processor->loadPatch(continuous,error));
        ASSERT_TRUE(processor->setOversamplingFactor(factor));processor->prepare(44100,128);
        ASSERT_TRUE(processor->enqueueMidi(0,0x90,69,100));
        std::array<double,128> left{},right{};std::vector<double> crossings;float previous=0;
        for(int block=0;block<120;++block)
        {
            ASSERT_TRUE(processor->process(left.data(),right.data(),128,120));
            for(int i=0;i<128;++i)
            {
                const float value=left[i];ASSERT_TRUE(std::isfinite(value));
                if(block>30 && previous<0 && value>=0)
                    crossings.push_back(block*128+i-1+(-previous)/(value-previous));
                previous=value;
            }
        }
        ASSERT_GT(crossings.size(),20u);
        const double frequency=(crossings.size()-1)*44100/(crossings.back()-crossings.front());
        if(factor==1)referenceFrequency=frequency;
        EXPECT_NEAR(frequency,referenceFrequency,.02);
        ASSERT_TRUE(processor->enqueueMidi(17,0x80,69,0));
        for(int block=0;block<80;++block)ASSERT_TRUE(processor->process(left.data(),right.data(),128,120));
        for(float value:left)EXPECT_LT(std::abs(value),1e-5f);
    }
}

TEST(Oversampling, FmSyncPhysicalPhasesSurviveQualityChangesAndMidiHistory)
{
    auto base=std::make_unique<synth2::host::ProcessorBridge>();
    auto high=std::make_unique<synth2::host::ProcessorBridge>();
    const std::string patch="Phase quality\nver=113\n0,0\n1,3\n2,80\n3,64\n4,1\n45,100\n6,1\n76,80\n73,1\n74,64\n25,0\n26,0\n27,127\n28,60\n19,100\n20,50\n23,70\n77,1\n78,2\n79,90\n80,64\n81,80\n59,0\n57,0\n58,0\n65,0\n66,0\n9,0\n";
    std::string error;ASSERT_TRUE(base->loadPatch(patch,error));ASSERT_TRUE(high->loadPatch(patch,error));
    ASSERT_TRUE(high->setOversamplingFactor(16));base->prepare(48000,64);high->prepare(48000,64);
    std::array<double,64> a{},b{},c{},d{};double energy=0;
    for(int block=0;block<240;++block)
    {
        auto event=[&](int offset,int status,int note,int value){ASSERT_TRUE(base->enqueueMidi(offset,status,note,value));ASSERT_TRUE(high->enqueueMidi(offset,status,note,value));};
        if(block==0)event(7,0x90,72,100);
        if(block==25)event(11,0xe0,0,80);
        if(block==40)event(19,0x90,84,100);
        if(block==60)event(23,0x80,72,0);
        if(block==90)event(3,0xb0,1,100);
        if(block==120)event(31,0x80,84,0);
        if(block==130)event(9,0x90,36,110);
        if(block==170)event(1,0x80,36,0);
        if(block%30==0)ASSERT_TRUE(high->setOversamplingFactor((std::array<int,5>{1,2,4,8,16})[(block/30)%5]));
        ASSERT_TRUE(base->process(a.data(),b.data(),64,120));ASSERT_TRUE(high->process(c.data(),d.data(),64,120));
        for(size_t core=0;core<32;++core){std::array<uint32_t,9> x{},y{};ASSERT_TRUE(base->engineForInspection().readCoreWords(core,x));ASSERT_TRUE(high->engineForInspection().readCoreWords(core,y));ASSERT_EQ(x,y);}
        for(float v:c){ASSERT_TRUE(std::isfinite(v));energy+=double(v)*v;}
    }
    EXPECT_GT(energy,1e-6);
}
