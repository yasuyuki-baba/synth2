# Shared modulation ownership

Engine owns `SharedModulationRuntime` for the lifetime of its consumers. Slots
retain their source, clock, previous rate, sync and touched/shared state. Consumers
borrow const clock/block/random views; ARP borrows the same Engine-owned CRT stream.
Establish those bindings before MIDI processing and never copy execution history
into a Voice parameter or serialized preset. Voices own no LFO generators.
Both compatible receivers and fallback waveforms use the Engine-owned slot pair.
Generate fallback samples per render span after MIDI dispatch, so first-key resets
and controller edits take effect at their actual sub-block positions.

Key sync resets on the first held upstream key across Poly, Mono and Legato.
Overlapping keys do not reset either source. Reset the phase without reseeding
the fallback random generator or discarding its held targets. Unknown native RNG
history remains unverified; moving its source to shared ownership does not recover
that history or enable native receiver admission.

Preparation/retiming, history application, synchronization, chunk sampling,
receiver queries and advancement remain separate ordered phases. Clock boundary
queries do not advance state. Retain slot-1-before-slot-2 query ordering, disabled
source advancement, nested partitions and shared draw chronology. Upstream key
tracking remains distinct from generated ARP assignments.

The facade depends on value-only history/settings types, not Engine, Voice, patch,
SDK or the render aggregate. `render/SharedModulationConfiguration.h` adapts the
loading and render controls. The bridge owns bounded history publication; Engine
owns region splitting, scheduling and output lifecycle.

See [modulation requirements](../contracts/modulation.md) and the LFO/CRT/ARP tests.
`tools/audit_dsp_headers.py` checks independent compilation and forbidden transitive
dependencies. [ModulationBehaviorTest.cpp](../../tests/ModulationBehaviorTest.cpp)
uses generated inputs to check reset, tempo, key overlap, disabled advancement and
read-only consumers. These properties do not establish every shared-draw sequence
or arbitrary live-history equivalence.
