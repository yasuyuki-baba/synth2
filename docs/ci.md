# Continuous integration and candidate packages

The public pipeline uses this repository and public dependencies. It does not
use an original DLL, Wine, private fixtures or another investigation checkout.
Workflow actions are pinned to commits and jobs have read-only repository access.

## Build checks

`build.yml` runs on main pushes, pull requests and manual dispatch. Its portable
matrix covers Ubuntu 24.04, Windows Server 2022/MSVC and macOS 15. Checkouts leave
iPlug2 uninitialized. Each job configures Release with native wrappers and
optional tools disabled, builds the actual portable bridge/tests, and runs all
portable CTest entries. Required Probe/comparator executables are included by the
test configuration. Linux also runs actionlint and the independent DSP header
audit. No virtual display or native audio/GUI packages are needed.

After portable checks pass, the reusable `native.yml` builds Windows VST3/CLAP/APP
and macOS VST3/CLAP/AUv2/APP. Windows uses MSVC x64; macOS uses the Xcode universal
arm64/x86_64 preset and macOS 13 deployment target. NanoVG uses its platform
default renderer (GL2/Metal). The unused SDK WebView extra is disabled so its
WIL/WebView2 downloads do not enter configuration. Plugin deployment to system folders stays disabled.
Native jobs do not configure the portable test target again.

The iPlug2 gitlink and lock entry must match. `prepare_native_sdks.py` also fetches
locked VST3/CLAP/helper commits and required VST3 submodules without relying on
upstream download-script branch defaults. It records source identities in
`build/dependencies.json`. Compiler/runtime setup must stay consistent: the
portable MSVC test runtime follows the parent CMake configuration, while the
native SDK uses its static-runtime configuration.

## Local equivalents

```sh
cmake -S . -B build-oos-ci -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSYNTH2_BUILD_IPLUG2=OFF -DSYNTH2_BUILD_TOOLS=OFF -DSYNTH2_BUILD_TESTS=ON
cmake --build build-oos-ci --config Release --parallel 4
ctest --test-dir build-oos-ci -C Release -L portable --output-on-failure --no-tests=error
python tools/audit_dsp_headers.py --build-dir build-oos-ci
```

The header audit requires a compile database or Ninja; the CI audit runs on Linux.
For native products, first initialize `iPlug2` and run the dependency helper as in
[the desktop guide](desktop-build.md). On the corresponding native platform:

```sh
cmake --preset windows -B build "-DSYNTH2_IPLUG2_FORMATS=VST3;CLAP;APP"
# On macOS instead:
cmake --preset macos -B build "-DSYNTH2_IPLUG2_FORMATS=VST3;CLAP;AU;APP"
cmake --build build --config Release --parallel 4
python tools/package_native.py --build build --platform windows --manifest build/dependencies.json --output dist
# Use --platform macos on macOS.
```

## Candidate artifacts and releases

Native jobs require every selected product and package only the known product
names from `build/out`. Windows packages use ZIP; macOS packages use tar.gz to
retain bundle permissions and symlinks. Archives include the dependency manifest,
project license and dependency notices. Failure diagnostics are uploaded separately.
Artifacts expire after 14 days and are identified by the workflow run/commit.

`release.yml` runs on `v*` tags or manual dispatch and reuses the complete build
pipeline to produce the same candidates. It does not create/publish a GitHub
Release or change repository visibility. Official distribution still follows the
[publication requirements](publication/README.md), including asset rights and the
chosen public history. Candidates are unsigned and macOS builds are not notarized.

A native compile/package result does not establish plugin scanning, GUI interaction,
DAW session compatibility, device behavior or hard real-time guarantees. Those
remain outside the automated-test inventory; use the separate
[manual platform guide](validation/manual-platform.md) when that work is requested.
