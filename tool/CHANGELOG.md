# Wolfpack Language Tool - Changelog

## 1.2.1 (unreleased)
- *Export for GitHub* of an opened pack keeps its `pack.json` (name shown in the tool, status, notes, authors) and raises the version (e.g. 1.0.0 -> 1.1.0) instead of resetting it to 0.1.0. Your GitHub name is added to the authors.

## 1.2.0 (2026-10-29)
- **Editor: new view "BepInEx"** for the hard-coded in-game texts (234 texts from `reference/hardcoded-en.txt`). Existing add-on translations (e.g. German) are loaded, extra and regex lines are kept as *custom*.
- **Add text:** saw an English text in the game that is not in the list? Type it in the BepInEx view, add a translation and press *Add text* (also works with `r:` regular expressions). Own entries can be removed again.
- **Collect new texts (experimental):** tick *Collect new texts* in the BepInEx view and press *Test in game*. While you play, the AutoTranslator writes every text it sees to `BepInEx/Translation/<code>/FoundTexts.txt`. After closing the game, *Import found* adds the new ones to the list. Texts the game already translates and texts already in the list are skipped. If the game keeps changing texts, the AutoTranslator may switch itself off (spam protection).
- Editor messages are shown as a single line, so the table keeps its space.
- *Export for GitHub* writes the add-on (`addons/autotranslator`: texts + config) next to the language files; *Test in game* installs it too when BepInEx is installed.
- The AutoTranslator plugins are shared: add-ons of new languages only need their texts, the tool installs the plugins.

## 1.1.1 (2026-10-29)
- Languages page: the update hint no longer pushes *Restore original English* out of the window (updates are shown on the card and offered on the dashboard).
- Update button on the dashboard: the "!" icon no longer touches the text.

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
