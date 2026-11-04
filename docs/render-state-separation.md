# Render controls and loading-state ownership

`RenderControls` contains render controls only. Source version, normalized source
values, omitted-field masks and mapper cache belong to loading/mapping adapters.
Load-history context is separate from both. Diagnostic `MappedSynthSnapshot`
objects must never become Engine or Voice parameters.

Mapper cache hits require matching destination, source values, presence, native
context and relevant history. A different destination forces remapping. Keep the
destination alive while using the cache; replacing an object in place requires
explicit remapping or replacing the mapper. Runtime performance offsets do not
belong in the cache or original source getters.

The bridge publishes/flushes load history before mapping and supplies its validity
explicitly to policy. Engine applies context at its clock/reload boundaries;
Voice does not receive a patch document or source version. State serialization
preserves loading/source metadata, not diagnostic snapshots or running DSP memory.
See [state](contracts/processor-state.md), [modulation](contracts/modulation.md)
and [loading](contracts/patch-loading.md) for behavior and regression entry points.
