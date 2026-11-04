# Public verification map

This map identifies automated product checks and their scope.
A checked scenario is not proof of every possible input or execution history.
The public suite uses generated inputs and product contracts.

Actual SDK GUI interaction, DAW sessions and physical MIDI/audio-device checks
are outside the automated-test scope by product decision. They are not missing
or pending automated tests and are omitted from this inventory. The optional
[manual platform guide](native-validation.md) is a reference for separate manual
work; automated success does not claim that such work was performed.

| Product requirement | Automated public checks | Scope / interpretation |
| --- | --- | --- |
| Portable C++ naming | NamingLint and synthetic NamingLintTests | File/type PascalCase and role suffixes across src, including patch/core; semantic naming and variable/function spelling require review |
| Header dependencies | HeaderBoundaryAudit on GNU-style GNU/Clang Ninja/Makefile builds | Scalar oscillator, section, prepared LFO data, arpeggiator settings/runtime and physical/logical voice headers compile independently with explicit forbidden dependencies |
| Source loading and versions | LoadingBehavior, PatchImportWorkspace, generated malformed source tokens | Acceptance of 1–113 is distinct from independently observed native conversion |
| Bank handling | PatchLibrary and BankBoundary: ZIP/file rejection, exact entry/expanded-byte limits, CRC rejection, symlink filtering, natural ordering, UTF-8, source retention, detached-bank recall | Broad adversarial ZIP fuzzing is not exhaustive |
| State and parameter ABI | ParameterContract, ProcessorState (including patch-name recall and legacy source-title fallback), StateRepresentation (child order and equivalent numeric spellings), generated truncated payloads, all-parameter live edits | Covers ProcessorBridge state and host parameter definitions |
| Persistent sound library | PatchCatalog (imports, duplicates, folder ordering, persistent tags/favorites, combined filters, removal and source-free recall) | UI-side storage and filtering; native interaction remains manual |
| Live changes | PlaybackBehavior: all host parameters at endpoints/midpoint, live program loads, recall and rejected loads; integer sub-level audio across legacy/continuous states and fractional sub-level interpolation/recall | Arbitrary host automation schedules are not exhaustively enumerated |
| MIDI and voice lifetime | PerformanceBehavior, HostLifecycle, GeneratedInput: exact dispatcher timeline, sustain, CC121, stops, overflow, polyphony/unison | Timeline and lifetime checks cover the exercised schedules |
| Instance ownership | InstanceIsolation: interleaved noise/random/ARP, unrelated instance creation/destruction, edits/reprepare of another instance, parallel separate instances | Calls into the same bridge must remain serialized |
| Callback heap use | RealtimeBehavior: first prepared callback, source automation, version changes, float/double, zero/oversize blocks, MIDI/history overflow and drain, parameter setters and MIDI publication | Intercepts global C++ new/delete; Linux also wraps static project malloc/calloc/realloc/free references. Shared-library/OS allocation, locks/I/O, drivers and hard real-time deadlines remain outside this instrumentation |
| Oscillators, envelopes and filters | DspBasics, FilterBehavior, HostLifecycle pitch across rates | Does not establish unrestricted spectral or original-engine identity |
| LFO and shared clocks | ModulationBehavior: all waveforms, bounds/reset, zero depth, tempo ratio, key sync/overlap, disabled shared-clock advancement and read-only consumers | Disabled waveform sources pause; shared native clocks can keep advancing. These are source/ownership properties rather than recorded RNG fingerprints |
| History publication | HistoryQueue: FIFO retention, drop counters, repeated saturation/reuse, concurrent producers, bridge invalidation and post-drain edits | A drained queue accepts new events; it does not reconstruct discarded execution history |
| Effects/output lifecycle | EffectsBehavior, EffectLifecycle, EqualizerBehavior: bypass/reset, isolated modes, tails, rate setup, silence gain and output regions | Component reset and cold-start checks do not establish every conditional Engine history |
| Editor source binding | EditorInteraction: shared native choice/toggle logic, every selector, cancellation, editing steps, fractional unit displays, host values and recall; EditorLayout: layout/labels/units, hidden metadata and Synth/MIDI/Library views | Covers shared source-selection logic, bridge binding and presentation |
| CI preparation and candidate archives | BuildToolsTests: local Git SDK fixtures with CRLF preservation/reuse and edited-code rejection; generated product files, missing-binary rejection, metadata/notices, unrelated-file exclusion, overwrite rejection and POSIX bundle modes/symlinks | Self-contained preparation/package contracts; no network or real SDK required, and no native build or DAW pass established |
| Memory and undefined behavior | AddressSanitizer/UndefinedBehaviorSanitizer/LeakSanitizer local C++ runs; generated input sentinels | Compiler, architecture and platform coverage must be recorded per run |

