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

#include "game.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "lang.h"
#include "packs.h"

// =============================================================================================
// Theme
// =============================================================================================
static float S = 1.f;  // dpi scale
static ImFont* g_bold = nullptr;
static float px(float v) { return v * S; }

static ImU32 col(float r, float g, float b, float a = 1.f) { return ImGui::GetColorU32(ImVec4(r, g, b, a)); }
static ImVec4 vec(float r, float g, float b, float a = 1.f) { return ImVec4(r, g, b, a); }

static const ImVec4 kBg(0.043f, 0.055f, 0.071f, 1);
static const ImVec4 kSide(0.062f, 0.078f, 0.098f, 1);
static const ImVec4 kCard(0.085f, 0.106f, 0.133f, 1);
static const ImVec4 kCardHi(0.115f, 0.140f, 0.175f, 1);
static const ImVec4 kBorder(0.17f, 0.20f, 0.25f, 1);
static const ImVec4 kText(0.92f, 0.94f, 0.96f, 1);
static const ImVec4 kMuted(0.56f, 0.62f, 0.69f, 1);
static const ImVec4 kAccent(0.96f, 0.69f, 0.25f, 1);
static const ImVec4 kAccentDim(0.96f, 0.69f, 0.25f, 0.14f);
static const ImVec4 kGood(0.36f, 0.78f, 0.52f, 1);
static const ImVec4 kWarn(0.96f, 0.69f, 0.25f, 1);
static const ImVec4 kBad(0.93f, 0.38f, 0.35f, 1);
static const ImVec4 kInfo(0.40f, 0.72f, 0.88f, 1);

static ImVec4 status_color(const std::string& s) {
    if (s == "complete") return kGood;
    if (s == "partial") return kWarn;
    if (s == "original") return kInfo;
    return vec(0.92f, 0.52f, 0.34f);  // needs-review
}

static ImU32 U(const ImVec4& c, float a = -1.f) {
    ImVec4 v = c;
    if (a >= 0) v.w = a;
    return ImGui::GetColorU32(v);
}

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    float keep = s.FontSizeBase;
    s = ImGuiStyle();
    s.FontSizeBase = keep;
    s.WindowRounding = 0; s.ChildRounding = 12; s.FrameRounding = 8; s.PopupRounding = 10; s.GrabRounding = 8; s.ScrollbarRounding = 8;
    s.FramePadding = ImVec2(12, 8); s.ItemSpacing = ImVec2(10, 10); s.WindowPadding = ImVec2(0, 0);
    s.ScrollbarSize = 10; s.FrameBorderSize = 0; s.ChildBorderSize = 0; s.PopupBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = kBg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = kCard;
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_FrameBg] = kCardHi;
    c[ImGuiCol_FrameBgHovered] = vec(0.14f, 0.17f, 0.21f);
    c[ImGuiCol_FrameBgActive] = vec(0.16f, 0.19f, 0.24f);
    c[ImGuiCol_Button] = kCardHi;
    c[ImGuiCol_ButtonHovered] = vec(0.16f, 0.19f, 0.24f);
    c[ImGuiCol_ButtonActive] = vec(0.19f, 0.23f, 0.29f);
    c[ImGuiCol_Header] = kAccentDim;
    c[ImGuiCol_HeaderHovered] = vec(0.96f, 0.69f, 0.25f, 0.22f);
    c[ImGuiCol_HeaderActive] = vec(0.96f, 0.69f, 0.25f, 0.32f);
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_TableHeaderBg] = vec(0.075f, 0.094f, 0.118f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = vec(1, 1, 1, 0.018f);
    c[ImGuiCol_TableBorderLight] = vec(0.17f, 0.20f, 0.25f, 0.5f);
    c[ImGuiCol_TableBorderStrong] = kBorder;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = vec(0.22f, 0.26f, 0.32f);
    c[ImGuiCol_ScrollbarGrabHovered] = vec(0.30f, 0.35f, 0.42f);
    c[ImGuiCol_ScrollbarGrabActive] = vec(0.38f, 0.44f, 0.52f);
    c[ImGuiCol_TextSelectedBg] = vec(0.96f, 0.69f, 0.25f, 0.30f);
    s.ScaleAllSizes(S);
    s.FontScaleDpi = S;
}

