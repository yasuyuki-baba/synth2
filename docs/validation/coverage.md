# Public verification map

This map identifies automated product checks and their scope.
A checked scenario is not proof of every possible input or execution history.
The public suite uses generated inputs and product contracts.

Actual SDK GUI interaction, DAW sessions and physical MIDI/audio-device checks
are outside the automated-test scope by product decision. They are not missing
or pending automated tests and are omitted from this inventory. The optional
[manual platform guide](manual-platform.md) is a reference for separate manual
work; automated success does not claim that such work was performed.

| Product requirement | Automated public checks | Scope / interpretation |
| --- | --- | --- |
| Portable C++ naming | NamingLint and synthetic NamingLintTests | File/type PascalCase and role suffixes across src, including patch/core; semantic naming and variable/function spelling require review |
| Header dependencies | HeaderBoundaryAudit on GNU-style GNU/Clang Ninja/Makefile builds | Scalar oscillator, section, prepared LFO data, arpeggiator settings/runtime and physical/logical voice, editor presentation, serialized-state and numeric-I/O headers compile independently with explicit forbidden dependencies |
| Source loading and versions | [LoadingBehavior](../../tests/LoadingBehaviorTest.cpp), [PatchImportWorkspace](../../tests/PatchImportWorkspaceTest.cpp), [PatchFormat](../../tests/PatchFormatTest.cpp): all accepted versions, extended coarse words, comma-containing titles, resolved-dial export, fractional version 200 patch/state recall, locale-independent decimal parsing/range rejection and malformed-input rejection | Acceptance of 1–113 is distinct from independently observed native conversion |
| Bank handling | [PatchLibrary](../../tests/PatchLibraryTest.cpp) and [BankBoundary](../../tests/BankBoundaryTest.cpp): ZIP/file rejection, exact entry/expanded-byte limits, CRC rejection, symlink/macOS metadata filtering, natural ordering, UTF-8, source retention, large nested collections and late-ordinal detached-bank recall | Broad adversarial ZIP fuzzing is not exhaustive |
| State and parameter ABI | [ParameterContract](../../tests/LoadingBehaviorTest.cpp), [ProcessorState](../../tests/LoadingBehaviorTest.cpp) (including patch-name recall and legacy source-title fallback), [StateRepresentation](../../tests/StateRepresentationTest.cpp) (child order, equivalent numeric spellings, host C-locale independence and legacy finite-number syntax), generated truncated payloads, all-parameter live edits | Covers ProcessorBridge state and host parameter definitions |
| Editor keyboard | [EditorKeyboard](../../tests/EditorKeyboardTest.cpp): CheapSynth01 key map, octave limits, repeat suppression, release ownership across PC/mouse/external MIDI, timestamped audio equivalence, zero-frame deferral and overflow recovery | Portable input and bridge behavior; native focus/resize interaction belongs to the manual platform guide |
| Persistent sound library | [PatchCatalog](../../tests/PatchCatalogTest.cpp) (imports, duplicates, recursive folder/ZIP banks and ancestor filters, folder ordering, persistent tags/favorites and sound IDs/ordinals, combined filters, safe archive removal and source-free recall) | UI-side storage and filtering; native interaction remains manual |
| Live changes | [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp): all host parameters at endpoints/midpoint, live program loads, recall and rejected loads; integer sub-level audio across legacy/continuous states and fractional sub-level interpolation/recall | Arbitrary host automation schedules are not exhaustively enumerated |
| MIDI and voice lifetime | [PerformanceBehavior](../../tests/PerformanceBehaviorTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp), [GeneratedInput](../../tests/GeneratedInputTest.cpp): exact dispatcher timeline, sustain, CC121, stops, overflow, polyphony/unison | Timeline and lifetime checks cover the exercised schedules |
| Instance ownership | [InstanceIsolation](../../tests/PlaybackBehaviorTest.cpp): interleaved noise/random/ARP, unrelated instance creation/destruction, edits/reprepare of another instance, parallel separate instances | Calls into the same bridge must remain serialized |
| Callback heap use | [RealtimeBehavior](../../tests/RealtimeBehaviorTest.cpp): first prepared callback, source automation, version changes, float/double, zero/oversize blocks, MIDI/history overflow and drain, parameter setters, editor-note ownership and MIDI publication | Intercepts global C++ new/delete; Linux also wraps static project malloc/calloc/realloc/free references. Shared-library/OS allocation, locks/I/O, drivers and hard real-time deadlines remain outside this instrumentation |
| Oscillators, envelopes and filters | [DspBasics](../../tests/DspBasicsTest.cpp), [FilterBehavior](../../tests/FilterBehaviorTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp) pitch across rates | Does not establish unrestricted spectral or original-engine identity |
| LFO and shared clocks | [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp): all waveforms, bounds/reset, zero depth, tempo ratio, key sync/overlap, disabled shared-clock advancement and read-only consumers | Disabled waveform sources pause; shared native clocks can keep advancing. These are source/ownership properties rather than recorded RNG fingerprints |
| History publication | [HistoryQueue](../../tests/HistoryQueueBehaviorTest.cpp): FIFO retention, drop counters, repeated saturation/reuse, concurrent producers, bridge invalidation and post-drain edits | A drained queue accepts new events; it does not reconstruct discarded execution history |
| Effects/output lifecycle | [EffectsBehavior](../../tests/EffectsBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp), [EqualizerBehavior](../../tests/EqualizerBehaviorTest.cpp): bypass/reset, isolated modes, tails, rate setup, silence gain and output regions | Component reset and cold-start checks do not establish every conditional Engine history |
| Editor source binding | [EditorInteraction](../../tests/EditorInteractionTest.cpp): shared native choice/toggle logic, every selector, cancellation, editing steps, fractional unit displays, host values and recall; [EditorLayout](../../tests/EditorLayoutTest.cpp): layout/labels/units, hidden metadata and Synth/MIDI/Library views | Covers shared source-selection logic, bridge binding and presentation |
| CI preparation and candidate archives | BuildToolsTests: local Git SDK fixtures with CRLF preservation/reuse and edited-code rejection; generated product files, missing-binary rejection, metadata/notices, unrelated-file exclusion, overwrite rejection and POSIX bundle modes/symlinks; Linux archive products, adjacent resources and x86_64 bundle validation | Self-contained preparation/package contracts; no network or real SDK required, and no native build or DAW pass established |
| Memory and undefined behavior | AddressSanitizer/UndefinedBehaviorSanitizer/LeakSanitizer local C++ runs; generated input sentinels | Compiler, architecture and platform coverage must be recorded per run |

