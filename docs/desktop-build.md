# Desktop instrument and build boundary

All product code lives in `projects/Synth2`, using the iPlug2OOS layout. The portable
core in `projects/Synth2/src` must remain independent of iPlug2 headers. Production,
tools and the portable build graph do not depend on JUCE or an original Synth1 DLL.
ZIP/XML support uses the bundled miniz/tinyxml2 implementations.

| Platform | Intended native formats |
| --- | --- |
| Windows | VST3, CLAP, standalone APP |
| macOS | VST3, CLAP, AUv2, standalone APP |

Use the root CMake presets for the configured toolchain/SDK. `core` disables the
native wrapper and builds the SDK-independent core/tests. Native format presets
require the appropriate iPlug2 checkout, SDK and platform compiler. The macOS
presets request arm64/x86_64 universal builds with macOS 13 as the deployment target.
A configured target is not evidence that it built or ran in a DAW.

## Preparing the native build

Install CMake 3.25+, Python 3 and Git. Portable `core` builds also require Ninja
and a C++20 compiler. Windows native presets require Visual Studio 2022 with the
Desktop development with C++ workload and a Windows SDK. macOS native presets
require full Xcode, its selected command-line tools and both requested architectures.
For Windows portable builds, use a Visual Studio developer shell so Ninja can
find the C++ compiler.

## Build targets and compiler requirements

`Synth2Core` is the shared portable library used by native wrappers, tools and
tests. `Synth1PatchCore` owns source parsing/conversion; resources and ZIP/XML
support have their own static libraries. Editor fonts are native-only. These are
build targets, not separately loaded runtime plugins.

`SYNTH2_BUILD_TOOLS` selects offline tools independently of native wrappers and
public tests. When tests are enabled without optional tools, only their required
`Synth2Probe` and `CompareWaves` executables are added. With tools and tests both
disabled, no offline executable targets are added. Native presets select this
product-only configuration; the portable core preset includes tools and tests.

C++20 is a public target requirement of `Synth2Core` and `Synth1PatchCore` and
propagates to linked consumers, including consumers in a parent CMake project.
Project libraries and executables declare their compiler requirements on targets;
they do not set a parent project's global C++ standard. The native SDK retains
its own C++/Objective-C++ configuration. Third-party XML support declares its own
C++11 requirement; C ZIP support is independent of C++ settings.

## SDK checkout and additional dependencies

`iPlug2/` is a Git submodule pinned to the revision in
[dependencies.lock.json](../dependencies.lock.json). Initialize the recorded
revision from the product repository root:

```sh
git submodule update --init iPlug2
```

Portable `core` builds need neither this checkout nor its additional SDKs. CMake
never initializes, fetches or advances the submodule. The default `IPLUG2_DIR`
points at it; `-DIPLUG2_DIR=/path/to/iPlug2` selects an external checkout instead.
For a reproducible external build, use the same recorded revision. Updating iPlug2
requires reviewing the new revision, validating affected native targets, and
updating both the gitlink and lock entry together.

The pinned iPlug2 checkout includes its small vendored libraries. VST3, CLAP and
CLAP helpers are separate dependencies with full revisions in the `native_sdks`
section of the lock file. Initialize iPlug2, then fetch the same SDKs used by CI:

```sh
python tools/prepare_native_sdks.py --output build/dependencies.json
```

The helper verifies the iPlug2 revision and installs the exact additional SDK
commits and required VST3 submodules. It refuses populated unmanaged SDK folders,
different installed revisions and modified SDK code. Tracked iPlug2 readme
placeholders are preserved; they are not compiled SDK files. Existing external
SDKs can be selected using `--iplug2 /path/to/iPlug2`. Pin and review dependency
updates in the lock file; upstream download-script defaults are moving branches.
APP and AUv2 do not require VST3/CLAP SDKs just to build those formats.

Synth2 does not enable the SDK WebView extra; its dependency downloads and unused
object library are excluded from native configuration/builds.
CI uses the vendored NanoVG backend: GL2 on Windows and Metal on macOS. Other
backends may need extra libraries; see `iPlug2/Dependencies/README.md` and
`iPlug2/Dependencies/IGraphics/README.md`. The platform/build pipeline and candidate
archive formats are described in [continuous integration](ci.md).

## Native presets

Return to the Synth2 root and select the preset for the desired format:

| Platform | Configure/build preset | Build directory | Formats |
| --- | --- | --- | --- |
| Windows | `windows` | `build-oos-win` | VST3, CLAP |
| Windows | `windows-app` | `build-oos-app` | APP |
| macOS | `macos` | `build-oos-mac` | VST3, CLAP, AUv2 |
| macOS | `macos-app` | `build-oos-mac-app` | APP |

For example:

```sh
cmake --preset macos
cmake --build --preset macos
```

Use the corresponding Windows or APP preset on that platform. Native build
presets select Release and disable portable tests. Portable validation uses
`cmake --preset core`, `cmake --build --preset core` and `ctest --preset core`.

## Host boundary and verification

The wrapper serializes calls into ProcessorBridge and preserves stable parameter
IDs/ranges, exact MIDI offsets, prepared capacity and state contracts. The editor
uses the same parameter/loading boundary as tests. Keep DAW plugin-identity
compatibility and native host/program enumeration separate from readable processor
state chunks.

See [processor contract](contracts/processor-state.md), [GUI contract](gui/README.md)
and [tests](../tests/README.md). Portable success does not establish native plugin,
DAW, signing/notarization or hardware real-time behavior.

The [optional manual guide](validation/manual-platform.md) describes wrapper, automation,
session recall and standalone-device checks. These are outside automated-test scope
and are not tracked as missing automated tests.

## Product identity

The plugin and APP are named `Synth2`. The four-character plugin ID is `Syn2`;
the displayed manufacturer name is `babab` and the four-character manufacturer ID
is `Baba`. The manufacturer ID includes an uppercase character because all-lowercase
Audio Unit manufacturer codes are reserved for Apple. This shared ID also applies
to native formats that derive their identity from `PLUG_MFR_ID`. Bundle
components are domain `io.github`, namespace owner `yasuyuki-baba` and name
`Synth2`, with the
format component (`vst3`, `clap`, `audiounit` or `app`) between manufacturer and
name (for example, `io.github.yasuyuki-baba.vst3.Synth2`). The bundle namespace
is independent of the displayed manufacturer and four-character manufacturer ID.
Native target names are `Synth2-vst3`, `Synth2-clap`, `Synth2-au` and
`Synth2-app` as applicable. Keep config.h, native resource metadata and build
presets consistent. Internal class names do not define parameter identities.

DAW plugin-identity compatibility is not established by readable state chunks.
Parameter IDs, ordering, ranges and processor state encoding remain independent
contracts.
