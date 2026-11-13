# Editor behavior and visual contract

Ordinary sound design belongs on one Synth surface with Synth1-inspired functional
groups. Oscillators occupy the wider left column above the two LFOs. Filter
and Amplifier share the center column; Voice/Unison and ARP share the right.
Effects and EQ/Pan occupy the bottom. Allocate panel space by control density
rather than forcing every column into identical row heights. Ordinary knobs
retain similar readable sizes; Cutoff remains the dominant larger control.
MIDI routing uses a supplementary view. Serialized metadata and reserved fields stay outside the
editor; their host IDs, patch values and state serialization remain supported.
There is no Advanced tab. Preset browsing and master
volume remain visible. The supplementary [Library view](patch-library.md) accumulates
imported sounds and combines bank, tag, keyword and favorite filters.
Use the Library tab for explicit view switching rather than a duplicate Library
action button. Clicking the current patch name also opens the Library view.
Keyword search belongs in the Library view; the persistent header does not
duplicate its text input. Search and filter state remain intact across view changes.
View transitions invalidate the complete editor, including its background, after
updating control visibility. Repainting must not invoke parameter edits or sound
loading actions.

Typography uses shared logical sizes: 12 for auxiliary text, 13 for values,
14 for parameter labels and bank/tag metadata, 15 for controls, headings and list
titles, 16 for text entry and SDK menus, and 19 for the selected patch title. Native
bank/tag popups retain the platform's standard menu typography. Secondary controls
stay quiet through color and placement rather than tiny type. Choice values use
measured ellipsis at a fixed size, with full values in tooltips; Random smooth
uses the short display name Rnd smooth. Source words and saved controls are unchanged.
The standard 1280-by-840 view remains supported. Knob label/value bands reserve
space for the larger text. Panel side padding is 8 units and controls start 38
units below the panel top. Both LFOs use a selector row followed by Rate, Amount,
Tempo and Key sync, with full-size Rate/Amount knobs. Chorus uses three full-width
lower-row columns.
Use Synth1's original functional groupings as a reference while retaining the
larger Nord Lead 2-inspired Cutoff and its filter emphasis. Oscillator rows group
OSC1/Sub, OSC2, Mod Env and shared mix/tuning controls. Voice separates play/glide
settings from Unison; Amplifier separates ADSR from level/velocity. EQ keeps
Frequency, Level and Q together. Muted rose headings and thin internal dividers
provide hierarchy without reducing label sizes or moving controls off the main
surface. Native drawing and review studies share divider and delay readout bounds;
studies use placeholders for live tempo and stereo delay times.
Native drawing and review tools consume the shared typography policy.

Window-frame resizing scales the complete logical canvas, including text and
input hit areas, rather than enlarging an unscaled drawing area. Standalone and
plugin views on every platform use the largest uniform scale that fits both
parent-supplied dimensions, subject to the 0.5–4 scale limits. A resize notification
never requests another parent-window resize. Different parent and canvas aspect
ratios leave unused space rather than stretching or clipping the canvas. This
policy applies equally to ordinary dragging, maximization and tiling layouts.
The corner handle also scales the UI. Both paths retain the scale when the
software keyboard changes the logical height. Platform pixel scaling is accounted
for separately. Bounds below the minimum scaled canvas
size cannot contain the full canvas at the supported minimum scale.

The persistent header places **Scale** below the logo on the left, so it stays
accessible when an enlarged view extends beyond the screen. The library sound
count is below the patch display, aligned with its left edge. The footer groups
**Oversampling** and **Keyboard** on the right, beside the version label.
Oversampling and Scale use a bordered value field with a dropdown arrow; Scale
uses a light header label and Oversampling a muted footer label. Oversampling keeps the current multiplier visible and opens the
existing 1x/2x/4x/8x/16x quality menu on click; it does not accept text entry.
Quality remains available
in every view; moving it does not change processing, latency or session recall.

**Scale** is an editable percentage with a dropdown arrow. The arrow offers
50/75/100/125/150/175/200%; clicking the numeric value accepts direct input from
50 to 400%, including decimals and an optional percent sign. Enter commits and
Escape cancels; invalid or out-of-range input preserves the current view.
100% means the logical editor size before OS DPI scaling. Requests preserve the
canvas aspect ratio. Standalone requests are limited to the desktop work area;
the WM or plugin host owns the accepted parent bounds. If the requested size is
adjusted or refused, the canvas fits the actual bounds and the field shows the
accepted scale, rounded to one decimal place. Frame/corner resizing and keyboard
height changes update that readout too. The scale remains available in every
view and survives closing/reopening the editor within the same instance. It is
not a sound parameter, an automation parameter or part of serialized patch/project
state; a new instance requests 100% within its available bounds.

The footer's **Keyboard** toggle opens/closes the software keyboard on every
view. It starts closed; opening increases the logical editor height from 840 to
936 while retaining the current display scale. This is an editor control, not a
sound parameter or a saved patch value. The background covers the full logical
editor bounds after every resize, including the keyboard margins.