## DSP and receiver coverage

This table groups generated component and bridge checks by responsibility. It is
an index to test sources, not a record of successive implementation changes.

| Area | Test entry points | Exercised properties |
| --- | --- | --- |
| Import and callback mapping | [PatchImportWorkspace](../../tests/PatchImportWorkspaceTest.cpp), [ProcessorState](../../tests/LoadingBehaviorTest.cpp), [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp) | Ordinary/workspace decoding across accepted versions; exact integer promotion including packed MIDI/out-of-host-range words; sparse defaults; fractional state-mode retention and resolved EQ/Delay/Chorus recall |
| Numerical precision | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp), [DspBasics](../../tests/DspBasicsTest.cpp) | Gain/velocity/saturation anchors, fine tuning, 27-bit phase storage, real signal transport versus discrete spans, fractional effect controls and decimation remainder; real envelope progress, retrigger anchors, release endpoints and final silence; representable Float32 filter/delay history and pitch-modulation base frequencies, Float32 time/rate frame conversion and exact widening for double host output (including oversampling) |
| LFO depth, rate and history | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp), [LfoPhasePrecision](../../tests/LfoPhasePrecisionTest.cpp) | Fractional depth/periods, discrete synced divisions, held spans, sine phase interpolation/wrapping and integer knots, key sync, one-draw random projection, held/smoothed targets and unchanged RNG advancement |
| Shared source ownership | [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp) | First-key resets after MIDI, overlapping keys/channels, subregions, unknown-history random retention without reseeding, read-only one/many-voice consumption and mixed modulated/unmodulated amplitude slots |
| Tuning, glide and unison | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [TuningBehavior](../../tests/DspBasicsTest.cpp), [PerformanceBehavior](../../tests/PerformanceBehaviorTest.cpp) | Common tracked/fixed tuning and fractional fine values; Float32 fine inputs, tracked/stack base transport, moving coarse exponent words and stack-cache edit history; fractional glide/retrigger progress; pitch/filter unison layout, stacking/fixed-phase caches and pending random draw counts, adjacent Float32 fixed-phase/Spread coordinates with preserved local/shared draws; ordinary bend with/without LFO or bound dispatcher, channel isolation, omni ingress and CC121 |
| Oscillator reads, Ring and Sub | [OscillatorRead](../../tests/OscillatorReadTest.cpp), [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp) | Fixed 22 kHz frequency ceiling on both oscillators across sample rates, post-modulation ordering and FM/Ring/Sync/stack/sub paths; adjacent-sample/wrapped interpolation without phase advance; direct/adapter agreement; Ring product and FM precedence; OSC1-linked Sub phase while muted, pulse-width conversion across repeated/base/modulation/lifecycle edits, fractional level scaling across dial 0.5 and stacking |
| Pulse width | [PwmPrecision](../../tests/PwmPrecisionTest.cpp), [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp), [OscillatorRead](../../tests/OscillatorReadTest.cpp) | Reciprocal-multiply integer knots under both normalizations, fractional base/additional coordinates, wrap continuity, ordinary/stack adapter agreement without clock changes, shared/envelope input composition and OSC2 one-octave control response |
| FM phase and receiver | [FmPhaseCorrection](../../tests/FmPhaseCorrectionTest.cpp), [FmBehavior](../../tests/DspBasicsTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp) | Constant/ramped/held coefficient correction, bounded fractional drift, reset/rebase/settling and synchronized noise; common waveform/envelope/history policy, Ring bypass, wide-depth transport, integral mapping knots, base-dial slew horizon/completion and sounding-retrigger retention |
| Modulation routing | [ModulationInputs](../../tests/ModulationBehaviorTest.cpp), [RenderPolicy](../../tests/RenderPolicyTest.cpp), [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp) | Endpoint units, per-slot consumption, wheel beside admitted sources, shared/envelope/additional FM composition, exact partitions and missing-source neutrality; common deterministic composition and restricted unknown-random admission |
| Filter and oscillator envelopes | [FilterEnvelopePrecision](../../tests/FilterEnvelopePrecisionTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp), [EnvelopeBehavior](../../tests/DspBasicsTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp) | Real peak/sustain/release coordinates, peak rescaling and zero-peak continuity, integer normalization knots, shared reciprocal sustain normalization through loading and parameter/controller edits, OSC2 destination/timing and direct physical envelopes; ordered live cutoff setters, slew restarts, zero-frame requests, saturation recovery and sound replacement |
| Filter kernels | [FilterPath](../../tests/FilterPathTest.cpp), [FilterBehavior](../../tests/FilterBehaviorTest.cpp), [RenderPolicy](../../tests/RenderPolicyTest.cpp) | Analytic coefficient precision before Float32 storage, double diode impulse-response state below Float32 sample range, resonance-only cache edits versus forced frequency preparation, resonance-boundary crossings, repeated edits, direct-index/sample state agreement, low-rate cutoff stability, common coordinate history/reset and source admission around restricted resonance |
| Physical VCA and allocation | [AmplitudeBehavior](../../tests/DspBasicsTest.cpp), [AmplitudeInputs](../../tests/ModulationBehaviorTest.cpp) | Direct Voice phases/retirement, repeated note-off, assigned velocity retention, release-first allocation, fractional attack-duration edits, independent shared/additional factors, output alignment, partitions, release state and disabled neutrality |
| Delay and chorus | [DelayPath](../../tests/DelayPathTest.cpp), [ChorusPath](../../tests/ChorusPathTest.cpp), [EffectsBehavior](../../tests/EffectsBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp) | Integral/fractional edit-history reset equivalence, impulse endpoints, delay tone/type and slew partitioning/settling, chorus x1/x2/x4 coordinate continuity, rounded negative delay-coordinate boundaries and fractional impulse positions and cursor-independent weights across ring wraps, adjacent Float32 tempo tap endpoints, compatible modulation clocks with Float32 audio history, sweep direction changes and effect tails |
| EQ and output tone | [OutputTonePrecision](../../tests/OutputTonePrecisionTest.cpp), [EqualizerBehavior](../../tests/EqualizerBehaviorTest.cpp), [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp) | Integer-knot/fractional continuity, rate changes, cache/recall, near-neutral activity, bypass history, reset/prepare retention, and sample/context/stereo graph agreement |
| Pan and stage order | [StereoBalance](../../tests/DspBasicsTest.cpp), [PanInputs](../../tests/DspBasicsTest.cpp), [StaticPanInputs](../../tests/DspBasicsTest.cpp), [OutputPipelinePath](../../tests/OutputPipelinePathTest.cpp) | Static coefficient bounds, real per-slot pan selection/depth/offsets, read-only subregion consumption and core/unison placement; mono/stereo balance after nonlinear stages and cubic-pan order versus independent stage composition |
| Output lifecycle | [OutputPipelinePath](../../tests/OutputPipelinePathTest.cpp), [EffectsBehavior](../../tests/EffectsBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp) | Common wet kernels at integer/fractional supported-rate endpoints, unsupported-rate bypass, EQ retention/reset, startup gain through silence, nonlinear/EQ/pan/wet order and surrounding-buffer sentinels |