Pulse-width mapping preserves reciprocal-multiply rounding for all integer dials
and fractional edits in `PlaybackBehaviorTest.cpp`. `ModulationBehaviorTest.cpp`
compares queued and synchronized full loads with explicit routing/rate setters
followed by the sync setter, including transitions in both directions.

Direct source mapping is compared with the typed importer across version families
in `PatchImportWorkspaceTest.cpp`. The processor explicitly preserves fractional
host mode through version changes and state recall in `PlaybackBehaviorTest.cpp`;
sparse envelope inputs are compared with explicit source defaults in
`ProcessorStateTest.cpp`.

Library tests use generated source banks and cover empty-library Init startup,
uniform tagging/favorites/removal even for a bank named Factory, and bank-free
state recall. Lifecycle stress and baseline-export inputs are self-authored or
explicitly supplied; no external preset bank is distributed with the product.

`PatchFormatTest.cpp` checks Synth2 version 200 complete float snapshots, quoted
UTF-8 names, legacy resolved-dial export, raw stereo equality after patch/state
recall, and invalid-input rejection. `PatchLibraryTest.cpp` covers library loading
of version 200 files with fractional values and their stored names.

## Callback mapping ownership

ProcessorBridge owns a prepared Synth1PatchMapper. Version changes reset mapper
cache/context without allocating a replacement. The mapper reuses caller-owned
source/import workspace; legacy immutable conversion maps initialize before audio
processing. Conversion order and loaded control meanings remain unchanged. Do not
move parsing, file access or state serialization into the audio callback.

The ordinary and workspace import paths are compared using generated documents
for every accepted source version. Byte/control comparisons against the preceding implementation remain necessary
when changing loading or DSP boundaries.

## Keeping coverage useful

Add a reproducible self-authored public case when a product defect can be stated
without recorded original outputs. Keep a failing requirement intact while fixing
its cause. Keep manual-only activities outside this inventory rather than listing them as
unexecuted automated tests. Do not call portable coverage a native pass. Record
the exact automated checks, sanitizer results and execution environment in the
validation record; this map describes scope, not dated pass counts.

Fractional output controls are checked by EffectsBehavior and Equalizer with
integer-point impulse equality, sub-integer audio changes, reset/prepare retention,
live cache edits and finite endpoint neighborhoods. PlaybackBehavior verifies
continuous/legacy state-mode transitions and near-neutral EQ activation. These
checks do not certify native GUI/DAW operation or unrestricted DLL equivalence.

PatchImportWorkspace checks exact integer-to-float promotion, including packed
MIDI words and resolved values outside host ranges, after source-version
conversion. Promotion must neither clamp nor convert the compatible state again.

BoundedPrecision checks compatible gain/velocity/saturation anchors, fine tuning,
fractional Delay/Chorus impulse positions, fractional Phaser/MultiFX controls and
sample-clock remainder retention in decimation. EffectsBehavior retains the
integer-point response checks; native LFO/FM/envelope control stepping is preserved.
It also checks fractional signal transport separately from integer span metadata,
exact storage of 27-bit phase units, FM dispatch/slew state, filter receiver-to-
coefficient propagation, amplitude modulation and pitch/pulse-width coordinates.
Envelope checks cover fractional sustain/depth through the actual configuration
and receiver adapters, sub-level release handoff, and live depth rescaling below
one level without altering discrete span lengths.
LFO depth checks cover sub-unit values and held spans for four deterministic
waveforms. Shared-source tests check fractional signals with unchanged clocks and
random history. Portamento checks cover fractional time mapping, sample-count
configuration and repeated-note audio in mono and legato modes.
Random-source checks cover one-draw projection, zero-range behavior, real held/
smoothed state and fractional edits without redraws. Shared tests cover fractional
random depth, key sync and equal CRT advancement. Glide tests cover fractional
retained history through retrigger and receiver transport; compatible whole-state
pitch stepping remains a separate boundary.
Free-rate checks cover distinct fractional periods, discrete synced divisions,
free-dial retention across sync changes, both shared slots and repeated updates
without extra clock advancement. Modulation-envelope timing checks cover mapping
and distinct Attack/Decay trajectories for pitch, FM and pulse-width destinations.
Filter-coordinate checks cover fractional cutoff through loading/render adapters,
integer controller replacement, retained keyboard offsets and real headroom/
sustain/release levels. MIDI note and velocity words remain discrete inputs.
Unison precision checks cover fractional pitch/filter layout, tuning adapters,
external filter-offset headroom and receiver transport. Fixed-phase spread checks
cover fractional loading and unchanged shared phase-random draw order.
Stack/fixed-phase checks cover fractional loading, lane frequencies and cache
refresh, scalar/stack and random/fixed boundaries, and exact pending draw counts
through fractional random-mode edits and fixed/random transitions.
ContinuousLevelTest checks fractional VCA Attack targets at whole settings,
release-window endpoints and final silence, and real cutoff transport into the
existing native filter kernels. DspBasics checks envelope monotonicity without
narrowing observed levels to integers. Sample-envelope checks also cover levels
below one former fixed-point unit, analytic Attack/Release progress and reset.
FM slew checks cover fractional targets, completion without residual restart,
reverse transitions and reset. Repeated Attack and Release retrigger checks
verify compatible whole-level sample-position anchors while preserving fractional
audio levels and gain history.

