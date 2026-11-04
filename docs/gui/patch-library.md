# Persistent sound library

The Library view supports narrowing a collection with an optional bank, a tag,
all search words and a favorites-only filter. Words are matched independently
against the patch display name, bank name and tags, in any order. ASCII matching
is case insensitive; non-ASCII text matches its original UTF-8 bytes. Search
commits on Enter. The result list displays a bounded page; paging and the mouse
wheel browse results. The header's previous/next buttons select sounds within
the current filter, and selecting a result loads it for auditioning with the
persistent keyboard or MIDI input.

The library starts empty and the processor starts with Init. No factory bank is
bundled or automatically added. Init is the startup sound, not a protected library
entry. Bank names do not change import, metadata or removal behavior.

Use **Add sounds...** to import a `.sy1`, a ZIP bank, or a directory of `.sy1`
files. Directory imports include nested folders, retain relative names and use
natural numeric order. Files and folders can also be dropped onto the result
list. The importer validates every source before publishing a bank, skips exact
source-text duplicates across the library and reports added/skipped counts.
Importing, filtering, tagging, favoriting and deleting do not load a sound or
change the current processor state. Only explicit sound selection loads a patch.

Imported sources are copied into a Synth2-owned library. The original `.sy1`,
ZIP or folder is not edited and can be moved after import. Each imported bank
has an independent directory and each patch has a stable library ID. Display
names are not identities, so equal names can have different tags. Every library
sound can be tagged, favorited and removed; the same applies to every added bank.

Tags are optional user metadata, separate from `.sy1` source text, host parameters
and DAW sound state. Select a sound, enter comma-separated tags, then press Enter.
Tag spelling is preserved; duplicate ASCII-case variants in an edit are collapsed.
A patch supports up to 32 tags of 128 UTF-8 bytes each. Automatic name-based or
LLM tagging is not part of this feature. A DAW project embeds source text and
patch display name and remains recallable after library removal; library tags
and favorites are shared user preferences rather than DAW project data.

## Storage and ownership

| Platform | Default library folder |
| --- | --- |
| Windows | `%APPDATA%/Synth2/Library` |
| macOS | `~/Library/Application Support/Synth2/Library` |

**Add sounds... → Open library folder** reveals the folder for backup or transfer.
Copy the entire folder to preserve the sounds, tags and favorites. Internal files
are managed by Synth2; use the import action to add external banks. Library
removal asks for confirmation and leaves the original import and current sound
intact. A bank can be removed after selecting its bank filter. Refresh or reopen
the editor to see another plugin instance's committed changes. Simultaneous
metadata edits to the same sound use the last completed write.

The `banks` directory contains committed `bank-*` folders. Each has a `name`,
numbered `.sy1` sources and corresponding `.name` display labels. Imports stage
in hidden temporary folders and become visible via a directory rename. Tags and
favorites live in separate `tags` and `favorites` directories keyed by library ID;
metadata replacements use temporary files and atomic replacement. Deleted banks
are unpublished before their owned files are removed. The importer never uses
ZIP entry names as filesystem paths.

Individual banks retain the PatchLibrary byte/entry limits. The complete catalog
is bounded to 100,000 patches and 512 MiB of source text. Bank
and display names are bounded to 16 KiB each. Invalid imports publish no partial
bank. Failed reloads retain the previous valid in-memory catalog and report the
error. All filesystem and catalog work belongs to the UI side, outside audio
callbacks. Catalog indices are not host program ordinals; selection retains the
source bank's ordinal.

## Verification boundary

`PatchCatalogTest.cpp` verifies accumulation, duplicate detection, folder ordering,
metadata persistence in a new catalog, combined filters, empty-library startup,
failed imports, independent instances and removal followed by bank-free state
recall. Existing library and processor tests cover the loading transaction and
source/state fidelity. These portable tests do not validate native mouse, focus,
file dialog or DAW interaction. See [the editor contract](README.md).
