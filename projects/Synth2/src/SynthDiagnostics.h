#pragma once
#include "host/ProcessorBridge.h"
#include <array>
#include "RenderControls.h"
#include <cstdint>

namespace synth2
{
    // Offline/stopped-processor inspection; serialize all calls with audio.
    struct MappedSynthSnapshot : RenderControls
    {
        int patchVersion {112};
        bool nativeRandomLfoHistoryValid {false};
    };
    bool readNativePatchControls(host::ProcessorBridge&,std::array<int,99>&) noexcept;
    bool readMappedSynthParams(host::ProcessorBridge&,RenderControls&) noexcept;
    bool readMappedSynthParams(host::ProcessorBridge&,MappedSynthSnapshot&) noexcept;
    // Actual logical rendering core, including generic local storage: active,
    // note, two phases, two increments, VCA phase/level/counter, filter phase/
    // level/counter, cutoff current/elapsed/anchor, physical slot (-1 if local).
    // Offline/stopped processor only; serialize with audio and loading.
    bool readRenderingCoreWords(host::ProcessorBridge&,size_t voice,size_t core,std::array<uint32_t,16>&) noexcept;
    // Physical OSC2 coarse current/target/anchor float bits, elapsed and pending.
    bool readPhysicalCoarseWords(host::ProcessorBridge&,size_t core,std::array<uint32_t,5>&) noexcept;
    // Physical FM current/anchor/remaining/target/settle/duration/elapsed.
    bool readPhysicalFmWords(host::ProcessorBridge&,size_t core,std::array<uint32_t,7>&) noexcept;
    // Same float layout as the physical pool, from the actual logical core.
    bool readRenderingCoreFloatWords(host::ProcessorBridge&,size_t voice,size_t core,std::array<uint32_t,22>&) noexcept;
    // Actual rendered core FM current/anchor/remaining/target/settle/duration/elapsed.
    bool readRenderingFmWords(host::ProcessorBridge&,size_t voice,size_t core,std::array<uint32_t,7>&) noexcept;
    // Actual OSC1 stack: depth float bits, eight base-frequency float words,
    // eight phase accumulators. Offline read only; serialize with processing.
    bool readRenderingStackWords(host::ProcessorBridge&,size_t voice,size_t core,std::array<uint32_t,17>&) noexcept;
    // Physical core: two phases, two last increments, envelope phase/level/
    // counter/boundary/stride. Read only; serialize with audio and loading.
    bool readPhysicalCoreWords(host::ProcessorBridge&,size_t index,std::array<uint32_t,9>&) noexcept;
    // Filter envelope phase/level/counter/boundary/stride; cutoff current/elapsed/
    // anchor; keyboard/external offsets; VCA phase/level/counter. Same serialization.
    bool readPhysicalCoreEnvelopeWords(host::ProcessorBridge&,size_t index,std::array<int,13>&) noexcept;
    // VCA current/target/scale/reference float bits, LP12 low/band/limiter bits,
    // LP24 eight recurrence words and six coefficient words, then the cached
    // integer VCA target. Same offline serialization contract.
    bool readPhysicalCoreFloatWords(host::ProcessorBridge&,size_t index,std::array<uint32_t,22>&) noexcept;
    // Offline/stopped-processor inspection; callers must serialize with audio.
    // Random words: native wave, destination, depth, two bounds, current/previous, CRT, valid.
    bool readNativeRandomLfoWords(host::ProcessorBridge&,size_t,std::array<uint32_t,9>&) noexcept;
    bool readNativeLfoHistoryDrops(host::ProcessorBridge&,uint64_t&) noexcept;
    bool readNativeLfoClockWords(host::ProcessorBridge&,size_t index,std::array<uint32_t,7>&) noexcept;
    bool readNativePitchBendWords(host::ProcessorBridge&,std::array<int,4>&) noexcept;
    // Offline MultiFX mix/downsample/bits, Ring oscillator words and sample counter.
    bool readNativeMultiEffectWords(host::ProcessorBridge&,std::array<uint32_t,7>&) noexcept;
}
