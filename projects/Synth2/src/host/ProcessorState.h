#pragma once

#include "ParameterInfo.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace synth2::host
{
// Serialized control/loading state, never running engine or receiver memory.
struct ProcessorState
{
    int version=3,program=0;
    bool nativeSource=false,directControls=false;
    std::array<float,processorParameterCount> values{};
    std::array<uint64_t,2> sourcePresence{},runtimeEdits{};
    std::array<int,99> initialControls{};
    unsigned missingEnvelopeFields=0;
    std::string presetText,patchName;
};

std::vector<uint8_t> encodeProcessorState(const ProcessorState&,
    const std::array<ParameterInfo,processorParameterCount>&);
std::optional<ProcessorState> decodeProcessorState(const void*,int,
    const std::array<ParameterInfo,processorParameterCount>&);
}
