# Handling a game update

1. Update the game in Steam and create a branch `version/<new-version>` from `main`.
2. Find the new game version: the string (e.g. `0.26R6`) is stored in `Wolfpack_Data/level2` and shown in the main menu; the Steam build id is in `steamapps/appmanifest_490920.acf`.
3. Copy `Wolfpack_Data/Languages/Official English.lang` to `reference/English.lang` and, if it changed, the official `English.xml` to `reference/English.xml`. Update `reference/VERSION.json`.
4. **Check whether keys were inserted in the middle.** The numbering shifts when the developers add an entry (this happened with `Very Large` at key 163). Existing packs must then be re-keyed by matching the English text of the old and the new file; a plain `sync.py` would put translations on the wrong keys.
4b. Also copy both reference files to `packs/en/Wolfpack_Data/...` (only the newest version is kept; older ones are in the version branches).
5. `python tools/sync.py` adds missing keys (English text as placeholder) to every pack.
6. `python tools/validate.py`, `python tools/build_index.py`, translate the new entries, bump `game_version` / `pack_version` in each `pack.json`.
7. `python tools/build.py`, tag `v<pack-version>` and attach `dist/*.zip` to a release.

## Finding more hard-coded text
`python tools/find_strings.py "<game>/Wolfpack_Data"` lists text literals from the game code that are not in any `.lang` file. The list contains engine noise, so review it manually. Texts found there cannot be translated through a `.lang` file; they are only reported to the game developers or handled by separate tools.
