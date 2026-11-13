# Attribution and source provenance

This repository modifies CrossPoint source for an independently maintained
BOOX Picco build. Credits identify the source of the work; they do not imply
that upstream authors support, endorse or maintain this fork.

## Foundation

- [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
  supplies the inherited reader, Library, settings, networking and application
  architecture. Original copyright remains with the CrossPoint Reader organization
  and other credited authors under their applicable notices.
- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) supplies board support,
  drivers, fonts and shared UI components. This firmware pins a
  [downstream SDK fork](https://github.com/DumbledoreCalrissian/freeink-sdk);
  the gitlink, rather than the branch name alone, identifies the dependency.
- [CrossPoint Simulator](https://github.com/crosspoint-reader/crosspoint-simulator)
  supplies the host simulator. The sibling
  [downstream simulator](https://github.com/DumbledoreCalrissian/crosspoint-simulator)
  includes the Picco test-link repair and download-test support.

Git history retains original authorship and the fork's modifications. The
runtime's inherited CrossPoint identifiers describe its source lineage; they
are not a claim that this is an official release.

## Downstream changes

This fork adds Todo, foreground Pomodoro, Sudoku and puzzle-pack management,
EPUB reading-statistics foundations, File Transfer navigation changes and
clippings-export filtering. It also includes supporting shared UI changes and
simulator checks. [README.md](README.md) and the application guides distinguish
implemented features from unfinished statistics integration and physical checks.

Sudoku puzzles come from the public-domain
[Sudoku Exchange puzzle bank](https://github.com/grantm/sudoku-exchange-puzzle-bank).
The [converter and format](docs/sudoku-bank-format.md) and release provenance
record the inputs, validation and generated packs. Lucide icons and other
third-party assets retain their own credits and licences.

Research references are not automatically copied implementations or dependencies.
When copying or adapting code or assets, retain their actual notices and identify
changes under the applicable terms; do not attribute original authorship to the fork.

## Licence notices

The root [LICENSE](LICENSE) retains the original MIT notice for CrossPoint code.
Separate dependency, font, icon and data terms remain in [licenses/](licenses/README.md)
and their source directories. The combined firmware uses GPLv3 because it includes
wolfSSL, as explained in that guide. This documentation cleanup does not relicense
upstream or third-party work or resolve the open licensing questions recorded there.

Upstream marketing copy, project websites and fundraising badges are not this
fork's identity or funding policy. Required notices and technical source references
are retained separately from that promotional material.
