# Bundled libarchive reader

Source: libarchive 3.8.11 release tarball. The URL and SHA-256 are pinned in
`dependencies.lock.json`. Upstream licensing is preserved in `COPYING`, the
source headers, and the collected `NOTICES` shipped with binary packages.
BLAKE2's offered CC0 terms are selected.

The vendored subset retains the root CMakeLists.txt, COPYING and README.md;
build/version, build/cmake and build/pkgconfig; all top-level files in
libarchive; and CMakeLists.txt in cat, tar, cpio, unzip and their test
subdirectories, plus libarchive/test. Upstream test data and program sources are omitted. The disabled program/test directories retain their CMake entry
points so the upstream build remains usable without rewriting it.

`cmake/Synth2Archives.cmake` builds the static library with external compression,
crypto, tools, tests and installation disabled. Only RAR, RAR5 and 7z format
readers are registered by the product. liblzma is supplied as a bundled static
target for 7z; other external codecs remain disabled. ZIP continues to use miniz. No CMake
download or additional runtime library is needed.

Local patch: apply `patches/rar4-stored-crc.patch` with `patch -p1`. Upstream's
stored RAR4 reader returns EOF when its read offset reaches the file size,
bypassing the checksum check in `read_data_stored`. Permit that final read
after sequentially consuming the full file; retain EOF after a partial seek.
`ArchiveBankTest` covers both successful stored entries and a flipped payload byte.

When updating, regenerate NOTICES from the unique leading comment blocks of
the bundled libarchive/*.c and *.h files, retaining each file's attribution.

Local patch: `patches/7z-resource-limits.patch` bounds entry/header counts to
65536, LZMA/PPMd decoder memory to 256 MiB, and decoded headers/solid folders/BCJ2
substreams to 64 MiB. Apply with `patch -p1`. These checks run inside the reader
before the product receives entries. `ArchiveBankTest` exercises oversized entry
counts, dictionary properties and declared contents without large allocations.