// =============================================================================================
// Widgets
// =============================================================================================
enum BtnStyle { Primary, Secondary, Ghost };

static bool btn(const char* label, BtnStyle st, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase);
    ImVec2 ts = ImGui::CalcTextSize(label);
    if (size.x <= 0) size.x = ts.x + px(32);
    if (size.y <= 0) size.y = px(38);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::InvisibleButton("##b", size);
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    ImGui::EndDisabled();
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg, fg, border = 0;
    float alpha = enabled ? 1.f : 0.4f;
    if (st == Primary) {
        ImVec4 a = kAccent;
        if (hov && enabled) { a.x += 0.03f; a.y += 0.05f; a.z += 0.08f; }
        if (act && enabled) { a.x -= 0.06f; a.y -= 0.06f; a.z -= 0.04f; }
        a.w = alpha;
        bg = U(a);
        fg = col(0.10f, 0.07f, 0.02f, alpha);
    } else if (st == Secondary) {
        bg = U(act ? vec(0.19f, 0.23f, 0.29f) : hov ? vec(0.16f, 0.19f, 0.24f) : kCardHi, alpha);
        border = U(kBorder, alpha);
        fg = U(kText, alpha);
    } else {
        bg = hov && enabled ? U(kCardHi) : 0;
        fg = U(hov ? kText : kMuted, alpha);
    }
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, px(8));
    if (border) dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), border, px(8));
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), fg, label);
    ImGui::PopFont();
    return clicked && enabled;
}

static void chip(const char* text, ImVec4 c, bool same_line = false) {
    if (same_line) ImGui::SameLine(0, px(6));
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float padx = px(9), pady = px(3);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 size(ts.x + padx * 2, ts.y + pady * 2);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), U(c, 0.16f), size.y * 0.5f);
    dl->AddText(ImVec2(p.x + padx, p.y + pady), U(c), text);
    ImGui::Dummy(size);
}

static void heading(const char* text, float scale) {
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * scale);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

// Rounded notice box with a coloured left edge. Returns true when the optional button was clicked.
static bool callout(const char* text, ImVec4 c, const char* button = nullptr) {
    float w = ImGui::GetContentRegionAvail().x;
    float pad = px(14);
    ImVec2 ts = ImGui::CalcTextSize(text, nullptr, false, w - pad * 2 - px(6));
    float bh = button ? px(38) + px(8) : 0;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ts.y + pad * 2 + bh;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(c, 0.10f), px(10));
    dl->AddRectFilled(p, ImVec2(p.x + px(4), p.y + h), U(c), px(10), ImDrawFlags_RoundCornersLeft);
    ImGui::SetCursorScreenPos(ImVec2(p.x + pad + px(6), p.y + pad));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - pad * 2 - px(6));
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    bool clicked = false;
    if (button) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + pad + px(6), p.y + pad + ts.y + px(8)));
        clicked = btn(button, Secondary);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
    ImGui::Dummy(ImVec2(w, px(6)));
    return clicked;
}

static void progress_bar(float f) {
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = px(6);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(kCardHi), h);
    if (f > 0) dl->AddRectFilled(p, ImVec2(p.x + w * std::min(f, 1.f), p.y + h), U(kAccent), h);
    ImGui::Dummy(ImVec2(w, h));
}

