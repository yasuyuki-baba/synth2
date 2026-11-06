# Synth2 product documentation

Use this index to find the authoritative document for each topic. Directory
READMEs introduce their local contents and link to these detailed contracts
rather than duplicating them.

`contracts/` defines required behavior, `architecture/` explains component
ownership, `validation/` contains verification scope and procedures, and `gui/`
covers editor and library behavior.

These documents define what the product must do, its ownership boundaries, how to
verify them and what remains unverified. Do not convert the current implementation
into a requirement merely because it exists. Distinguish intended behavior,
independently observed behavior, implementation regressions and unresolved gaps.

| Need | Maintained contract |
| --- | --- |
| Source formats, defaults and transactional loading | [Patch loading](contracts/patch-loading.md) |
| Stable parameters, saved state and callback interface | [Processor/state](contracts/processor-state.md) |
| MIDI, ARP and physical/logical voices | [MIDI and voices](contracts/midi-voices.md) |
| Shared clocks, random streams and input history | [Modulation](contracts/modulation.md) |
| DSP/output order, numerical behavior and silence | [Audio rendering](contracts/audio-rendering.md) |
| Test oracles, provenance and compatibility scope | [Validation](contracts/validation.md) |
| Source-version strategy boundary | [Source conversion](architecture/source-conversion.md) |
| Component inputs and state ownership | [Render boundaries](architecture/render-boundaries.md) |
| Voice audio component inputs and state | [Voice audio components](architecture/voice-audio-components.md) |
| Output order and lifecycle retention | [Output pipeline](architecture/output-pipeline.md) |
| Offline callback timing | [Host benchmark](../tools/README.md#offline-host-benchmark) |
| Automated verification scope | [Verification map](validation/coverage.md) |
| Manual platform checks | [Platform guide](validation/manual-platform.md) |
| Before/after audio and loading comparisons | [Reference comparisons](validation/reference-comparisons.md) |
| CI checks and candidate artifacts | [Continuous integration](ci.md) |
| Native formats and toolchains | [Desktop build boundary](desktop-build.md) |
| Editor behavior and design | [GUI](gui/README.md) |

[status.md](status.md) is a short capability/verification index, not a development
journal. Each requirement should name a regression test or retained evidence and
state its limits. Change a contract only for an explicit product decision or new
independent evidence; do not silently bless a failing implementation or weaken a bound.

Public readers can use [validation requirements](contracts/validation.md) and the
included test sources for local verification. Source and binary packages follow
the [distribution requirements](publication/README.md).
