#pragma once
#include <functional>
#include <string>
#include <vector>

#include "util.h"

struct PackFile {
    std::string path;  // repo-relative, e.g. packs/de/Wolfpack_Data/Languages/Community German.lang
    size_t size = 0;
    std::string sha256;
};

struct Pack {
    std::string code, name, native_name, pack_version, game_version, status, notes;
    std::vector<std::string> authors;  // "@handle" = GitHub account, otherwise a plain name
    std::vector<PackFile> files, addon_files;
    bool has_lang() const;
    bool has_xml() const;
    // Human readable hints, e.g. "tooltips not translated" or a game version mismatch.
    std::vector<std::string> warnings(const std::string& installed_game_version) const;
};

struct Index {
    std::string game_version;
    std::vector<PackFile> reference;  // English.lang, English.xml
    std::vector<Pack> packs;
};

// Hard-coded repository location (override with the env var WOLFPACK_L10N_BASE for tests).
std::string base_url();
bool fetch_index(Index& out, std::string& err);
// Downloads one file and verifies size and SHA-256.
bool fetch_file(const PackFile& f, std::vector<char>& data, std::string& err);
// Path of a pack file relative to the game folder (strips "packs/<code>/").
std::string install_rel(const Pack& p, const PackFile& f);

// The marker file ".wpl" in the game folder: which pack is installed right now (only one at a time).
struct Wpl {
    bool present = false;
    std::string pack, pack_version, game_version, installed_at;
    std::vector<std::string> files;  // relative to the game folder; removed again when another pack is installed
};
Wpl read_wpl(const fs::path& game_root);

struct FileBlob {
    std::string rel;  // relative to the game folder
    std::vector<char> data;
};

struct InstallResult {
    int installed = 0;
    std::vector<std::string> missing;  // files that could not be downloaded or failed verification
    std::string error;                 // fatal error (e.g. write failure)
    bool need_admin = false;
};
using ProgressFn = std::function<void(float, const std::string&)>;
// default_xml: the original English tooltip file. It is written when the pack has no tooltips of its own,
// so the tooltips of a previously installed pack do not stay behind (the file is one shared slot in the game).
// Single-install core: removes the files of the previously installed pack, writes the new files and the .wpl marker.
// legacy_cleanup: language files of every known pack; they are removed when no .wpl exists yet (installed by hand/older tool).
InstallResult apply_install(const fs::path& game_root, const std::string& pack_code, const std::string& pack_version,
                            const std::string& game_version, const std::vector<FileBlob>& files,
                            const std::vector<std::string>& legacy_cleanup, const ProgressFn& progress);
InstallResult install_pack(const Pack& p, const fs::path& game_root, bool with_addon, const ProgressFn& progress,
                           const PackFile* default_xml = nullptr, const std::vector<std::string>& legacy_cleanup = {});