LfoPhasePrecisionTest checks sine amplitude/pan interpolation across table
coordinates and wrapping, exact integer phase knots, independent fractional depth
and phase, native phase-word wrapping, unchanged held spans and the retained whole
sine coordinates for other receivers. These generated component inputs do not
establish unrestricted original-engine equivalence.

FmPhaseCorrectionTest checks compatible disabled advancement, bounded constant-input
fractional-product drift and phase return after settling, reset/rebase behavior,
and synchronized-noise history through both the component and oscillator section.
Coefficient ramps, turns and held jumps with integral products retain their native
phase offsets. Adapter checks cover all sine/triangle/saw/square/random waveform
mixtures and envelope enable states through the same native FM policy, plus
static/envelope/performance selection and bypass. Native Ring dispatch preserves
uncorrected phase/audio. Static FM mapping checks exact integral knots under both
source normalizations and distinct fractional edits. The production audio adapter
checks wide-depth phase retention with unchanged synchronized noise, compared
with direct wide and narrowed oscillator inputs. Coefficient changes retain outstanding correction rather
than resetting it. These are generated numerical checks, not listening tests,
unbiased fractional integration or exhaustive original-engine parity.

FilterEnvelopePrecisionTest covers peak-rescaling continuity around integers,
exact ratios, unchanged counters, fractional Release-trajectory retention through peak edits, zero-peak
behavior and Sustain edits. The common filter configurator checks integral
source knots under both division and reciprocal-multiply normalization.
HostLifecycle checks that live host filter setters restart cutoff slews for held
and releasing cores, retain fractional edits within integer dials, suppress only
consecutive identical setters and preserve pending requests across zero-frame
callbacks. Ordered pre-render edits retain a zero-depth round trip and update the Release
starting level before same-callback Note Off. Saturation reports loss and recovers
after draining; sound replacement discards pending edits for the previous state.

DelayPathTest checks integral/fractional edit-history equivalence after reset,
all delay types and tone sides, exact integral impulse endpoints, one-sample versus
block partition equivalence during slews, and the common coordinate settling rule.
Existing EffectLifecycle checks retain silence/tail/reset coverage.

RenderPolicyTest covers common native receiver selection across source waveforms
and destinations, restricted fallback when random history is unavailable or the
source is non-native, and the explicit resonant-noise/idle-reset lifecycle rules.

ChorusPathTest checks common-coordinate continuity on both sides of integral
time/depth/rate settings in x1/x2/x4 modes, reset equivalence after fractional
edit history, and exact integral/fractional zero-depth impulse delays.

OutputTonePrecisionTest checks continuity around integer settings across sample
rates, bypass history retention, fractional setting recall during preparation,
and explicit history preservation/reset. Equalizer checks equivalent native
integer normalizations and fractional controls without an opt-in precision flag.
PlaybackBehavior verifies resolved EQ/Delay/Chorus dials across version 1/2 legacy
state recall and version 3 fractional recall, including near-neutral EQ activity.

PwmPrecisionTest checks all integer source-width knots under both normalized
representations, fractional base resolution, two-coordinate continuity, ordinary
and stack adapter audio changes without OSC1 clock changes, equivalent direct and
explicit coordinate entry points, and wrapped pulse reads without phase advance.

FilterPathTest checks selected compatible kernels across repeated resonance112
crossings at multiple rates/modes, exact sample/direct-index state equivalence,
local edit continuity, shared coordinate-adapter history/reset and low-rate
cutoff sweep stability with the same mode kernels.
RenderPolicy checks source-control preparation and common composition around
restricted resonance admission boundaries across LFO destinations and waveforms.

BoundedPrecision checks native sub-receiver presence and real gain scaling below
and across dial0.5 with and without oscillator stacking, through the actual mapper.

