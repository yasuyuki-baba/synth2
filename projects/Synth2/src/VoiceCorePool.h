#pragma once
#include "VoiceCore.h"
#include <array>
#include <cstdint>

namespace synth2
{
    class SynthVoice;

    // Physical state survives logical note retirement and group boundaries.
    class VoiceCorePool
    {
    public:
        void prepare (const ::synth2::audio::ProcessSpec&,bool preservePhase=false);
        void updateSampleRate (const ::synth2::audio::ProcessSpec&);
        void setAudioFactor(double rate,int factor) noexcept
        {for(auto& core:slots)core.setAudioFactor(rate,factor);}
        void requestPhaseReset() noexcept;
        void setPhaseDial (int) noexcept;
        void setGroupCount (int) noexcept;
        void limitCapacity (int) noexcept;
        SynthVoice* primaryOwner() const noexcept {return owners[0];}
        void setOsc2Coarse(float exponent,bool refreshFrequency) noexcept
        {for(auto& core:slots){core.coarse.set(exponent,core.modulationEnvelope.isActive());if(refreshFrequency)core.coarse.refresh();}}
        void restartFilterCutoff() noexcept
        {for(auto& core:slots)core.cutoff.restart();}
        void setControllerDial(int destination,int rate) noexcept
        {
            if(destination==19 || destination==21 || destination==22 || destination==17)restartFilterCutoff();
            if(destination==45)for(auto& core:slots)core.fm.setBase(rate);
        }
        VoiceCore* claim (SynthVoice&, int capacity,int fixedSlot=-1);
        void release (SynthVoice&, VoiceCore&) noexcept;
        // Offline inspection; serialize with rendering and pool mutations.
        bool readCoreWords(size_t index,std::array<uint32_t,9>& words) const noexcept
        {
            if(index>=slots.size())return false;
            const auto& core=slots[index];const auto phases=core.oscillator.readPhaseWords();
            const auto& env=core.modulationEnvelope;
            words={{phases[0],phases[1],phases[2],phases[3],static_cast<uint32_t>(env.getPhase()),
                static_cast<uint32_t>(env.getLevel()),static_cast<uint32_t>(env.getCounter()),
                static_cast<uint32_t>(env.getBoundary()),static_cast<uint32_t>(env.getStep())}};
            return true;
        }
        bool readCoreEnvelopeWords(size_t index,std::array<int,13>& words) const noexcept
        {
            if(index>=slots.size())return false;
            const auto& core=slots[index];const auto& env=core.filterEnvelope;
            words={{static_cast<int>(env.getPhase()),static_cast<int>(env.getLevel()),env.getCounter(),env.getBoundary(),env.getStep(),
                static_cast<int>(core.cutoff.getCurrent()),core.cutoff.getElapsed(),static_cast<int>(core.cutoff.getAnchor()),static_cast<int>(core.keyboardOffset),static_cast<int>(core.unisonOffset),
                static_cast<int>(core.amplitude.getPhase()),static_cast<int>(core.amplitude.getLevel()),core.amplitude.getCounter()}};
            return true;
        }
        bool readCoreCoarseWords(size_t index,std::array<uint32_t,5>& words) const noexcept
        {
            if(index>=slots.size())return false;
            const auto& state=slots[index].coarse;
            const std::array<float,3> floats{static_cast<float>(state.current),static_cast<float>(state.target),static_cast<float>(state.anchor)};
            std::memcpy(words.data(),floats.data(),sizeof(floats));
            words[3]=static_cast<uint32_t>(state.elapsed);words[4]=state.pending ? 1u : 0u;return true;
        }
        bool readCoreFmWords(size_t index,std::array<uint32_t,7>& words) const noexcept
        {
            if(index>=slots.size())return false;
            const auto& fm=slots[index].fm;
            words={static_cast<uint32_t>(static_cast<int64_t>(fm.current)),static_cast<uint32_t>(static_cast<int64_t>(fm.anchor)),static_cast<uint32_t>(fm.remaining),
                static_cast<uint32_t>(static_cast<int64_t>(fm.target)),static_cast<uint32_t>(fm.settle),static_cast<uint32_t>(fm.duration),static_cast<uint32_t>(fm.elapsed)};return true;
        }
        bool readCoreFloatWords(size_t index,std::array<uint32_t,22>& words) const noexcept
        {
            if(index>=slots.size())return false;
            words=slots[index].readFloatWords();
            return true;
        }
    private:
        std::array<VoiceCore,32> slots;
        std::array<SynthVoice*,32> owners {};
        std::array<uint64_t,32> order {};
        uint64_t serial {};
        int previous {-1};
        int groupCount {-1};
        int capacity {-1};
    };

}
