#include "ui.h"

#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <thread>

#include <nlohmann/json.hpp>

#include "flags.h"
#include "game.h"
#include "http.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "lang.h"
#include "packs.h"

// =============================================================================================
// Theme (angular, near-black, neon lime - inspired by game launchers)
// =============================================================================================
static float S = 1.f;  // dpi scale
static ImFont* g_bold = nullptr;   // Segoe UI Semibold: buttons, labels
static ImFont* g_black = nullptr;  // Segoe UI Black: headings
static HWND g_hwnd = nullptr;
static float px(float v) { return v * S; }

static ImU32 col(float r, float g, float b, float a = 1.f) { return ImGui::GetColorU32(ImVec4(r, g, b, a)); }
static ImVec4 vec(float r, float g, float b, float a = 1.f) { return ImVec4(r, g, b, a); }

static const ImVec4 kBg(0.039f, 0.039f, 0.043f, 1);
static const ImVec4 kCard(0.067f, 0.067f, 0.075f, 1);
static const ImVec4 kCardHi(0.098f, 0.098f, 0.110f, 1);
static const ImVec4 kBorder(0.145f, 0.145f, 0.160f, 1);
static const ImVec4 kText(0.96f, 0.96f, 0.97f, 1);
static const ImVec4 kMuted(0.53f, 0.55f, 0.61f, 1);
static const ImVec4 kAccent(0.78f, 1.0f, 0.0f, 1);   // lime
static const ImVec4 kGood(0.78f, 1.0f, 0.0f, 1);
static const ImVec4 kWarn(1.0f, 0.70f, 0.0f, 1);     // amber
static const ImVec4 kBad(1.0f, 0.33f, 0.30f, 1);
static const ImVec4 kInfo(0.42f, 0.62f, 1.0f, 1);

static ImVec4 status_color(const std::string& s) {
    if (s == "complete") return kGood;
    if (s == "partial") return kWarn;
    if (s == "original") return kInfo;
    return vec(1.0f, 0.50f, 0.28f);  // needs-review
}

static ImU32 U(const ImVec4& c, float a = -1.f) {
    ImVec4 v = c;
    if (a >= 0) v.w = a;
    return ImGui::GetColorU32(v);
}

static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    float keep = s.FontSizeBase;
    s = ImGuiStyle();
    s.FontSizeBase = keep;
    s.WindowRounding = 0; s.ChildRounding = 0; s.FrameRounding = 0; s.PopupRounding = 0; s.GrabRounding = 0; s.ScrollbarRounding = 0;
    s.FramePadding = ImVec2(12, 9); s.ItemSpacing = ImVec2(12, 12); s.WindowPadding = ImVec2(0, 0);
    s.ScrollbarSize = 8; s.FrameBorderSize = 1; s.ChildBorderSize = 0; s.PopupBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = kBg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = kCard;
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_FrameBg] = vec(0.055f, 0.055f, 0.062f);
    c[ImGuiCol_FrameBgHovered] = vec(0.085f, 0.085f, 0.095f);
    c[ImGuiCol_FrameBgActive] = vec(0.10f, 0.10f, 0.115f);
    c[ImGuiCol_Button] = kCardHi;
    c[ImGuiCol_ButtonHovered] = vec(0.13f, 0.13f, 0.15f);
    c[ImGuiCol_ButtonActive] = vec(0.16f, 0.16f, 0.18f);
    c[ImGuiCol_Header] = vec(0.78f, 1.0f, 0.0f, 0.10f);
    c[ImGuiCol_HeaderHovered] = vec(0.78f, 1.0f, 0.0f, 0.18f);
    c[ImGuiCol_HeaderActive] = vec(0.78f, 1.0f, 0.0f, 0.28f);
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_TableHeaderBg] = vec(0.055f, 0.055f, 0.062f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = vec(1, 1, 1, 0.015f);
    c[ImGuiCol_TableBorderLight] = vec(0.145f, 0.145f, 0.16f, 0.6f);
    c[ImGuiCol_TableBorderStrong] = kBorder;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = vec(0.20f, 0.20f, 0.23f);
    c[ImGuiCol_ScrollbarGrabHovered] = vec(0.30f, 0.30f, 0.34f);
    c[ImGuiCol_ScrollbarGrabActive] = vec(0.78f, 1.0f, 0.0f, 0.8f);
    c[ImGuiCol_TextSelectedBg] = vec(0.78f, 1.0f, 0.0f, 0.25f);
    s.ScaleAllSizes(S);
    s.FontScaleDpi = S;
}

// =============================================================================================
// Widgets
// =============================================================================================
// Rectangle with the bottom-right corner cut off.
static void chamfer(ImDrawList* dl, ImVec2 a, ImVec2 b, float cut, ImU32 fill, ImU32 border, float bw = 1.f) {
    ImVec2 pts[5] = {{a.x, a.y}, {b.x, a.y}, {b.x, b.y - cut}, {b.x - cut, b.y}, {a.x, b.y}};
    if (fill) dl->AddConvexPolyFilled(pts, 5, fill);
    if (border) dl->AddPolyline(pts, 5, border, ImDrawFlags_Closed, bw);
}

// Letter-spaced text (the font has no tracking). Returns the width.
static float stext(ImDrawList* dl, ImVec2 p, ImU32 c, const std::string& t, float spacing, bool draw = true) {
    float x = p.x;
    for (size_t i = 0; i < t.size();) {
        size_t n = 1;
        while (i + n < t.size() && ((unsigned char)t[i + n] & 0xC0) == 0x80) ++n;
        std::string ch = t.substr(i, n);
        if (draw) dl->AddText(ImVec2(x, p.y), c, ch.c_str());
        x += ImGui::CalcTextSize(ch.c_str()).x + spacing;
        i += n;
    }
    return x - p.x - spacing;
}

// Small uppercase caption in the muted colour.
static void label(const std::string& text, ImVec4 c = kMuted, float scale = 0.78f) {
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * scale);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = stext(ImGui::GetWindowDrawList(), p, U(c), upper(text), px(1.6f), false);
    stext(ImGui::GetWindowDrawList(), p, U(c), upper(text), px(1.6f));
    ImGui::Dummy(ImVec2(w, ImGui::GetTextLineHeight()));
    ImGui::PopFont();
}

static void eyebrow(const std::string& text) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.78f);
    float lh = ImGui::GetTextLineHeight();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(p.x, p.y + lh * 0.5f), ImVec2(p.x + px(16), p.y + lh * 0.5f), U(kAccent), px(2));
    float w = stext(dl, ImVec2(p.x + px(24), p.y), U(kAccent), upper(text), px(1.6f));
    ImGui::Dummy(ImVec2(w + px(24), lh));
    ImGui::PopFont();
}

static void heading(const char* text, float scale, ImVec4 c = kText) {
    ImGui::PushFont(g_black, ImGui::GetStyle().FontSizeBase * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

enum BtnStyle { Primary, Amber, Secondary, Ghost };

static bool btn(const char* label_text, BtnStyle st, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    std::string t = upper(label_text);
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.86f);
    float tw = stext(ImGui::GetWindowDrawList(), ImVec2(0, 0), 0, t, px(1.2f), false);
    float th = ImGui::GetTextLineHeight();
    if (size.x <= 0) size.x = tw + px(36);
    if (size.y <= 0) size.y = px(40);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label_text);
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::InvisibleButton("##b", size);
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    ImGui::EndDisabled();
    ImGui::PopID();
    float a = enabled ? 1.f : 0.35f;
    ImU32 bg = 0, fg, border = 0;
    if (st == Primary || st == Amber) {
        ImVec4 base = st == Primary ? kAccent : kWarn;
        ImVec4 v = base;
        if (hov && enabled) { v.x = std::min(1.f, v.x + 0.08f); v.y = std::min(1.f, v.y + 0.04f); v.z = std::min(1.f, v.z + 0.2f); }
        if (act && enabled) { v.x *= 0.85f; v.y *= 0.85f; }
        v.w = a;
        bg = U(v);
        fg = col(0.05f, 0.05f, 0.02f, a);
    } else if (st == Secondary) {
        bg = hov && enabled ? U(kCardHi) : 0;
        border = hov && enabled ? U(kAccent, 0.7f) : U(vec(0.28f, 0.28f, 0.31f), a);
        fg = U(kText, a);
    } else {
        fg = U(hov ? kText : kMuted, a);
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    chamfer(dl, p, ImVec2(p.x + size.x, p.y + size.y), px(8), bg, border);
    stext(dl, ImVec2(p.x + (size.x - tw) * 0.5f, p.y + (size.y - th) * 0.5f), fg, t, px(1.2f));
    ImGui::PopFont();
    return clicked && enabled;
}

static void chip(const char* text, ImVec4 c, bool same_line = false) {
    if (same_line) ImGui::SameLine(0, px(6));
    std::string t = upper(text);
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.72f);
    float tw = stext(ImGui::GetWindowDrawList(), ImVec2(0, 0), 0, t, px(1.1f), false);
    float th = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float padx = px(8), pady = px(4);
    ImVec2 size(tw + padx * 2, th + pady * 2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), U(c, 0.10f));
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), U(c, 0.55f));
    stext(dl, ImVec2(p.x + padx, p.y + pady), U(c), t, px(1.1f));
    ImGui::PopFont();
    ImGui::Dummy(size);
}

