# Logical voices and physical-core lifetime

| Logical state | Start | Release | Retire |
| --- | --- | --- | --- |
| Idle | Playing | Idle | Idle |
| Playing | Playing | Releasing | Idle |
| Releasing | Playing | Releasing | Idle |

Logical state is not a physical VCA phase or filter/FM clock. A restart/retirement
must not implicitly terminate a retained physical release or receiver history.
Pending Retrigger, Legato and RetainedRetrigger plus glide remain separate values;
consume them at the defined start/prepare boundaries. Retirement alone does not
clear an unconsumed transition.

`CoreReusePolicy` selects reuse actions from actual physical VCA activity, not
logical note flags. Idle and sounding reuse retain their distinct filter,
filter-envelope and FM settling rules. Voice performs the selected actions in
order after glide/wave/phase setup. Natural VCA completion, logical retirement,
allocation cursor updates and physical association cleanup are distinct events.

Envelope configuration takes narrow settings and caller-owned envelopes/reference
values. Preserve native duration/peak conversions, velocity references, keyboard
and unison offsets, and arithmetic order. Sample-rate updates do not manufacture
a note-on or reset every physical state.

See [HostLifecycleTest.cpp](../../tests/HostLifecycleTest.cpp) and
[PlaybackBehaviorTest.cpp](../../tests/PlaybackBehaviorTest.cpp). Observed reuse rules apply within their admitted rendering/history domains.

Each physical core owns the only amplitude envelope. Direct callers and mapped
patches use the same VCA configuration, per-core aligned gain block and natural
completion path. Sample velocity sensitivity at note assignment; gain/sensitivity
edits during playback retain the assigned velocity reference. An explicit offline
amplitude bypass is separate from normal instrument rendering.

When the physical pool is full, reuse the oldest releasing core before stealing
a held one. Inspect physical VCA phase rather than treating a key-up or logical
retirement flag as proof that its tail ended. Free-slot cursor order and oldest
order within the release/held groups remain separate. Idle core cleanup, pending
FM/filter transitions and state retention keep their existing boundaries.

Physical filter and oscillator AD envelopes serve direct callers and mapped
patches alike. Configure oscillator AD timing/depth from resolved dials and its
actual destination. History flags retain their core-reuse and receiver-admission
meaning, without selecting a parallel sample envelope. Filter release and
oscillator AD continuation after note-off keep their distinct schedules.

Every nonlegato core start invalidates the FM target/elapsed clock. Idle starts
settle; sounding retriggers retain current/anchor and pending slew length. Static
FM uses that same state. Logical note assignment must not reset FM to a separate
Voice-level target. Base-dial MIDI setters preserve their receiver restart horizon.