Oscillator pitch uses generated sine tables at multiple, including fractional,
sample rates with a 0.02 Hz tolerance for zero crossings and phase quantization.
GeneratedInput varies controls, play modes, unison, rates, callback lengths, MIDI
channels and events using a fixed input seed without changing the engine RNG.
It checks finite stereo output, buffer sentinels, unchanged state and release
below -100 dBFS stereo RMS in the final window after channel stops. Performance
release checks use the same bound; retained history need not become exact zero.

Effects cover delay, chorus x1/x2/x4, phaser and all six MultiFX modes. Biased
waveshapers can produce zero-input transients: channel isolation compares the
unexcited channel with its zero-input run. Cross-mixing chorus modes are checked
for reset rather than channel independence.

## Scope and maintenance

Generated checks establish the exercised API, numerical and ownership properties.
They do not establish unrestricted spectral/listening equivalence, arbitrary
callback-partition identity, unavailable random histories or every native
receiver/draw sequence. Recorded comparisons and preceding/candidate audio and
control checks remain separate under the [validation contract](../contracts/validation.md).

Keep this map grouped by responsibility and link to actual tests. Put build,
focused-test, sanitizer and SDK commands in the [test guide](../../tests/README.md),
architecture constraints in the relevant design/contract, and dated results in
validation records. Add self-authored regression cases without weakening the
requirement, and keep manual-only activities outside the automated inventory.

