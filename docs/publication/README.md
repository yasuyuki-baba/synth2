# Preparing the public product repository

Public users must be able to understand product requirements, build the portable
core and run its tests using the included documentation and public dependencies.

## License choice and remaining decisions

| Item | Observed state | Publication decision |
| --- | --- | --- |
| Project license | GPL-3.0-or-later selected; full text in [LICENSE](../../LICENSE) and grant in [README](../../README.md#license) | Confirm authorship/contributor scope; the choice does not override third-party terms |
| Patch-core source | Project-owned source maintained and built directly in Synth2 under GPL-3.0-or-later | Preserve applicable authorship and notices; confirm any other contributor scope |
| Preset assets | No factory bank is bundled; Init is the startup sound and tests generate their own banks | Preserve Init provenance; independently review any future bundled assets |
| Third-party code/fonts | miniz, tinyxml2, iPlug2 and font notices are included | Preserve their terms/notices; review additional SDK dependencies for native distributions |
| Publication history | A source snapshot needs its own reviewed root commit and refs | Preserve development history separately; review the complete candidate before publishing |

## Source snapshot and provenance

A current-tree export includes only tracked product files. It must exclude local
builds, recordings, downloaded banks and development history. Preserve the pinned
iPlug2 gitlink and public submodule URL; a source archive does not include submodule
contents, so native builders must initialize the documented public dependency.

Preserve authorship and copyright notices in source files and the included
third-party license notices when starting a new history. A new root commit does
not resolve contributor scope or grant redistribution rights for separately
authored assets. Retain development
history separately rather than rewriting existing refs as routine cleanup.

Review the actual exported files, commit graph, refs and dependency pins. A path
inventory or a secret scanner alone cannot certify publication readiness. Keep
dated review results outside the product requirements and distinguish scanner
exclusions from reviewed content.

## Publication candidate procedure

1. Complete contributor-scope and asset/fixture checks under the selected GPL-3.0-or-later project license.
2. Build/test a clean tracked-tree export with public dependencies or explicit
   source caches. Documentation links and test inputs must be self-contained.
3. Choose and review the public history separately from the private development
   repository. Review intended branch/tag/release metadata as well as files.
4. Publish the approved candidate, then check the documented build/test paths as
   a reader using only public dependencies and the product checkout.

The public reader entry points are [product contracts](../README.md),
[tests](../../tests/README.md), [desktop build requirements](../iplug2-migration.md)
and [test inputs and evidence](../evidence.md). The selected source-code license does not grant rights over separately authored
assets, settle contributor scope or change repository visibility.
