#pragma once

#include "ParameterInfo.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace synth2 { class SynthEngine; }

namespace synth2::host
{
// SDK-independent instrument boundary. Owns the portable engine, parameter and
// loading state directly; no plugin framework is needed by this library.
// Calls are serialized by the hosting SDK. State and patch I/O are non-RT.
class ProcessorBridge
{
public:
    // Retain the preceding host's conservative release/tail notification.
    // Effect-tail duration remains approximate; this does not truncate DSP.
    static constexpr double tailLengthSeconds = 60.0;
    static constexpr int parameterCount = processorParameterCount;
    static constexpr std::size_t midiCapacity = 4096;
    ProcessorBridge();
    ~ProcessorBridge();
    ProcessorBridge(const ProcessorBridge&) = delete;
    ProcessorBridge& operator=(const ProcessorBridge&) = delete;

    const ParameterInfo& parameter(int index) const;
    double normalizedValue(int index) const;
    void setNormalizedValue(int index, double value);
    void prepare(double sampleRate, int maximumFrames);
    void release();
    // Channel messages only; status includes the zero-based MIDI channel nibble.
    bool enqueueMidi(int offset, uint8_t status, uint8_t data1, uint8_t data2) noexcept;
    // Stereo non-interleaved output. Frames must fit the prepared capacity.
    // Invalid blocks are silenced without resizing or splitting the DSP callback.
    bool process(float* left, float* right, int frames, double tempo);
    bool process(double* left, double* right, int frames, double tempo);
    uint64_t droppedMidiMessages() const noexcept;
    uint64_t droppedFilterControlEdits() const noexcept;

    bool loadPatch(const std::string& text, std::string& error);
    // Non-RT complete Synth2 ver=200 patch in dial units, including master gain.
    std::string savePatch() const;
    // UI-side bank selection records an ordinal with the source transaction.
    // The source text in state remains sufficient for recall without the bank.
    bool loadProgram(const std::string& text, int programIndex, std::string& error,
                     const std::string& displayName = {});
    int currentProgram() const noexcept;
    std::string currentPatchName() const;
    std::vector<uint8_t> saveState() const;
    void restoreState(const void* data, int size);

    // Stopped/offline inspection only; serialize these calls with rendering.
    const synth2::SynthEngine& engineForInspection() const noexcept;
    bool readNativeControls(std::array<int,99>&) const noexcept;
    int mappedVersion() const noexcept;
    uint64_t historyDrops() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
