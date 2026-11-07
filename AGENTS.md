# Working on Synth2

## Communication and project context

- Write code, documentation and commit messages in English.
- Start with [capabilities and verification boundaries](docs/status.md) and the
  [product contracts](docs/README.md). Read the relevant contract and independent
  evidence before changing compatibility behavior.
- Maintain public GitHub Actions checks for portable tests and native build
  candidates. Validate locally while developing; CI build success does not certify
  GUI/DAW/device behavior. See [continuous integration](docs/ci.md).
  Let normal pushes run CI; use skip directives only when the owner explicitly
  requests pausing Actions for the current work.
- Desktop configurations target Windows VST3/CLAP and macOS VST3/CLAP/AUv2,
  plus standalone APP on those platforms. Linux targets VST3/CLAP/APP.
  All three platforms use the same pinned development branch of iPlug2;
  see the desktop build guide. Official macOS builds target
  Apple Silicon arm64 only; keep the iPlug2 universal build option disabled.
  Use the iPlug2OOS layout in `projects/Synth2`; keep JUCE out of production,
  tools and the portable build graph. Keep all product source there; the portable
  core lives in `projects/Synth2/src` and remains independent of iPlug2 headers.
  Public tests use the actual portable bridge with self-authored inputs and
  public product requirements. Tests must be self-contained and must not require
  another repository or recorded original-engine output. See
  [desktop build requirements](docs/desktop-build.md). Pin the iPlug2 submodule
  and lock entry together; keep external IPLUG2_DIR overrides and SDK-free portable
  builds supported. Do not auto-fetch SDKs from CMake. Keep project compiler
  requirements target-scoped and offline tools selectable independently of native
  products. Portable bridge tests do
  not establish that the native iPlug2 plugins build or run in a DAW.
- Keep the GUI's Synth1-inspired functional groups on one main Synth surface;
  MIDI routing and the library use supplementary views. Keep reserved numeric fields
  out of the editor while preserving host IDs and saved values. Preserve opaque patch
  headers through export and recall; show named patch colors in the library. Named controls
  must preserve source selection values and host parameter IDs/ranges; continuous
  controls may retain fractional values in version 3 processor states.
  Use integer host/UI editing steps for discrete choices and counts; preserve
  source ranges and recalled bridge values rather than reindexing sparse enums.
  See the [shared native/preview layout](docs/gui/README.md).
- The GUI should suggest Nord Lead 2 subtly through restrained red accents,
  warm white legends and black controls. Keep Synth2's own identity and give
  high-use controls visual priority; keep oversampling as a quiet footer selector
  beside Keyboard; use shared readable type sizes across Synth/MIDI/Library,
  reserving room for labels and values rather than shrinking choice text.
  Window-frame dragging scales the complete GUI just like the corner handle;
  retain logical bounds and platform-scale handling without adding zoom menus.
  Do not introduce unsupported instrument
  features merely to match a reference panel. Bundle the editor fonts rather than
  depending on installed system font families; see the
  [editor contract](docs/gui/README.md).
- Imported sounds accumulate in a user library. Keep tags/favorites separate
  from original `.sy1` files and DAW sound state; combine bank, tag and keyword
  filters without loading a sound. Infer tags on the catalog worker from names and
  resolved dials; retain manual precedence and persistent hidden-tag overrides.
  Cache derived tags/validation by exact source contents; prune on scan and bump
  the cache revision when inference or resolved loading semantics change. Match
  all selected tags and preserve legacy single-tag session recall.
  Keep keyboard browsing separate from audition: arrows/pages move a candidate,
  Enter loads it, and Ctrl/Command+F focuses search without intercepting text entry.
  Use a continuous list with a draggable scrollbar and minimal candidate reveal,
  not page buttons or page-aligned jumps; wheel input retains fractional deltas.
  Show automatic tags in muted text within the shared tag list. See [library behavior](docs/gui/patch-library.md).
  Do not bundle or privilege a factory bank. Start with Init and an empty library;
  all added banks share the same import and metadata behavior. Use a hierarchical
  bank list with indentation, classification-only parents, direct-bank selection
  and All banks; group previous/next bank buttons on the selector's right in selectable menu order,
  preserving other filters and the sounding patch; do not use submenus. Separate the header patch title from its bank line. File deletion and
  renaming belong in the system file manager, not editor removal controls.

