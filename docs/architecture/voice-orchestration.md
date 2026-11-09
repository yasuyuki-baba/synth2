# Voice tuning and input orchestration

`VoiceDispatcher` owns MIDI/note dispatch and receiver regions. `SynthVoice` owns
logical note lifecycle and borrows assigned physical `VoiceCore` state from
`VoiceCorePool` in every play mode. Pool and phase-random references are mandatory;
Voice holds no private core array or substitute phase-random source. Their declarations and
pool/dispatcher implementations live in matching files. Core/pool/dispatcher headers
must compile independently; the pool and dispatcher declarations use forward
references rather than importing SynthVoice or SynthEngine.

Voice owns note/core lifecycle and physical slot selection. `VoiceTuning` receives
value-only settings and caller-supplied note and per-slot detune values.
Preserve native shifted-note, tracked/fixed-frequency, float/double conversion and
multiply order for every caller, independently of source or receiver admission.
Physical-core Portamento supplies glide at the control stage; do not add a second
Voice frequency glide. Tuning owns no note or shared random history.

VoiceDispatcher owns ordinary PitchBendSmoother state per MIDI channel and
advances moving receivers at root regions of at most32 frames. Native omni
input maps performance messages to channel1 before dispatch. Voice receives the
resolved integer offset and owns no local bend component or alternate slew.
Ordinary bend advancement does not depend on an LFO provider. Keep it separate
from routed MIDI pitch and CC121 routing/sustain state.

Voice borrows read-only LfoBlock receiver values and spans. Engine alone prepares
both slots after MIDI dispatch. Render policy resolves controls only, without
source/history admission flags or a normalized modulation router. Evidence
validity remains separate from execution and cannot select another algorithm.

See [render boundaries](render-boundaries.md), [modulation](../contracts/modulation.md)
and [voice lifecycle](voice-lifecycle.md).
[HostLifecycleTest.cpp](../../tests/HostLifecycleTest.cpp) checks pitch across sample
rates and reprepare playability; [ModulationBehaviorTest.cpp](../../tests/ModulationBehaviorTest.cpp)
checks shared clocks, physical controls and read-only consumer properties. These generated-input
checks do not establish every tuning/receiver history or independent native identity.
