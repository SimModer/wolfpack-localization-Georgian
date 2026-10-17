#include "game.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <fstream>
#include <regex>

static bool is_game_root(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p / L"Wolfpack.exe", ec) && fs::is_directory(p / L"Wolfpack_Data", ec);
}

static std::wstring reg_string(HKEY root, const wchar_t* key, const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2];
    DWORD size = sizeof buf;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) return buf;
    return L"";
}

std::vector<fs::path> find_game_dirs() {
    std::vector<fs::path> libs, found;
    auto add_lib = [&](fs::path p) {
        if (p.empty()) return;
        std::error_code ec;
        p = fs::weakly_canonical(p, ec);
        if (std::find(libs.begin(), libs.end(), p) == libs.end()) libs.push_back(p);
    };

    std::wstring steam = reg_string(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (steam.empty()) steam = reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (!steam.empty()) {
        add_lib(steam);
        std::ifstream vdf(fs::path(steam) / L"steamapps" / L"libraryfolders.vdf");
        std::string text((std::istreambuf_iterator<char>(vdf)), std::istreambuf_iterator<char>());
        std::regex re("\"path\"\\s+\"([^\"]+)\"");
        for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
            std::string p = (*it)[1];
            std::string unescaped;
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i] == '\\' && i + 1 < p.size() && p[i + 1] == '\\') ++i;
                unescaped += p[i];
            }
            add_lib(u8_path(unescaped));
        }
    }
    // Fallback: typical library names on every drive (e.g. Y:\SteamLibrary).
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(drives & (1u << i))) continue;
        std::wstring d = std::wstring(1, (wchar_t)(L'A' + i)) + L":\\";
        UINT type = GetDriveTypeW(d.c_str());
        if (type != DRIVE_FIXED && type != DRIVE_REMOTE) continue;
        for (const wchar_t* sub : {L"SteamLibrary", L"Steam", L"Program Files (x86)\\Steam", L"Games\\Steam", L"Games\\SteamLibrary"})
            add_lib(fs::path(d) / sub);
    }
    for (auto& lib : libs) {
        fs::path g = lib / L"steamapps" / L"common" / L"Wolfpack";
        if (is_game_root(g) && std::find(found.begin(), found.end(), g) == found.end()) found.push_back(g);
    }
    return found;
}

bool normalize_game_dir(const fs::path& in, fs::path& root) {
    std::error_code ec;
    fs::path p = in;
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    // the user may have picked Wolfpack_Data or a sub folder of it
    for (int up = 0; up < 3 && !p.empty(); ++up) {
        if (is_game_root(p)) { root = p; return true; }
        p = p.parent_path();
    }
    return false;
}

std::string read_game_version(const fs::path& root) {
    std::vector<char> data;
    if (!read_file(root / L"Wolfpack_Data" / L"level2", data)) return "";
    // Unity stores the string length-prefixed (4 bytes), e.g. 6 0 0 0 "0.26R6"
    static const std::regex re("[0-9]{1,2}\\.[0-9]{1,3}R[0-9]{1,3}");
    size_t n = data.size();
    for (size_t i = 4; i + 4 < n; ++i) {
        if (!isdigit((unsigned char)data[i])) continue;
        uint32_t len;
        memcpy(&len, &data[i - 4], 4);
        if (len < 4 || len > 16 || i + len > n) continue;
        std::string s(&data[i], len);
        if (std::regex_match(s, re)) return s;
    }
    return "";
}

bool dir_writable(const fs::path& dir) {
    fs::path probe = dir / L".l10n_write_test.tmp";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

bool is_elevated() {
    HANDLE token = nullptr;
    TOKEN_ELEVATION e{};
    DWORD sz = sizeof e;
    bool r = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        r = GetTokenInformation(token, TokenElevation, &e, sizeof e, &sz) && e.TokenIsElevated;
        CloseHandle(token);
    }
    return r;
}

bool relaunch_as_admin() {
    wchar_t exe[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", exe, nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

bool game_running() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e{sizeof e};
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &e); ok && !found; ok = Process32NextW(snap, &e))
        found = _wcsicmp(e.szExeFile, L"Wolfpack.exe") == 0;
    CloseHandle(snap);
    return found;
}

fs::path downloads_dir() {
    PWSTR p = nullptr;
    fs::path r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p))) r = p;
    CoTaskMemFree(p);
    return r;
}
