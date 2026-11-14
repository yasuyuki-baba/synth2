# Self-authored RAR fixtures

All archive contents are authored for Synth2 tests. `input` contains the exact
expected patch bytes, a README and macOS metadata that must be ignored.
Repeated comment lines exercise actual compression. Japanese filenames and a
CRLF patch exercise Unicode paths and source preservation independently of the
host locale.

Regenerate with `python3 generate.py /path/to/rar-6.24`. The proprietary encoder
is used only to create fixtures and is not required by tests or distributed
with Synth2. Both RAR4 and RAR5, stored and compressed entries, and RAR5 solid
compression are exercised. Negative fixtures cover encrypted headers, the
unsupported RAR4 solid variant, empty patches, archives without patches, and
oversized patch/non-patch contents. Corruption and truncation are derived at
test runtime from the stored RAR4 fixture.
