# Product capabilities and verification boundaries

Start with the [product contracts](README.md) and [test guide](../tests/README.md).
Synth2 builds and runs its portable suite without an original Synth1 DLL, Wine,
JUCE or another repository. Native wrappers require their platform SDKs.

| Area | Required behavior / boundary |
| --- | --- |
| Product source | iPlug2 wrapper/editor and SDK-independent core under projects/Synth2 |
| Patch loading | Synth2 version 200 preserves fractional dials; accepts legacy 1–113 and named unversioned sources; native evidence for 100/104–113; transactional failure; original source values retained |
| State/parameters | Stable host metadata; readable legacy state chunks; source-aware recall without the external bank |
| Rendering | Control-driven policy, preserved numerical/event ordering, retained physical histories and effect tails |
| GUI | Shared layout/value semantics, bundled fonts, main Synth surface and supplementary MIDI/library views |
| Native targets | Windows VST3/CLAP/APP; macOS VST3/CLAP/AUv2/APP configurations |

The [verification map](validation/coverage.md) identifies public automated coverage.
Actual GUI/DAW/device checks are manual-only and excluded from the automated-test
inventory. Public tests enforce synthetic loading/state/host/library/editor and DSP/MIDI
properties without captured original outputs.

Native target configuration and portable success do not certify DAW loading,
release toolchains, DAW plugin-identity compatibility, signing/notarization, device
latency or hard real-time operation. Readable legacy state chunks and DAW
plugin identity are separate compatibility questions. Generated-input properties and
sampled reference results do not prove unrestricted audio equivalence.

Public-facing requirements and tests remain self-contained;
see [test inputs and evidence](contracts/validation.md) and [distribution requirements](publication/README.md).