## File-backed preset management

`EditorLayoutTest.cpp` checks the current-preset header's bank/name formatting,
missing-bank fallback, UTF-8 text and edited marker. Native selected-row colors,
accent stripe and bold title remain platform presentation checks.

`PresetFilesTest.cpp` covers recursive loose/ZIP discovery with invalid rows,
registration identity, keep/replace/skip and apply-to-all, contained staged saving,
current-format fractional/master-gain preservation, independent baselines and
transactional wrapper-v1/v2 recall. `PatchCatalogTest.cpp` retains metadata and
source-bank ordinal checks and adds exact registration filters and compatibility
with existing managed-bank storage. Native adapters preserve these boundaries;
actual SDK dialogs and standalone teardown are platform interactions.

Dropped-ZIP refresh regressions cover ordinary folders, legacy storage folder
names, mixed managed/loose sources, and unrelated files named `banks`. Managed
IDs and metadata remain stable without suppressing newly copied source files.

OversamplingTest uses generated impulses, DC, sine/cubic signals and actual bridge
playback to check factor/state validation, fixed latency, folded-harmonic reduction,
audible pitch, note-off tails and host-rate LFO/envelope clocks. RealtimeBehavior
checks prepared factor changes and short/chunked callbacks without heap activity.

Phase-preserving quality checks in OscillatorReadTest cover base-frame state
and sample identity across waveform/Ring/Sync/sub/stack/control combinations,
signed FM paths beyond half a cycle, and an analytically known folded carrier.
OversamplingTest also compares actual bridge phase words across quality changes,
MIDI offsets, overlapping notes and releases with nonlinear filter/drive enabled.
These properties do not establish unrestricted source compatibility or remove
aliasing already embedded in the retained base-rate feedback model.

