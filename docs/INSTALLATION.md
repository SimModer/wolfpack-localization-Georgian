# Installation

1. Download the zip for your language from the [Releases](../../releases) page (`Wolfpack-L10n_<code>_game-<version>_v<pack>.zip`). Pick the pack whose game version matches yours (shown in the main menu).
2. Steam: right-click *Wolfpack*, then **Manage, Browse local files**.
3. Extract the zip into that folder. It contains a `Wolfpack_Data` folder. Confirm merging and **overwrite** existing files (this replaces `Language/Official/English.xml`; Steam's *Verify integrity of game files* restores the original).
4. Start the game and choose the language under **Settings, Language**.

## Optional: AutoTranslator add-on (German)
Some texts are hard-coded in the game and are not part of the `.lang` file. The add-on translates them at runtime using [BepInEx 6 (IL2CPP)](https://github.com/BepInEx/BepInEx) and [XUnity.AutoTranslator](https://github.com/bbepis/XUnity.AutoTranslator).
1. Install BepInEx 6 IL2CPP and XUnity.AutoTranslator (IL2CPP build) first.
2. Extract `..._autotranslator-addon.zip` into the game's main folder (next to `Wolfpack.exe`).

## Troubleshooting
- Language not listed: the file must be in `Wolfpack_Data/Languages` and end in `.lang`.
- Game rejects the pack: run `python tools/validate.py <code>`. Missing keys or invalid JSON are the usual cause.
- Texts are shifted or wrong after a game update: the pack belongs to an older game version, use the matching one.
