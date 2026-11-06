#pragma once
namespace synth2::editor
{
// Embedded, unmodified Liberation fonts (SIL OFL), used by the native editor on
// every desktop host. No dependency on the host's installed font families.
struct EditorFont { void* data; int size; };
EditorFont editorRegularFont();
EditorFont editorBoldFont();
EditorFont editorMonoFont();
}
