#include "util.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <fstream>

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string path_u8(const fs::path& p) { return narrow(p.wstring()); }
fs::path u8_path(const std::string& s) { return fs::path(widen(s)); }

std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

bool read_file(const fs::path& p, std::vector<char>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file_atomic(const fs::path& p, const void* data, size_t size, std::string& err, bool* access_denied) {
    if (access_denied) *access_denied = false;
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p;
    tmp += L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (access_denied) *access_denied = (e == ERROR_ACCESS_DENIED);
        err = "cannot write " + path_u8(p) + " (error " + std::to_string(e) + ")";
        return false;
    }
    const char* ptr = (const char*)data;
    size_t left = size;
    bool ok = true;
    while (left > 0) {
        DWORD chunk = (DWORD)std::min<size_t>(left, 1 << 20), written = 0;
        if (!WriteFile(h, ptr, chunk, &written, nullptr) || written == 0) { ok = false; break; }
        ptr += written;
        left -= written;
    }
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        err = "write failed: " + path_u8(p);
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DWORD e = GetLastError();
        DeleteFileW(tmp.c_str());
        if (access_denied) *access_denied = (e == ERROR_ACCESS_DENIED);
        err = "cannot replace " + path_u8(p) + " (error " + std::to_string(e) + ")";
        return false;
    }
    return true;
}

std::string sha256_hex(const void* data, size_t size) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    UCHAR digest[32] = {};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
            BCryptHashData(hash, (PUCHAR)data, (ULONG)size, 0);
            BCryptFinishHash(hash, digest, sizeof digest, 0);
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (UCHAR b : digest) { out += hex[b >> 4]; out += hex[b & 15]; }
    return out;
}
