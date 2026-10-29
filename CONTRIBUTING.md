# Contributing

## Repository layout

```
packs/<code>/            one folder per language
  pack.json              metadata (status, game version, authors)
  addons/                optional BepInEx/AutoTranslator texts (own work only)
  Wolfpack_Data/         copied 1:1 into the game folder
    Languages/*.lang     main UI texts (JSON, numeric keys)
    Language/Official/English.xml   control tooltips (optional)
reference/               original English files of the current game version
templates/new-language/  copy this to start a new language
tool/                    installer & editor (C++, PolyForm Noncommercial)
tools/                   validate / build_index / / sync / build / find_strings (Python 3, no dependencies)
docs/                    installation, game updates
```

Each game version has its own branch: `version/0.26`, `version/0.27`, `version/0.26R6`, ... `main` always carries the newest version.

## Adding or improving a translation
Easiest: use the Editor of the Language Tool and export. Or copy the template by hand (below).

**After changing anything below `packs/` or `reference/` run `python tools/build_index.py`** - the tool reads `packs/index.json`; CI checks that it is current.
See [templates/new-language/README.md](templates/new-language/README.md). Run `python tools/validate.py` before every PR.

## Game updates
See [docs/GAME-UPDATES.md](docs/GAME-UPDATES.md).

## Rules
- Credit yourself in `pack.json` -> `authors`. Write `@handle` for a GitHub account (the tool links it), otherwise a plain name. Everybody who contributes to a pack is listed there and shown in the tool.
- Never change the numeric `key`s, only the `value`s.
- A `.lang` file must keep all four parts: `header`, `pairs`, `suffixPairs` (unit suffixes) and `morsePairs` (radio phrases). If `suffixPairs` or `morsePairs` is missing, the game hangs on "Loading languages" (see issue #30). `validate.py` checks this.
- Keep placeholders and tags (`{0}`, `<b>`, `%s`).
- UTF-8, LF line endings.
- Pack status: `original` (unmodified game files, only updated on game updates), `complete` (everything translated and tested in-game), `partial` (all `.lang` entries done, hard-coded texts open), `needs-review` (migrated or partly English), `outdated`.
