# Reference files

Original files of the current game version, taken from the Steam installation. They are the source of truth for the key numbering that every language pack must follow.

| File | Game path |
|---|---|
| `English.lang` | `Wolfpack_Data/Languages/Official English.lang` |
| `English.xml` | `Wolfpack_Data/Language/Official/English.xml` (tooltips for controls) |

- Game version: **0.26R6** (string stored in `Wolfpack_Data/level2`, shown in the main menu)
- Steam build id: `24991795` (2026-08-29), Unity `2020.3.49f1`

When the game updates, replace both files and run `python tools/sync.py` to add the new keys to all packs.
