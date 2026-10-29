# Wolfpack Language Tool - Changelog

## 1.1.0 (2026-10-29)
- **Fix for "game hangs on Loading languages" ([#30](https://github.com/niclqs/wolfpack-localization/issues/30)):** all language packs were missing the `suffixPairs` and `morsePairs` lists. The packs on GitHub are fixed; open the tool and press **Update** if you installed a pack before.
- The editor keeps and translates **Units & Morse** (unit suffixes such as " kts" and radio phrases) and writes files exactly in the game's own format. Exports and *Test in game* of older versions produced broken files.
- **BepInEx 6 with one click:** install and uninstall the mod loader needed by the add-ons (official `6.0.0-pre.2` build, SHA-256 checked).
- Dashboard shows whether BepInEx is installed.
- Fixed a developer warning on the Changelog page ("conflicting ID").

## 1.0.1 (2026-10-17)
- German add-on for hard-coded texts (XUnity.AutoTranslator plugins, config, texts, texture).
- Hint and link when BepInEx is missing.
- Flags and contributors in the language list.

## 1.0.0 (2026-10-17)
- First release: game detection, one-language-at-a-time install with `.wpl` marker, update detection, restore original English, editor with export for pull requests, changelog from GitHub.
