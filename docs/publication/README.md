# Source and binary distribution requirements

Project-owned source is GPL-3.0-or-later, with the full text in
[LICENSE](../../LICENSE) and the grant in [README](../../README.md#license).
Preserve applicable source authorship and copyright notices. The project license
does not relicense third-party components or grant rights over separately
created patches, fonts or other assets; resolve contributor scope and asset
permissions before distributing additional material.

## Included assets and dependencies

- Init is the startup sound and the library is initially empty. No factory bank
  is bundled; public tests generate their own patch banks. Preserve Init
  provenance and review rights for any future bundled sounds independently.
- miniz, tinyxml2, iPlug2 and editor fonts retain their included license notices.
  Preserve those notices in source and applicable binary packages; review SDK
  terms for each native format. See the [desktop build guide](../desktop-build.md).
- Preserve the pinned iPlug2 gitlink and public submodule URL in source checkouts.
  Source archives do not include submodule contents; native builders must
  initialize the documented dependency.

## Distribution contents and verification

Distribute product source and required assets without local builds, downloaded
banks, recordings, credentials or investigation outputs. Source and binary
packages must retain their applicable licenses, notices and provenance.
Publishing a source snapshot does not itself clear historical asset rights or
resolve contributor scope.

Product requirements, build instructions and portable tests must remain
self-contained. Verify source packages with public dependencies or explicit
source caches using the [test guide](../../tests/README.md). Review actual package
contents, dependency pins and metadata; an inventory or secret scanner alone does
not establish distribution readiness. Native packaging and its verification
boundaries are described in [CI](../ci.md).

See [validation](../contracts/validation.md) for test input and evidence rules and
[product contracts](../README.md) for supported behavior. Portable success does
not certify native plugin/DAW operation, and these distribution rules do not
change repository visibility.