// Small line icons drawn with primitives (no icon font needed).
static void icon(ImDrawList* dl, int kind, ImVec2 c, float r, ImU32 color) {
    float t = px(1.8f);
    if (kind == 0) {  // download arrow into tray
        dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r * 0.35f), color, t);
        dl->AddLine(ImVec2(c.x - r * 0.5f, c.y - r * 0.15f), ImVec2(c.x, c.y + r * 0.35f), color, t);
        dl->AddLine(ImVec2(c.x + r * 0.5f, c.y - r * 0.15f), ImVec2(c.x, c.y + r * 0.35f), color, t);
        dl->AddLine(ImVec2(c.x - r * 0.8f, c.y + r * 0.85f), ImVec2(c.x + r * 0.8f, c.y + r * 0.85f), color, t);
    } else if (kind == 1) {  // pencil
        dl->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.7f), ImVec2(c.x + r * 0.55f, c.y - r * 0.55f), color, t * 2.2f);
        dl->AddTriangleFilled(ImVec2(c.x - r * 0.95f, c.y + r * 0.95f), ImVec2(c.x - r * 0.9f, c.y + r * 0.45f), ImVec2(c.x - r * 0.4f, c.y + r * 0.9f), color);
    } else {  // info
        dl->AddCircle(c, r, color, 24, t);
        dl->AddLine(ImVec2(c.x, c.y - r * 0.05f), ImVec2(c.x, c.y + r * 0.5f), color, t);
        dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.4f), t * 0.7f, color);
    }
}

static bool nav_item(const char* label, int icon_kind, bool active) {
    float w = ImGui::GetContentRegionAvail().x;
    float h = px(44);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    bool clicked = ImGui::InvisibleButton("##nav", ImVec2(w, h));
    ImGui::PopID();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(kAccent, 0.12f), px(10));
        dl->AddRectFilled(ImVec2(p.x, p.y + px(10)), ImVec2(p.x + px(3), p.y + h - px(10)), U(kAccent), px(2));
    } else if (hov) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(kCard), px(10));
    }
    ImU32 c = active ? U(kAccent) : hov ? U(kText) : U(kMuted);
    icon(dl, icon_kind, ImVec2(p.x + px(26), p.y + h * 0.5f), px(8), c);
    ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase);
    dl->AddText(ImVec2(p.x + px(48), p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), c, label);
    ImGui::PopFont();
    return clicked;
}

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
    if (!g_root.empty()) {
        std::lock_guard<std::mutex> l(g_data);
        std::set<std::string> xml_hashes;
        for (auto& f : g_index.reference)
            if (f.path.size() > 4 && f.path.compare(f.path.size() - 4, 4, ".xml") == 0) xml_hashes.insert(f.sha256);
        for (auto& p : g_index.packs)
            for (auto& f : p.files)
                if (is_xml_path(f.path)) xml_hashes.insert(f.sha256);
        std::string installed_xml = local_hash(g_root / L"Wolfpack_Data" / L"Language" / L"Official" / L"English.xml");
        bool xml_stale = !installed_xml.empty() && !xml_hashes.count(installed_xml);
        for (auto& p : g_index.packs) {
            bool installed = false, outdated = false;
            for (auto& f : p.files) {
                fs::path local = g_root / u8_path(install_rel(p, f));
                if (is_lang_path(f.path)) {
                    if (fs::exists(local)) {
                        installed = true;
                        if (local_hash(local) != f.sha256) outdated = true;
                    }
                }
            }
            if (!installed) continue;
            if (xml_stale && p.has_xml()) outdated = true;
            g_installed.insert(p.code);
            g_state[p.code] = outdated ? 2 : 1;
        }
    }
    g_installed_dirty = false;
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
    std::thread([pack, addon, root, ref_xml, has_ref_xml] {
        InstallResult r = install_pack(pack, root, addon, [](float p, const std::string& s) { g_install_task.set(p, s); }, has_ref_xml ? &ref_xml : nullptr);
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
    bool only_untranslated = false, with_xml = true;
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
    std::string err, lang = dump_lang(ed_lang());
    bool denied = false;
    bool ok = write_file_atomic(g_root / L"Wolfpack_Data" / L"Languages" / u8_path(lang_filename()), lang.data(), lang.size(), err, &denied);
    if (ok && ed.with_xml) {
        std::string xml = dump_xml(ed_xml());
        ok = write_file_atomic(g_root / L"Wolfpack_Data" / L"Language" / L"Official" / L"English.xml", xml.data(), xml.size(), err, &denied);
    }
    if (denied) g_need_admin = true;
    ed.status = ok ? "Test copy written to the game folder. Start the game and pick the language in the settings." : err;
    ed.status_bad = !ok;
}

// =============================================================================================
// Pages
// =============================================================================================
void ui_init(ImFont* bold) {
    g_bold = bold;
    detect_game();
    load_index_async();
}

static void page_header(const char* title, const char* subtitle) {
    heading(title, 1.7f);
    ImGui::TextColored(kMuted, "%s", subtitle);
    ImGui::Dummy(ImVec2(0, px(6)));
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

static void card_bg(ImVec2 size, float rounding = 12) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), U(kCard), px(rounding));
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), U(kBorder), px(rounding));
}