static float chip_w(const char* text) {
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.72f);
    float w = stext(ImGui::GetWindowDrawList(), ImVec2(0, 0), 0, upper(text), px(1.1f), false) + px(16);
    ImGui::PopFont();
    return w;
}

static void card_bg(ImVec2 size, float = 0) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    chamfer(ImGui::GetWindowDrawList(), p, ImVec2(p.x + size.x, p.y + size.y), px(14), U(kCard), U(kBorder));
}

// Notice box with a coloured left edge. Returns true when the optional button was clicked.
static bool callout(const char* text, ImVec4 c, const char* button = nullptr, float width = 0) {
    float w = width > 0 ? width : ImGui::GetContentRegionAvail().x;
    float pad = px(14);
    ImVec2 ts = ImGui::CalcTextSize(text, nullptr, false, w - pad * 2 - px(6));
    float bh = button ? px(40) + px(10) : 0;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ts.y + pad * 2 + bh;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    chamfer(dl, p, ImVec2(p.x + w, p.y + h), px(10), U(c, 0.07f), U(c, 0.30f));
    dl->AddRectFilled(p, ImVec2(p.x + px(3), p.y + h), U(c));
    ImGui::SetCursorScreenPos(ImVec2(p.x + pad + px(6), p.y + pad));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - pad * 2 - px(6));
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    bool clicked = false;
    if (button) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + pad + px(6), p.y + pad + ts.y + px(10)));
        clicked = btn(button, c.y > 0.9f && c.x < 0.9f ? Primary : Amber);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, px(6)));
    return clicked;
}

static void progress_bar(float f) {
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = px(4);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(vec(0.12f, 0.12f, 0.13f)));
    if (f > 0) dl->AddRectFilled(p, ImVec2(p.x + w * std::min(f, 1.f), p.y + h), U(kAccent));
    ImGui::Dummy(ImVec2(w, h));
}

// App logo mark: two slanted bars (a stylised "W").
static void logo(ImDrawList* dl, ImVec2 p, float h, ImU32 c) {
    float w = h * 0.9f, t = h * 0.22f, sk = h * 0.28f;
    auto bar = [&](float x, float y0, float y1) {
        ImVec2 pts[4] = {{x + sk, y0}, {x + sk + t, y0}, {x + t, y1}, {x, y1}};
        dl->AddConvexPolyFilled(pts, 4, c);
    };
    bar(p.x, p.y, p.y + h);
    bar(p.x + w * 0.42f, p.y + h * 0.35f, p.y + h);
    bar(p.x + w * 0.84f, p.y, p.y + h);
}

static std::string g_last_check;  // HH:MM:SS of the last successful index load

static bool pick_folder(std::wstring& out) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    bool ok = false;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; ok = true; }
            CoTaskMemFree(p);
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

