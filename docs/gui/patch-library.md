# Persistent sound library

The Library view combines bank, tag, keyword and favorites filters. All search
words match independently against the title, relative file/ZIP entry location and
tags. ASCII matching is case insensitive; other UTF-8 text matches its original
bytes. The bounded result page and mouse wheel browse results; previous/next selects
within that filter. Only explicit sound selection loads a sound. Registration,
refresh, filtering and metadata edits preserve the sounding processor.
The processor starts with Init and the library starts empty; no factory bank is
bundled, privileged or automatically added.

The persistent sound header displays the patch name and edited marker on the first
line, with the selected entry's bank path on a smaller second line. Init and sounds
without a matching catalog entry show only the patch name. The tooltip includes
both lines. The full display remains clickable to open the Library view. The
selected result row uses a contrasting background, a left accent stripe and a bold
title; favorites remain a separate indicator. Long titles are ellipsized at the
end and long bank paths at the beginning, using measured UTF-8-safe text widths
to prevent column overlap. Row tooltips expose the full title, bank path and tags. Filtering does not change the selected
sound or mark another row as selected.

## Files and registration

The configured user directory is recursively scanned for `.sy1` files and `.sy1`
entries in ZIPs. External additions and replacements become visible on refresh or
editor reopen. A manual Refresh requested during another library job is queued
and rescans after that job completes. ZIPs remain single files: they are read in memory and never expanded
or rewritten. Invalid sources remain visible with an error and do not prevent
other files from appearing. Selecting an invalid row preserves the current sound.

Add a file or folder from **Add...**, or drop it on the list. Registration copies
actual `.sy1` and ZIP files and leaves the source intact. Folder names and internal
hierarchy are retained. Already stored files are referenced without copying. A
bank is a directory: loose `.sy1` files in the same folder share a bank, and
ZIP entry subdirectories form banks under the archive's relative filename.
For example, `Pads/Collection.zip/Analog/Soft` is distinct from
`Leads/Collection.zip/Analog/Soft`. The bank menu shows directory and ZIP hierarchy in one indented list, without
submenus. The bank selector uses the platform popup menu so long lists use OS
scrolling rather than the SDK custom popup, whose wheel handler is unimplemented.
Parents are disabled classification headings. A parent with direct sounds
offers an indented **This folder** row. Selecting a bank includes only its direct sounds; **All banks**
at the top searches across every bank while retaining tag, keyword and favorite
filters. Empty directories are omitted. Files directly in the library root use `Library root`.
Bank paths use forward slashes on every platform. Empty folders are omitted.
Grouping leaves sound IDs, tags, favorites, source text and original ZIP program
ordinals unchanged. ZIP directory grouping never expands or rewrites the archive.
A physical filename conflict offers Replace, Keep both (numbered suffix), or Skip,
with an apply-to-all option. Source text and sound titles do not determine
collisions. Two different filenames may intentionally contain identical sounds.

After registration, the browser clears other filters and shows exactly that
registration's item IDs, including the entries of registered ZIPs. The registration
filter is indicated in the header. **Show all sounds**, or choosing a bank, clears
that restriction. The result summary distinguishes registered/skipped files and
errors. No sound is automatically selected. Filesystem scans, registration and
saving run on worker jobs outside rendering; conflict choices are handled by the
editor with dialog lifetime guards.

## Saving and edited sounds

**Save → Save** writes to the loaded loose library `.sy1`. ZIP sources, Init and
sources no longer in the library use Save as. **Save → Save as...** chooses a name
and a destination within the configured directory, including its subfolders.
Existing destinations require confirmation. Saving uses the existing Synth2
`ver=200` patch API, preserving fractional controls, reserved values and master
volume. A ZIP itself is never modified. Complete writes are staged before file
publication; failures retain the sounding state and its edit baseline.

Successful saves select and reveal the saved file and establish a new baseline.
The current sound name shows `*` when its parameter values differ from the last
load/save baseline; returning all values to that baseline clears it. Performance
MIDI, tags, favorites and browser operations do not mark the sound edited. DAW
parameter automation does. DAW project saving preserves the baseline and therefore
does not clear unsaved preset edits. Sound changes do not prompt on every audition.

## Metadata and storage

Tags and favorites remain shared user preferences separate from source files and
DAW sound state. Library refresh infers short automatic tags for existing and new
sounds from the patch title and resolved controls, on the library worker using an
isolated loading bridge. It does not render audio or load the sounding processor.
Tag inference never uses filenames, bank identity or source hashes. Invalid sources
receive no inferred tags.