static void page_install() {
    page_header("Install a language", "Pick a language pack from GitHub and install it into your Wolfpack folder.");
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
        float w = ImGui::GetContentRegionAvail().x, h = px(68);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##card", ImVec2(w, h));
        bool hov = ImGui::IsItemHovered();
        if (clicked) g_selected = i;
        bool sel = g_selected == i;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), U(sel ? kCardHi : hov ? vec(0.10f, 0.125f, 0.155f) : kCard), px(12));
        dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), sel ? U(kAccent, 0.8f) : U(kBorder), px(12), 0, sel ? px(1.5f) : 1.f);
        ImGui::PushFont(g_bold, ImGui::GetStyle().FontSizeBase * 1.15f);
        dl->AddText(ImVec2(pos.x + px(18), pos.y + px(11)), U(kText), p.native_name.c_str());
        ImGui::PopFont();
        std::string sub = p.name + "   " + p.code;
        dl->AddText(ImVec2(pos.x + px(18), pos.y + px(37)), U(kMuted), sub.c_str());
        // chips right aligned
        const char* st = p.status.c_str();
        float cw = ImGui::CalcTextSize(st).x + px(18);
        ImVec2 save = ImGui::GetCursorScreenPos();
        float right = pos.x + w - px(16);
        float cy = pos.y + (h - ImGui::GetTextLineHeight() - px(6)) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(right - cw, cy));
        chip(st, status_color(p.status));
        if (g_installed.count(p.code)) {
            bool upd = g_state[p.code] == 2;
            const char* label = upd ? "update available" : "installed";
            float cw2 = ImGui::CalcTextSize(label).x + px(18);
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
    ImGui::BeginChild("detail", ImVec2(detail_w, avail.y), ImGuiChildFlags_AlwaysUseWindowPadding);
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
        heading(p.native_name.c_str(), 1.5f);
        ImGui::TextColored(kMuted, "%s  -  %s", p.name.c_str(), p.code.c_str());
        ImGui::Dummy(ImVec2(0, px(2)));
        chip(p.status.c_str(), status_color(p.status));
        chip(("v" + p.pack_version).c_str(), kInfo, true);
        if (g_installed.count(p.code)) {
            if (g_state[p.code] == 2) chip("update available", kAccent, true);
            else chip("installed, up to date", kGood, true);
        }
        if (!p.notes.empty()) {
            ImGui::Dummy(ImVec2(0, px(4)));
            ImGui::TextColored(kMuted, "%s", p.notes.c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, px(4)));
        ImGui::BeginChild("warn", ImVec2(inner, px(150)));
        for (auto& w : p.warnings(g_game_ver)) callout(w.c_str(), kWarn);
        ImGui::EndChild();
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
        ImGui::TextColored(kMuted, "You can keep several languages installed and switch in the game settings. Control tooltips are a single shared file: the last installed pack decides their language.");
        ImGui::PopTextWrapPos();
        float bw = inner;
        bool upd = g_installed.count(p.code) && g_state[p.code] == 2;
        bool same = g_installed.count(p.code) && g_state[p.code] == 1;
        if (btn(busy ? "Working..." : upd ? "Update" : same ? "Reinstall" : "Install", Primary, ImVec2(bw, px(44)), !g_root.empty() && !busy && !g_game_running))
            install_async(p, can_addon && bepinex && g_with_addon);
        if (btn("Restore original English", Secondary, ImVec2(bw, px(38)), !g_root.empty() && !busy && !g_game_running))
            for (auto& q : packs)
                if (q.code == "en") install_async(q, false);
    }
    ImGui::EndGroup();
    ImGui::EndChild();
}

