#include "bepinex.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include "http.h"

using nlohmann::json;

namespace {
// Pinned official build. Compatible with the XUnity.AutoTranslator IL2CPP plugin of the add-ons
// (it references BepInEx.Unity.IL2CPP / Il2CppInterop, the API of 6.0.0-pre.2).
const char* kVersion = "6.0.0-pre.2";
const char* kUrl = "https://github.com/BepInEx/BepInEx/releases/download/v6.0.0-pre.2/BepInEx-Unity.IL2CPP-win-x64-6.0.0-pre.2.zip";
const char* kSha256 = "616ec7eb06cf11b2a0000e8fcef04d1b12bb58e84a2e0bdac9523234fc193ceb";
const size_t kSize = 34146254;

fs::path marker(const fs::path& root) { return root / L"BepInEx" / L".wpl-bepinex"; }

// Runs Windows' own tar.exe (ships with Windows 10 1803+) to extract the zip.
bool extract_zip(const fs::path& zip, const fs::path& dir, std::string& err) {
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(sys) + L"\\tar.exe\" -xf \"" + zip.wstring() + L"\" -C \"" + dir.wstring() + L"\"";
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        err = "cannot start tar.exe (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    WaitForSingleObject(pi.hProcess, 120000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0) err = "extracting the archive failed (tar exit code " + std::to_string(code) + ")";
    return code == 0;
}
}  // namespace

const char* bepinex_version() { return kVersion; }

BepInExState bepinex_state(const fs::path& root) {
    BepInExState s;
    std::error_code ec;
    if (root.empty()) return s;
    s.installed = fs::is_directory(root / L"BepInEx" / L"core", ec) && fs::exists(root / L"winhttp.dll", ec);
    std::vector<char> d;
    if (read_file(marker(root), d)) {
        try {
            s.version = json::parse(d.begin(), d.end()).value("version", "");
            s.by_tool = true;
        } catch (...) {
        }
    }
    return s;
}

BepInExResult bepinex_install(const fs::path& root, const BepProgress& progress) {
    BepInExResult r;
    progress(0.05f, "Downloading BepInEx " + std::string(kVersion) + " (34 MB)");
    HttpResult h = http_get(kUrl);
    if (!h.ok()) { r.error = "download failed: " + h.error; return r; }
    progress(0.55f, "Verifying checksum");
    if (h.body.size() != kSize || sha256_hex(h.body.data(), h.body.size()) != kSha256) {
        r.error = "the downloaded file does not match the expected checksum - not installed";
        return r;
    }
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec) / (L"wolfpack-bepinex-" + std::to_wstring(GetCurrentProcessId()));
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / L"x", ec);
    std::string err;
    if (!write_file_atomic(tmp / L"bepinex.zip", h.body.data(), h.body.size(), err)) { r.error = err; return r; }
    progress(0.65f, "Extracting");
    if (!extract_zip(tmp / L"bepinex.zip", tmp / L"x", r.error)) { fs::remove_all(tmp, ec); return r; }

    progress(0.8f, "Copying into the game folder");
    std::vector<std::string> files;
    for (auto it = fs::recursive_directory_iterator(tmp / L"x", ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        fs::path rel = fs::relative(it->path(), tmp / L"x", ec);
        fs::path dst = root / rel;
        fs::create_directories(dst.parent_path(), ec);
        if (!fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec)) {
            r.error = "cannot write " + path_u8(rel) + " (" + ec.message() + ")";
            r.need_admin = ec == std::errc::permission_denied;
            fs::remove_all(tmp, ec);
            return r;
        }
        files.push_back(path_u8(rel.generic_wstring()));
    }
    fs::remove_all(tmp, ec);

    json j = {{"app", "WolfpackL10nTool"}, {"version", kVersion}, {"source", kUrl}, {"files", files}};
    std::string text = j.dump(2);
    if (!write_file_atomic(marker(root), text.data(), text.size(), err)) { r.error = err; return r; }
    progress(1.f, "BepInEx installed");
    r.ok = true;
    return r;
}

BepInExResult bepinex_uninstall(const fs::path& root) {
    BepInExResult r;
    std::error_code ec;
    // loader files in the game folder; changelog.txt only when we know it came from BepInEx
    std::vector<std::wstring> loose = {L"winhttp.dll", L"doorstop_config.ini", L".doorstop_version"};
    std::vector<char> d;
    if (read_file(marker(root), d)) {
        try {
            for (auto& f : json::parse(d.begin(), d.end()).value("files", json::array())) {
                std::string rel = f.get<std::string>();
                if (rel.find('/') == std::string::npos) loose.push_back(widen(rel));
            }
        } catch (...) {
        }
    }
    for (auto& f : loose) {
        fs::path p = root / f;
        if (fs::exists(p, ec) && !fs::remove(p, ec) && ec) {
            r.error = "cannot remove " + narrow(f) + " (" + ec.message() + ")";
            r.need_admin = ec == std::errc::permission_denied;
            return r;
        }
    }
    for (const wchar_t* dir : {L"BepInEx", L"dotnet"}) {
        fs::remove_all(root / dir, ec);
        if (ec) {
            r.error = "cannot remove " + narrow(dir) + " (" + ec.message() + ")";
            r.need_admin = ec == std::errc::permission_denied;
            return r;
        }
    }
    r.ok = true;
    return r;
}
