#include "packs.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include "http.h"

using nlohmann::json;

static const char* kDefaultBase = "https://raw.githubusercontent.com/niclqs/wolfpack-localization/main/";

std::string base_url() {
    char buf[512];
    DWORD n = GetEnvironmentVariableA("WOLFPACK_L10N_BASE", buf, sizeof buf);
    if (n > 0 && n < sizeof buf) {
        std::string s(buf, n);
        if (s.back() != '/') s += '/';
        return s;
    }
    return kDefaultBase;
}

static std::string url_encode_path(const std::string& p) {
    std::string o;
    for (unsigned char c : p) {
        if (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else {
            char b[4];
            snprintf(b, sizeof b, "%%%02X", c);
            o += b;
        }
    }
    return o;
}

bool Pack::has_lang() const {
    for (auto& f : files)
        if (f.path.size() > 5 && f.path.compare(f.path.size() - 5, 5, ".lang") == 0) return true;
    return false;
}

bool Pack::has_xml() const {
    for (auto& f : files)
        if (f.path.find("/Language/Official/English.xml") != std::string::npos) return true;
    return false;
}

std::vector<std::string> Pack::warnings(const std::string& installed) const {
    std::vector<std::string> w;
    if (!has_lang()) w.push_back("language file missing in repository");
    if (code != "en" && !has_xml()) w.push_back("control tooltips are not translated (will stay English)");
    if (status == "partial") w.push_back("partly translated: some in-game texts stay English");
    if (status == "needs-review") w.push_back("needs review: some entries are still English");
    if (!installed.empty() && !game_version.empty() && installed != game_version)
        w.push_back("made for game version " + game_version + ", you have " + installed);
    return w;
}

static void read_files(const json& arr, std::vector<PackFile>& out) {
    for (auto& f : arr) out.push_back({f.at("path"), f.at("size"), f.at("sha256")});
}

bool fetch_index(Index& out, std::string& err) {
    HttpResult r = http_get(base_url() + "packs/index.json");
    if (!r.ok()) { err = "index.json: " + r.error; return false; }
    try {
        json j = json::parse(r.body.begin(), r.body.end());
        out = {};
        out.game_version = j.at("game_version");
        read_files(j.at("reference"), out.reference);
        for (auto& p : j.at("packs")) {
            Pack k;
            k.code = p.at("code");
            k.name = p.at("name");
            k.native_name = p.at("native_name");
            k.pack_version = p.at("pack_version");
            k.game_version = p.at("game_version");
            k.status = p.at("status");
            k.notes = p.value("notes", "");
            read_files(p.at("files"), k.files);
            read_files(p.value("addon_files", json::array()), k.addon_files);
            out.packs.push_back(std::move(k));
        }
        return true;
    } catch (const std::exception& e) {
        err = std::string("index.json is invalid: ") + e.what();
        return false;
    }
}

bool fetch_file(const PackFile& f, std::vector<char>& data, std::string& err) {
    HttpResult r = http_get(base_url() + url_encode_path(f.path));
    if (!r.ok()) { err = f.path + ": " + r.error; return false; }
    if (r.body.size() != f.size || sha256_hex(r.body.data(), r.body.size()) != f.sha256) {
        err = f.path + ": checksum mismatch (repository changed, press Refresh)";
        return false;
    }
    data = std::move(r.body);
    return true;
}

std::string install_rel(const Pack& p, const PackFile& f) {
    std::string prefix = "packs/" + p.code + "/";
    return f.path.compare(0, prefix.size(), prefix) == 0 ? f.path.substr(prefix.size()) : f.path;
}

InstallResult install_pack(const Pack& p, const fs::path& root, bool with_addon, const ProgressFn& progress, const PackFile* default_xml) {
    InstallResult res;
    std::vector<std::pair<const PackFile*, std::string>> todo;  // file, path relative to the game folder
    for (auto& f : p.files) todo.push_back({&f, install_rel(p, f)});
    if (with_addon)
        for (auto& f : p.addon_files) todo.push_back({&f, install_rel(p, f)});
    if (default_xml && !p.has_xml()) todo.push_back({default_xml, "Wolfpack_Data/Language/Official/English.xml"});

    // Download everything first so a broken connection never leaves half a pack in the game folder.
    std::vector<std::pair<std::pair<const PackFile*, std::string>, std::vector<char>>> loaded;
    for (size_t i = 0; i < todo.size(); ++i) {
        progress((float)i / (float)(todo.size() * 2), "Downloading " + todo[i].first->path);
        std::vector<char> data;
        std::string err;
        if (fetch_file(*todo[i].first, data, err)) loaded.push_back({todo[i], std::move(data)});
        else res.missing.push_back(err);
    }
    for (size_t i = 0; i < loaded.size(); ++i) {
        const std::string& rel = loaded[i].first.second;
        progress(0.5f + (float)i / (float)(loaded.size() * 2), "Installing " + rel);
        std::string err;
        bool denied = false;
        if (!write_file_atomic(root / u8_path(rel), loaded[i].second.data(), loaded[i].second.size(), err, &denied)) {
            res.error = err;
            res.need_admin = denied;
            return res;
        }
        ++res.installed;
    }
    progress(1.0f, "Done");
    return res;
}
