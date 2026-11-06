# Synth2 patch format

New Synth2 patch exports use `ver=200` in a UTF-8 `.sy1` file. This is a Synth2
format; Synth1 integer files remain importable with their original version rules.
The explicit Synth1 semantic export API still writes version 113.

The canonical header is:

```text
ver=200
name="Patch name"
```

The name is double quoted. Backslashes and double quotes are escaped with a
backslash; `#`, `;`, commas and UTF-8 characters inside the name are literal.
Names cannot contain line breaks. A missing or empty name becomes `Untitled`.
An optional unquoted title before the version header is accepted for manually
authored files. The formatter always writes the quoted metadata form.

After the header, each line is `index,value`, with one entry for every parameter
0–125, 127 and 128. Order is immaterial. Duplicate indices, missing parameters,
non-finite numbers, malformed tokens and out-of-range values are errors. The
entire file is validated before it changes the selected program or sound.
Blank lines, CRLF and `#`/`;` comments outside the quoted name are accepted.

Values use the existing dial units, not normalized 0–1 host values. The ranges
are 0–127 except p9 (-24–24), p86/p88 (-1–65535) and master volume p128 (0–1).
Decimal and scientific notation are accepted independently of the system locale.
Formatting retains enough decimal digits to recover every stored float exactly.
Selectors still use their existing discrete render meanings; serialization does
not discard fractional values from continuous controls.

p126 is excluded: the file version is the header. Its legacy host ID and 0–127
range remain stable; a version 200 load initializes that legacy value to 113.
The version 200 mapping mode is independent of p126 host edits and is preserved
in version 3 processor states with `patchFormat="200"`.

Imported native file dials are resolved before export and live edits override
those resolved values. Version 200 reads these dials directly without applying
the old file-version correction again. Export is a sound preset, not a snapshot
of running oscillators, clocks, RNG state or effect buffers; compare cold playback
with matching preparation and MIDI histories. Host state retains original source
and loading metadata separately from a newly exported patch.

`ProcessorBridge::savePatch()` returns the complete text outside the audio
callback. `loadPatch()`/`loadProgram()` accept both Synth2 and legacy files; bank
and library import use that same validation boundary.
