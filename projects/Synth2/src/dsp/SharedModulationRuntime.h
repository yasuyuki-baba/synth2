#pragma once
#include "audio/Midi.h"
#include "SharedModulationSettings.h"
#include "LfoBlock.h"
#include "LfoClock.h"
#include "RandomLfoRuntime.h"
#include "LfoHistoryEvent.h"

namespace synth2::dsp
{
    // Engine-owned Facade. Only this owner prepares/advances shared sources;
    // consumers borrow stable read-only blocks and diagnostic state.
    class SharedModulationRuntime
    {
    public:
        SharedModulationRuntime()=default;
        SharedModulationRuntime(const SharedModulationRuntime&)=delete;
        SharedModulationRuntime& operator=(const SharedModulationRuntime&)=delete;
        void retimeClocks(float sampleRate);
        void prepareBlock(int capacity);
        void synchronize(const SharedModulationSettings&,float sampleRate);
        void beginChunk(int start);
        int randomBoundary(int count,const SharedModulationSettings&) const noexcept;
        void renderSpan(int start,int count,const SharedModulationSettings&);
        void trackKey(const ::synth2::audio::MidiMessage&,const SharedModulationSettings&);
        void releaseKey(int channel,int note) noexcept;
        void releaseKeys(int channel) noexcept;
        void applyHistory(const LfoHistoryEvent&,float bpm) noexcept;
        void requestReload() noexcept { reload=true; }
        void touchRate(size_t index) noexcept { slots[index].rateTouched=true; }
        bool isInitial() const noexcept { return initial; }
        int capacity() const noexcept { return sharedBlock.frameCapacity; }
        const LfoBlock& block() const noexcept { return sharedBlock; }
        const LfoClock& clock(size_t index) const noexcept { return slots[index].clock; }
        const RandomLfoRuntime& randomState() const noexcept { return random; }
        void initializeRandomHistory() noexcept { random=RandomLfoRuntime{}; }
        void invalidateRandomHistory() noexcept { random.valid=false; }
        // The audited ARP remains a consumer of this same owned CRT stream.
        CrtRandom& arpeggioRandom() noexcept { return random.random; }
    private:
        struct Slot
        {
            LfoClock clock;
            bool tempoSync {},rateTouched {};
            float previousRate {};
        };
        std::array<Slot,2> slots{};
        LfoBlock sharedBlock;
        RandomLfoRuntime random;
        MidiRoutingHistory midiRouting;
        std::array<std::array<bool,128>,16> heldKeys {};
        float previousBpm {120};
        bool initial {true},reload {},loadAlreadyApplied {};
        void updateRate(size_t slot,float dial,float bpm) noexcept;
    };
}
