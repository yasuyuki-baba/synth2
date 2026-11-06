#pragma once

namespace synth2
{
    // Serialized MIDI source words carry status/controller bytes, unlike knobs.
    constexpr int sourceMinimum(int index) noexcept { return index == 9 ? -24 : (index == 86 || index == 88) ? -1 : 0; }
    constexpr int sourceMaximum(int index) noexcept
    {
        return index == 9 ? 24 : (index == 86 || index == 88) ? 65535 : 127;
    }
}
