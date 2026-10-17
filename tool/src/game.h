#pragma once
#include <string>
#include <vector>

#include "util.h"

// Candidate game folders (the folder that contains Wolfpack.exe), best guess first.
std::vector<fs::path> find_game_dirs();
// Accepts the game folder, Wolfpack.exe or Wolfpack_Data and returns the game folder.
bool normalize_game_dir(const fs::path& in, fs::path& root);
// Reads the version string (e.g. "0.26R6") from Wolfpack_Data/level2. Empty if not found.
std::string read_game_version(const fs::path& root);
bool dir_writable(const fs::path& dir);
bool is_elevated();
// Starts this exe again with administrator rights (UAC prompt). Returns false if the user declined.
bool relaunch_as_admin();
fs::path downloads_dir();
