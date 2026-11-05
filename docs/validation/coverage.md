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
| Source loading and versions | [LoadingBehavior](../../tests/LoadingBehaviorTest.cpp), [PatchImportWorkspace](../../tests/PatchImportWorkspaceTest.cpp), [PatchFormat](../../tests/PatchFormatTest.cpp): all accepted versions, resolved-dial export, fractional version 200 patch/state recall, locale-independent decimal parsing/range rejection and malformed-input rejection | Acceptance of 1–113 is distinct from independently observed native conversion |
| Bank handling | [PatchLibrary](../../tests/PatchLibraryTest.cpp) and [BankBoundary](../../tests/BankBoundaryTest.cpp): ZIP/file rejection, exact entry/expanded-byte limits, CRC rejection, symlink filtering, natural ordering, UTF-8, source retention, detached-bank recall | Broad adversarial ZIP fuzzing is not exhaustive |
| State and parameter ABI | [ParameterContract](../../tests/LoadingBehaviorTest.cpp), [ProcessorState](../../tests/LoadingBehaviorTest.cpp) (including patch-name recall and legacy source-title fallback), [StateRepresentation](../../tests/StateRepresentationTest.cpp) (child order, equivalent numeric spellings, host C-locale independence and legacy finite-number syntax), generated truncated payloads, all-parameter live edits | Covers ProcessorBridge state and host parameter definitions |
| Persistent sound library | [PatchCatalog](../../tests/PatchCatalogTest.cpp) (imports, duplicates, folder ordering, persistent tags/favorites, combined filters, removal and source-free recall) | UI-side storage and filtering; native interaction remains manual |
| Live changes | [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp): all host parameters at endpoints/midpoint, live program loads, recall and rejected loads; integer sub-level audio across legacy/continuous states and fractional sub-level interpolation/recall | Arbitrary host automation schedules are not exhaustively enumerated |
| MIDI and voice lifetime | [PerformanceBehavior](../../tests/PerformanceBehaviorTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp), [GeneratedInput](../../tests/GeneratedInputTest.cpp): exact dispatcher timeline, sustain, CC121, stops, overflow, polyphony/unison | Timeline and lifetime checks cover the exercised schedules |
| Instance ownership | [InstanceIsolation](../../tests/PlaybackBehaviorTest.cpp): interleaved noise/random/ARP, unrelated instance creation/destruction, edits/reprepare of another instance, parallel separate instances | Calls into the same bridge must remain serialized |
| Callback heap use | [RealtimeBehavior](../../tests/RealtimeBehaviorTest.cpp): first prepared callback, source automation, version changes, float/double, zero/oversize blocks, MIDI/history overflow and drain, parameter setters and MIDI publication | Intercepts global C++ new/delete; Linux also wraps static project malloc/calloc/realloc/free references. Shared-library/OS allocation, locks/I/O, drivers and hard real-time deadlines remain outside this instrumentation |
| Oscillators, envelopes and filters | [DspBasics](../../tests/DspBasicsTest.cpp), [FilterBehavior](../../tests/FilterBehaviorTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp) pitch across rates | Does not establish unrestricted spectral or original-engine identity |
| LFO and shared clocks | [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp): all waveforms, bounds/reset, zero depth, tempo ratio, key sync/overlap, disabled shared-clock advancement and read-only consumers | Disabled waveform sources pause; shared native clocks can keep advancing. These are source/ownership properties rather than recorded RNG fingerprints |
| History publication | [HistoryQueue](../../tests/HistoryQueueBehaviorTest.cpp): FIFO retention, drop counters, repeated saturation/reuse, concurrent producers, bridge invalidation and post-drain edits | A drained queue accepts new events; it does not reconstruct discarded execution history |
| Effects/output lifecycle | [EffectsBehavior](../../tests/EffectsBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp), [EqualizerBehavior](../../tests/EqualizerBehaviorTest.cpp): bypass/reset, isolated modes, tails, rate setup, silence gain and output regions | Component reset and cold-start checks do not establish every conditional Engine history |
| Editor source binding | [EditorInteraction](../../tests/EditorInteractionTest.cpp): shared native choice/toggle logic, every selector, cancellation, editing steps, fractional unit displays, host values and recall; [EditorLayout](../../tests/EditorLayoutTest.cpp): layout/labels/units, hidden metadata and Synth/MIDI/Library views | Covers shared source-selection logic, bridge binding and presentation |
| CI preparation and candidate archives | BuildToolsTests: local Git SDK fixtures with CRLF preservation/reuse and edited-code rejection; generated product files, missing-binary rejection, metadata/notices, unrelated-file exclusion, overwrite rejection and POSIX bundle modes/symlinks | Self-contained preparation/package contracts; no network or real SDK required, and no native build or DAW pass established |
| Memory and undefined behavior | AddressSanitizer/UndefinedBehaviorSanitizer/LeakSanitizer local C++ runs; generated input sentinels | Compiler, architecture and platform coverage must be recorded per run |

## DSP and receiver coverage

