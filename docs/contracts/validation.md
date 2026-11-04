# Validation and evidence contract

## Required checks

Run the product portable suite without an original DLL, Wine or another checkout.
Keep independent inputs unchanged during refactors. An expected value must come from retained evidence or an explicit product contract, not from
running a second copy of the changed implementation and treating it as an oracle.

Distinguish: a product requirement; an observed result under specified conditions;
and a known or unverified gap. A failing requirement is a defect, not a reason to
rewrite the specification or relax its comparison bound. An old measurement count
is evidence for that experiment, not a current product guarantee.

For DSP/loading refactors, compare candidate WAV bytes and loaded controls against
the preceding implementation as well as existing product expectations.
Check complete stereo and exact event timelines. Gains, RNG seeds, alignment,
downmixing and threshold changes must not be used to hide a difference.
After DSP component-boundary changes, run the independent/transitive header audit.
Build affected targets and distinguish portable success from native plugin/DAW runs.

## Precision audio acceptance

Use raw complete-stereo normalized error without gain fitting, waveform alignment,
downmixing or RNG fitting. Keep the numerical measurement separate from acceptance:

| Raw stereo normalized error | Decision |
| --- | --- |
| Below 0.005 | Normal pass |
| From 0.005 to strictly below 0.01 | Review required; conditional pass only with confirmed intentional precision difference |
| At or above 0.01, invalid or missing measurement | Fail |

A conditional review must explain the cause and establish preserved control
meanings, integer-input arithmetic where applicable, loading and update timing.
Bind it to the measured candidate binary, input/reference/output identities and
render conditions. A changed candidate or comparison requires review again.
Missing captures, mismatched controls and known implementation defects cannot be
accepted by an audio-error review. NRMSE is not a perceptual audibility guarantee;
listening can support a review but does not replace causal evidence.

Retain the 0.005 target. Report normal and conditional passes separately, keep
historical bounds/results immutable, and record any explicitly authorized policy
change separately from remeasuring or changing DSP. A prior strict-bound failure
remains a failure of that historical requirement.

## Intentional behavior differences

Keep product behavior decisions separate from numerical comparison results.
The product deliberately preserves a nonzero fractional envelope position through
a full-cutoff round trip completed without rendering samples, even when a legacy
integer implementation would erase that state. See the
[audio rendering contract](audio-rendering.md). This state retention is accepted
product behavior; reproducing that legacy state loss is not a required DSP fix.

Retain the measured error and its original numerical outcome. A comparison at
or above0.01 remains a failed audio-bound comparison; record the accepted product
behavior as an intentional specification difference, outside normal/conditional
audio passes. Do not raise the general thresholds, rewrite historical reports or
use this decision to excuse lost notifications, incorrect controls, MIDI ordering
or unrelated edits. Each recorded behavior disposition must identify its exact
input/event history, producer/reference/output identities, causal evidence and
owner decision. Unmeasured combinations are not certified by this scope decision.

## Test inputs

Synth2 keeps self-authored product tests and necessary product assets. State round
trips, API rejection, signal properties, MIDI scheduling, library and layout tests
use generated inputs and explicit product requirements.
No external checkout, credentials or original recording is required.

## Limits and procedure

See [tests/README.md](../../tests/README.md) for reproducible product commands and
the [reference comparison procedure](../native-measurements.md) for self-authored
inputs, resolved-control inspection and exact before/after checks.
A portable test does not establish a DAW build, a particular audio device, concurrent
host safety or a hard real-time deadline. Record exactly what ran and which source,
binary, loading history, sample rate and comparison conditions were used.

Public users verify product contracts using generated inputs
and test sources; see [test inputs and evidence](../evidence.md). Publishing
Synth2 must not introduce an external checkout, credential or original-recording
requirement into its build or portable tests.
