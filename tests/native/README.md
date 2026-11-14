# Built-plugin audio smoke tests

`Synth2AudioSmoke` loads the actual build-tree VST3/CLAP modules on Linux,
Windows and macOS, plus AUv2 on macOS. It creates no editor, audio device or DAW.
Each adapter uses the public format's module lifecycle, state restoration,
MIDI/event delivery and stereo Float32 rendering APIs. AU's factory is read
from the built component's Info.plist and registered only inside the test
process; system plugin installation/discovery is not required.

The small `.sy1` patches in `patches/` were authored for these tests and are
project-owned source under the repository license. Tone/mute differ in title and
Amp Level (100 versus 0). They select a sustained open-filter tone, fast release,
and disabled LFOs, ARP, unison and effects. No third-party patch or recording is used.
The production patch loader and session writer encode them as self-contained
state, then the host restores it through `IComponent::setState`, CLAP's state
extension or AU's ClassInfo property. The state writer is not an audio oracle.

`legacy.sy1` is an independently authored version 100 patch with envelopes,
width modulation and wet effects. VST3 also runs two fresh instances from the
same state: one receives every sound/quality value read from `IEditController`
back through `process(inputParameterChanges)` before MIDI, while the other
receives no parameter echo. Their complete stereo output must be bit-identical.
The same check runs for a version 200 state derived from this patch with
fractional Cutoff and Chorus Voices. This covers lost legacy conversion and
rounding of recalled fractional selectors at both 1x/8x, including macOS VST3.

At 48 kHz with 128-frame blocks and 120 BPM, the same plugin instance loads
tone → mute → tone at both 1x and 8x oversampling. MIDI note 69 uses full velocity
(127 / VST3 1.0), avoiding unrelated fractional-velocity conversion differences.
Each load must satisfy these independently specified signal properties:

- Before MIDI: quiet output.
- Held note: tone stereo RMS above `1e-4`, muted patch peak at most `1e-8`.
- After note-off and 256 ms release/drain: quiet output again.
- Every rendered sample, including the drain, must be finite.

An unmuted idle path can contain the native EQ's intentional semantic-silence
fill; the quiet bound is `1e-6`, consistent with the existing PlaybackBehavior
release checks. The zero-level patch retains the stricter `1e-8` bound throughout.
No alignment, gain fitting, downmixing or reference PCM is involved. Logs contain
complete-stereo RMS/peak for each phase; failure returns nonzero to CTest/CI.

After preparing the pinned native SDKs, enable the test in the normal native
build, for example:

```sh
cmake --preset linux -DSYNTH2_BUILD_NATIVE_SMOKE=ON
cmake --build build-oos-linux --target Synth2AudioSmoke --parallel 4
ctest --test-dir build-oos-linux -C Release -L native --output-on-failure --no-tests=error
```

Use the `windows` / `macos` presets and their build directories on those systems.
The test executable depends on the selected plugin targets. CI enables this
option in its existing native matrix and preserves CTest logs on failure.

This detects broken module loading, rejected state, absent MIDI/audio, stale
audible/muted state and missing note release in these short scenarios. It does
not establish detailed timbre equivalence, DLL compatibility, live Library GUI
notifications, DAW/wrapper behavior, GUI behavior or standalone audio-device
operation. The separate VST3 patch-publication regression also checks serialized
state, real host edits and explicit UI setter history; this test exercises the shipped modules instead of a
headless recompile of the wrapper.
