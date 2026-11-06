#include "SynthDiagnostics.h"
#include "SynthEngine.h"
#include <cstring>

bool synth2::readNativeMultiEffectWords(synth2::host::ProcessorBridge& processor,std::array<uint32_t,7>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();

    const auto state=engine->getNativeMultiEffectState();
    std::memcpy(words.data(),state.data(),sizeof(words));return true;
}

bool synth2::readNativeLfoClockWords(synth2::host::ProcessorBridge& processor,size_t index,std::array<uint32_t,7>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    if(index>=2)return false;
    const auto& clock=engine->getNativeLfoClock(index);
    words={{static_cast<uint32_t>(clock.sampleRate),static_cast<uint32_t>(clock.step),0,0,
        static_cast<uint32_t>(clock.counter),static_cast<uint32_t>(clock.period),static_cast<uint32_t>(clock.rate)}};
    std::memcpy(words.data()+2,&clock.currentHz,4);std::memcpy(words.data()+3,&clock.targetHz,4);
    return true;
}

bool synth2::readNativePatchControls(synth2::host::ProcessorBridge& processor,std::array<int,99>& values) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return processor.readNativeControls(values);
}

bool synth2::readMappedSynthParams(synth2::host::ProcessorBridge& processor,RenderControls& values) noexcept
{
    const auto* engine=&processor.engineForInspection();
    values=engine->getParams();return true;
}

bool synth2::readMappedSynthParams(synth2::host::ProcessorBridge& processor,MappedSynthSnapshot& values) noexcept
{
    const auto* engine=&processor.engineForInspection();
    if(processor.mappedVersion()==0)return false;
    static_cast<RenderControls&>(values)=engine->getParams();
    values.patchVersion=processor.mappedVersion();
    values.nativeRandomLfoHistoryValid=engine->getRenderHistory().nativeRandomLfoHistoryValid;
    return true;
}

bool synth2::readPhysicalCoreWords(synth2::host::ProcessorBridge& processor,size_t index,std::array<uint32_t,9>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readCoreWords(index,words);
}

bool synth2::readRenderingCoreWords(synth2::host::ProcessorBridge& processor,size_t voice,size_t core,std::array<uint32_t,16>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readRenderingCoreWords(voice,core,words);
}

bool synth2::readRenderingCoreFloatWords(synth2::host::ProcessorBridge& processor,size_t voice,size_t core,std::array<uint32_t,22>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readRenderingCoreFloatWords(voice,core,words);
}

bool synth2::readRenderingFmWords(synth2::host::ProcessorBridge& processor,size_t voice,size_t core,std::array<uint32_t,7>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readRenderingFmWords(voice,core,words);
}

bool synth2::readRenderingStackWords(synth2::host::ProcessorBridge& processor,size_t voice,size_t core,std::array<uint32_t,17>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readRenderingStackWords(voice,core,words);
}

bool synth2::readPhysicalCoreEnvelopeWords(synth2::host::ProcessorBridge& processor,size_t index,std::array<int,13>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readCoreEnvelopeWords(index,words);
}

bool synth2::readPhysicalCoreFloatWords(synth2::host::ProcessorBridge& processor,size_t index,std::array<uint32_t,22>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();
    return engine->readCoreFloatWords(index,words);
}

bool synth2::readNativeRandomLfoWords(synth2::host::ProcessorBridge& processor,size_t slot,std::array<uint32_t,9>& words) noexcept
{
    const auto* engine=&processor.engineForInspection();if(slot>=2)return false;
    const auto& runtime=engine->getNativeRandomLfos();const auto& c=runtime.controls[slot];
    const auto& t=runtime.targets[slot];const auto bounds=runtime.ranges[slot];
    words={static_cast<uint32_t>(c.wave),static_cast<uint32_t>(c.destination),static_cast<uint32_t>(c.depth),
        static_cast<uint32_t>(bounds.first),static_cast<uint32_t>(bounds.second),static_cast<uint32_t>(static_cast<int64_t>(t.current)),
        static_cast<uint32_t>(static_cast<int64_t>(t.previous)),runtime.random.state,static_cast<uint32_t>(runtime.valid)};return true;
}

bool synth2::readNativeLfoHistoryDrops(synth2::host::ProcessorBridge& processor,uint64_t& count) noexcept
{
    const auto* engine=&processor.engineForInspection();
    count=processor.historyDrops();return true;
}

bool synth2::readNativePitchBendWords(host::ProcessorBridge& p,std::array<int,4>& values) noexcept
{values=p.engineForInspection().nativeBendWords();return true;}
bool synth2::readPhysicalCoarseWords(host::ProcessorBridge& p,size_t core,std::array<uint32_t,5>& words) noexcept
{return p.engineForInspection().readPhysicalCoarseWords(core,words);}
bool synth2::readPhysicalFmWords(host::ProcessorBridge& p,size_t core,std::array<uint32_t,7>& words) noexcept
{return p.engineForInspection().readPhysicalFmWords(core,words);}
