# Bundled liblzma decoder

Source: XZ Utils 5.8.4 release tarball. The URL and SHA-256 are pinned in
`dependencies.lock.json`. liblzma is licensed under 0BSD; preserve COPYING,
COPYING.0BSD and the source notices. No upstream source changes are applied.

The subset retains the root CMakeLists.txt, COPYING, COPYING.0BSD,
COPYING.LGPLv2.1, AUTHORS and README; cmake; src/common; src/liblzma; and lib.
The last directory contains the GNU getopt replacement that upstream CMake
configures on Windows even with all tools disabled. It is not linked into any
Synth2 product; its LGPL source license is retained in this directory.

`cmake/Synth2Archives.cmake` builds static, single-threaded liblzma decoders.
Encoders, tools, translations and documentation are disabled. The upstream
test tree is omitted. The vendored target supplies the archive reader directly,
without system-library discovery, CMake downloads or a runtime decompressor.
