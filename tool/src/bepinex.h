#pragma once
#include <functional>
#include <string>

#include "util.h"

// BepInEx 6 (IL2CPP) - the mod loader the AutoTranslator add-ons need.
// The tool downloads one pinned build from the official GitHub release and checks its SHA-256.
struct BepInExState {
    bool installed = false;   // BepInEx/core and winhttp.dll exist
    bool by_tool = false;     // installed by this tool (marker BepInEx/.wpl-bepinex exists)
    std::string version;      // from the marker, empty when unknown
};

BepInExState bepinex_state(const fs::path& game_root);
const char* bepinex_version();  // the pinned version, e.g. "6.0.0-pre.2"

struct BepInExResult {
    bool ok = false;
    std::string error;
    bool need_admin = false;
};
using BepProgress = std::function<void(float, const std::string&)>;

// Download, verify and extract into the game folder; records every file in the marker.
BepInExResult bepinex_install(const fs::path& game_root, const BepProgress& progress);
// Removes the loader files and the whole BepInEx folder (including plugins, configs and generated files).
BepInExResult bepinex_uninstall(const fs::path& game_root);