## Architecture boundaries

- Run `python3 tools/lint_naming.py` (also part of portable CTest) for portable
  C++ filenames/types, including patch/core. Use PascalCase, Settings for processing
  settings and Controls/Input/State/View for their roles; do not add Params,
  Parameters or Config type suffixes. Identifier meaning remains a review concern.
- Reserve the Synth1 prefix for patch/source loading and conversion interfaces.
  Name product DSP, render controls and runtime state by their responsibility;
  preserve compatibility evidence and numerical behavior when renaming them.
- Keep patch parsing, source-version conversion, value mapping, render-policy
  selection and DSP processing separate. Select version conversion strategies
  at load time; do not spread file-version checks through DSP kernels.
  Synth1PatchMapper is shared across source versions; version-specific behavior
  belongs to the conversion strategies, not version-named mapper subclasses.
- `RenderControls` contains render controls only. Source version, normalized source
  values and mapping caches belong to the loading adapter. Load-history context
  is separate; diagnostic snapshots must not become Voice or Engine parameters.
- Give DSP components narrow settings, caller-owned state and read-only control
  views. Do not make them depend on the full render aggregate, Voice, Engine,
  APVTS or patch documents. Adapt aggregate settings in `projects/Synth2/src/render`.
- SharedModulationRuntime is Engine-owned. Keep its blocks/clocks read-only for
  consumers and preserve the ARP binding to the same native CRT stream. History
  setters, receiver queries and advances have distinct native ordering contracts.
- Keep MIDI route configuration in the loading/render adapters and running
  controller inputs in ControllerRuntime. Performance offsets must not enter
  mapper caches, source getters, APVTS or saved patches; restore mapped render
  controls at the callback boundary.
- Keep sustain-held upstream keys distinct from generated ARP notes and Voice
  key flags. Equal-value controller setters can still partition the renderer or
  restart a receiver; combine root span limits before advancing shared state.
- Native input has one omni keyboard/performance state. CC121 resets ordinary
  bend separately from routed pitch, wheel and pedal state. Preserve per-note
  velocity gain and actual pooled/local core ownership across program changes.
- Use one compatible shifted-note/tracked/fixed tuning and physical-core Portamento
  for every source and direct caller. Keep one ordinary PitchBendSmoother formula,
  channel-owned by Engine (the same local receiver for direct Voice callers), with
  native7-bit slew/32-frame root spans and separate14-bit routed pitch. Do not
  restore tuning flags, Voice exponential glide, a duplicate portamento-admission
  flag or direct wheel target/amount. Ordinary bend advancement must not depend
  on an LFO provider; bind a borrowed channel receiver only when the dispatcher
  has render controls to advance it.
- Native mono and poly notes share physical core storage. Mono admission pauses
  higher groups; logical retirement must not erase their receiver histories.
  Keep OSC2 coarse exponent, cached frequency and pending-note reset distinct;
  its wrapper partitions FM calls and refreshes differently on legato assignment.
- A changed effective voice count resets the physical pool allocation cursor;
  it does not erase retained receiver state. Equal-count loads preserve the cursor.
- Native ARP stop messages clear held keys/pedal but restart an empty clock and
  consume its shared draw. Absent-key Note Off can also restart that clock; keep
  upstream LFO key/pedal tracking consistent with the ARP keyboard. Deterministic
  LFO waveforms do not invalidate the native ARP clock in mono/legato; receiver
  eligibility and running-history validity remain separate decisions.
- Keep VoiceCore, VoiceCorePool and VoiceDispatcher declarations in their own
  headers; pool/dispatcher declarations borrow forward references. LfoBlock and
  ArpeggiatorSettings are value-only inputs independent of their generators and
  the render aggregate. Preserve these dependency boundaries with the header audit.
- Preserve Engine ownership of shared RNG/clocks and Voice ownership of note/core
  lifecycle. Keep physical core history distinct from logical note lifetime.
- OscillatorSection owns common native FM phase correction and its local compatible
  Sync reference. Keep audible correction separate from slave/noise reset timing;
  explicit phase starts reset correction. Keep FM arithmetic independent of LFO
  waveform/envelope stage, preserving receiver timing and coefficient steps without
  an extra filter. Phase feedback is an explicit compatibility constraint, not
  unbiased fractional integration. PWM interpolates compatible base/control knots
  and shares real-offset reads across ordinary/stack oscillators, retaining each
  path's phase lookup and gain rules. See the audio contract.
