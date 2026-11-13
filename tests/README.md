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
| HeaderBoundaryAudit | GNU-style GNU/Clang frontends with Ninja/Makefiles: independent DSP/voice/editor/state header compilation and forbidden transitive dependencies |

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

## Focused and instrumented runs

The [verification map](../docs/validation/coverage.md) identifies individual test suites
and their scope. Select the relevant CTest entry or GoogleTest suite, for example:

```sh
ctest --test-dir build-oos-core --output-on-failure -R '^BuildToolsTests$'
./build-oos-core/Synth2PatchTests --gtest_filter='HostLifecycle.*'
```

Use `Synth2PatchTests.exe` on Windows. These examples run subsets, not the complete
suite; retain the executed filter when recording results.

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
as missing tests. An optional [manual guide](../docs/validation/manual-platform.md) is available.

## SDK parameter check

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

## VST3 patch host check

The SDK test compiles the production Synth2 wrapper and pinned iPlug2/VST3 sources
with a headless editor delegate. It verifies host readback during patch
notifications, an audible-to-silent patch change followed by a host parameter
readback/echo, preservation of recalled fractional controls, and one latency
notification per oversampling change with no repeat notifications for unchanged
latency. It also checks framework UI MIDI queue delivery against the same
ordinary MIDI sequence, including note release. Only window/UI callbacks are omitted. It does not exercise a DAW or GUI interaction.

After initializing iPlug2, prepare the pinned format SDKs and run:

```sh
python3 tools/prepare_native_sdks.py --output build/dependencies.json
cmake -S tests/sdk/vst3 -B build-vst3-test -DCMAKE_BUILD_TYPE=Release
cmake --build build-vst3-test --config Release --parallel 4
ctest --test-dir build-vst3-test -C Release --output-on-failure
```

The existing native CI jobs run this check on Windows and Linux. It is separate
from the SDK-free portable suite.

## Callback heap checks

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

## Continuous integration

[GitHub Actions](../docs/ci.md) runs this SDK-free portable suite on Linux,
Windows and macOS. Native wrapper builds are separate jobs using pinned SDKs;
those jobs do not replace the product tests or manual platform checks.
