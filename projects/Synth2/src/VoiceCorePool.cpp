#include "VoiceCorePool.h"
#include "SynthVoice.h"
#include <algorithm>

namespace synth2
{
    void VoiceCorePool::prepare (const ::synth2::audio::ProcessSpec& spec,bool preservePhase)
    {
        owners.fill (nullptr); order.fill (0); serial = 0; previous = groupCount = capacity = -1;
        for (int i = 0; i < 32; ++i) { slots[i].prepare(spec,preservePhase); slots[i].physicalSlot = i; }
    }

    void VoiceCorePool::updateSampleRate (const ::synth2::audio::ProcessSpec& spec)
    {
        for (auto& core : slots)
        {
            core.oscillator.prepare(spec);
            core.filter.setSampleRate(spec.sampleRate);
            core.cutoff.setSampleRate(static_cast<int>(spec.sampleRate));
            core.coarse.elapsed=static_cast<int>(spec.sampleRate)/20;
        }
    }

    void VoiceCorePool::requestPhaseReset() noexcept
    {
        for (auto& core : slots) core.phase.pending = true;
    }

    void VoiceCorePool::setPhaseDial (int dial) noexcept
    {
        for (auto& core : slots) core.phase.setDial(dial);
    }

    void VoiceCorePool::setGroupCount (int count) noexcept
    {
        count = std::clamp(count,1,8);
        // DLL0x371d0 resets allocation order on an enabled Unison count edit,
        // retaining each physical core's pending flag and running phases.
        if (count != groupCount) { previous = -1; groupCount = count; }
    }

    VoiceCore* VoiceCorePool::claim (SynthVoice& owner, int capacity,int fixedSlot)
    {
        capacity = std::clamp(capacity,1,32);
        setGroupCount(owner.params.unisonVoices);
        int selected = fixedSlot>=0 ? std::clamp(fixedSlot,0,capacity-1) : -1;
        if(selected>=0 && owners[selected]!=nullptr)owners[selected]->detachCore(slots[selected]);
        const int first = previous + 1 < capacity ? previous + 1 : 0;
        for (int offset = 0; selected<0 && offset < capacity; ++offset)
        {
            const int index = (first + offset) % capacity;
            if (owners[index] == nullptr) { selected = index; break; }
        }
        if (selected < 0)
        {
            selected = 0;
            // Reuse a physical release before stealing a held note. Within
            // each group retain the existing oldest-allocation ordering.
            const auto releasing=[&](int i) {
                return slots[i].amplitude.getPhase()==dsp::AmplitudeEnvelope::Phase::Release;
            };
            for (int i = 1; i < capacity; ++i)
                if((releasing(i) && !releasing(selected))
                    || (releasing(i)==releasing(selected) && order[i]<order[selected]))selected=i;
            owners[selected]->detachCore(slots[selected]);
        }
        if(fixedSlot<0)previous = selected;
        owners[selected] = &owner; order[selected] = ++serial;
        return &slots[selected];
    }

    void VoiceCorePool::release (SynthVoice& owner, VoiceCore& core) noexcept
    {
        const int index = core.physicalSlot;
        if (index >= 0 && index < 32 && owners[index] == &owner) owners[index] = nullptr;
    }

    void VoiceCorePool::limitCapacity (int requested) noexcept
    {
        const int next=std::clamp(requested,1,32);
        // Native voice-count setter370e0 restarts the cyclic allocation cursor
        // when the count changes, retaining each physical receiver's history.
        if(next!=capacity){capacity=next;previous=-1;}
        for (int i = next; i < 32; ++i)
            if (owners[i] != nullptr) { owners[i]->detachCore(slots[i]); owners[i] = nullptr; }
    }

}