static void open_url(const char* url) { ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
static void open_folder(const fs::path& p) { ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// =============================================================================================
// State
// =============================================================================================
struct Task {
    std::atomic<bool> running{false};
    std::atomic<float> progress{0.f};
    std::mutex m;
    std::string msg;
    void set(float p, const std::string& s) {
        progress = p;
        std::lock_guard<std::mutex> l(m);
        msg = s;
    }
    std::string get() {
        std::lock_guard<std::mutex> l(m);
        return msg;
    }
};

static std::mutex g_data;  // guards index, ref, install results
static Index g_index;
static bool g_index_ready = false;
static std::string g_index_err;
static LangFile g_ref;
static XmlFile g_refxml;
static bool g_ref_ready = false;
static Task g_index_task;

static fs::path g_root;
static std::string g_game_ver;
static bool g_writable = true;
static std::vector<fs::path> g_candidates;
static std::string g_game_msg;
static std::set<std::string> g_installed;
static std::map<std::string, int> g_state;  // pack code -> 1 up to date, 2 update available (only for installed packs)
static Wpl g_wpl;                           // the .wpl marker of the game folder (what is installed)
static std::vector<Pack> g_queue;           // packs waiting for 'Update all'
static bool g_installed_dirty = true;

static int g_selected = -1;
static bool g_with_addon = false;
static Task g_install_task;
static std::string g_install_result;
static std::vector<std::string> g_install_missing;
static bool g_need_admin = false;
static int g_page = 0;
static bool g_game_running = false;  // polled about once per second

static void set_game(const fs::path& root) {
    g_root = root;
    g_game_ver = root.empty() ? "" : read_game_version(root);
    g_writable = root.empty() ? true : dir_writable(root / L"Wolfpack_Data");
    g_installed_dirty = true;
    g_need_admin = false;
}

static void detect_game() {
    g_candidates = find_game_dirs();
    if (!g_candidates.empty()) {
        set_game(g_candidates[0]);
        g_game_msg.clear();
    } else {
        set_game({});
        g_game_msg = "Wolfpack was not found automatically. Pick the folder that contains Wolfpack.exe.";
    }
}

static void load_index_async() {
    if (g_index_task.running.exchange(true)) return;
    g_index_task.set(0.f, "");
    std::thread([] {
        Index idx;
        std::string err;
        bool ok = fetch_index(idx, err);
        LangFile ref;
        XmlFile refxml;
        bool ref_ok = false;
        if (ok) {
            for (auto& f : idx.reference) {
                std::vector<char> data;
                std::string e2;
                if (!fetch_file(f, data, e2)) continue;
                std::string text(data.begin(), data.end());
                if (f.path.size() > 5 && f.path.compare(f.path.size() - 5, 5, ".lang") == 0) ref_ok = parse_lang(text, ref, e2);
                else parse_xml(text, refxml, e2);
            }
        }
        {
            std::lock_guard<std::mutex> l(g_data);
            g_index_ready = ok;
            g_index_err = ok ? "" : err;
            if (ok) {
                g_index = std::move(idx);
                g_ref = std::move(ref);
                g_refxml = std::move(refxml);
                g_ref_ready = ref_ok;
            }
            g_installed_dirty = true;
            SYSTEMTIME st;
            GetLocalTime(&st);
            char tb[16];
            snprintf(tb, sizeof tb, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
            if (ok) g_last_check = tb;
        }
        g_index_task.running = false;
    }).detach();
}

static bool is_lang_path(const std::string& p) { return p.size() > 5 && p.compare(p.size() - 5, 5, ".lang") == 0; }
static bool is_xml_path(const std::string& p) { return p.find("/Language/Official/English.xml") != std::string::npos; }

static std::string local_hash(const fs::path& f) {
    std::vector<char> d;
    return read_file(f, d) ? sha256_hex(d.data(), d.size()) : std::string();
}

// A pack counts as installed when its language file exists. It has an update when a language file differs
// from the repository (e.g. after a merged pull request). The tooltip file English.xml is one shared slot
// in the game, so it only counts when the installed copy matches no pack at all (stale).
static void refresh_installed() {
    g_installed.clear();
    g_state.clear();
    g_wpl = Wpl();
    if (!g_root.empty()) {
        std::lock_guard<std::mutex> l(g_data);
        g_wpl = read_wpl(g_root);
        if (g_wpl.present) {
            // exactly one pack is installed; it counts as outdated when a file differs from GitHub
            for (auto& p : g_index.packs) {
                if (p.code != g_wpl.pack) continue;
                bool outdated = g_wpl.pack_version != p.pack_version;
                for (auto& f : p.files)
                    if (local_hash(g_root / u8_path(install_rel(p, f))) != f.sha256) outdated = true;
                g_installed.insert(p.code);
                g_state[p.code] = outdated ? 2 : 1;
            }
        } else {
            // no marker yet (installed by hand or by an older tool): detect by language file
            for (auto& p : g_index.packs)
                for (auto& f : p.files)
                    if (is_lang_path(f.path) && p.code != "en" && fs::exists(g_root / u8_path(install_rel(p, f)))) {
                        g_installed.insert(p.code);
                        g_state[p.code] = local_hash(g_root / u8_path(install_rel(p, f))) == f.sha256 ? 1 : 2;
                        break;
                    }
        }
    }
    g_installed_dirty = false;
}

// Language files of all packs but the original English one. Used to clean up when no .wpl exists yet.
static std::vector<std::string> legacy_files() {
    std::vector<std::string> v;
    std::lock_guard<std::mutex> l(g_data);
    for (auto& p : g_index.packs) {
        if (p.code == "en") continue;
        for (auto& f : p.files)
            if (is_lang_path(f.path)) v.push_back(install_rel(p, f));
    }
    return v;
}

static void install_async(Pack pack, bool addon) {
    PackFile ref_xml;
    bool has_ref_xml = false;
    {
        std::lock_guard<std::mutex> l(g_data);
        for (auto& f : g_index.reference)
            if (f.path.size() > 4 && f.path.compare(f.path.size() - 4, 4, ".xml") == 0) { ref_xml = f; has_ref_xml = true; }
    }
    if (g_install_task.running.exchange(true)) return;
    g_install_task.set(0.f, "");
    {
        std::lock_guard<std::mutex> l(g_data);
        g_install_result.clear();
        g_install_missing.clear();
    }
    fs::path root = g_root;
    std::vector<std::string> legacy = legacy_files();
    std::thread([pack, addon, root, ref_xml, has_ref_xml, legacy] {
        InstallResult r = install_pack(pack, root, addon, [](float p, const std::string& s) { g_install_task.set(p, s); }, has_ref_xml ? &ref_xml : nullptr, legacy);
        {
            std::lock_guard<std::mutex> l(g_data);
            if (!r.error.empty()) g_install_result = "ERROR: " + r.error;
            else g_install_result = "Installed " + std::to_string(r.installed) + " files.";
            g_install_missing = r.missing;
            if (r.need_admin) g_need_admin = true;
        }
        g_installed_dirty = true;
        g_install_task.running = false;
    }).detach();
}

// ------------------------------------------------------------------ editor model
struct Row {
    int key;
    std::string en, tr;
    bool untranslated() const { return tr.empty() || tr == en; }
};
struct XRow {
    std::string tag, en, tr;
    bool untranslated() const { return tr.empty() || tr == en; }
};
static struct Editor {
    bool open = false;
    std::string code, name, native, author, filter;
    bool only_untranslated = false, with_xml = true, view_xml = false;
    std::vector<Row> rows;
    std::vector<XRow> xrows;
    std::string status;
    bool status_bad = false;
    fs::path exported;
    int open_pack = 0, open_installed = 0;
    std::vector<fs::path> installed_files;
    bool installed_scanned = false;
} ed;
static Task g_open_task;
static std::mutex g_ed_m;  // guards ed against the async "open from GitHub"

static void ed_blank() {
    ed = Editor();
    ed.open = true;
    for (auto& e : g_ref.entries) ed.rows.push_back({e.key, e.value, ""});
    for (auto& e : g_refxml.entries) ed.xrows.push_back({e.tag, e.text, ""});
}

static void ed_fill(const LangFile* lf, const XmlFile* xf) {
    std::map<int, std::string> lv;
    std::map<std::string, std::string> xv;
    if (lf) for (auto& e : lf->entries) lv[e.key] = e.value;
    if (xf) for (auto& e : xf->entries) xv[e.tag] = e.text;
    for (auto& r : ed.rows) if (lv.count(r.key)) r.tr = lv[r.key];
    for (auto& r : ed.xrows) if (xv.count(r.tag)) r.tr = xv[r.tag];
    ed.with_xml = xf != nullptr;
    if (lf) ed.native = lf->header;
}

static void ed_open_pack_async(Pack pack) {
    if (g_open_task.running.exchange(true)) return;
    std::thread([pack] {
        LangFile lf;
        XmlFile xf;
        bool has_l = false, has_x = false;
        for (auto& f : pack.files) {
            std::vector<char> data;
            std::string err;
            if (!fetch_file(f, data, err)) continue;
            std::string text(data.begin(), data.end());
            if (f.path.size() > 5 && f.path.compare(f.path.size() - 5, 5, ".lang") == 0) has_l = parse_lang(text, lf, err);
            else if (f.path.find("English.xml") != std::string::npos) has_x = parse_xml(text, xf, err);
        }
        std::lock_guard<std::mutex> l(g_ed_m);
        std::lock_guard<std::mutex> l2(g_data);
        ed_blank();
        ed_fill(has_l ? &lf : nullptr, has_x ? &xf : nullptr);
        ed.code = pack.code;
        ed.name = pack.name;
        if (ed.native.empty()) ed.native = pack.native_name;
        g_open_task.running = false;
    }).detach();
}

static std::string safe_filename(std::string s) {
    for (char& c : s) if (strchr("<>:\"/\\|?*", c)) c = '_';
    return s;
}

static bool valid_code(const std::string& c) { return std::regex_match(c, std::regex("[a-z]{2,3}(-[A-Za-z0-9]{2,8})*")); }

static LangFile ed_lang() {
    LangFile lf;
    lf.header = ed.native.empty() ? ed.name : ed.native;
    for (auto& r : ed.rows) lf.entries.push_back({r.key, r.tr.empty() ? r.en : r.tr});
    return lf;
}

static XmlFile ed_xml() {
    XmlFile xf;
    for (auto& r : ed.xrows) xf.entries.push_back({r.tag, r.tr.empty() ? r.en : r.tr});
    return xf;
}

static std::string lang_filename() { return "Community " + safe_filename(ed.name) + ".lang"; }

static bool ed_check_meta() {
    if (!valid_code(ed.code) || ed.name.empty()) {
        ed.status = "Fill in a valid language code (e.g. sv, pt-BR) and an English name first.";
        ed.status_bad = true;
        return false;
    }
    return true;
}

static void ed_export() {
    if (!ed_check_meta()) return;
    fs::path dir = downloads_dir() / L"Wolfpack-L10n-Export" / L"packs" / u8_path(ed.code);
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string err;
    std::string lang = dump_lang(ed_lang());
    bool ok = write_file_atomic(dir / L"Wolfpack_Data" / L"Languages" / u8_path(lang_filename()), lang.data(), lang.size(), err);
    if (ok && ed.with_xml) {
        std::string xml = dump_xml(ed_xml());
        ok = write_file_atomic(dir / L"Wolfpack_Data" / L"Language" / L"Official" / L"English.xml", xml.data(), xml.size(), err);
    }
    if (ok) {
        std::string gv;
        {
            std::lock_guard<std::mutex> l(g_data);
            gv = g_index.game_version;
        }
        nlohmann::json meta = {{"code", ed.code}, {"name", ed.name}, {"native_name", ed.native.empty() ? ed.name : ed.native},
                               {"pack_version", "0.1.0"}, {"game_version", gv}, {"status", "needs-review"},
                               {"tooltips_xml", ed.with_xml}, {"authors", {ed.author.empty() ? "your GitHub name" : ed.author}}, {"notes", ""}};
        std::string m = meta.dump(2, ' ', false) + "\n";
        ok = write_file_atomic(dir / L"pack.json", m.data(), m.size(), err);
    }
    if (ok) {
        ed.exported = dir.parent_path().parent_path();
        ed.status = "Exported to " + path_u8(ed.exported) + ". Upload the packs folder to your fork on GitHub and open a pull request.";
        ed.status_bad = false;
        open_folder(ed.exported);
    } else {
        ed.status = err;
        ed.status_bad = true;
    }
}

static void ed_test_in_game() {
    if (!ed_check_meta()) return;
    if (g_root.empty()) { ed.status = "Select the game folder first."; ed.status_bad = true; return; }
    std::vector<FileBlob> files;
    std::string lang = dump_lang(ed_lang());
    files.push_back({"Wolfpack_Data/Languages/" + lang_filename(), std::vector<char>(lang.begin(), lang.end())});
    {   // tooltips: the edited ones, or the original English ones (the file is shared by every pack)
        std::string xml;
        if (ed.with_xml) xml = dump_xml(ed_xml());
        else {
            std::lock_guard<std::mutex> l(g_data);
            xml = dump_xml(g_refxml);
        }
        files.push_back({"Wolfpack_Data/Language/Official/English.xml", std::vector<char>(xml.begin(), xml.end())});
    }
    InstallResult r = apply_install(g_root, "test-" + ed.code, "0.0.0-test", g_game_ver, files, legacy_files(), [](float, const std::string&) {});
    if (r.need_admin) g_need_admin = true;
    ed.status = r.error.empty() ? "Test copy installed (it replaces the installed language). Start the game and pick it in the settings." : r.error;
    ed.status_bad = !r.error.empty();
    g_installed_dirty = true;
}

// =============================================================================================
// Changelog (latest commits of the repository, loaded from the GitHub API)
// =============================================================================================
struct Commit {
    std::string date, msg, sha, author, url;  // url: the pull request (merge commits) or the commit page
};
static std::vector<Commit> g_changelog;
static std::string g_changelog_err;
static Task g_changelog_task;
static std::mutex g_cl_m;

static std::string api_base() {
    char buf[512];
    DWORD n = GetEnvironmentVariableA("WOLFPACK_L10N_API", buf, sizeof buf);
    if (n > 0 && n < sizeof buf) {
        std::string s(buf, n);
        if (s.back() != '/') s += '/';
        return s;
    }
    return "https://api.github.com/";
}

static std::string commit_title(const std::string& message) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= message.size()) {
        size_t e = message.find('\n', pos);
        if (e == std::string::npos) e = message.size();
        std::string l = message.substr(pos, e - pos);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (!l.empty()) lines.push_back(l);
        pos = e + 1;
    }
    if (lines.empty()) return "";
    // merge commits carry the interesting text in the body
    if (lines[0].rfind("Merge pull request", 0) == 0 && lines.size() > 1) return lines[1];
    return lines[0];
}

static std::string pretty_date(const std::string& iso) {
    static const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    if (iso.size() < 10) return iso;
    int m = atoi(iso.substr(5, 2).c_str());
    if (m < 1 || m > 12) return iso.substr(0, 10);
    return std::to_string(atoi(iso.substr(8, 2).c_str())) + " " + months[m - 1] + " " + iso.substr(0, 4);
}

static void load_changelog_async() {
    if (g_changelog_task.running.exchange(true)) return;
    std::thread([] {
        HttpResult r = http_get(api_base() + "repos/niclqs/wolfpack-localization/commits?per_page=40&sha=main");
        std::vector<Commit> list;
        std::string err;
        if (!r.ok()) {
            err = r.status == 403 ? "GitHub rate limit reached. Try again later." : "Could not load the changelog (" + r.error + ").";
        } else {
            try {
                auto j = nlohmann::json::parse(r.body.begin(), r.body.end());
                for (auto& c : j) {
                    Commit k;
                    k.date = c["commit"]["author"]["date"].get<std::string>();
                    k.msg = commit_title(c["commit"]["message"].get<std::string>());
                    k.sha = c["sha"].get<std::string>().substr(0, 7);
                    k.author = c["commit"]["author"]["name"].get<std::string>();
                    k.url = c.value("html_url", std::string());
                    {   // link merged pull requests ("Merge pull request #29 ..." or "title (#29)") to the PR itself
                        std::string full = c["commit"]["message"].get<std::string>();
                        std::smatch m;
                        std::string head = full.substr(0, full.find('\n'));
                        if (std::regex_search(head, m, std::regex("^Merge pull request #([0-9]+)")) || std::regex_search(head, m, std::regex("\\(#([0-9]+)\\)$")))
                            k.url = "https://github.com/niclqs/wolfpack-localization/pull/" + m[1].str();
                    }
                    if (!k.msg.empty()) list.push_back(k);
                }
            } catch (const std::exception& e) {
                err = std::string("Unexpected answer from GitHub: ") + e.what();
            }
        }
        {
            std::lock_guard<std::mutex> l(g_cl_m);
            g_changelog = std::move(list);
            g_changelog_err = err;
        }
        g_changelog_task.running = false;
    }).detach();
}

// =============================================================================================
// Shell: init, notices
// =============================================================================================
void ui_init(ImFont* semibold, ImFont* black, HWND hwnd, ID3D11Device* device) {
    flags_init(device);
    g_bold = semibold;
    g_black = black;
    g_hwnd = hwnd;
    detect_game();
    load_index_async();
    load_changelog_async();
}

static void notices() {
    if (g_game_running) callout("Wolfpack is running. Close the game to install or test language files.", kInfo);
    if (!g_game_msg.empty() && g_root.empty()) callout(g_game_msg.c_str(), kWarn);
    if (!g_root.empty() && (!g_writable || g_need_admin)) {
        if (callout("This game folder is write-protected. Administrator rights are needed to install or test language files.", kWarn,
                    is_elevated() ? nullptr : "Restart as administrator"))
            if (relaunch_as_admin()) ExitProcess(0);
    }
}

// A contributor name; "@handle" links to the GitHub profile.
static void author_chip(const std::string& a, bool same_line) {
    bool handle = !a.empty() && a[0] == '@';
    std::string text = handle ? a.substr(1) : a;
    chip(text.c_str(), handle ? kInfo : kMuted, same_line);
    if (handle && ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseClicked(0)) open_url(("https://github.com/" + text).c_str());
    }
}

