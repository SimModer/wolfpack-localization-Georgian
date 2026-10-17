#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);
std::string path_u8(const fs::path& p);
fs::path u8_path(const std::string& s);
std::string lower(std::string s);

bool read_file(const fs::path& p, std::vector<char>& out);
// Writes to "<file>.tmp" and renames over the target. err gets a readable reason,
// access_denied is set when the failure was missing permissions.
bool write_file_atomic(const fs::path& p, const void* data, size_t size, std::string& err, bool* access_denied = nullptr);
std::string sha256_hex(const void* data, size_t size);