- Optional audio oversampling is instance quality configuration, separate from
  patch controls. Keep 1x as the unchanged default; 2x/4x/8x/16x must not accelerate
  MIDI, shared clocks, control envelopes or intentional decimator timing. Keep
  oscillator/FM/Sync and noise state on the base-frame trajectory; reconstruct
  signed intermediate phases for fine waveform reads. Report
  the actual resampling latency and retain prepared, allocation-free callbacks.
- Preserve real-valued VCA Attack targets. Do not restore integer Attack
  truncation to reduce distortion-sensitive reference errors; keep audio
  comparisons and their unresolved bounds separate from the precision choice.
- Every Voice uses its physical-core VCA, including direct callers. Keep one
  absolute gain/velocity configuration and aligned per-core gain block; do not
  restore a sample-clocked amplitude envelope, separate release clock or an
  amplitude-selection flag. Keep pending VCA gain interpolation separate from
  envelope lifetime and shared-clock setter/advance cadence. Only projected
  amplitude inputs or generated ARP events request an extended gain horizon;
  ordinary physical dispatches use their actual length. Anchor ARP gain spans to
  Engine time and cap every repeated span at known input/ARP/receiver events,
  including silent gates; lookahead must not advance clocks or draw RNG words.
  Capture velocity sensitivity at note assignment;
  running control edits retain that reference. At full pool capacity, reuse a
  physical Release before stealing a held core, keeping oldest order within each
  group. Explicit offline bypasses belong to the separately linked DiagnosticProbe core;
  keep the normal product independent of DSP bypass environment variables.
- Use physical-core filter and oscillator AD envelopes for every caller. Resolve
  oscillator timing/depth from dials and destination; do not restore parallel
  sample envelopes or approximate modulation-envelope math in the router. Keep
  history admission for reuse/receiver transport and preserve remaining generic
  LFO/performance contributions without double-counting admitted receivers.
- Static and modulated FM share physical-core FrequencyModulation state, receiver
  partitions and downstream phase correction. Do not restore a Voice-owned
  static smoother or an audio depth selector. Preserve base-setter horizons,
  idle settling, sounding retrigger invalidation and live slew retention.
- Resolve modulation consumption per LFO slot in render adapters. Convert remaining
  prepared normalized signals through compatible destination/depth endpoints to
  physical receiver units before FM/filter transfer and pitch/PW mapping. Preserve
  native scheduling views, change boundaries and missing-source neutrality; do not
  restore provisional FM phase multipliers or history-wide input suppression.
- Pan LFO belongs to the output receiver for every source. Engine prepares real
  pan projections at each MIDI subregion; select admitted/projected values per
  slot without duplicate application or source-history promotion. Keep output
  chunks within prepared data and preserve native region timing, semantic silence,
  stereo-only behavior, device history and core/unison placement. Static global
  balance also belongs to output after MultiFX/Tone and before wet effects for
  every source, retaining its mono coefficient average. Do not restore a
  history-based static-balance enable, Voice global gain or summed-unison global
  balance factor. Do not restore
  per-voice global pan offsets or re-project using a later host-block depth.
- Keep logical Voice lifecycle separate from physical VCA/filter/FM history.
  Reuse strategies inspect physical VCA activity; logical state alone must not
  reset or skip receiver histories. Preserve pending-transition consumption points.
  Connected full-gate ARP assignments include observed random order: same-time
  note-off/on pairs must not release the core between assignments.
- OutputPipeline uses only MultiEffect/Phaser, Equalizer and EffectPair; keep
  removed experimental effect and alternate Delay/Chorus implementations out of
  the product. Unsupported wet rates bypass without an alternate algorithm.
- OutputPipeline owns output-device history; Engine decides its region and
  prepare/release boundaries. Preserve the existing retention conditions.
