# Contributing

## Repository layout

```
packs/<code>/            one folder per language
  pack.json              metadata (status, game version, authors)
  Wolfpack_Data/         copied 1:1 into the game folder
    Languages/*.lang     main UI texts (JSON, numeric keys)
    Language/Official/English.xml   control tooltips (optional)
  addons/                optional extras (e.g. German AutoTranslator texts)
reference/               original English files of the current game version
templates/new-language/  copy this to start a new language
tools/                   validate / sync / build / find_strings (Python 3, no dependencies)
docs/                    installation, game updates
```

Each game version has its own branch: `version/0.26`, `version/0.27`, `version/0.26R6`, ... `main` always carries the newest version.

## Adding or improving a translation
See [templates/new-language/README.md](templates/new-language/README.md). Run `python tools/validate.py` before every PR.

## Game updates
See [docs/GAME-UPDATES.md](docs/GAME-UPDATES.md).

## Rules
- Never change the numeric `key`s, only the `value`s.
- Keep placeholders and tags (`{0}`, `<b>`, `%s`).
- UTF-8, LF line endings.
- Pack status: `complete` (all entries translated and tested), `needs-review` (migrated or partly English), `outdated`.
