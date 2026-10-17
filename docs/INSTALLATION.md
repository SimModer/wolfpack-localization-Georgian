# Installation

## Recommended: the Wolfpack Language Tool
Download `WolfpackL10n.exe` from the [Releases](../../releases) page and start it (portable, no installer). It finds your game, lists all language packs from this repository and installs the one you pick. *Restore original English* undoes it. See [tool/README.md](../tool/README.md).

## Manual installation
The tool allows exactly one installed language. If you install by hand, remove the old `Community *.lang` first.

1. Download the zip for your language from the [Releases](../../releases) page (`Wolfpack-L10n_<code>_game-<version>_v<pack>.zip`). Pick the pack whose game version matches yours (shown in the main menu).
2. Steam: right-click *Wolfpack*, then **Manage, Browse local files**.
3. Extract the zip into that folder. It contains a `Wolfpack_Data` folder. Confirm merging and **overwrite** existing files (this replaces `Language/Official/English.xml`; Steam's *Verify integrity of game files* restores the original).
4. Start the game and choose the language under **Settings, Language**.

## Optional: add-on for hard-coded texts
> **BepInEx is required:** download BepInEx 6 (IL2CPP, 64 bit) from <https://github.com/BepInEx/BepInEx>, extract it into the Wolfpack folder (next to `Wolfpack.exe`) and start the game once so it creates its folders. The add-on only contains the translation plugin, not BepInEx itself.

Packs with an add-on also translate texts that are hard-coded in the game. The add-on contains the XUnity.AutoTranslator plugins; you only need [BepInEx 6 (IL2CPP)](https://github.com/BepInEx/BepInEx) in the game folder first (the language tool then offers the add-on as a checkbox). Then extract the add-on zip into the game's main folder (next to `Wolfpack.exe`). To remove it, delete the `BepInEx/Translation/<code>` folder.

## Reverting to the original
Install the `en` pack (`Wolfpack-L10n_en_...zip`) the same way. It contains the unmodified `Official English.lang` and `English.xml`, so it overwrites the translated tooltips. Then select *Official English* in the language settings. Steam's *Verify integrity of game files* does the same.

## Troubleshooting
- Language not listed: the file must be in `Wolfpack_Data/Languages` and end in `.lang`.
- Game rejects the pack: run `python tools/validate.py <code>`. Missing keys or invalid JSON are the usual cause.
- Texts are shifted or wrong after a game update: the pack belongs to an older game version, use the matching one.
