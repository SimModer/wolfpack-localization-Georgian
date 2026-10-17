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
            for (auto& a : p.value("authors", json::array())) k.authors.push_back(a.get<std::string>());
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

Wpl read_wpl(const fs::path& root) {
    Wpl w;
    std::vector<char> d;
    if (!read_file(root / L".wpl", d)) return w;
    try {
        json j = json::parse(d.begin(), d.end());
        w.pack = j.at("pack");
        w.pack_version = j.value("pack_version", "");
        w.game_version = j.value("game_version", "");
        w.installed_at = j.value("installed_at", "");
        for (auto& f : j.value("files", json::array())) w.files.push_back(f.get<std::string>());
        w.present = true;
    } catch (...) {
    }
    return w;
}

InstallResult apply_install(const fs::path& root, const std::string& code, const std::string& version, const std::string& game_version,
                            const std::vector<FileBlob>& files, const std::vector<std::string>& legacy_cleanup, const ProgressFn& progress) {
    InstallResult res;
    // 1. remove what the previous pack left behind (the original English files are never deleted)
    Wpl old = read_wpl(root);
    std::vector<std::string> remove;
    if (old.present) {
        if (old.pack != "en") remove = old.files;
    } else {
        remove = legacy_cleanup;
    }
    for (auto& rel : remove) {
        bool kept = false;
        for (auto& f : files) kept |= (f.rel == rel);
        if (kept) continue;  // will be overwritten anyway
        std::error_code ec;
        fs::path target = root / u8_path(rel);
        if (!fs::exists(target, ec)) continue;
        progress(0.5f, "Removing " + rel);
        if (!fs::remove(target, ec) && ec) {
            res.error = "cannot remove " + rel + " (" + ec.message() + ")";
            res.need_admin = ec == std::errc::permission_denied;
            return res;
        }
    }
    // 2. write the new files
    for (size_t i = 0; i < files.size(); ++i) {
        progress(0.5f + 0.45f * (float)i / (float)std::max<size_t>(1, files.size()), "Installing " + files[i].rel);
        std::string err;
        bool denied = false;
        if (!write_file_atomic(root / u8_path(files[i].rel), files[i].data.data(), files[i].data.size(), err, &denied)) {
            res.error = err;
            res.need_admin = denied;
            return res;
        }
        ++res.installed;
    }
    // 3. the marker
    SYSTEMTIME st;
    GetSystemTime(&st);
    char ts[32];
    snprintf(ts, sizeof ts, "%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    json j = {{"app", "WolfpackL10nTool"}, {"pack", code}, {"pack_version", version}, {"game_version", game_version}, {"installed_at", ts}, {"files", json::array()}};
    for (auto& f : files) j["files"].push_back(f.rel);
    std::string text = j.dump(2, ' ', false) + "\n", err;
    bool denied = false;
    if (!write_file_atomic(root / L".wpl", text.data(), text.size(), err, &denied)) {
        res.error = err;
        res.need_admin = denied;
        return res;
    }
    progress(1.0f, "Done");
    return res;
}

InstallResult install_pack(const Pack& p, const fs::path& root, bool with_addon, const ProgressFn& progress, const PackFile* default_xml,
                           const std::vector<std::string>& legacy_cleanup) {
    InstallResult res;
    std::vector<std::pair<const PackFile*, std::string>> todo;  // file, path relative to the game folder
    for (auto& f : p.files) todo.push_back({&f, install_rel(p, f)});
    if (with_addon)
        for (auto& f : p.addon_files) todo.push_back({&f, install_rel(p, f)});
    if (default_xml && !p.has_xml()) todo.push_back({default_xml, "Wolfpack_Data/Language/Official/English.xml"});

    // Download everything first so a broken connection never leaves half a pack in the game folder.
    std::vector<FileBlob> loaded;
    for (size_t i = 0; i < todo.size(); ++i) {
        progress(0.5f * (float)i / (float)todo.size(), "Downloading " + todo[i].first->path);
        FileBlob b;
        b.rel = todo[i].second;
        std::string err;
        if (fetch_file(*todo[i].first, b.data, err)) loaded.push_back(std::move(b));
        else res.missing.push_back(err);
    }
    if (!res.missing.empty() && loaded.empty()) {
        res.error = "nothing could be downloaded";
        return res;
    }
    InstallResult r = apply_install(root, p.code, p.pack_version, p.game_version, loaded, legacy_cleanup, progress);
    r.missing = res.missing;
    return r;
}
