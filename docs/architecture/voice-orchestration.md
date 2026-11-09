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

Engine owns one ordinary PitchBendSmoother per MIDI channel and advances moving
receivers at root regions of at most32 frames. Native omni ingress maps performance
messages to channel1 before dispatch. Voice borrows its channel receiver; direct
Voice callers own the same receiver and use the same span and transfer rules.
Keep ordinary bend separate from routed MIDI pitch. CC121 centers only the
ordinary bend receiver on its channel; it does not clear routed inputs or sustain.

Ordinary bend advancement does not depend on an LFO provider. A dispatcher with
bound render controls advances its channel receivers even without shared LFO
sources. Without bound render controls, Voice uses its own instance of the same
bend component. Glide enable follows the resolved duration dial directly; do not
store a duplicate admission-dependent portamento flag.

`VoiceLfoRenderer` consumes `VoiceLfoSettings` and `VoiceLfoView` to read prepared
shared inputs. Voices own no LFO generator, key-sync phase or LFO random stream.
Only Engine advances and resets sources, after upstream MIDI dispatch. Receiver admission and history validity
are distinct inputs. Preserve slot/receiver spans when selecting a view.

Render-policy mutation phases are ordered. Pass the oscillator admission result
explicitly into dependent filter, LFO and gain phases. Avoid hidden reads of
partially mutated aggregate controls.

See [render boundaries](render-boundaries.md), [modulation](../contracts/modulation.md)
and [voice lifecycle](voice-lifecycle.md).
[HostLifecycleTest.cpp](../../tests/HostLifecycleTest.cpp) checks pitch across sample
rates and reprepare playability; [ModulationBehaviorTest.cpp](../../tests/ModulationBehaviorTest.cpp)
checks shared clock, fallback source and read-only consumer properties. These generated-input
checks do not establish every tuning/receiver history or independent native identity.