static std::string by_line(const Pack& p) {
    std::string s;
    for (auto& a : p.authors) s += (s.empty() ? "" : ", ") + (a[0] == '@' ? a.substr(1) : a);
    return s;
}

static void page_install() {
    notices();

    std::vector<Pack> packs;
    std::string err;
    {
        std::lock_guard<std::mutex> l(g_data);
        packs = g_index.packs;
        err = g_index_err;
    }
    if (!err.empty()) callout(("Could not load the language list: " + err).c_str(), kBad, "Try again") ? load_index_async() : void();
    if (g_installed_dirty) refresh_installed();
    if (!g_install_task.running && !g_queue.empty()) {
        Pack next = g_queue.front();
        g_queue.erase(g_queue.begin());
        install_async(next, false);
    }
    {
        std::string names;
        std::vector<Pack> outdated;
        for (auto& q : packs)
            if (g_state.count(q.code) && g_state[q.code] == 2) {
                outdated.push_back(q);
                names += (names.empty() ? "" : ", ") + q.native_name;
            }
        if (!outdated.empty() && g_queue.empty() && callout(("Updates available for: " + names + ". The files in your game folder differ from the repository.").c_str(), kAccent, (g_install_task.running || g_game_running) ? nullptr : "Update all"))
            g_queue = outdated;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float detail_w = px(340), gap = px(16);
    float list_w = avail.x - detail_w - gap;

    // ---- list
    ImGui::BeginChild("list", ImVec2(list_w, avail.y), ImGuiChildFlags_None);
    if (g_index_task.running) ImGui::TextColored(kMuted, "Loading language packs...");
    for (int i = 0; i < (int)packs.size(); ++i) {
        const Pack& p = packs[i];
        ImGui::PushID(i);
        float w = ImGui::GetContentRegionAvail().x - px(14), h = px(68);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##card", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered();
        if (clicked) g_selected = i;
        bool sel = g_selected == i;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        chamfer(dl, pos, ImVec2(pos.x + w, pos.y + h), px(12), U(sel ? kCardHi : hov ? vec(0.085f, 0.085f, 0.095f) : kCard), sel ? U(kAccent, 0.9f) : U(kBorder), sel ? px(1.5f) : 1.f);
        if (sel) dl->AddRectFilled(pos, ImVec2(pos.x + px(3), pos.y + h), U(kAccent));
        float fw = flag_draw(dl, p.code, ImVec2(pos.x + px(18), pos.y + (h - px(26)) * 0.5f), px(26), px(46));
        float tx = pos.x + px(18) + fw + px(14);
        ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 1.15f);
        dl->AddText(ImVec2(tx, pos.y + px(11)), U(kText), p.native_name.c_str());
        ImGui::PopFont();
        std::string sub = p.name;
        if (!p.authors.empty() && p.code != "en") sub += "   by " + by_line(p);
        dl->AddText(ImVec2(tx, pos.y + px(37)), U(kMuted), sub.c_str());
        // chips right aligned
        const char* st = p.status.c_str();
        float cw = chip_w(st);
        ImVec2 save = ImGui::GetCursorScreenPos();
        float right = pos.x + w - px(16);
        float cy = pos.y + (h - ImGui::GetTextLineHeight() - px(6)) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(right - cw, cy));
        chip(st, status_color(p.status));
        if (g_installed.count(p.code)) {
            bool upd = g_state[p.code] == 2;
            const char* label = upd ? "update available" : "installed";
            float cw2 = chip_w(label);
            ImGui::SetCursorScreenPos(ImVec2(right - cw - cw2 - px(6), cy));
            chip(label, upd ? kAccent : kGood);
        }
        ImGui::SetCursorScreenPos(save);
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::PopID();
    }
    ImGui::EndChild();

    // ---- detail
    ImGui::SameLine(0, gap);
    ImVec2 dp = ImGui::GetCursorScreenPos();
    (void)dp;
    card_bg(ImVec2(detail_w, avail.y));
    ImGui::BeginChild("detail", ImVec2(detail_w, avail.y), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(px(20), px(18)));
    ImGui::BeginGroup();
    float inner = detail_w - px(40);
    if (g_selected < 0 || g_selected >= (int)packs.size()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
        ImGui::Dummy(ImVec2(0, px(120)));
        ImGui::TextColored(kMuted, "Select a language on the left to see details and install it.");
        ImGui::PopTextWrapPos();
    } else {
        const Pack& p = packs[g_selected];
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
        {
            ImVec2 fp = ImGui::GetCursorScreenPos();
            float fw = flag_draw(ImGui::GetWindowDrawList(), p.code, ImVec2(fp.x, fp.y + px(4)), px(30), px(54));
            ImGui::SetCursorScreenPos(ImVec2(fp.x + fw + px(10), fp.y));
        }
        heading(p.native_name.c_str(), 1.5f);
        ImGui::SetCursorPosX(px(20));
        ImGui::TextColored(kMuted, "%s", p.name.c_str());
        ImGui::Dummy(ImVec2(0, px(2)));
        chip(p.status.c_str(), status_color(p.status));
        chip(("v" + p.pack_version).c_str(), kInfo, true);
        if (g_installed.count(p.code)) {
            if (g_state[p.code] == 2) chip("update available", kAccent, true);
            else chip("installed, up to date", kGood, true);
        }
        if (!p.notes.empty() && p.status != "needs-review") {
            ImGui::Dummy(ImVec2(0, px(4)));
            ImGui::TextColored(kMuted, "%s", p.notes.c_str());
        }
        ImGui::PopTextWrapPos();
        if (!p.authors.empty()) {
            ImGui::Dummy(ImVec2(0, px(6)));
            label(p.code == "en" ? "Source" : "Contributors");
            float used = 0;
            for (size_t i = 0; i < p.authors.size(); ++i) {
                float cw = chip_w(p.authors[i][0] == '@' ? p.authors[i].c_str() + 1 : p.authors[i].c_str()) + px(6);
                bool same = i > 0 && used + cw <= inner;
                if (!same) used = 0;
                author_chip(p.authors[i], same);
                used += cw;
            }
        }
        ImGui::Dummy(ImVec2(0, px(4)));
        for (auto& w : p.warnings(g_game_ver)) {
            if (w.rfind("partly translated", 0) == 0 || w.rfind("needs review", 0) == 0) continue;  // already told by the status chip
            callout(w.c_str(), kWarn, nullptr, inner);
        }
        bool can_addon = !p.addon_files.empty();
        bool bepinex = !g_root.empty() && fs::exists(g_root / L"BepInEx");
        if (can_addon) {
            ImGui::BeginDisabled(!bepinex);
            ImGui::Checkbox("Install the add-on for hard-coded texts", &g_with_addon);
            ImGui::EndDisabled();
            if (!bepinex) ImGui::TextColored(kMuted, "Needs BepInEx + XUnity.AutoTranslator.");
        }

        // bottom anchored actions
        float actions_h = px(38 + 44 + 70);
        float y = ImGui::GetWindowHeight() - actions_h - px(18);
        if (ImGui::GetCursorPosY() < y) ImGui::SetCursorPosY(y);
        bool busy = g_install_task.running;
        if (busy) {
            progress_bar(g_install_task.progress);
            ImGui::TextColored(kMuted, "%s", g_install_task.get().c_str());
        } else {
            std::lock_guard<std::mutex> l(g_data);
            if (!g_install_result.empty()) {
                bool bad = g_install_result.rfind("ERROR", 0) == 0;
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
                ImGui::TextColored(bad ? kBad : kGood, "%s", g_install_result.c_str());
                if (!bad) ImGui::TextColored(kMuted, "Now choose it in the game under Settings > Language.");
                for (auto& m : g_install_missing) ImGui::TextColored(kWarn, "Missing: %s", m.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
        {
            std::string cur;
            for (auto& q : packs) if (g_installed.count(q.code) && q.code != p.code) cur = q.native_name;
            std::string hint = cur.empty() ? "Only one language can be installed. Installing another one removes the current one."
                                           : "Only one language can be installed: this replaces " + cur + ".";
            ImGui::TextColored(kMuted, "%s", hint.c_str());
        }
        ImGui::PopTextWrapPos();
        float bw = inner;
        bool upd = g_installed.count(p.code) && g_state[p.code] == 2;
        bool same = g_installed.count(p.code) && g_state[p.code] == 1;
        if (btn(busy ? "Working..." : upd ? "Update" : same ? "Reinstall" : "Install", upd ? Amber : Primary, ImVec2(bw, px(44)), !g_root.empty() && !busy && !g_game_running))
            install_async(p, can_addon && bepinex && g_with_addon);
        if (btn("Restore original English", Secondary, ImVec2(bw, px(38)), !g_root.empty() && !busy && !g_game_running))
            for (auto& q : packs)
                if (q.code == "en") install_async(q, false);
    }
    ImGui::EndGroup();
    ImGui::EndChild();
}

// One 40 px high row across the full width: [New] [pack list] [Open from GitHub] [installed list] [Open installed]
static void ed_toolbar(const std::vector<Pack>& packs) {
    const float h = px(40), gap = px(12);
    float W = ImGui::GetContentRegionAvail().x;
    float b1 = px(170), b2 = px(190), b3 = px(170);
    float combo_w = std::floor((W - b1 - b2 - b3 - gap * 4) / 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12), (h - ImGui::GetFontSize()) * 0.5f));

    if (btn("New language", Primary, ImVec2(b1, h))) {
        std::lock_guard<std::mutex> l(g_data);
        ed_blank();
    }
    ImGui::SameLine(0, gap);
    ImGui::SetNextItemWidth(combo_w);
    int pi = packs.empty() ? 0 : std::min(ed.open_pack, (int)packs.size() - 1);
    if (ImGui::BeginCombo("##pk", packs.empty() ? "-" : (packs[pi].native_name + " (" + packs[pi].code + ")").c_str())) {
        for (int i = 0; i < (int)packs.size(); ++i)
            if (ImGui::Selectable((packs[i].native_name + " (" + packs[i].code + ")").c_str(), i == ed.open_pack)) ed.open_pack = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, gap);
    if (btn("Open from GitHub", Secondary, ImVec2(b2, h), !packs.empty() && !g_open_task.running)) ed_open_pack_async(packs[pi]);

    if (!ed.installed_scanned && !g_root.empty()) {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(g_root / L"Wolfpack_Data" / L"Languages", ec))
            if (e.path().extension() == L".lang" && e.path().filename() != L"Official English.lang") ed.installed_files.push_back(e.path());
        ed.installed_scanned = true;
    }
    ImGui::SameLine(0, gap);
    int ii = ed.installed_files.empty() ? 0 : std::min(ed.open_installed, (int)ed.installed_files.size() - 1);
    ImGui::SetNextItemWidth(combo_w);
    ImGui::BeginDisabled(g_root.empty());
    if (ImGui::BeginCombo("##inst", ed.installed_files.empty() ? "No installed packs" : path_u8(ed.installed_files[ii].filename()).c_str())) {
        for (int i = 0; i < (int)ed.installed_files.size(); ++i)
            if (ImGui::Selectable(path_u8(ed.installed_files[i].filename()).c_str(), i == ed.open_installed)) ed.open_installed = i;
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, gap);
    if (btn("Open installed", Secondary, ImVec2(b3, h), !ed.installed_files.empty())) {
        std::vector<char> data;
        LangFile lf;
        std::string err;
        fs::path file = ed.installed_files[ii];
        if (read_file(file, data) && parse_lang(std::string(data.begin(), data.end()), lf, err)) {
            auto files = ed.installed_files;
            std::lock_guard<std::mutex> l(g_data);
            ed_blank();
            ed_fill(&lf, nullptr);
            ed.installed_files = files;
            ed.installed_scanned = true;
            ed.open_installed = ii;
            ed.name = path_u8(file.stem());
            if (ed.name.rfind("Community ", 0) == 0) ed.name = ed.name.substr(10);
            ed.with_xml = true;
        } else {
            ed.status = err.empty() ? "Cannot read the file." : err;
            ed.status_bad = true;
        }
    }
    ImGui::PopStyleVar();
    if (g_open_task.running) ImGui::TextColored(kMuted, "Loading...");
}

