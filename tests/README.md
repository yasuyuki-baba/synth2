# Public product tests

These tests use self-authored inputs, generated temporary files, public API/math
requirements and generated banks. No original-instrument observations,
recorded PCM, clock/RNG fingerprints or previous-implementation state archive is
part of this test tree. No external checkout, DLL, Wine or recording is required.

```sh
cmake --preset core
cmake --build --preset core --parallel 4
ctest --preset core
python3 tools/lint_naming.py
```

Requirements are CMake 3.25+, Ninja, a C++20 toolchain, Python 3, Git and the public
GoogleTest source (fetched initially, or supplied using
FETCHCONTENT_SOURCE_DIR_GOOGLETEST). The `core` preset uses `build-oos-core`.

| Entry | Public contract |
| --- | --- |
| PatchCompatibilityTests | Synthetic patch/state/host/library/editor, ARP and DSP properties |
| RealtimeBehaviorTests | Prepared callback, parameter setter and MIDI publication heap checks |
| GainRegressionTests | Analytic gain formula and self-authored Probe input |
| WaveComparisonTests | Generated WAVs and generic comparator behavior |
| MeasurementToolsTests | Standard-library WAV/hash/subprocess contracts |
| BuildToolsTests | Local Git SDK fixtures and synthetic native archive completeness, notices/metadata and bundle preservation |
| NamingLint / NamingLintTests | Portable C++ file/type spelling and setting-type suffixes, including patch/core; synthetic lint diagnostics |
| HeaderBoundaryAudit | GNU-style GNU/Clang frontends with Ninja/Makefiles: independent extracted-header compilation and forbidden transitive dependencies, plus DSP boundary audit |

CTest entries group the public checks. Compatibility in a test name refers to
supported product API/input behavior, not comparison
against recorded original-engine outputs.

Examples: state round trips restore the submitted values; failed loads preserve
state; filter high-pass/DC/reset properties use generated signals; ARP block
partitioning and held-key sorting use synthetic MIDI. Source/version behavior can
be checked as a public contract without bundling observed original outputs.

Public tests exercise the actual product core. Passing them establishes the
specified API and signal properties under the tested inputs; it does not establish
unrestricted original-instrument equivalence. See
[validation](../docs/contracts/validation.md).

## Additional product contracts

- `LoadingBehaviorTest.cpp`: every accepted explicit source version (1–113),
  including legacy 99/101–103, named unversioned input, comments/CRLF, rejection
  of out-of-range versions and malformed tokens,
  and atomic program/state preservation.
- `PatchFormatTest.cpp`: Synth2 version 200 float serialization, direct resolved
  mapping, patch/state stereo recall, legacy import/export and atomic rejection.
- `ParameterContract` in that file: all host parameter IDs, ordering, ranges and
  declared defaults, using explicit expectations independent of production helpers.
- `PerformanceBehaviorTest.cpp`: production MIDI dispatch with an observation
  voice checks absolute sample positions, equal-time ordering, pedal lifetime and
  subregion offsets across block sizes. Real bridge audio checks sustain and
  CC121; routed inputs must not alter saved state, source getters or mapped controls.
  Release acceptance is below -100 dBFS stereo RMS over the final test window;
  retained physical histories need not become exact zero.
- `ControllerBehavior`: summed route offsets, clamping and selective input reset.
- `EffectLifecycleTest.cpp`: generated impulses exercise pending delay echoes,
  explicit reset/rate reconfiguration, and output-gain advancement through silent
  regions without touching samples outside the requested region.

Dispatcher equality does not assert whole-engine byte identity across arbitrary
callback sizes. Native receiver partitions, buffer alignment and gain-finishing
boundaries are part of the render contract. These tests do not establish
plugin/DAW operation.

## DSP properties and generated-input coverage

`DspBasicsTest.cpp` supplies a mathematically generated sine table to the actual
oscillator and measures pitch at multiple rates, including a fractional rate.
Pitch tolerance is 0.02 Hz for interpolated zero crossings and phase quantization.
It checks octave tuning/reset and the production amplitude/filter envelope
lifecycles, bounded levels, inversion and requested segment coverage. These are
instrument properties, not original-engine sample/trace comparisons.

`EffectsBehaviorTest.cpp` checks pipeline bypass, disabled wet devices, isolated
stereo modes, cold preparation and explicit reset. It covers delay, chorus x1/x2/x4,
phaser and all six native MultiFX modes at representative dial endpoints/rates.
Biased waveshapers can generate zero-input transients: channel isolation compares
the unexcited channel against its zero-input run rather than requiring zero.
Cross-mixing chorus modes are tested for reset, not channel independence.

