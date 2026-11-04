#pragma once
#include <algorithm>
#include <array>

namespace synth2::dsp
{
    // Signal amplitudes and phase-unit values must retain fractional data.
    using ControlSignal=double;
    using VoiceControlBuffer=std::array<ControlSignal,256>;
    using VoiceSpanBuffer=std::array<int,256>;

    inline int constantControlSpan(const VoiceControlBuffer& values,int position,int frames) noexcept
    {
        int count=1;
        while(count<frames && values[position+count]==values[position])++count;
        return count;
    }

    struct ReceiverLfoView
    {
        struct Receiver { bool enabled {};int destination {}; };
        std::array<Receiver,2> receivers {};
        std::array<const ControlSignal*,2> values {};
        std::array<const int*,2> remaining {};
        bool available {};
        static bool reachesOscillator(const Receiver& receiver) noexcept
        {
            return receiver.enabled && (receiver.destination==0 || receiver.destination==1
                || receiver.destination==4 || receiver.destination==5);
        }
        int oscillatorFrames(int position,int frames) const noexcept
        {
            if(available)for(size_t slot=0;slot<2;++slot)
                if(reachesOscillator(receivers[slot]))frames=std::min(frames,remaining[slot][position]);
            return frames;
        }
    };

}
