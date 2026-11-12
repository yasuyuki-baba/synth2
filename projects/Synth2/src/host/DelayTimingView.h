#pragma once

namespace synth2::host
{
// Approximate read-only audio-thread telemetry for the editor, not DSP controls.
struct DelayTimingView
{
    float bpm {},leftMilliseconds {},rightMilliseconds {};
};
}