static void ed_toolbar(const std::vector<Pack>& packs) {
    if (btn("New language", Primary)) {
        std::lock_guard<std::mutex> l(g_data);
        ed_blank();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(160));
    int pi = packs.empty() ? 0 : std::min(ed.open_pack, (int)packs.size() - 1);
    if (ImGui::BeginCombo("##pk", packs.empty() ? "-" : (packs[pi].native_name + " (" + packs[pi].code + ")").c_str())) {
        for (int i = 0; i < (int)packs.size(); ++i)
            if (ImGui::Selectable((packs[i].native_name + " (" + packs[i].code + ")").c_str(), i == ed.open_pack)) ed.open_pack = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (btn("Open from GitHub", Secondary, ImVec2(0, 0), !packs.empty() && !g_open_task.running)) ed_open_pack_async(packs[pi]);
    if (!g_root.empty()) {
        if (!ed.installed_scanned) {
            std::error_code ec;
            for (auto& e : fs::directory_iterator(g_root / L"Wolfpack_Data" / L"Languages", ec))
                if (e.path().extension() == L".lang" && e.path().filename() != L"Official English.lang") ed.installed_files.push_back(e.path());
            ed.installed_scanned = true;
        }
        ImGui::SameLine(0, px(16));
        int ii = ed.installed_files.empty() ? 0 : std::min(ed.open_installed, (int)ed.installed_files.size() - 1);
        ImGui::SetNextItemWidth(px(170));
        if (ImGui::BeginCombo("##inst", ed.installed_files.empty() ? "no installed packs" : path_u8(ed.installed_files[ii].filename()).c_str())) {
            for (int i = 0; i < (int)ed.installed_files.size(); ++i)
                if (ImGui::Selectable(path_u8(ed.installed_files[i].filename()).c_str(), i == ed.open_installed)) ed.open_installed = i;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (btn("Open installed", Secondary, ImVec2(0, 0), !ed.installed_files.empty())) {
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
    }
    if (g_open_task.running) { ImGui::SameLine(); ImGui::TextColored(kMuted, "Loading..."); }
}

static void page_editor() {
    page_header("Language editor", "Create a new translation or improve an existing one, then export it for a pull request.");
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
    ImGui::SetCursorPos(ImVec2(px(14), px(14)));
    float fw = (cw - px(28) - px(30)) / 4.f;
    ImGui::SetNextItemWidth(fw * 0.7f); ImGui::InputTextWithHint("##code", "Code (sv, pt-BR)", &ed.code);
    ImGui::SameLine(); ImGui::SetNextItemWidth(fw); ImGui::InputTextWithHint("##name", "English name", &ed.name);
    ImGui::SameLine(); ImGui::SetNextItemWidth(fw * 1.2f); ImGui::InputTextWithHint("##nat", "Name shown in the game", &ed.native);
    ImGui::SameLine(); ImGui::SetNextItemWidth(fw * 1.1f); ImGui::InputTextWithHint("##auth", "Your GitHub name", &ed.author);
    ImGui::EndChild();

    // ---- filter row
    int done = 0;
    for (auto& r : ed.rows) done += !r.untranslated();
    ImGui::SetNextItemWidth(px(280));
    ImGui::InputTextWithHint("##flt", "Search key, English or translation", &ed.filter);
    ImGui::SameLine();
    ImGui::Checkbox("Only untranslated", &ed.only_untranslated);
    ImGui::SameLine(0, px(24));
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kMuted, "%d of %d translated", done, (int)ed.rows.size());
    ImGui::SameLine();
    {
        float f = ed.rows.empty() ? 0.f : (float)done / (float)ed.rows.size();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + px(12));
        ImGui::PushItemWidth(px(160));
        progress_bar(f);
        ImGui::PopItemWidth();
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

    float footer = px(ed.xrows.empty() ? 96 : 130) + (ed.status.empty() ? 0 : px(70));
    ImGui::BeginChild("tbl", ImVec2(0, -footer), ImGuiChildFlags_None);
    card_bg(ImGui::GetContentRegionAvail(), 12);
    if (ImGui::BeginTable("rows", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, px(64));
        ImGui::TableSetupColumn("English", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn("Translation", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin((int)idx.size(), px(46));
        while (clip.Step()) {
            for (int n = clip.DisplayStart; n < clip.DisplayEnd; ++n) {
                Row& r = ed.rows[idx[n]];
                ImGui::TableNextRow(0, px(46));
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
    ImGui::EndChild();

    if (!ed.xrows.empty()) {
        ImGui::Checkbox("Include control tooltips (English.xml)", &ed.with_xml);
        ImGui::SameLine();
        if (ImGui::TreeNode("Edit tooltips")) {
            ImGui::BeginChild("xml", ImVec2(0, px(220)), ImGuiChildFlags_Borders);
            for (int i = 0; i < (int)ed.xrows.size(); ++i) {
                XRow& r = ed.xrows[i];
                if (!match(r.en, r.tr, r.untranslated(), r.tag)) continue;
                ImGui::PushID(i);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(r.untranslated() ? kAccent : kMuted, "%s", r.tag.c_str());
                ImGui::SameLine(px(230));
                ImGui::TextUnformatted(r.en.c_str());
                ImGui::SameLine(px(480));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##x", r.en.c_str(), &r.tr);
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::TreePop();
        }
    }

    if (!ed.status.empty()) callout(ed.status.c_str(), ed.status_bad ? kBad : kGood);
    if (btn("Export for GitHub", Primary, ImVec2(0, px(42)))) ed_export();
    ImGui::SameLine();
    if (btn("Test in game", Secondary, ImVec2(0, px(42)), !g_root.empty() && !g_game_running)) ed_test_in_game();
    ImGui::SameLine();
    if (!ed.exported.empty()) {
        if (btn("Open export folder", Ghost, ImVec2(0, px(42)))) open_folder(ed.exported);
        ImGui::SameLine();
    }
    if (btn("Repository", Ghost, ImVec2(0, px(42)))) open_url("https://github.com/niclqs/wolfpack-localization");
}

static void page_about() {
    page_header("About", "Wolfpack Language Tool");
    float w = std::min(ImGui::GetContentRegionAvail().x, px(640));
    ImGui::BeginChild("about", ImVec2(w, px(250)));
    card_bg(ImVec2(w, px(250)), 12);
    ImGui::SetCursorPos(ImVec2(px(22), px(20)));
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(w - px(22));
    ImGui::TextWrapped("An unofficial fan project. Wolfpack is developed by Usurpator.");
    ImGui::Dummy(ImVec2(0, px(6)));
    ImGui::TextColored(kAccent, "Tool license");
    ImGui::TextWrapped("PolyForm Noncommercial 1.0.0. You may use, copy and modify the tool, but not for commercial purposes.");
    ImGui::Dummy(ImVec2(0, px(6)));
    ImGui::TextColored(kAccent, "Language packs");
    ImGui::TextWrapped("The translations live in the GitHub repository (MIT). Contributions are welcome: use the Editor, export and open a pull request.");
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, px(8)));
    if (btn("Open repository", Secondary)) open_url("https://github.com/niclqs/wolfpack-localization");
    ImGui::EndGroup();
    ImGui::EndChild();
}

// ---------------------------------------------------------------- sidebar
static void sidebar(float w, float h) {
    ImGui::BeginChild("side", ImVec2(w, h), ImGuiChildFlags_None);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U(kSide));
    dl->AddLine(ImVec2(p.x + w - 1, p.y), ImVec2(p.x + w - 1, p.y + h), U(kBorder));
    ImGui::SetCursorPos(ImVec2(px(20), px(24)));
    heading("WOLFPACK", 1.45f);
    ImGui::SetCursorPosX(px(20));
    ImGui::TextColored(kAccent, "LANGUAGE TOOL");
    ImGui::SetCursorPos(ImVec2(px(12), px(96)));
    ImGui::BeginGroup();
    ImGui::PushItemWidth(w - px(24));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, px(4)));
    ImGui::BeginChild("nav", ImVec2(w - px(24), px(160)));
    if (nav_item("Install", 0, g_page == 0)) g_page = 0;
    if (nav_item("Editor", 1, g_page == 1)) g_page = 1;
    if (nav_item("About", 2, g_page == 2)) g_page = 2;
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    // game card pinned to the bottom
    float ch = px(168);
    ImGui::SetCursorPos(ImVec2(px(12), h - ch - px(14)));
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float cw = w - px(24);
    dl->AddRectFilled(cp, ImVec2(cp.x + cw, cp.y + ch), U(kCard), px(12));
    dl->AddRect(cp, ImVec2(cp.x + cw, cp.y + ch), U(kBorder), px(12));
    ImGui::SetCursorScreenPos(ImVec2(cp.x + px(14), cp.y + px(12)));
    ImGui::BeginGroup();
    ImGui::TextColored(kMuted, "GAME FOLDER");
    if (g_root.empty()) {
        ImGui::TextColored(kBad, "Not found");
    } else {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cw - px(28));
        ImGui::TextUnformatted(path_u8(g_root).c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, px(2)));
        chip(g_game_ver.empty() ? "version unknown" : ("version " + g_game_ver).c_str(), g_game_ver.empty() ? kMuted : kInfo);
    }
    ImGui::SetCursorScreenPos(ImVec2(cp.x + px(14), cp.y + ch - px(46)));
    if (btn("Detect", Secondary, ImVec2((cw - px(40)) * 0.5f, px(32)))) detect_game();
    ImGui::SameLine(0, px(8));
    if (btn("Browse", Secondary, ImVec2((cw - px(40)) * 0.5f, px(32)))) {
        std::wstring wp;
        if (pick_folder(wp)) {
            fs::path root;
            if (normalize_game_dir(wp, root)) { set_game(root); g_game_msg.clear(); }
            else g_game_msg = "That folder does not contain Wolfpack.exe.";
        }
    }
    ImGui::EndGroup();
    ImGui::EndChild();
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

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);

    // soft top glow behind the content
    ImDrawList* bg = ImGui::GetWindowDrawList();
    bg->AddRectFilledMultiColor(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + px(260)), col(0.10f, 0.12f, 0.16f), col(0.10f, 0.12f, 0.16f), col(0.043f, 0.055f, 0.071f), col(0.043f, 0.055f, 0.071f));

    float sw = px(230);
    sidebar(sw, vp->Size.y);
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("content", ImVec2(vp->Size.x - sw, vp->Size.y), ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(px(32), px(26)));
    ImGui::BeginGroup();
    ImGui::BeginChild("page", ImVec2(vp->Size.x - sw - px(64), vp->Size.y - px(50)), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    if (g_page == 0) page_install();
    else if (g_page == 1) page_editor();
    else page_about();
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::End();
    return true;
}