`RenderDiagnosticsTests` checks that the normal product probe ignores legacy DSP
bypass environment variables and rejects switches reserved for the separately
linked diagnostic probe. This does not validate native GUI/device operation.

`BuildVersionTests` checks release-tag and development-date metadata generation,
prerelease numeric fields and rejection of out-of-range plugin versions without
native SDKs. It validates generated metadata, not native bundle/DAW operation.

`PatchFormatTest.cpp` checks opaque header and named color decoding, duplicate
unknown keys, export after edits, renaming, project recall and metadata clearing
on replacement. Native library marker drawing requires manual GUI verification.

Editor bank-menu tests cover classification-only folders, direct sounds alongside
sub-banks, duplicate display names and root banks. Catalog tests distinguish direct
bank filtering from All banks. Flat-row tests cover indentation, disabled headings, This folder and empty-library
All banks. Header tests separate title/edit markers from bank text. Native popup
and header appearance require manual verification.

Automatic tag tests use self-authored names and controls to check word/prefix
boundaries, distinct Mono/Legato modes, Ring/FM precedence and deferred conflicts.
Catalog tests cover inference across legacy and version 200, combined search,
manual precedence, legacy sidecars, persistent suppression/restore and unchanged
source text and sounding processor state. Native tag colors and popup interaction
require manual verification; these tests do not establish classifier accuracy.

Editor layout checks verify shared result/header column boundaries and measured
text fitting, including UTF-8 truncation, narrow cells and preserved bank suffixes.
Native rendering and row tooltips require manual verification.

Multi-tag/cache tests check AND matching across manual and automatic tags, filter
composition and session recall, warm/new-instance cache reuse, same-size and
same-timestamp content updates, additions/deletions, ZIP-entry changes, cached
invalid-source diagnostics and corrupt-cache rebuilding without losing overrides.
Portable checks do not certify native menu interaction or scan timing on other
machines. No filesystem watcher is part of this contract.

Header presentation tests check selected patch tags, manual/automatic origin and
complete tooltip text, including sounds without bank metadata. Native three-line
header appearance requires manual verification.

Library navigation tests cover empty results, initial candidates, clamped ends,
page movement, first/last jumps and stale indices. Native keyboard checks must
verify focus/text-entry ownership, candidate-versus-loaded display and Enter-only
audition; portable index tests do not establish host shortcut behavior.

Library scroll tests cover fractional offsets, empty/short lists, full final
viewports and minimal candidate reveal across viewport edges. Native clipping,
scrollbar dragging, wheel deltas and animation timing require manual checks.

Bank navigation tests verify menu-order stepping, classification heading skips,
selectable parent folders, All banks, empty catalogs and non-wrapping ends.

Editor resize tests cover proportional scaling, platform pixel ratios, keyboard
height, uniform fitting to both parent bounds for every platform and format,
wide/tall viewports, one-pixel reductions, limits and zero sizes.
Display-scale tests cover percentage parsing/formatting, invalid/out-of-range
edits, preset choices and readouts from accepted bounds independently of OS DPI.
Repeated keyboard-size requests retain their scale across SDK pixel rounding;
actual one-pixel parent reductions still recompute the fit.
Native window notifications, dragging and scaled hit testing require manual checks.

`ModulationInputs.RingRetainsPhysicalFmSlewAndResumesWhenDisabled` checks Ring
receiver retention, resumption and independence between physical FM states.

`Arpeggiator.OrderedFullGateKeepsIdlePeriodsWithoutRandomDraws` covers all three
ordered modes, callback offsets and sizes, key release, sustain release, stop and
disable. `ModulationInputs.GlidePublishesNestedFmDispatchSpansWithoutChangingPitch`
checks nested upstream/glide boundaries, pitch preservation and FM dispatch timing.