static void page_editor() {
    notices();

    bool ref_ready;
    std::vector<Pack> packs;
    {
        std::lock_guard<std::mutex> l(g_data);
        ref_ready = g_ref_ready;
        packs = g_index.packs;
    }
    if (!ref_ready) {
        callout("The English reference could not be loaded yet. Check your internet connection and press Refresh on the Install page.", kBad, "Try again") ? load_index_async() : void();
        return;
    }
    std::lock_guard<std::mutex> lock(g_ed_m);
    ed_toolbar(packs);
    ImGui::Dummy(ImVec2(0, px(2)));

    if (!ed.open) {
        ImGui::Dummy(ImVec2(0, px(60)));
        float w = ImGui::GetContentRegionAvail().x;
        const char* t = "Start a new language or open an existing one.";
        ImGui::SetCursorPosX((w - ImGui::CalcTextSize(t).x) * 0.5f + px(32));
        ImGui::TextColored(kMuted, "%s", t);
        return;
    }

    // ---- metadata card
    float cw = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("meta", ImVec2(cw, px(64)), ImGuiChildFlags_None);
    card_bg(ImVec2(cw, px(64)), 12);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12), (px(40) - ImGui::GetFontSize()) * 0.5f));
    ImGui::SetCursorPos(ImVec2(px(12), px(12)));
    float mg = px(12), mw = cw - px(24) - mg * 3;
    ImGui::SetNextItemWidth(mw * 0.16f); ImGui::InputTextWithHint("##code", "Code (sv, pt-BR)", &ed.code);
    ImGui::SameLine(0, mg); ImGui::SetNextItemWidth(mw * 0.28f); ImGui::InputTextWithHint("##name", "English name", &ed.name);
    ImGui::SameLine(0, mg); ImGui::SetNextItemWidth(mw * 0.32f); ImGui::InputTextWithHint("##nat", "Name shown in the game", &ed.native);
    ImGui::SameLine(0, mg); ImGui::SetNextItemWidth(mw * 0.24f); ImGui::InputTextWithHint("##auth", "Your GitHub name", &ed.author);
    ImGui::PopStyleVar();
    ImGui::EndChild();

    // ---- filter row
    int done = 0;
    for (auto& r : ed.rows) done += !r.untranslated();
    {
        const float gap = px(12), row_h = px(40);
        float W = ImGui::GetContentRegionAvail().x;
        float right = px(176) + px(190) + px(170) + gap * 4;  // checkbox + counter + progress
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12), (row_h - ImGui::GetFontSize()) * 0.5f));
        ImGui::SetNextItemWidth(std::max(px(200), W - right));
        ImGui::InputTextWithHint("##flt", "Search key, English or translation", &ed.filter);
        ImGui::SameLine(0, gap);
        ImGui::Checkbox("Only untranslated", &ed.only_untranslated);
        ImGui::SameLine(0, gap * 2);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kMuted, "%d of %d translated", done, (int)ed.rows.size());
        ImGui::SameLine(0, gap);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float f = ed.rows.empty() ? 0.f : (float)done / (float)ed.rows.size();
        float bw = std::max(px(60), ImGui::GetContentRegionAvail().x), by = p.y + (row_h - px(4)) * 0.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(p.x, by), ImVec2(p.x + bw, by + px(4)), U(vec(0.12f, 0.12f, 0.13f)));
        dl->AddRectFilled(ImVec2(p.x, by), ImVec2(p.x + bw * f, by + px(4)), U(kAccent));
        ImGui::Dummy(ImVec2(bw, row_h));
        ImGui::PopStyleVar();
    }

    std::string f = lower(ed.filter);
    auto match = [&](const std::string& en, const std::string& tr, bool untr, const std::string& extra) {
        if (ed.only_untranslated && !untr) return false;
        if (f.empty()) return true;
        return lower(en).find(f) != std::string::npos || lower(tr).find(f) != std::string::npos || lower(extra).find(f) != std::string::npos;
    };
    std::vector<int> idx;
    for (int i = 0; i < (int)ed.rows.size(); ++i)
        if (match(ed.rows[i].en, ed.rows[i].tr, ed.rows[i].untranslated(), std::to_string(ed.rows[i].key))) idx.push_back(i);

    float footer = px(56) + (ed.status.empty() ? 0 : px(70));
    ImGui::BeginChild("tbl", ImVec2(0, -footer), ImGuiChildFlags_None);
    card_bg(ImGui::GetContentRegionAvail(), 12);
    const float row_h = px(40);
    if (!ed.view_xml) {
        if (ImGui::BeginTable("rows", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, px(64));
            ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
            ImGui::TableSetupColumn("Translation", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clip;
            clip.Begin((int)idx.size(), row_h);
            while (clip.Step()) {
                for (int n = clip.DisplayStart; n < clip.DisplayEnd; ++n) {
                    Row& r = ed.rows[idx[n]];
                    ImGui::TableNextRow(0, row_h);
                    if (r.untranslated()) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, U(kAccent, 0.05f));
                    ImGui::PushID(r.key + 1000);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(r.untranslated() ? kAccent : kMuted, "%d", r.key);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(r.en.c_str());
                    if (ImGui::IsItemHovered() && ImGui::CalcTextSize(r.en.c_str()).x > ImGui::GetColumnWidth()) ImGui::SetTooltip("%s", r.en.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::InputTextWithHint("##t", r.en.c_str(), &r.tr);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    } else if (ImGui::BeginTable("xrows", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Tooltip", ImGuiTableColumnFlags_WidthFixed, px(230));
        ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn("Translation", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)ed.xrows.size(); ++i) {
            XRow& r = ed.xrows[i];
            if (!match(r.en, r.tr, r.untranslated(), r.tag)) continue;
            ImGui::TableNextRow(0, row_h);
            if (r.untranslated()) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, U(kAccent, 0.05f));
            ImGui::PushID(i + 5000);
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(r.untranslated() ? kAccent : kMuted, "%s", r.tag.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(r.en.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##x", r.en.c_str(), &r.tr);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    if (!ed.status.empty()) callout(ed.status.c_str(), ed.status_bad ? kBad : kGood);

    // ---- one action row: buttons on the left, tooltip options on the right
    const float bh = px(42);
    if (btn("Export for GitHub", Primary, ImVec2(0, bh))) ed_export();
    ImGui::SameLine();
    if (btn("Test in game", Secondary, ImVec2(0, bh), !g_root.empty() && !g_game_running)) ed_test_in_game();
    if (!ed.exported.empty()) {
        ImGui::SameLine();
        if (btn("Open export folder", Ghost, ImVec2(0, bh))) open_folder(ed.exported);
    }
    if (!ed.xrows.empty()) {
        float rw = px(150) + px(250) + px(12);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - rw);
        if (btn(ed.view_xml ? "Show strings" : "Edit tooltips", Secondary, ImVec2(px(150), bh))) ed.view_xml = !ed.view_xml;
        ImGui::SameLine(0, px(12));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(12), (bh - ImGui::GetFontSize()) * 0.5f));
        ImGui::Checkbox("Include tooltips (English.xml)", &ed.with_xml);
        ImGui::PopStyleVar();
    }
}

// =============================================================================================
// Dashboard
// =============================================================================================
static void kv_row(const char* key, const std::string& value, ImVec4 c, float w) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(p, ImVec2(p.x + w, p.y), U(kBorder));
    ImGui::Dummy(ImVec2(0, px(8)));
    label(key);
    ImGui::SameLine();
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.86f);
    std::string v = upper(value);
    float vw = stext(dl, ImVec2(0, 0), 0, v, px(1.2f), false);
    ImVec2 q = ImGui::GetItemRectMin();
    stext(dl, ImVec2(p.x + w - vw, q.y - px(1)), U(c), v, px(1.2f));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, px(8)));
}

