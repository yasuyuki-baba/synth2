# Synth2 maintained patch core

This directory contains the portable parser, raw document, source-format
conversion and typed patch model used by Synth2. It builds as `Synth1PatchCore`
using standard-library C++20, without plugin SDKs, JUCE, Wine or an original DLL.

## Related documentation

| Topic | Guide |
| --- | --- |
| Import/export APIs and conversion boundaries | [Source conversion](../../../../../docs/architecture/source-conversion.md) |
| Accepted inputs, defaults and transactional loading | [Patch loading](../../../../../docs/contracts/patch-loading.md) |
| Saved source and runtime edits | [Processor/state](../../../../../docs/contracts/processor-state.md) |
| Public tests, execution and verification limits | [Testing](../../../../../tests/README.md) |

## Standalone build

For a standalone build of the same core target, without dependency fetching or
plugin/framework configuration, run from the repository root:

```sh
cmake -S projects/Synth2/src/patch/core -B build/patch-core-standalone -G Ninja
cmake --build build/patch-core-standalone --parallel 2
```

The standalone build compiles the library; product tests use the root `core` preset.
