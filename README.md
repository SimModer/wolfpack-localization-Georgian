# Wolfpack Localization

Community language packs for the submarine game *Wolfpack* (Usurpator, Steam).

**Current game version: 0.26R6** | [Installation](docs/INSTALLATION.md) | [Contributing](CONTRIBUTING.md) | [Game updates](docs/GAME-UPDATES.md)

## Language packs (`packs/`)

| Code | Language | Status |
|---|---|---|
| `en` | English (original game files, use to revert) | original |
| `de` | Deutsch (historical Kriegsmarine terminology) | partial (menus and tooltips done, hard-coded texts open) |
| `fr` `zh-Hans` `ka` | Français, 简体中文, ქართული | needs-review (tooltips translated) |
| `nl` `es` `el` `it` `ja` `ko` `pl` `pt-BR` `ru` `tr` | Dutch, Spanish, Greek, Italian, Japanese, Korean, Polish, Portuguese (BR), Russian, Turkish | needs-review |

"needs-review": migrated from game version 0.26/0.27 to the current key numbering; three new entries are still English. Help welcome!

## Installer & editor tool
The [Wolfpack Language Tool](tool/README.md) (portable `.exe`, see Releases) installs packs and lets you create or edit translations. Its source code is in `tool/` under the PolyForm Noncommercial License.

## Versions
Every game version lives on its own branch (`version/0.26`, `version/0.27`, `version/0.26R6`); `main` is the newest.

## Start a new language
Copy [`templates/new-language`](templates/new-language) to `packs/<code>`, translate, run `python tools/validate.py`, open a PR.

## Deutsch
Anleitung: [docs/INSTALLATION.md](docs/INSTALLATION.md). ZIP aus den Releases in den Wolfpack-Ordner entpacken (Dateien überschreiben).

## License
Language packs and docs: MIT, see [LICENSE](LICENSE). The tool in `tool/`: PolyForm Noncommercial 1.0.0, see [tool/LICENSE](tool/LICENSE).