static void banner(const char* title, const char* subtitle, ImVec4 c, const char* button, BtnStyle bs, bool enabled, bool* clicked) {
    float w = ImGui::GetContentRegionAvail().x, h = px(76);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    chamfer(dl, p, ImVec2(p.x + w, p.y + h), px(14), U(kCard), U(kBorder));
    dl->AddRectFilled(p, ImVec2(p.x + px(3), p.y + h), U(c));
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.95f);
    stext(dl, ImVec2(p.x + px(24), p.y + px(15)), U(kText), upper(title), px(1.4f));
    ImGui::PopFont();
    dl->AddText(ImVec2(p.x + px(24), p.y + px(40)), U(kMuted), subtitle);
    if (button) {
        ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.86f);
        float bw = stext(dl, ImVec2(0, 0), 0, upper(button), px(1.2f), false) + px(36) + px(36);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(p.x + w - bw - px(14), p.y + (h - px(44)) * 0.5f));
        // small "!" square like a status icon inside the button
        ImVec2 bp = ImGui::GetCursorScreenPos();
        if (btn(button, bs, ImVec2(bw, px(44)), enabled)) *clicked = true;
        if (bs == Amber) {
            float s = px(18);
            dl->AddRectFilled(ImVec2(bp.x + px(14), bp.y + (px(44) - s) * 0.5f), ImVec2(bp.x + px(14) + s, bp.y + (px(44) + s) * 0.5f), col(0.05f, 0.05f, 0.02f, enabled ? 1.f : 0.4f));
            dl->AddText(ImVec2(bp.x + px(14) + s * 0.5f - ImGui::CalcTextSize("!").x * 0.5f, bp.y + (px(44) - ImGui::GetTextLineHeight()) * 0.5f), U(kWarn), "!");
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, px(12)));
}

