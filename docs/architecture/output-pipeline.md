# Output processing ownership

`OutputPipeline` owns output-device history. Engine determines the processed region,
semantic input silence and prepare/release boundaries. Pass narrow OutputPipelineSettings,
OutputProcessContext and read-only Pan LFO views; do not pass Engine, Voice, source
version, patch documents or the complete render aggregate.
Process output at dispatched root regions for every source, using that region's
controls and prepared views. A later MIDI control edit cannot alter earlier
audio in the callback.

Process nonlinear MultiFX/Phaser and EQ/Tone, global balance/Pan,
Delay then Chorus, and startup gain in that order. Preserve native gain units,
wet sample-rate guards, mono handling and the recurrence/finishing boundaries.
The delay must capture the balanced/Pan-processed input in its recirculating path.

MultiEffect and Phaser are the only nonlinear/modulation effect implementations;
EffectPair owns the only Delay/Chorus kernels. OutputPipelineSettings has no implementation
selector or experimental-effect payload. The supported wet rate domain is
10,000 through 384,000 Hz inclusive. Outside that domain wet stages bypass; a
fallback algorithm must not be selected. Prepare wet state only inside its rate
domain, and clear it on other explicit preparation boundaries.

EQ/Tone has one PeakEq -> OutputTone implementation for mapped controls and direct
component callers. EQ settings contain only frequency, level, Q and Tone;
neutral Peak level bypasses that band, while neutral Tone retains its defined
low-pass response. Explicit context/effect bypass is separate from neutral controls.
No RBJ/shelf or measured Tone approximation is selected by caller or source format.
Startup gain always advances in the output pipeline, including effect-bypassed
and silent regions; there is no alternate gain-disabled rendering mode.

Silence is semantic state, not merely a scan for zero input samples. Wet devices
may retain audible history, and gain advances even through silent regions.
Process only the requested output region; do not touch surrounding samples.

Engine owns the retention predicate. The admitted retained-voice route requires
initialized shared modulation, poly play mode and at
least one native shared LFO clock. Do not widen this route without independent
history evidence.

| Lifecycle event | Required treatment |
| --- | --- |
| Prepare in the retained-voice route | Keep held voices/allocation and startup-gain history; update rates when changed |
| Prepare outside that route | Stop notes and reset allocation/startup gain rather than promising held-note continuation |
| Native Pan/Phaser/MultiFX prepare | Preserve state after shared modulation has initialized |
| Rate change while retaining voices | Update Tone sample rate without erasing its retained history; prepare the other effect stages |
| Release in the retained-voice route | Retain Tone delay samples; reset the other release-owned effect stages |
| Release outside that route | Reset Tone along with the other release-owned effect stages |

Patch loading must not introduce an unconditional output-history reset. Output
processing never advances shared LFO clocks. These retention rules are required
behavior; component lifecycle checks alone do not establish every conditional
Engine transition or arbitrary host-history equivalence.

See [audio rendering](../contracts/audio-rendering.md) for operation-order constraints.

[EffectLifecycleTest.cpp](../../tests/EffectLifecycleTest.cpp) uses generated impulses
for delay tail/reset contracts and checks that gain advances through silent input
while preserving samples outside the requested output region. Explicit delay
reset/rate setup is tested separately from Engine's conditional history-retention
policy; component reset tests do not establish every whole-pipeline history.

Delay frame targets use compatible source-dial anchors, but all integral and
fractional coordinates share one sample-processing path. A prior fractional edit
must not select a different path after reset or a subsequent integral time edit.
Tap coordinates use Float32; the common slew settles within 0.1 frame
using the native endpoint rule, then reaches the exact target. Delay Tone maps
integer coefficient knots and fractional interpolation to one three-coefficient
recurrence, with a neutral bypass that retains filter history. Algebraic
reassociation can change float rounding; record those differences separately.