OutputPipelinePathTest checks that mono/stereo wet processing uses EffectPair
at supported rate endpoints and a fractional rate, preserves surrounding samples,
bypasses wet processing outside the supported domain, and retains/clears EQ
history at the documented release boundary.

Shared LFO ownership checks cover first-key resets for both compatible and
fallback signals after MIDI dispatch, continuation across overlapping channels
and subspans, unknown-history random target retention without reseeding, and
identical read-only signals for one or many Voice consumers. These generated
properties do not certify unavailable native random histories.

OscillatorReadTest checks adjacent-sample interpolation at fractional/wrapped
phase coordinates without phase advance, and exact agreement between the default
Voice adapter and direct oscillator processing. RenderPolicy checks deterministic
amplitude receiver and gain consistency across source/history, waveform, tempo,
key-sync and play-mode inputs. ModulationBehavior checks that a shared amplitude
slot does not erase another slot's fallback modulation.

Equalizer checks sample, context and stereo graph agreement under generated
stereo signals and fractional controls. Context bypass preserves both audio and
filter history. OutputPipelinePath checks the complete EQ -> wet-device order
and unsupported wet-rate bypass with the common upstream EQ and settled startup
gain. EffectLifecycle checks startup advancement through silence without an
implementation-selection flag. These properties do not establish every event
history or original-engine equivalence.

RenderPolicy checks deterministic receiver and oscillator-envelope composition
across source/history admission, both LFO slots, destinations, waveforms, sync,
key sync and play modes. Unknown random sources still require both source and
history admission; a deterministic amplitude slot remains available beside them.
These selection properties do not replace audio or execution-history comparisons.

OscillatorRead checks the Ring product mix and FM phase precedence using generated
tables, and Sub phase continuity across muted/fractional level edits with and
without stacking and Ring. RenderPolicy checks one Sub level scale independently
of source/random admission. Existing FM Sync-reference tests retain their phase
constraints after removal of the redundant Ring-selector argument.

AmplitudeBehavior checks direct Voice use of physical VCA phases and retirement,
repeated note-off stability, assigned-velocity retention during running edits and
release-priority allocation at full capacity. HostLifecycle retains generated
reprepare, load, recall and callback coverage. These product properties do not
establish arbitrary host or original-engine history equivalence.

EnvelopeBehavior checks direct filter/OSC2 envelope rendering without receiver
admission. OscillatorRead checks that common pulse input preserves shared, envelope and
additional receiver contributions and that the reserved both-pitch destination
uses its one-octave range with or without pitch receiver transport. These checks
use generated inputs; they do not establish unknown random-history equivalence.

FmBehavior compares direct static-FM rendering with and without history admission
through base-dial edits, checks the sample-rate-derived slew horizon and exact
completion, and verifies retained sounding-retrigger state. ContinuousLevel checks
real FM levels and final targets in the common physical receiver. FmPhaseCorrection
checks that waveform/envelope/history flags do not select a different correction
policy, while explicit offline bypass remains separate.

ModulationInputs checks compatible endpoint units, per-slot consumption, direct
wheel transport beside admitted histories, shared/envelope/additional FM
composition, exact additional-control boundaries and missing-source neutrality.
OscillatorRead checks shared/envelope/additional pulse composition with generated
wave tables. ModulationBehavior retains mixed shared/fallback amplitude checks.

AmplitudeInputs checks independent shared/additional slots through the physical
VCA at every output alignment, retained change partitions and release state,
real fractional depth knots and disabled-source neutrality. StereoBalance checks
center/edge/out-of-range gains and agreement between per-voice balance and the
static factor in the smoothed pan receiver without relocating either stage.

PanInputs checks real endpoint projection, per-slot source selection and offsets,
retained depth changes at source-generation subregions, read-only consumption,
Voice/unison placement without global pan, oversized unknown-history callbacks
and buffer sentinels. OutputPipelinePath checks the actual nonlinear-effect then
cubic-pan order against an independent stage composition and rejects reversed
order. These cases do not establish unknown random-source history parity.

StaticPanInputs checks that direct Voice/core placement is independent of global
Pan and that the actual output balance follows nonlinear drive for mono/stereo
and both source-history states. The stage-composition test keeps buffer sentinels
and demonstrates that the reversed nonlinear order produces different samples.

PerformanceBehavior checks ordinary bend with or without an LFO provider or bound
dispatcher controls, channel-isolated ordinary bend, native omni ingress,
CC121 centering, actual wheel-to-FM/filter routes and immutable stored controls.
TuningBehavior checks common tracked/fixed note tuning and fractional fine values.
These generated-input tests do not certify unrestricted native histories.

AmplitudeBehavior checks that attack-duration edits preserve progress and retain
real envelope levels rather than quantizing the level to an integer.
