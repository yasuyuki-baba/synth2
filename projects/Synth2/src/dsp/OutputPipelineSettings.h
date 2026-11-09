#pragma once
#include "EffectSettings.h"
#include <array>

namespace synth2::dsp
{
    struct OutputPipelineSettings
    {
        bool panLfo {};
        bool nativeGainSilence {},nativeSilenceGraph {},nativePhaser {};
        int nativeMultiEffectType {-1};
        std::array<double,3> nativeMultiEffectDials {};
        double nativePhaserDepth {},nativePhaserRate {},nativePhaserFeedback {64};
        int nativePhaserPairs {1};
        double pan {},masterVolume {1},hostBpm {120};
        EqualizerSettings eq;
        ChorusSettings chorus;
        DelaySettings delay;
    };
    struct OutputProcessContext
    {
        double sampleRate {44100};
        int gainBlockCapacity {256};
        bool bypassEffects {},inputSilent {};
    };
}