- Common render-policy composition resolves controls directly and retains
  explicit noise/core-reset decisions. Run restricted oscillator/filter/performance
  admission only for unadmitted random histories, preserving its existing phase
  order. Resolved deterministic LFOs and mapped oscillator envelopes use common
  receiver composition independently of source format; retain source admission
  and history checks together for random sources.
  Oscillator table reads use one adjacent-sample interpolation path, and mapped
  amplitude uses one gain curve/VCA path. Do not restore source-format or ARP
  switches for table averaging or gain arithmetic. Ring uses one product-mix kernel and
  takes precedence over FM; keep waveform-specific wrapper gains explicit. Sub
  uses OSC1-linked full phase and one level/normalizer mapping, including while
  muted. Do not restore an independent sub oscillator or Ring/sub mode selectors. Deterministic amplitude LFO receivers
  share clock/control/VCA processing across source formats; retain random-history
  admission separately. Route admitted/additional amplitude attenuation through
  independent physical VCA slots, preserving held spans, signal changes and
  alignment; do not restore a postmix tremolo factor or shared-renderer selector.
  StereoBalance shares static coefficients only; preserve core/unison/output
  placement, multiplication order and PanLfo cubic/slew state.
  Every filter route uses the same compatible mode kernels and canonical output
  tone/peak processing. Direct EQ callers share that graph; do not restore RBJ or
  fitted Tone alternatives. EQ controls contain no algorithm or derived-enable
  selector. Startup output gain always advances, including bypassed/silent regions. Keep source/history-aware control scheduling separate from
  audio kernel selection; do not restore approximation-specific gain correction.
  Engine owns every LFO source, including fallback and key-synchronized inputs.
  Voice input adapters borrow prepared signals and receiver views without owning,
  resetting or advancing clocks. Generate shared signals after MIDI dispatch.
- Keep output processing order, semantic silence and lifecycle retention intact.
  Silent input does not imply an empty effect tail or permit skipping gain
  advancement. Shared LFO views are read-only throughout the output pipeline.
- Preserve loading support for all accepted legacy source versions (1–113) and
  named unversioned files when changing conversion or mapper classes. Synth2
  patches use version 200 with resolved fractional dials; keep that codec and its
  direct mapping separate from Synth1 integer source conversion.

See [source strategies](docs/architecture/source-conversion.md),
[render boundaries](docs/architecture/render-boundaries.md),
[Voice audio components](docs/architecture/voice-audio-components.md) and
[Voice orchestration](docs/architecture/voice-orchestration.md) and
[output pipeline](docs/architecture/output-pipeline.md) and
[Voice lifecycle](docs/architecture/voice-lifecycle.md) and
[shared modulation ownership](docs/architecture/shared-modulation.md).

- Keep serialized ProcessorState decoding/validation separate from applying a
  state to ProcessorBridge. Share locale-independent numeric I/O without changing
  the host's global locale or tightening legacy state spellings to patch syntax.
  Keep historical family-model corrections in the legacy typed-import adapter;
  resolved native control decoding must not apply those corrections again.
- Keep SDK-independent presentation in src/editor under synth2::editor. Native
  drawing controls and library/menu/dialog adapters belong to projects/Synth2/editor;
  Synth2Editor coordinates layout and lifetime. Preserve shared control tags,
  catalog/selection ownership and weak dialog-lifetime guards when splitting files.
  Audit presentation headers against host/engine/SDK dependencies.

## Build versions

