#pragma once
#include "SynthVoice.h"
#include "VoiceCorePool.h"

namespace synth2::test
{
    // Tests provide the same mandatory borrowed storage as the Engine.
    struct VoiceStorage
    {
        dsp::OscillatorPhaseRandom phase;
        VoiceCorePool pool;
        VoiceStorage() { pool.prepare({44100,512,2},false); }
    };
    class PooledTestVoice : private VoiceStorage, public SynthVoice
    {
    public:
        explicit PooledTestVoice(RenderControls& controls)
            : SynthVoice(controls,VoiceStorage::phase,VoiceStorage::pool) {}
        void setControllerDial(int destination) noexcept
        { VoiceStorage::pool.setControllerDial(destination,44100); }
    };
}