FM phase tests cover integer depth with fractional sources, adjacent product-knot
interpolation and fractional-depth synchronized-noise histories. Filter precision
tests separate retained real levels from discrete retrigger anchors.

The normalized LFO generator/router and their projection-only tests were retired
with that API. Their source-specific callback-invariance test does not apply to
compatible receiver queries. Common clocks retain native setter/query/advance
cadence; do not infer block-size-independent LFO audio from gain-horizon tests.
Physical ARP gain-fragment checks, receiver-knot/phase tests, history/draw-order
checks, and end-to-end public playback coverage remain in the suite.

`Arpeggiator.RepeatedTempoChangesRescaleWholeSampleCounters` checks integer
counter truncation across repeated edits, modes, gates, callback partitions and
long-running absolute clocks. `Arpeggiator.PeriodEditsRecomputeGateFromRescaledCounter`
checks gate reconstruction; `Arpeggiator.PeriodRescalingHandlesSaturatedSlowTempo`
checks saturated slow-tempo transitions. Membership cursor, first-key velocity,
held-key press order, non-panic bypass and live gate edits have dedicated
`ArpeggiatorBehaviorTest.cpp` properties.

`PerformanceBehavior.SameTimeLegatoReplacementRetainsPhysicalAmplitudePhase`
checks the actual bridge's physical VCA phase/level/counter through a same-time
Off/controller/On replacement against the same partition without replacement.

Polyphonic keyboard admission is exercised through the portable bridge with
permuted simultaneous chords, repeated/cancelled pending keys, capacity limits
and a pending key released under sustain in `PerformanceBehaviorTest.cpp`.
Physical frequency words and paired generated-input audio establish admission
order and absence of extra starts without captured original outputs.

`PerformanceBehaviorTest.cpp` checks actual-bridge Fine refresh/zero-band rules
and the FM base setter's full slew horizon. `DelayPathTest.cpp` checks completion
of long integer/fractional delay-time slews without rounding stalls, alongside
partition invariance, reset history and impulse endpoint properties.

`PerformanceBehaviorTest.cpp` checks bend state through the actual bridge for
large callbacks and varied MIDI offsets, including positions around 256 frames.
An explicit MIDI-region pair checks audio equality only when internal spans
match; arbitrary callback partitions retain their documented DSP sensitivity.

`PerformanceBehavior.LargerLegatoGroupAdmitsCoresWithoutRetriggeringContinuingCores`
loads a generated larger Unison program through the actual bridge. It checks
retained amplitude counters and filter tracking in continuing cores, new starts
in added cores and advancing physical phases. It does not establish arbitrary
program/state-transition audio equivalence.

`PerformanceBehavior.ReturningFromMonoDoesNotResumeRelinquishedPolyphonicNotes`
checks retained paused core words through mono/poly program loads and verifies
that subsequent poly admission reuses the relinquished group.

`PerformanceBehavior.StateRestoreRetainsTheReleaseFromThePrecedingMonoGroupBoundary`
recalls a generated mono state after a smaller poly group. It checks the partial
physical release and subsequent legato admission/filter-note retention through
the actual bridge; it does not establish unrestricted state-recall equivalence.

`PerformanceBehavior.PedalUpReselectsTheNewestHeldKeyAfterPolyToMonoRecall`
checks the physical pitch after pedal-up when a newer held key was admitted
during a polyphonic interval and a mono state was recalled.

`PerformanceBehavior.IdleLanesFreezeAndLeaveTheSurvivingVoiceBeforeReuse` checks
physical phase/controller/filter-memory retention with a partially idle Unison
group at 1x/4x, continued held-core progress, legato reactivation and unique
physical-core views when a new polyphonic note claims the freed lanes.

Poly admission regressions cover newest-note identity for Unison groups within
physical budgets, including non-divisible budgets and partial groups. Mono
reselection covers first entry from Poly through program and state recall,
Note Off and pedal-up, for Mono and Legato modes.

`FilterResponse.IndexedSpansPreserveFixedSettingsBypassAndRetainedHistory`
compares span configuration against repeated sample configuration bit for bit,
including changing modes, resonance and saturation, bypass/resume and retained
filter memory at integral and fractional sample rates.