This table groups generated component and bridge checks by responsibility. It is
an index to test sources, not a record of successive implementation changes.

| Area | Test entry points | Exercised properties |
| --- | --- | --- |
| Import and callback mapping | [PatchImportWorkspace](../../tests/PatchImportWorkspaceTest.cpp), [ProcessorState](../../tests/LoadingBehaviorTest.cpp), [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp) | Ordinary/workspace decoding across accepted versions; exact integer promotion including packed MIDI/out-of-host-range words; sparse defaults; fractional state-mode retention and resolved EQ/Delay/Chorus recall |
| Numerical precision | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp), [DspBasics](../../tests/DspBasicsTest.cpp) | Gain/velocity/saturation anchors, fine tuning, 27-bit phase storage, real signal transport versus discrete spans, fractional effect controls and decimation remainder; real envelope progress, retrigger anchors, release endpoints and final silence |
| LFO depth, rate and history | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp), [LfoPhasePrecision](../../tests/LfoPhasePrecisionTest.cpp) | Fractional depth/periods, discrete synced divisions, held spans, sine phase interpolation/wrapping and integer knots, key sync, one-draw random projection, held/smoothed targets and unchanged RNG advancement |
| Shared source ownership | [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp) | First-key resets after MIDI, overlapping keys/channels, subregions, unknown-history random retention without reseeding, read-only one/many-voice consumption and mixed admitted/fallback amplitude slots |
| Tuning, glide and unison | [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp), [TuningBehavior](../../tests/DspBasicsTest.cpp), [PerformanceBehavior](../../tests/PerformanceBehaviorTest.cpp) | Common tracked/fixed tuning and fractional fine values; fractional glide/retrigger progress; pitch/filter unison layout, stacking/fixed-phase caches and pending random draw counts; ordinary bend with/without LFO or bound dispatcher, channel isolation, omni ingress and CC121 |
| Oscillator reads, Ring and Sub | [OscillatorRead](../../tests/OscillatorReadTest.cpp), [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp) | Adjacent-sample/wrapped interpolation without phase advance; direct/adapter agreement; Ring product and FM precedence; OSC1-linked Sub phase while muted, fractional level scaling across dial 0.5 and stacking |
| Pulse width | [PwmPrecision](../../tests/PwmPrecisionTest.cpp), [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp), [OscillatorRead](../../tests/OscillatorReadTest.cpp) | Reciprocal-multiply integer knots under both normalizations, fractional base/additional coordinates, wrap continuity, ordinary/stack adapter agreement without clock changes, shared/envelope/additional input composition and one-octave reserved both-pitch range |
| FM phase and receiver | [FmPhaseCorrection](../../tests/FmPhaseCorrectionTest.cpp), [FmBehavior](../../tests/DspBasicsTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp) | Constant/ramped/held coefficient correction, bounded fractional drift, reset/rebase/settling and synchronized noise; common waveform/envelope/history policy, Ring bypass, wide-depth transport, integral mapping knots, base-dial slew horizon/completion and sounding-retrigger retention |
| Modulation routing | [ModulationInputs](../../tests/ModulationBehaviorTest.cpp), [RenderPolicy](../../tests/RenderPolicyTest.cpp), [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp) | Endpoint units, per-slot consumption, wheel beside admitted sources, shared/envelope/additional FM composition, exact partitions and missing-source neutrality; common deterministic composition and restricted unknown-random admission |
| Filter and oscillator envelopes | [FilterEnvelopePrecision](../../tests/FilterEnvelopePrecisionTest.cpp), [ContinuousLevel](../../tests/ContinuousLevelTest.cpp), [EnvelopeBehavior](../../tests/DspBasicsTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp) | Real peak/sustain/release coordinates, peak rescaling and zero-peak continuity, integer normalization knots, OSC2 destination/timing and direct physical envelopes; ordered live cutoff setters, slew restarts, zero-frame requests, saturation recovery and sound replacement |
| Filter kernels | [FilterPath](../../tests/FilterPathTest.cpp), [FilterBehavior](../../tests/FilterBehaviorTest.cpp), [RenderPolicy](../../tests/RenderPolicyTest.cpp) | Resonance-boundary crossings, repeated edits, direct-index/sample state agreement, low-rate cutoff stability, common coordinate history/reset and source admission around restricted resonance |
| Physical VCA and allocation | [AmplitudeBehavior](../../tests/DspBasicsTest.cpp), [AmplitudeInputs](../../tests/ModulationBehaviorTest.cpp) | Direct Voice phases/retirement, repeated note-off, assigned velocity retention, release-first allocation, fractional attack-duration edits, independent shared/additional factors, output alignment, partitions, release state and disabled neutrality |
| Delay and chorus | [DelayPath](../../tests/DelayPathTest.cpp), [ChorusPath](../../tests/ChorusPathTest.cpp), [EffectsBehavior](../../tests/EffectsBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp) | Integral/fractional edit-history reset equivalence, impulse endpoints, delay tone/type and slew partitioning/settling, chorus x1/x2/x4 coordinate continuity, fractional impulse positions and effect tails |
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