Manual tags use the ordinary text color; automatic tags use the muted text color
in the same list. Both participate in the same tag and keyword filters. Enter up
to 32 comma-separated manual tags of 128 UTF-8 bytes each and press Enter. Manual
tag spelling wins over an ASCII-case-equivalent automatic tag, which is shown
only once. Removing that manual tag reveals the inference again unless hidden.
**Auto tags...** hides individual inferred tags or restores hidden tags for the
selected sound. Hidden automatic tags remain hidden across refresh/reopen, even
when inference rules or the source change. Hiding a tag does not remove a manual
copy. User overrides are published atomically in the existing `.tags` sidecar;
legacy comma-separated tag sidecars remain readable.

Name rules recognize whole English words, camel-case boundaries and selected
leading abbreviations such as BS, LD and PD. They suggest Bass, Lead, Pad, Pluck,
Keys, Organ, Strings, Brass, Bell, Percussion, FX and Arp. Obvious disagreements
with envelope settings (or a disabled arpeggiator for an Arp name) defer that
name suggestion. Other name matches remain heuristic; they are not ground truth.

Feature tags include Mono and Legato as distinct modes, Unison, FM, Sync, Ring mod,
Noise, Arp, Distortion, Slow attack, Long release, Short decay and Filter pluck.
They describe settings, not guaranteed audible characteristics. Slow attack means
at least 150 ms nominal amp attack; Long release means at least one second nominal
amp release. Short decay requires fast attack, low amp sustain and decay within
two seconds. Filter pluck considers a fast low-pass envelope with low sustain,
positive depth and a restrained base cutoff. FM is omitted when Ring takes
precedence. No inferred tag changes DSP settings or original patch/ZIP bytes.

Metadata IDs use relative file plus ZIP-entry identity; safe encoded keys prevent
path/underscore collisions. Existing managed `banks/bank-*` directories and their
metadata remain readable with their original IDs and ordinals. Only recognized
legacy records are excluded from file indexing to avoid duplicates. Ordinary
ZIP/`.sy1` files remain discoverable alongside managed records.

| Platform | Default library folder |
| --- | --- |
| Windows | `%APPDATA%/Synth2/Library` |
| macOS | `~/Library/Application Support/Synth2/Library` |
| Linux portable tools | `$XDG_DATA_HOME/Synth2/Library` or `~/.local/share/Synth2/Library` |

**Add... → Open library folder** reveals this folder. **Choose library folder...**
changes the shared root without moving files. Other instances adopt the configured
root on editor reopen. Back up the complete directory to retain metadata.
Delete or rename files through the file manager opened by **Open library folder**,
then Refresh to update the browser. The editor has no per-sound or per-bank Remove
buttons. File deletion does not replace the sounding patch or saved project state. Separate instances see committed file/metadata changes on refresh.

Per-file and per-ZIP PatchLibrary limits still apply. The catalog retains its
100,000-sound/512-MiB source limit; bank/display names remain bounded to 16 KiB.
A failed catalog reload retains the previous valid snapshot. Catalog indices are
not host program ordinals; ZIP entries retain their source-bank ordinal.

## Session recall

Each plugin instance saves its query, bank/tag/favorite filters, registration IDs,
selection, result-page offset, view and edit baseline in wrapper state v2. Wrapper
v1 remains readable and retains the processor's patch name. The existing processor
state independently embeds the sounding source and edits. Recall never reloads a
selected library file, so moved/deleted/replaced files cannot overwrite the saved
sound. A scan completes before the restored result offset is clamped. A project
restore also supersedes pending jobs' browser/baseline updates.

Standalone APP stores the same complete session on normal teardown and restores
it on startup. Library root configuration is a shared per-user setting; browser
state and the sounding patch are instance-owned.

## Verification boundary

`PatchCatalogTest.cpp` checks file-backed accumulation, metadata, exact registration
filters, recursive directory/ZIP banks, direct-bank and All banks selection, unchanged source-bank
ordinals and metadata identities, legacy managed-bank readability and removal without
changing sound recall. `PresetFilesTest.cpp` checks recursive/ZIP scanning, physical
file conflicts, confined saves, fractional `ver=200` round trips, independent edit
baselines and transactional wrapper-v1/v2 recall. Other loading/state/patch-format
tests retain the source and rendering contracts. These portable checks do not
certify native dialogs, DAW interaction or platform device operation.

`EditorLayoutTest.cpp` checks header labels with and without a bank, duplicate
patch names across banks, UTF-8 labels and the independent edited marker.

Patch headers remain part of the sound: `color=red`, `green`, `blue`, `yellow`,
`magenta` and `cyan` display a small marker beside the library title. `default`,
missing and unknown colors use the ordinary row appearance. Color markers are
independent of selection and favorites. No color editing or color filter is provided.
Unknown header lines retain their spelling, order and duplicate keys through
version 200 export and project recall; `name` and `ver` are generated by the codec.
Colors and opaque headers do not change audio controls.
