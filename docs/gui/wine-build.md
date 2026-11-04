# Native Windows APP capture

`tools/capture_iplug_wine.py` captures a built Windows APP under Wine with audio
and MIDI I/O disabled. It requires the built PE, Wine/Xvfb/X11 and ImageMagick.
Use its `--help` for explicit executable, Wine, prefix and output paths. Preserve
runtime trust/proxy configuration and use a fresh output file.

A captured native frame checks that the actual APP editor can initialize and
paint in the specified environment. It does not establish DAW plugin behavior,
audio-device operation, native macOS operation or unrestricted GUI interactions.
The HTML/SVG preview is a separate design aid and must not be labeled a native frame.

See the [GUI contract](README.md) and [desktop build boundary](../iplug2-migration.md).
Record the executable identity and capture conditions with any reported frame.
