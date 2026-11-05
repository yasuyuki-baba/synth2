# Editor behavior and visual contract

Ordinary sound design belongs on one Synth surface with Synth1-inspired functional
groups. Oscillators, Filter and Amplifier occupy the top; two LFOs, Voice/Unison
and ARP occupy the middle; effects and EQ/Pan occupy the bottom. MIDI routing uses
a supplementary view. Serialized metadata and reserved fields stay outside the
editor; their host IDs, patch values and state serialization remain supported.
There is no Advanced tab. Preset browsing and master
volume remain visible. The supplementary [Library view](patch-library.md) accumulates
imported sounds and combines bank, tag, keyword and favorite filters.

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
and control input; `editor/Synth2LibraryEditor.cpp` owns catalog/menu/dialog actions
and host notification after a successful sound transaction. Shared native tags and
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
