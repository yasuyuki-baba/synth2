#include "Synth2.h"
#include "IPlug_include_in_plug_src.h"
#include "ParameterSetup.h"
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace iplug;

Synth2::Synth2(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(synth2::host::ProcessorBridge::parameterCount+1, 1))
{
    for (int index=0; index<synth2::host::ProcessorBridge::parameterCount; ++index)
    {
        synth2::host::initializeHostParameter(*GetParam(index),index,processor.parameter(index));
    }
    GetParam(qualityParameter)->InitEnum("Oversampling",0,{"1x","2x","4x","8x","16x"},IParam::kFlagCannotAutomate,"Quality");
    syncParametersFromProcessor();browser.capture(processor);
#if defined(APP_API)
    const auto session=synth2::host::PresetFiles::loadStandaloneState();
    if(!session.empty()){IByteChunk chunk;chunk.PutBytes(session.data(),session.size());UnserializeState(chunk,0);}
#endif
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [this]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                            editorScale);
    };
    mLayoutFunc = [this](igraphics::IGraphics* graphics) { layoutEditor(graphics); };
#endif
}

void Synth2::syncParametersFromProcessor()
{
    for (int index=0; index<synth2::host::ProcessorBridge::parameterCount; ++index)
        GetParam(index)->SetNormalized(processor.normalizedValue(index));
    int choice=0;while((1<<choice)<processor.oversamplingFactor())++choice;
    GetParam(qualityParameter)->Set(choice);
}

void Synth2::OnReset()
{
    if (GetSampleRate() > 0 && GetBlockSize() > 0)
    {
        processor.prepare(GetSampleRate(), GetBlockSize());
        SetTailSize(static_cast<int>(std::min<double>(std::numeric_limits<int>::max(),
            std::ceil(GetSampleRate()*synth2::host::ProcessorBridge::tailLengthSeconds))));
        syncQualityLatency();
        prepared = true;
    }
}
void Synth2::OnActivate(bool active)
{
    if (!active) { processor.release(); prepared = false; }
    else if (!prepared) OnReset();
}
void Synth2::syncQualityLatency()
{
    const int next=processor.latencySamples();
    if(next!=reportedLatency){reportedLatency=next;SetLatency(next);}
}
void Synth2::OnParamChange(int index)
{
    if(index==qualityParameter)processor.setOversamplingFactor(1<<GetParam(index)->Int());
    else processor.setNormalizedValue(index, GetParam(index)->GetNormalized());
}
void Synth2::OnParamReset(EParamSource source)
{
    // iPlug2 broadcasts resets at initialization/activation. Replaying every
    // unchanged source setter would change native LFO/load history. Restore
    // already synchronizes DSP and parameters transactionally below.
    for (int index=0; index<synth2::host::ProcessorBridge::parameterCount; ++index) OnParamChangeUI(index, source);
}
void Synth2::ProcessMidiMsg(const IMidiMsg& message)
{
    processor.enqueueMidi(message.mOffset, message.mStatus, message.mData1, message.mData2);
}
void Synth2::ProcessBlock(sample**, sample** outputs, int frames)
{
    processor.process(outputs ? outputs[0] : nullptr, outputs ? outputs[1] : nullptr, frames, GetTempo());
}

synth2::host::PresetBrowserState Synth2::captureBrowser() const
{
    auto state=browser;state.query=libraryFilter.query;state.bankId=libraryFilter.bankId;state.tag=libraryFilter.tag;state.selectedTags=libraryFilter.selectedTags;
    state.favoritesOnly=libraryFilter.favoritesOnly;state.restricted=libraryFilter.registeredOnly;
    state.importedIds=libraryFilter.registeredIds;state.selectedId=selectedPatchId;state.patchName=currentPatch;
    state.scroll=static_cast<int>(resultOffset);state.page=page;return state;
}
void Synth2::restoreBrowser()
{
    libraryFilter.query=browser.query;libraryFilter.bankId=browser.bankId;libraryFilter.tag.clear();libraryFilter.selectedTags=browser.selectedTags;
    if(libraryFilter.selectedTags.empty() && !browser.tag.empty())libraryFilter.selectedTags.push_back(browser.tag);
    libraryFilter.favoritesOnly=browser.favoritesOnly;libraryFilter.registeredOnly=browser.restricted;
    libraryFilter.registeredIds=browser.importedIds;selectedPatchId=browser.selectedId;currentPatch=browser.patchName;
    resultOffset=browser.scroll;page=browser.page;
}
Synth2::~Synth2()
{
    if(importPrompt){{std::lock_guard lock(importPrompt->mutex);importPrompt->alive=false;}importPrompt->changed.notify_all();}
#if defined(APP_API)
    try {std::string error;synth2::host::PresetFiles::saveStandaloneState(synth2::host::savePresetSession(processor,captureBrowser()),error);}catch(const std::exception&){}
#endif
}
bool Synth2::SerializeState(IByteChunk& chunk) const
{
    const auto bytes=synth2::host::savePresetSession(processor,captureBrowser());chunk.PutBytes(bytes.data(),bytes.size());return true;
}
int Synth2::UnserializeState(const IByteChunk& chunk,int start)
{
    if(start<0 || start>chunk.Size())return -1;
    const int n=synth2::host::restorePresetSession(chunk.GetData()+start,chunk.Size()-start,processor,browser);
    if(n<0)return -1;
    browserGeneration.fetch_add(1);restoreBrowser();syncParametersFromProcessor();syncQualityLatency();stateRestored.store(true);return start+n;
}
