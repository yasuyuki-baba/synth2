#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace synth2::editor {
// Editor-thread state. Releases retain their original pitch across octave changes.
class ComputerKeyboard {
public:
    ComputerKeyboard() { notes.fill(-1); }
    template<class Send> bool key(int code,bool up,bool modified,Send send) {
        if(code>='a' && code<='z')code-='a'-'A';
        constexpr std::string_view keys="AWSEDFTGYHUJKOL";
        const auto index=code>=0 && code<128 ? keys.find(static_cast<char>(code)) : keys.npos;
        if(index!=keys.npos) {
            auto& note=notes[index];
            if(up) { if(note>=0)send(note,0);note=-1;down[index]=false; }
            else {
                if(!modified && !down[index]) { note=base+static_cast<int>(index);send(note,127); }
                down[index]=true;
            }
            return up || !modified;
        }
        if(code=='Z' || code=='X') {
            auto& held=octaveKeys[code=='X'];
            if(up) { const bool wasHeld=held;held=false;return wasHeld || !modified; }
            if(modified)return false;
            if(!held) {
                held=true;releaseNotes(send);
                base=std::clamp(base+(code=='X' ? 12 : -12),24,96);
            }
            return true;
        }
        return false;
    }
    template<class Send> void release(Send send) { releaseNotes(send);down.fill(false);octaveKeys.fill(false); }
    int baseNote() const noexcept { return base; }
private:
    template<class Send> void releaseNotes(Send send) {
        for(auto& note:notes) { if(note>=0)send(note,0);note=-1; }
    }
    std::array<int,15> notes;
    std::array<bool,15> down{};
    std::array<bool,2> octaveKeys{};
    int base=48;
};
}
