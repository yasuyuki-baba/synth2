# Host lifecycle and performance requirements

Serialized host calls must preserve state recall, repeated program changes,
prepared capacity, zero-frame MIDI, note release and subsequent playability after
reprepare. State is a source/control document, not a running DSP snapshot.
Oversized callbacks are rejected and silenced rather than silently resized.

[HostLifecycleTest.cpp](../tests/HostLifecycleTest.cpp) covers variable block sizes,
rates, generated-patch recall, dense voices, stop/retrigger and finite output. Processor
state/bridge tests cover rejected inputs, queues, source edits and parameter IDs.
See [processor state](contracts/processor-state.md) for the maintained contract.

`Synth2HostBenchmark` is a non-gating offline diagnostic. It exercises explicitly
specified dry/wet synthetic profiles, checks core occupancy and audio, and reports
callback timing separately from setup and validation work. Use fresh CSV outputs,
record compiler/binary/CPU/run context and compare like-for-like conditions.

```sh
cmake --preset core
cmake --build build-oos-core --target Synth2HostBenchmark
./build-oos-core/Synth2HostBenchmark build-oos-core/host-benchmark.csv 128
```

On Windows, invoke `build-oos-core/Synth2HostBenchmark.exe` instead. The CSV path
must not exist; choose a fresh filename for each run.

Offline over-budget observations are not detected audio-device underruns. Neither
average nor sampled maximum callback time establishes a hard real-time deadline.
Actual DAW/device testing and concurrency contracts require separate validation.
