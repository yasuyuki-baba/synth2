# Shared modulation ownership

Engine owns one SharedModulationRuntime with two compatible clocks, real-valued
control blocks and one CRT stream shared with ARP. Slot consumers borrow const
values/spans; they never generate waveforms, reset clocks or draw random words.
There is no normalized sample buffer, secondary waveform generator or xorshift
substitute. Pitch, pulse, filter, FM, amplitude and output pan use these same slots.

Setters, queries and advances remain ordered actions. Preserve MIDI routing's
rate re-dispatches, same-value setters, nested slot queries, disabled-clock
advancement, native held spans and random draw chronology. Generate controls
after MIDI dispatch. First-key resets use the upstream keyboard across play
modes; overlapping keys do not reset phase. Reprepare retains its documented
history boundaries.

Fractional depth/rate/phase interpolation retains compatible whole-value knots.
Random targets retain current/previous values and project the same CRT word
through compatible bounds. Zero ranges consume no draw; queries consume none.
Unknown or dropped history stays marked unverified without selecting another
clock, seed, modulation algorithm or receiver graph. This common execution path
does not reconstruct missing history or establish original-DLL equivalence.

ARP has one period/gate calculation and borrows the same CRT stream, including
empty-keyboard boundaries. Boundary lookahead advances no clocks or RNG.
Render adapters select active destinations directly from controls, preserving
per-slot products and scheduling spans. Shared blocks contain physical receiver
coordinates and their remaining spans; output pan uses the prepared value from
its own MIDI subregion.

[Modulation requirements](../contracts/modulation.md) and the public modulation,
phase-precision, playback and ARP tests define the verification boundaries.
