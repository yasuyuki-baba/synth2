# Optional manual platform checks

Portable tests exercise the actual SDK-independent core and bridge. They do not
instantiate an iPlug2 wrapper, editor, operating-system audio device or DAW.
These are reference steps for separate manual work on intended release platforms.
Actual GUI interaction, DAW sessions and physical devices are outside automated-test
scope. This page is not an automated coverage checklist or a pending-test backlog.
Use it when manual platform checks are requested, and record only what actually ran.

## Build and load

Check out the pinned iPlug2 SDK and required format dependencies as described in
[the build boundary](../desktop-build.md). Build Release with the root presets:

| Platform | Configure/build presets | Targets to load |
| --- | --- | --- |
| Windows | `windows`, `windows-app` | VST3, CLAP, standalone APP |
| macOS | `macos`, `macos-app` | VST3, CLAP, AUv2, standalone APP |

For example, run `cmake --preset windows`, then
`cmake --build --preset windows`. Repeat with the applicable APP/macOS preset.
A successful compile is a build result; verify each resulting format in a host.
On macOS verify Apple Silicon products with a native arm64 host. Record the product and SDK commits, compiler, architecture, format,
host/version, sample rate, block size and actual result outside this specification.

## Host acceptance

For every plugin format, use a fresh host session and a self-authored patch or the
explicitly imported bank. Record host input/output and steps when a failure occurs.

| Scenario | Required result |
| --- | --- |
| Scan, instantiate and open/close editor repeatedly | Host recognizes the format; editor and audio remain functional |
| Play note/chord, release keys, use sustain and release pedal | Audible notes; pedal holds released keys; release finishes without stuck notes |
| Record/play automation of source control and master volume | Correct named control moves and audible change; reopening the editor shows host state |
| Save session after editing and selecting a file-based patch | Reopened session restores controls and selected patch without the original bank file |
| Reload session, render a new note from stopped transport | Restored source produces the same cold-start sound under equal input/setup; running DSP history is not serialized |
| Change tempo with a tempo-synced patch | Host tempo reaches the instrument without non-finite output or a crash |
| Stop/restart playback and change supported rate/buffer configuration | Instrument remains playable; normal note release and effect tails remain valid |
| Multiple instances and repeated instance deletion | Edits and state belong to their own instance; host remains stable |

Compare cold-start audio only after equal prepare/loading/MIDI history and equal
callback configuration. Arbitrary callback partitions can affect native receiver
and gain boundaries. Host plugin-identity compatibility is a separate check from
readability of an old state chunk.

For standalone APP, repeat editor interaction and MIDI/pedal checks with a real
MIDI/audio device, verify device selection and sample-rate changes, and close and
reopen the application. Do not infer device latency or hard real-time guarantees
from portable tests or successful offline rendering.

## Native Windows APP capture

`tools/capture_iplug_wine.py` captures a built Windows APP under Wine with audio
and MIDI I/O disabled. It requires the built PE, Wine/Xvfb/X11 and ImageMagick.
Use its `--help` for explicit executable, Wine, prefix and output paths. Preserve
runtime trust/proxy configuration and use a fresh output file.

A captured native frame checks that the actual APP editor can initialize and
paint in the specified environment. It does not establish DAW plugin behavior,
audio-device operation, native macOS operation or unrestricted GUI interactions.
The HTML/SVG preview is a separate design aid and must not be labeled a native frame.

See the [GUI contract](../gui/README.md) and [desktop build boundary](../desktop-build.md).
Record the executable identity and capture conditions with any reported frame.

For a library with more banks than fit on screen, open the bank selector and check
platform-menu scrolling, wheel/trackpad behavior and selection near both ends.
Verify indentation, disabled parent headings, All banks and This folder, then open
another menu to confirm custom popup rendering is restored. Check both Windows
and macOS; portable presentation tests do not exercise native menu interaction.

Refresh a library with named Bass/Pad sounds and feature-rich patches. Check that
manual tags use ordinary text and inferred tags use muted text in the same row.
Filter/search both kinds without changing the sounding patch. Use Auto tags to
hide and restore a suggestion, reopen the editor, and confirm persistence. Enter
the same tag manually with different case; it should appear once in the manual
color. Check narrow/overflowing tag columns and selection/readability on both OSes.

Toggle multiple tags in the checked tag menu and verify AND results, the selection
count/tooltip, All tags reset and DAW session recall. Refresh/reopen a large
library twice, then add, remove and replace loose/ZIP sounds externally and
refresh. Check that the updated list and tags reflect the current sources while
the sounding patch and manual/hidden-tag overrides remain intact.

With a filtered library, use Ctrl/Command+F, type a query and confirm it with
Enter. Move with arrows, pages and Home/End: the candidate outline should move
without changing the sounding title. Enter should load it; Escape should return
to the sounding entry if still in the results. Check empty results, invalid
sources, catalog refresh, mouse selection, software-keyboard performance, and
ordinary text editing/host shortcuts while the editor is unfocused.

Scroll the Library with wheel/trackpad input and drag the scrollbar thumb from
top to bottom; click the track to jump. Check a short/empty list, fractional
rows, first/last bounds, clipped drawing and row/favorite hit tests after scrolling.
Move the keyboard candidate across either viewport edge: reveal only the needed
rows with a short transition, keeping the sounding patch unchanged until Enter.
Check that refresh/filter changes clamp the viewport and session recall restores
the first visible row after scanning. No page buttons should remain.

Use the bank selector arrows alongside its menu. Check identical selectable
ordering, parent folders with direct sounds, All banks and disabled end buttons.
Keywords/tags/favorites and the sounding patch must survive a bank change, while
the registration-only restriction and scroll/candidate reset.

At the standard editor size, check the shared larger type across Synth, MIDI,
Library, header/footer and popup menus. Exercise long choices, long patch/bank
names and tags, extreme unit readouts, Chorus lower-row labels and the compact
filter envelopes. Text should remain inside its control without reducing the
choice font. Confirm numeric-entry hit areas and menu/full-value tooltips.

Resize the standalone window from its frame horizontally, vertically and at a
corner, then shrink it again. Text and controls should scale together at a stable
aspect ratio; test the editor corner handle too. Open/close Keyboard at a changed
scale and exercise popup, numeric-entry and note hit areas. In a DAW, resize the
host-owned view and check uniform fit without resize-request loops. Repeat on
displays with different platform pixel scales.
