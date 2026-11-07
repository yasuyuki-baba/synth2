#pragma once

namespace synth2::editor
{
// Logical sizes at the standard editor scale; secondary controls use color,
// rather than very small type, to stay visually subordinate.
struct EditorTypography
{
    static constexpr float auxiliary=12.f;
    static constexpr float label=14.f;
    static constexpr float value=13.f;
    static constexpr float button=15.f;
    static constexpr float body=15.f;
    static constexpr float input=16.f;
    static constexpr float heading=15.f;
    static constexpr float patchTitle=19.f;
};
}
