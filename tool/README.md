# Wolfpack Language Tool

Portable installer and editor for the language packs in this repository. One `.exe`, no installation, nothing is written next to it.

## Features
- Launcher-style UI: dashboard (game version, installed languages, pack status), languages, editor and a changelog loaded live from the GitHub commits. Frameless window with a custom title bar.
- Finds the Wolfpack folder automatically (Steam libraries, all drives) or lets you pick it. Reads the game version from the game files.
- Loads the list of language packs from GitHub (`packs/index.json`, every file is verified with SHA-256) and installs the selected one into the game folder, overwriting the old files. Warns about missing files, untranslated tooltips, unfinished packs and a game version that does not match the pack.
- **One language at a time.** The tool writes a marker file `.wpl` into the Wolfpack folder (pack, version, files, time). Installing another language removes the files of the previous one first; *Restore original English* brings back the original files. Packs installed by hand (no `.wpl`) are cleaned up on the first install.
- Shows flags and the contributors of every pack (`authors` in `pack.json`).
- Asks for administrator rights only if the game folder is write-protected (the button restarts the tool elevated).
- *Restore original English* installs the `en` pack with the unmodified game files.
- Editor: create a new language from the English reference or edit an existing pack (from GitHub or already installed). *Export for GitHub* writes a ready `packs/<code>/` folder to `Downloads\Wolfpack-L10n-Export`; upload it to your fork and open a pull request. *Test in game* copies the work in progress into the game folder.

## Build (Windows)
Needs Visual Studio Build Tools (C++), CMake and Ninja. Dependencies (Dear ImGui, nlohmann/json) are downloaded by CMake.

```
tool\build.bat
```
Result: `tool\build\WolfpackL10n.exe` (statically linked, ~1 MB).

`WolfpackL10n.exe --selftest <logfile>` runs a headless check (game detection, download, hash check, install into a temp folder, parser round trip). Set the environment variable `WOLFPACK_L10N_BASE` to point the tool at a different repository URL (e.g. a local `python -m http.server`).

## License
[PolyForm Noncommercial 1.0.0](LICENSE): free for any non-commercial use, commercial use is not allowed. This applies to the tool in this folder only. The language packs in `packs/` are MIT licensed (see the repository root). Dear ImGui and nlohmann/json are MIT licensed.