While open and focused, PC keys `A W S E D F T G Y H U J K O L` play chromatically
from MIDI note 48 at velocity 127, matching CheapSynth01. `Z`/`X` lower/raise the
base by an octave within MIDI notes 24–96. Held PC keys are released before an
octave change; key repeat does not retrigger notes or repeatedly change octaves.
Ctrl/Command/Alt shortcuts remain available. Text entry releases and suspends PC
notes. Hiding the keyboard, losing editor focus or closing the editor releases
its UI notes. Mouse and PC ownership are independent, and releasing UI notes does
not release externally held MIDI notes of the same pitch. External MIDI timing,
channels and repeated note behavior remain intact when no UI note owns the pitch.
UI ownership is published atomically and consumed without allocation at audio
block boundaries; it never enters patch/session sound state.

Use restrained red accents, warm-white legends and black controls, with subtle
Nord Lead 2 influence and Synth2's own identity. Prioritize high-use controls,
especially cutoff; do not invent unsupported instrument features to match a panel.
Bundle editor fonts so installed font families cannot change the layout.

Named selectors write actual source integers while preserving host IDs/ranges.
For example, chorus choices write 1/2/4. Opening the editor must not normalize
noncanonical values: show Default/Custom where appropriate. Numeric editing uses
source values; units are presentation only. Nominal envelope durations are not
measured note lengths. Context-dependent parameters retain source readouts until
an accurate unit conversion is available.

Host and editor numeric editing use integer steps for discrete selections,
quantized tuning, counts and MIDI source/destination words. Continuous dials
retain fractional values, including fine tuning and route sensitivity. Envelope,
fine-tuning and EQ unit readouts use the fractional dial
position rather than rounding to an adjacent integer first. Unit readouts remain
presentation only; numeric entry and host automation use the original source
scale. See the [processor contract](../contracts/processor-state.md).

`EditorLayout`, `EditorAppearance`, `EditorValueDisplay` and the exporter provide
shared native/preview geometry, appearance and value displays. Failed library
reads or invalid source loads preserve valid state. State recall retains source
text and its patch display name and does not require the source ZIP. UI notifications must not sweep imported
values through native setters and thereby change loading history.

The SDK-independent presentation headers live in `projects/Synth2/src/editor`
under `synth2::editor`; `src/host` owns the processor/state and catalog boundaries.
The header audit compiles each presentation header independently and rejects
dependencies on host/engine objects or iPlug2. `ParameterPresentation` is shared
with native host parameter registration without depending on ProcessorBridge.

The native adapter is split by responsibility: `Synth2Editor.cpp` coordinates
layout, page visibility and editor lifetime; `editor/EditorControls` owns drawing
and control input, including software keyboard mouse ownership; `editor/Synth2LibraryEditor.cpp` owns catalog/menu/dialog actions
and host notification after a successful sound transaction. The native keyboard
focus helper checks the editor view on Windows/macOS; `ComputerKeyboard` owns the
SDK-independent PC key map and release behavior. Shared native tags and
appearance helpers keep layout and library updates consistent. Catalog, selection
and dialog lifetime remain owned by Synth2; controls borrow them. Both native
build paths consume the same editor source list.

```sh
cmake --preset core
cmake --build build-oos-core --target ExportEditorLayout
python3 tools/preview_editor.py --exporter build-oos-core/ExportEditorLayout
python3 tools/render_editor_study.py --exporter build-oos-core/ExportEditorLayout
```

These commands generate local studies at `build/editor/synth2-preview.html` and
`build/editor/synth2-study.svg`; open those files to inspect the design. Generated
studies are excluded from Git. On Windows, use
`build-oos-core/ExportEditorLayout.exe` as the exporter path. Optional PNG output
adds `--png` to the study command and requires discoverable librsvg, Cairo and
GObject shared libraries; SVG/HTML generation does not require those libraries.
The optional PNG is another local generated export alongside the SVG.

The HTML preview illustrates the main synthesis controls; the persistent Library
view is covered by the native editor and portable catalog tests. The preview has
no audio engine or complete versioned loading resolver. SVG
and PNG studies are design renderings, not native plugin screenshots. Native
focus, scaling, DAW interaction and platform accessibility need platform testing.
See [EditorLayoutTest.cpp](../../tests/EditorLayoutTest.cpp), the patch-library tests
and [native APP capture](../validation/manual-platform.md#native-windows-app-capture).

The generated layout study and interactive preview are presentation aids governed
by this contract, not audio/DAW validation.

Momentary editor actions use the shared native ActionButtonControl, which starts
the SDK completion animation before invoking application code. Click feedback
must return to idle even when a menu or dialog captures the release event.
Tab and source-selection controls retain their independent selected-state behavior.

Delay Time uses host-synchronized note choices while preserving source values on
patch/state recall. The Delay panel displays the playback tempo and current left
and right tap times in milliseconds, including Spread, buffer limits and tempo
glides. The readout is audio-thread telemetry; before processing it is unavailable.
