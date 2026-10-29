# Third-party components

| Component | Where | License |
|---|---|---|
| [XUnity.AutoTranslator](https://github.com/bbepis/XUnity.AutoTranslator) and ResourceRedirector (DLLs, readme) | `packs/de/addons/autotranslator/BepInEx/plugins` | MIT, see below |
| [BepInEx 6 (IL2CPP)](https://github.com/BepInEx/BepInEx) | **not** included in the repository; the tool downloads the official release `6.0.0-pre.2` on request (SHA-256 pinned) | LGPL-2.1 |
| German AutoTranslator texts, texture `fish_1` and config | `packs/de/addons/autotranslator` | taken over from [Apade1/Wolfpack-German-Localization](https://github.com/Apade1/Wolfpack-German-Localization) with the permission of the repository owner; the texture is not modified |
| [Dear ImGui](https://github.com/ocornut/imgui), [nlohmann/json](https://github.com/nlohmann/json) | built into the tool | MIT |
| [stb_image](https://github.com/nothings/stb) | `tool/third_party` | public domain / MIT |
| Flag images from [flagcdn.com](https://flagcdn.com) | `tool/assets/flags` | free to use |

The helper executables of XUnity (`Translators/FullNET`) are not included: they are only needed for online translators, the add-on translates offline.

## XUnity.AutoTranslator license (MIT)

MIT License

Copyright (c) 2018 Bepis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
