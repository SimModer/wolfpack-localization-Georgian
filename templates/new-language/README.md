# New language template

1. Copy this folder to `packs/<code>/` (use a [BCP 47 code](https://www.iana.org/assignments/language-subtag-registry), e.g. `sv`, `pt-BR`, `zh-Hant`).
2. Rename `Community YourLanguage.lang` to `Community <English name>.lang`.
3. In the `.lang` file change **`header`** to the name shown in the game's language menu and translate every `"value"`. Never change `"key"`.
4. Translate the tags in `Wolfpack_Data/Language/Official/English.xml` (control tooltips). Do not rename the tags. Delete the file if you do not translate it (and set `tooltips_xml` to `false`).
5. Fill in `pack.json`.
6. Run `python tools/validate.py <code>` - it must report `ok`.
7. Open a pull request (the PR template has a checklist).

Keep placeholders like `{0}` and tags like `<b>` exactly as they are. Save files as UTF-8.
