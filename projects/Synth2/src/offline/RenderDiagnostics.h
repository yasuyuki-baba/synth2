#pragma once

namespace synth2::offline
{
// Only the separately linked diagnostic probe has mutable bypass state.
// Its caller configures this on the rendering thread outside processing callbacks.
#if defined(SYNTH2_OFFLINE_DIAGNOSTICS)
    struct RenderDiagnostics { bool envelopes{}, filter{}, effects{}; };
    inline thread_local RenderDiagnostics diagnostics;
    inline bool bypassEnvelopes() noexcept { return diagnostics.envelopes; }
    inline bool bypassFilter() noexcept { return diagnostics.filter; }
    inline bool bypassEffects() noexcept { return diagnostics.effects; }
#else
    constexpr bool bypassEnvelopes() noexcept { return false; }
    constexpr bool bypassFilter() noexcept { return false; }
    constexpr bool bypassEffects() noexcept { return false; }
#endif
}