static void page_dashboard() {
    std::vector<Pack> packs;
    std::string idx_err, idx_game;
    bool ready;
    std::string last_check;
    {
        std::lock_guard<std::mutex> l(g_data);
        packs = g_index.packs;
        idx_err = g_index_err;
        idx_game = g_index.game_version;
        ready = g_index_ready;
        last_check = g_last_check;
    }
    if (g_installed_dirty) refresh_installed();
    std::vector<Pack> outdated;
    for (auto& p : packs) {
        if (g_state.count(p.code) && g_state[p.code] == 2) outdated.push_back(p);
    }

    // which banners are needed (max two, most important first)
    struct B { int kind; };
    std::vector<int> banners;
    if (!g_root.empty() && (!g_writable || g_need_admin) && !is_elevated()) banners.push_back(0);
    if (g_root.empty()) banners.push_back(1);
    if (g_game_running) banners.push_back(2);
    if (!outdated.empty()) banners.push_back(3);
    else if (g_installed.empty()) banners.push_back(4);
    else banners.push_back(5);
    if (banners.size() > 2) banners.resize(2);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float banners_h = (float)banners.size() * px(88);
    float cards_h = avail.y - banners_h - px(4);
    float gap = px(16);
    float left_w = std::floor((avail.x - gap) * 0.40f);
    float right_w = avail.x - gap - left_w;
    float game_h = std::floor((cards_h - gap) * 0.52f);
    float lang_h = cards_h - gap - game_h;
    float pad = px(22);

    ImGui::BeginChild("left", ImVec2(left_w, cards_h));
    {
        ImGui::BeginChild("game", ImVec2(left_w, game_h));
        card_bg(ImVec2(left_w, game_h));
        ImGui::SetCursorPos(ImVec2(pad, px(18)));
        ImGui::BeginGroup();
        label("Game");
        if (g_root.empty()) heading("NOT FOUND", 1.6f, kBad);
        else heading(g_game_ver.empty() ? "UNKNOWN" : g_game_ver.c_str(), 2.1f, kAccent);
        label(g_root.empty() ? "Select the Wolfpack folder" : "Version detected in the game files", kMuted, 0.72f);
        if (!g_root.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + left_w - pad * 2);
            ImGui::TextColored(kMuted, "%s", path_u8(g_root).c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::SetCursorPosY(game_h - px(54));
        if (btn("Detect", Secondary, ImVec2(px(110), px(36)))) detect_game();
        ImGui::SameLine(0, px(8));
        if (btn("Browse", Secondary, ImVec2(px(110), px(36)))) {
            std::wstring wp;
            if (pick_folder(wp)) {
                fs::path root;
                if (normalize_game_dir(wp, root)) { set_game(root); g_game_msg.clear(); }
                else g_game_msg = "That folder does not contain Wolfpack.exe.";
            }
        }
        ImGui::EndGroup();
        ImGui::EndChild();

        ImGui::SetCursorPosY(game_h + gap);
        ImGui::BeginChild("langs", ImVec2(left_w, lang_h));
        card_bg(ImVec2(left_w, lang_h));
        ImGui::SetCursorPos(ImVec2(pad, px(18)));
        ImGui::BeginGroup();
        label("Installed language");
        const Pack* active = nullptr;
        for (auto& q : packs) if (g_installed.count(q.code)) active = &q;
        if (active) {
            ImVec2 fp = ImGui::GetCursorScreenPos();
            float fw = flag_draw(ImGui::GetWindowDrawList(), active->code, ImVec2(fp.x, fp.y + px(6)), px(34), px(62));
            ImGui::SetCursorScreenPos(ImVec2(fp.x + fw + px(10), fp.y));
            heading(active->native_name.c_str(), 2.1f);
            ImGui::SetCursorPosX(pad);
            bool upd = g_state[active->code] == 2;
            std::string sub = std::string(upd ? "Update available" : "Up to date") + "  /  v" + active->pack_version;
            label(sub, upd ? kWarn : kAccent, 0.72f);
            if (!active->authors.empty()) {
                ImGui::Dummy(ImVec2(0, px(2)));
                float used = 0;
                for (size_t i = 0; i < active->authors.size(); ++i) {
                    float cw = chip_w(active->authors[i][0] == '@' ? active->authors[i].c_str() + 1 : active->authors[i].c_str()) + px(6);
                    bool same = i > 0 && used + cw <= left_w - pad * 2;
                    if (!same) used = 0;
                    author_chip(active->authors[i], same);
                    used += cw;
                }
            }
        } else {
            heading("NONE", 2.1f, kMuted);
            label("Open Languages to install one", kMuted, 0.72f);
        }
        ImGui::EndGroup();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine(0, gap);
    ImGui::BeginChild("right", ImVec2(right_w, cards_h));
    card_bg(ImVec2(right_w, cards_h));
    ImGui::SetCursorPos(ImVec2(pad, px(18)));
    ImGui::BeginGroup();
    float inner = right_w - pad * 2;
    label("Language packs on GitHub");
    {
        std::string big = std::to_string(packs.size()) + " PACKS";
        heading(big.c_str(), 2.1f);
    }
    label(ready ? "Index loaded - all files verified by checksum" : idx_err.empty() ? "Loading..." : "Offline", ready ? kAccent : kBad, 0.72f);
    ImGui::Dummy(ImVec2(0, px(4)));
    {   // one segment per pack, coloured by status
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        int n = std::max<int>(1, (int)packs.size());
        float sg = px(4), sw = (inner - sg * (n - 1)) / n, sh = px(7);
        for (int i = 0; i < (int)packs.size(); ++i) {
            float x = p.x + i * (sw + sg);
            dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + sw, p.y + sh), U(status_color(packs[i].status)));
        }
        if (packs.empty()) dl->AddRectFilled(p, ImVec2(p.x + inner, p.y + sh), U(vec(0.15f, 0.15f, 0.17f)));
        ImGui::Dummy(ImVec2(inner, sh + px(10)));
    }
    ImGui::PushItemWidth(inner);
    kv_row("Link", ready ? "Operational" : "Offline", ready ? kAccent : kBad, inner);
    {
        std::string cmp = g_game_ver.empty() || idx_game.empty() ? "Unknown" : g_game_ver == idx_game ? "Packs match your game" : "Packs target " + idx_game;
        ImVec4 cc = g_game_ver.empty() || idx_game.empty() ? kMuted : g_game_ver == idx_game ? kAccent : kWarn;
        kv_row("Compatibility", cmp, cc, inner);
    }
    kv_row("Translated", std::to_string(std::count_if(packs.begin(), packs.end(), [](const Pack& p) { return p.status == "complete" || p.status == "original"; })) + " of " + std::to_string(packs.size()) + " complete", kText, inner);
    kv_row("Last check", last_check.empty() ? "-" : last_check, kText, inner);
    ImGui::PopItemWidth();
    ImGui::SetCursorPosY(cards_h - px(54));
    if (btn("Check again", Secondary, ImVec2(px(140), px(36)), !g_index_task.running)) { load_index_async(); load_changelog_async(); }
    ImGui::EndGroup();
    ImGui::EndChild();

    ImGui::SetCursorPosY(cards_h + px(12));
    bool clicked = false;
    for (int kind : banners) {
        clicked = false;
        bool busy = g_install_task.running || g_game_running;
        switch (kind) {
        case 0:
            banner("Administrator rights required", "The game folder is write-protected. Restart the tool as administrator to install.", kWarn, "Restart as admin", Amber, true, &clicked);
            if (clicked && relaunch_as_admin()) ExitProcess(0);
            break;
        case 1:
            banner("Game not found", "Pick the folder that contains Wolfpack.exe.", kBad, nullptr, Primary, true, &clicked);
            break;
        case 2:
            banner("Game is running", "Close Wolfpack to install or update language files.", kInfo, nullptr, Primary, true, &clicked);
            break;
        case 3:
            banner("Update required", (std::to_string(outdated.size()) + " installed language pack(s) differ from GitHub.").c_str(), kWarn, "Update available", Amber, !busy && !g_root.empty(), &clicked);
            if (clicked) g_queue = outdated;
            break;
        case 4:
            banner("No language installed", "Open Languages and install a pack to get started.", kMuted, "Browse languages", Primary, true, &clicked);
            if (clicked) g_page = 1;
            break;
        default:
            banner("All up to date", "Your installed language files match the repository.", kAccent, nullptr, Primary, true, &clicked);
        }
    }
}

// =============================================================================================
// Changelog
// =============================================================================================
static void page_changelog() {
    std::vector<Commit> list;
    std::string err;
    {
        std::lock_guard<std::mutex> l(g_cl_m);
        list = g_changelog;
        err = g_changelog_err;
    }
    ImVec2 avail = ImGui::GetContentRegionAvail();
    label("Latest changes on GitHub (main branch)");
    ImGui::SameLine(avail.x - px(130));
    if (btn("Refresh", Secondary, ImVec2(px(130), px(34)), !g_changelog_task.running)) load_changelog_async();
    if (!err.empty()) callout(err.c_str(), kWarn);
    if (g_changelog_task.running) ImGui::TextColored(kMuted, "Loading...");

    float w = ImGui::GetContentRegionAvail().x, h = ImGui::GetContentRegionAvail().y;
    card_bg(ImVec2(w, h));
    ImGui::BeginChild("log", ImVec2(w, h));
    ImGui::SetCursorPos(ImVec2(px(24), px(18)));
    ImGui::BeginGroup();
    std::string last_date;
    float text_w = w - px(24) * 2 - px(26) - px(160);
    for (size_t i = 0; i < list.size(); ++i) {
        const Commit& c = list[i];
        std::string d = pretty_date(c.date);
        if (d != last_date) {
            if (!last_date.empty()) ImGui::Dummy(ImVec2(0, px(8)));
            label(d, kAccent, 0.8f);
            ImGui::Dummy(ImVec2(0, px(2)));
            last_date = d;
        }
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(p.x + px(26), p.y));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_w);
        ImGui::TextUnformatted(c.msg.c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(ImVec2(p.x + px(26), ImGui::GetCursorScreenPos().y));
        ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.72f);
        std::string meta = c.sha + "   " + c.author;
        ImGui::TextColored(kMuted, "%s", upper(meta).c_str());
        ImGui::PopFont();
        float y1 = ImGui::GetCursorScreenPos().y;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (!c.url.empty()) {   // READ MORE button, right aligned on the entry
            bool is_pr = c.url.find("/pull/") != std::string::npos;
            ImGui::SetCursorScreenPos(ImVec2(p.x + (w - px(24) * 2) - px(150), p.y + px(2)));
            if (btn(is_pr ? "View pull request" : "Read more", Ghost, ImVec2(px(150), px(32)))) open_url(c.url.c_str());
            ImGui::SetCursorScreenPos(ImVec2(p.x, y1));
        }
        dl->AddLine(ImVec2(p.x + px(6), p.y + px(14)), ImVec2(p.x + px(6), y1 + px(8)), U(kBorder), px(1.5f));
        dl->AddRectFilled(ImVec2(p.x + px(2), p.y + px(6)), ImVec2(p.x + px(10), p.y + px(14)), U(kAccent));
        ImGui::Dummy(ImVec2(0, px(8)));
    }
    if (list.empty() && err.empty() && !g_changelog_task.running) ImGui::TextColored(kMuted, "No entries.");
    ImGui::EndGroup();
    ImGui::EndChild();
}

// =============================================================================================
// Frame: title bar, header with tabs, footer, splash
// =============================================================================================
static float g_caption_h = 0, g_caption_btn_w = 0;
float ui_caption_height() { return g_caption_h; }
float ui_caption_buttons_width() { return g_caption_btn_w; }