- Derive release metadata from Git tags; keep version numbers out of maintained
  CMake project/config/resource sources. Local builds use dated development labels.
  Generate plugin version words and native resource metadata together; see
  [build versions](docs/desktop-build.md#build-versions).

## Public distribution

- Keep product specifications, build instructions and portable tests self-contained.
  Do not document companion investigation repositories or their workflows here.
- Project-owned source is GPL-3.0-or-later; keep the full GPL v3 text at the root
  LICENSE and the explicit or-later grant in README. Do not relicense third-party
  components or infer asset rights from the project license.
- Preserve authorship, source provenance and third-party notices. Record unresolved
  contributor-scope and asset/fixture distribution decisions; do not invent grants.
- Current-tree cleanup does not remove historical objects. Plan publication history
  separately without rewriting refs or changing visibility as routine maintenance.
  See [distribution requirements](docs/publication/README.md).

## Compatibility and real-time behavior

- Precision changes must preserve DLL-compatible legacy loading: resolve the
  integer parameter state first, then promote those exact words to floating point
  and apply fractional edits afterward. The compatibility boundary is loaded
  parameters, not a requirement to retain integer arithmetic throughout DSP.
- Keep real-valued receiver signals separate from discrete sample/span metadata;
  do not narrow fractional data when adapting DSP components. Precision extensions
  should share real-state calculations across integral/fractional inputs; keep
  necessary native control-knot mapping explicit and test edit-history continuity.
  Output DSP consumes real controls without a precision-mode flag; source/state
  adapters resolve legacy integer controls before publishing them.
- Complete audio identity is not required for precision improvements. Raw stereo
  normalized error below 0.005 passes normally. Values from 0.005 to strictly
  below 0.01 require a recorded review confirming an intentional precision change
  and preserved control/loading/update semantics; unexplained differences remain
  unresolved. Values at or above 0.01 fail. Keep the 0.005 target and report
  conditional acceptance separately. Do not fit gains/seeds or waive defects.

- Production behavior must follow control meanings and native evidence, never
  patch number, title, bank identity or source hash.
- When investigating audio differences, consider patch loading and resolved
  controls as well as DSP. Check DLL observations when existing evidence does
  not establish the original behavior; do not assume the importer is correct.
- Preserve floating/integer operation order, frequency rounding, fractional
  sample-rate handling, receiver segment boundaries, buffer alignment, RNG
  advancement and state lifetime. Treat changes to these as behavioral changes.
- Preserve all-core envelope → glide → FM → filter control order, and per-sample
  oscillator → noise RNG → filter → VCA/pan order unless native evidence justifies
  a change. Do not add allocations, blocking locks or I/O to audio callbacks.
- Do not fit gains or RNG seeds, align waveforms or change thresholds to
  conceal differences. Do not present aligned, downmixed or gain-fitted metrics
  as raw stereo compatibility evidence.

## Validation

- Follow [tests/README.md](tests/README.md) for build dependencies, test selection
  and headless execution. The portable suite must work without Synth1 or Wine:

  ```sh
  ctest --preset core -L portable
  ```

- Maintain the [verification map](docs/validation/coverage.md) when public contract
  coverage changes. Actual SDK GUI interaction, DAW sessions and physical-device
  checks are outside automated-test scope; do not list them as missing/pending
  automated tests. Keep the manual guide separate; portable success is not a native pass.
- Run focused public tests while developing, then freeze changes and run the
  applicable product suite and preceding/candidate comparisons. Use generated
  inputs and public product contracts; do not introduce external checkout,
  credential or original-recording requirements into this workflow.
  Repeat broad validation when new changes or failures justify it.
- For DSP or loading refactors, check candidate WAV byte identity and loaded
  control equality against the preceding implementation, as well as existing
  comparisons to the original. Passing an error bound alone does not establish
  that a refactor preserved behavior.
- Preserve independent test inputs and expected results during refactors. Do not
  generate new expectations from the changed implementation to hide a regression.
- Live MIDI comparisons must retain the exact event timeline, including equal
  timestamps and sub-block offsets, through capture, paired observers and replay.
  Single-note evidence does not establish repeated-note or controller behavior.
- Build affected targets; for shared synthesis changes include Probe,
  LoadingProbe, Tests and the affected iPlug2 formats/APP where a native SDK and
  toolchain are available. Record what was actually verified,
  distinguishing retained-corpus results from unrestricted audio compatibility.
- After changing component boundaries, run the DSP header audit:
  `python3 tools/audit_dsp_headers.py --build-dir build-oos-core`. It checks independent
  compilation and transitive dependencies.
- Product docs state current requirements, design constraints, verification scope
  and reproducible procedures. Do not include migration narratives or experiment
  journals; preserve provenance, legal notices and current compatibility limits.
  Do not hard-code current test counts or machine-specific temporary paths here.

See [validation requirements](docs/contracts/validation.md) and
[development tools](tools/README.md) for reference comparison workflows.

## Documentation contracts

- Write current product behavior, ownership, verification and limits in docs.
  Keep required behavior distinct from observations and implementation snapshots.
- Do not bless the current implementation or relax a bound to resolve a failure.
  Change a contract only for an explicit product decision or independent evidence.
- Do not use dated pass counts as normative product requirements. Keep docs focused
  on product behavior and reproducible checks rather than experiment histories.

## Maintaining these instructions

- Keep project-wide AI instructions in this file as the single source of truth.
  Keep technical details and measurement records in the linked documentation.

- Update this file as part of relevant work when durable architecture decisions,
  user preferences or development workflows change. Routine maintenance does not
  need a separate request from the user.
- Keep instructions concise and current: revise or remove superseded guidance,
  and link to detailed documentation instead of accumulating progress logs.
