#include "Synth2.h"
#include "IPlug_include_in_plug_src.h"
#include "ParameterSetup.h"
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace iplug;

Synth2::Synth2(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(synth2::host::ProcessorBridge::parameterCount, 1))
{
    for (int index=0; index<NParams(); ++index)
    {
        synth2::host::initializeHostParameter(*GetParam(index),index,processor.parameter(index));
    }
    syncParametersFromProcessor();
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [this]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                            GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };
    mLayoutFunc = [this](igraphics::IGraphics* graphics) { layoutEditor(graphics); };
#endif
}

void Synth2::syncParametersFromProcessor()
{
    for (int index=0; index<NParams(); ++index)
        GetParam(index)->SetNormalized(processor.normalizedValue(index));
}

void Synth2::OnReset()
{
    if (GetSampleRate() > 0 && GetBlockSize() > 0)
    {
        processor.prepare(GetSampleRate(), GetBlockSize());
        SetTailSize(static_cast<int>(std::min<double>(std::numeric_limits<int>::max(),
            std::ceil(GetSampleRate()*synth2::host::ProcessorBridge::tailLengthSeconds))));
        prepared = true;
    }
}
void Synth2::OnActivate(bool active)
{
    if (!active) { processor.release(); prepared = false; }
    else if (!prepared) OnReset();
}
void Synth2::OnParamChange(int index) { processor.setNormalizedValue(index, GetParam(index)->GetNormalized()); }
void Synth2::OnParamReset(EParamSource source)
{
    // iPlug2 broadcasts resets at initialization/activation. Replaying every
    // unchanged source setter would change native LFO/load history. Restore
    // already synchronizes DSP and parameters transactionally below.
    for (int index=0; index<NParams(); ++index) OnParamChangeUI(index, source);
}
void Synth2::ProcessMidiMsg(const IMidiMsg& message)
{
    processor.enqueueMidi(message.mOffset, message.mStatus, message.mData1, message.mData2);
}
void Synth2::ProcessBlock(sample**, sample** outputs, int frames)
{
    processor.process(outputs ? outputs[0] : nullptr, outputs ? outputs[1] : nullptr, frames, GetTempo());
}

bool Synth2::SerializeState(IByteChunk& chunk) const
{
    const auto bytes = processor.saveState();
    const uint32_t magic = 0x32504953; // SIP2, format version 1
    const uint32_t version = 1;
    const uint32_t size = static_cast<uint32_t>(bytes.size());
    chunk.Put(&magic); chunk.Put(&version); chunk.Put(&size);
    chunk.PutBytes(bytes.data(), static_cast<int>(size));
    return true;
}
int Synth2::UnserializeState(const IByteChunk& chunk, int start)
{
    uint32_t magic=0, version=0, size=0;
    int position = chunk.Get(&magic, start);
    if (position < 0) return -1;
    position = chunk.Get(&version, position);
    if (position < 0) return -1;
    position = chunk.Get(&size, position);
    if (position < 0 || magic != 0x32504953 || version != 1 || size == 0 || size > 4*1024*1024
        || position > chunk.Size() || size > static_cast<uint32_t>(chunk.Size()-position)) return -1;
    processor.restoreState(chunk.GetData()+position, static_cast<int>(size));
    syncParametersFromProcessor();
    stateRestored.store(true);
    return position + static_cast<int>(size);
}
