#pragma once
#include "../audio/Midi.h"
#include <array>
#include <atomic>

namespace synth2::host {
enum class EditorNoteSource : unsigned { computer=1, mouse=2 };

// One editor producer, one serialized audio consumer. Desired ownership cannot
// overflow an event queue or lose a note-off when the editor closes.
class EditorMidiInput {
public:
    void set(int note,int velocity,EditorNoteSource source) noexcept {
        if(note<0 || note>127)return;
        const auto mask=static_cast<unsigned>(source);
        const auto before=desired[note].load(std::memory_order_relaxed);
        const auto owners=(before&3u&~mask)|(velocity>0 ? mask : 0u);
        const auto level=velocity>0 ? static_cast<unsigned>(std::clamp(velocity,1,127)) : before>>8;
        desired[note].store(owners ? owners|(level<<8) : 0u,std::memory_order_release);
    }
    void release() noexcept {
        for(auto& note:desired)note.store(0,std::memory_order_release);
    }
    bool pressed(int note) const noexcept {
        return note>=0 && note<128 && desired[note].load(std::memory_order_acquire)!=0;
    }
    template<class Send> void consume(Send send) {
        for(int note=0;note<128;++note) {
            const auto value=desired[note].load(std::memory_order_acquire);
            if(value && !applied[note] && !external[note])send(audio::MidiMessage(0x90,note,value>>8));
            if(!value && applied[note] && !external[note])send(audio::MidiMessage(0x80,note,0));
            applied[note]=value!=0;
        }
    }
    // Called in host timestamp order. With no editor owner, MIDI is unchanged,
    // including repeated/absent note-offs and native panic/controller behavior.
    template<class Send> void host(const audio::MidiMessage& message,Send send) {
        if(message.isNoteOnOrOff()) {
            const int note=message.getNoteNumber();
            const unsigned channel=1u<<(message.getChannel()-1);
            if(message.isNoteOn())external[note]|=channel;
            else {
                external[note]&=~channel;
                if(applied[note])return;
            }
        }
        if(message.isAllNotesOff() || message.isAllSoundOff())external.fill(0);
        send(message);
    }
    void resetAudio() noexcept { applied.fill(false);external.fill(0); }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::array<std::atomic<unsigned>,128> desired{};
    std::array<bool,128> applied{};
    std::array<unsigned,128> external{};
};
}