static void title_bar(float W) {
    float h = px(42), bw = px(46);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(0, h), ImVec2(W, h), U(kBorder));
    logo(dl, ImVec2(px(18), px(13)), px(16), U(kText));
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.86f);
    stext(dl, ImVec2(px(52), (h - ImGui::GetTextLineHeight()) * 0.5f), U(kText), "WOLFPACK L10N", px(1.6f));
    ImGui::PopFont();
    for (int i = 0; i < 2; ++i) {
        float x = W - bw * (2 - i);
        ImGui::SetCursorPos(ImVec2(x, 0));
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##cap", ImVec2(bw, h - 1));
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hov) dl->AddRectFilled(ImVec2(x, 0), ImVec2(x + bw, h - 1), i == 1 ? col(0.85f, 0.18f, 0.18f) : U(kCardHi));
        ImVec2 c(x + bw * 0.5f, h * 0.5f);
        ImU32 ic = U(hov ? kText : kMuted);
        float r = px(5);
        if (i == 0) dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), ic, px(1.4f));
        else {
            dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), ic, px(1.4f));
            dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), ic, px(1.4f));
        }
        if (clicked) {
            if (i == 0) ShowWindow(g_hwnd, SW_MINIMIZE);
            else PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        }
    }
    g_caption_h = h;
    g_caption_btn_w = bw * 2;
}

static void tab_strip(float right_x, float y) {
    static const char* names[] = {"Dashboard", "Languages", "Editor", "Changelog"};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.78f);
    float widths[4], total = 0;
    for (int i = 0; i < 4; ++i) {
        widths[i] = stext(dl, ImVec2(0, 0), 0, upper(names[i]), px(1.6f), false) + px(40);
        total += widths[i];
    }
    float x = right_x - total, h = px(40);
    for (int i = 0; i < 4; ++i) {
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::PushID(i + 100);
        if (ImGui::InvisibleButton("##tab", ImVec2(widths[i], h))) g_page = i;
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        ImVec2 p = ImGui::GetItemRectMin();
        bool act = g_page == i;
        if (act) {
            dl->AddRectFilled(p, ImVec2(p.x + widths[i], p.y + h), U(kCard));
            dl->AddRectFilled(p, ImVec2(p.x + widths[i], p.y + px(3)), U(kAccent));
        } else if (hov) {
            dl->AddRectFilled(p, ImVec2(p.x + widths[i], p.y + h), U(kCard, 0.5f));
        }
        stext(dl, ImVec2(p.x + px(20), p.y + (h - ImGui::GetTextLineHeight()) * 0.5f + px(1)), U(act ? kText : hov ? kText : kMuted), upper(names[i]), px(1.6f));
        x += widths[i];
    }
    ImGui::PopFont();
}

static void footer(float W, float H) {
    float h = px(32);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(0, H - h), ImVec2(W, H - h), U(kBorder));
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.68f);
    float ty = H - h + (h - ImGui::GetTextLineHeight()) * 0.5f;
    float x = px(28);
    stext(dl, ImVec2(x, ty), U(kMuted), "V1.0.0", px(1.4f));

    float rw = stext(dl, ImVec2(0, 0), 0, "REPOSITORY", px(1.4f), false);
    float rx = W - px(28) - rw;
    ImGui::SetCursorPos(ImVec2(rx - px(4), H - h));
    if (ImGui::InvisibleButton("##repo", ImVec2(rw + px(8), h))) open_url("https://github.com/niclqs/wolfpack-localization");
    bool hov = ImGui::IsItemHovered();
    stext(dl, ImVec2(rx, ty), U(hov ? kAccent : kMuted), "REPOSITORY", px(1.4f));
    std::string lic = "POLYFORM NONCOMMERCIAL";
    float lw = stext(dl, ImVec2(0, 0), 0, lic, px(1.4f), false);
    stext(dl, ImVec2(rx - px(24) - lw, ty), U(kMuted, 0.7f), lic, px(1.4f));
    ImGui::PopFont();
}

static void splash(float W, float H, float shown, bool done_loading) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // corner brackets
    float m = px(26), l = px(22), t = px(2);
    ImU32 c = U(kAccent);
    dl->AddLine(ImVec2(m, m), ImVec2(m + l, m), c, t);          dl->AddLine(ImVec2(m, m), ImVec2(m, m + l), c, t);
    dl->AddLine(ImVec2(W - m, m), ImVec2(W - m - l, m), c, t);  dl->AddLine(ImVec2(W - m, m), ImVec2(W - m, m + l), c, t);
    dl->AddLine(ImVec2(m, H - m), ImVec2(m + l, H - m), c, t);  dl->AddLine(ImVec2(m, H - m), ImVec2(m, H - m - l), c, t);
    dl->AddLine(ImVec2(W - m, H - m), ImVec2(W - m - l, H - m), c, t); dl->AddLine(ImVec2(W - m, H - m), ImVec2(W - m, H - m - l), c, t);

    // centre: logo + name
    ImGui::PushFont(g_black, ImGui::GetStyle().FontSizeBase * 2.4f);
    ImVec2 ts = ImGui::CalcTextSize("WOLFPACK L10N");
    float logo_h = px(64), gapx = px(22);
    float total = logo_h * 0.9f + gapx + ts.x;
    float x = (W - total) * 0.5f, y = H * 0.5f - ts.y * 0.5f - px(10);
    logo(dl, ImVec2(x, y + (ts.y - logo_h) * 0.5f), logo_h, U(kText));
    dl->AddText(ImVec2(x + logo_h * 0.9f + gapx, y), U(kText), "WOLFPACK L10N");
    ImGui::PopFont();
    float tx = x + logo_h * 0.9f + gapx;
    dl->AddLine(ImVec2(tx, y + ts.y + px(6)), ImVec2(tx + px(34), y + ts.y + px(6)), U(kAccent), px(2));
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 0.72f);
    stext(dl, ImVec2(tx, y + ts.y + px(14)), U(kMuted), "LANGUAGE INSTALLER & EDITOR", px(2.4f));

    // bottom progress
    float bx = px(44), bw = W - px(88), by = H - px(64);
    stext(dl, ImVec2(bx, by - px(24)), U(kMuted), done_loading ? "READY" : "LOADING LANGUAGE PACKS", px(1.4f));
    char pct[16];
    snprintf(pct, sizeof pct, "%d%%", (int)(shown * 100));
    float pw = stext(dl, ImVec2(0, 0), 0, pct, px(1.4f), false);
    stext(dl, ImVec2(bx + bw - pw, by - px(24)), U(kAccent), pct, px(1.4f));
    ImGui::PopFont();
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + px(4)), U(vec(0.12f, 0.12f, 0.13f)));
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * shown, by + px(4)), U(kAccent));
    for (int i = 0; i <= 10; ++i) dl->AddLine(ImVec2(bx + bw * i / 10.f, by + px(12)), ImVec2(bx + bw * i / 10.f, by + px(18)), U(kBorder));
}

bool ui_frame(float scale) {
    static float applied = 0.f;
    S = scale;
    if (applied != scale) { apply_style(); applied = scale; }

    static double last_poll = -10;
    if (ImGui::GetTime() - last_poll > 1.0) {
        g_game_running = game_running();
        last_poll = ImGui::GetTime();
    }
    if (!g_install_task.running && !g_queue.empty() && !g_game_running) {
        Pack next = g_queue.front();
        g_queue.erase(g_queue.begin());
        install_async(next, false);
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
    float W = vp->Size.x, H = vp->Size.y;

    // ---- splash while the first data loads
    static float shown = 0.f;
    static bool in_splash = true;
    if (in_splash) {
        float target = g_index_task.running ? 0.6f : 1.0f;
        shown += (target - shown) * std::min(1.f, ImGui::GetIO().DeltaTime * 3.f);
        bool done = !g_index_task.running;
        if (done && shown > 0.985f && ImGui::GetTime() > 1.6) in_splash = false;
        splash(W, H, std::min(shown, 1.f), done);
        g_caption_h = H;
        g_caption_btn_w = 0;
        ImGui::End();
        return true;
    }

    title_bar(W);

    // ---- header: eyebrow + title on the left, tabs on the right
    static const char* eyebrows[] = {"Overview", "Install", "Create", "What's new"};
    static const char* titles[] = {"DASHBOARD", "LANGUAGES", "EDITOR", "CHANGELOG"};
    float pad = px(32), top = g_caption_h + px(26);
    ImGui::SetCursorPos(ImVec2(pad, top));
    eyebrow(eyebrows[g_page]);
    ImGui::SetCursorPos(ImVec2(pad, top + px(24)));
    heading(titles[g_page], 2.1f);
    tab_strip(W - pad, top + px(30));

    float line_y = top + px(24) + px(62);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(pad, line_y), ImVec2(W - pad, line_y), U(kBorder));
    float content_y = line_y + px(20), footer_h = px(32);
    ImGui::SetCursorPos(ImVec2(pad, content_y));
    ImGui::BeginChild("page", ImVec2(W - pad * 2, H - content_y - footer_h - px(14)), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    if (g_page == 0) page_dashboard();
    else if (g_page == 1) page_install();
    else if (g_page == 2) page_editor();
    else page_changelog();
    ImGui::EndChild();

    footer(W, H);
    ImGui::End();
    return true;
}