`GeneratedInputTest.cpp` uses a fixed input-generator seed for reproducibility,
without changing the engine RNG. A bounded matrix varies source controls, play
modes, unison, sample rates, callback lengths, MIDI channels and controller events.
It checks finite stereo output, buffer sentinels, unchanged serialized state and
release below -100 dBFS RMS after channel stops. Generated malformed numeric
fields and truncated state payloads must preserve the selected program/state.
These finite samples are not exhaustive fuzzing or a real-time deadline proof.

For an additional local memory/undefined-behavior check with GCC or Clang:

```sh
cmake -S . -B build/sanitized -DSYNTH2_BUILD_IPLUG2=OFF \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/sanitized --target Synth2PatchTests --parallel 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ./build/sanitized/Synth2PatchTests
```

Actual GUI/DAW/device checks are outside automated-test scope and are not tracked
as missing tests. An optional [manual guide](../docs/native-validation.md) is available.

## Live operation, ownership, editor binding and callback allocation

See the [verification map](../docs/test-coverage.md) for requirements and limits.
`PlaybackBehaviorTest.cpp` checks live edits/loads/recall and independent instances,
including simultaneous rendering on separate threads. It does not make a single
ProcessorBridge safe for concurrent calls.

`ModulationBehaviorTest.cpp` checks local sources and the actual shared clock
facade. `EditorInteractionTest.cpp` exercises the same source selection/toggle/
label helpers used by the native editor and sends their source values through the
real bridge. Actual SDK mouse/keyboard/host notification checks are manual-only
and excluded from the automated coverage inventory.

An optional check uses the actual pinned iPlug2 parameter class to verify integer
versus continuous host registration, sparse choice names, full source ranges and
MIDI source words. It needs the public `iPlug2` submodule but no plugin SDK, GUI or
audio device. On Linux with a C++20 compiler, run from the repository root:

```sh
mkdir -p build/parameter-sdk
c++ -std=c++20 -DNDEBUG -I iPlug2/IPlug -I iPlug2/WDL -I projects/Synth2 \
  tests/sdk/ParameterSetupTest.cpp iPlug2/IPlug/IPlugParameter.cpp -pthread \
  -o build/parameter-sdk/ParameterSetupTest
build/parameter-sdk/ParameterSetupTest
```

This is an SDK parameter-class check, not native plugin/host acceptance. The
portable core preset remains independent of the submodule.

`Synth2RealtimeTests` is a separate executable with thread-local interception of
global C++ new/delete (including arrays, aligned and nothrow forms). Setup, state/patch I/O, assertions and inspection stay outside measured callbacks.
Separate checks also include parameter setters and MIDI publication in the measured scope. A positive tracker
check prevents a silently inactive instrument. Prepared processing must perform
neither C++ allocations nor deallocations in the measured scenarios. On Linux with GNU-compatible linking it also intercepts static project references
to malloc/calloc/realloc/free and checks the C tracker positively. This does not
intercept every shared-library/OS allocation, inspect locks/I/O or prove deadlines.

```sh
ctest --preset core -L realtime
```

For sanitizer validation, build both `Synth2PatchTests` and `Synth2RealtimeTests`
in the sanitizer directory and run both with the documented sanitizer environment.
LeakSanitizer requires an environment without ptrace attachment. Do not suppress
an actual sanitizer report to obtain a passing run.

## State representation, bank boundaries and history saturation

`StateRepresentationTest.cpp` verifies that reordered parameter elements and
fixed/scientific spellings of the same numbers restore identical state. Invalid
or duplicate IDs still reject the entire transaction.

`BankBoundaryTest.cpp` generates ZIPs at the declared entry and expanded-byte
limits, then tests one entry/byte above each limit. It also corrupts stored payload
bytes to check CRC rejection and marks central-directory entries as Unix symlinks
to check filtering without creating filesystem links. Rejection must preserve
the previously loaded library. The size-limit test uses a highly compressible
self-authored payload; it does not require a large checked-in fixture.

`HistoryQueueBehaviorTest.cpp` checks exact drop accounting, retained FIFO order,
slot reuse across repeated saturation, concurrent notification producers and the
bridge's response to lost history. New edits work after a drain, while discarded
history remains invalid for compatibility admission. The realtime suite measures
saturated draining and subsequent edits for heap activity.

## Continuous integration

[GitHub Actions](../docs/ci.md) runs this SDK-free portable suite on Linux,
Windows and macOS. Native wrapper builds are separate jobs using pinned SDKs;
those jobs do not replace the product tests or manual platform checks.
