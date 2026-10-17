#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>

#include <cstdio>

#include "game.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "lang.h"
#include "packs.h"
#include "ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static void create_rtv() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

static bool create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) return false;
    create_rtv();
    return true;
}

static void destroy_device() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_dev && wp != SIZE_MINIMIZED) {
            if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
            g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            create_rtv();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static ImFont* load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    const float size = 17.f;
    // Regular for text, Semibold for headings/buttons; both get the same fallbacks so
    // every language (CJK, Georgian, symbols) can be shown and edited.
    auto add = [&](const char* base) -> ImFont* {
        ImFont* f = io.Fonts->AddFontFromFileTTF(base, size);
        if (!f) return nullptr;
        for (const char* fb : {"C:/Windows/Fonts/sylfaen.ttf", "C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/YuGothM.ttc",
                               "C:/Windows/Fonts/malgun.ttf", "C:/Windows/Fonts/seguisym.ttf"}) {
            if (GetFileAttributesA(fb) == INVALID_FILE_ATTRIBUTES) continue;
            ImFontConfig cfg;
            cfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(fb, size, &cfg);
        }
        return f;
    };
    ImFont* regular = add("C:/Windows/Fonts/segoeui.ttf");
    if (!regular) regular = io.Fonts->AddFontDefault();
    ImFont* bold = add("C:/Windows/Fonts/seguisb.ttf");
    return bold ? bold : regular;
}

// ---------------------------------------------------------------- self test (no window)
static int selftest(const wchar_t* log) {
    if (log && *log) _wfreopen(log, L"w", stdout);
    else {
        AttachConsole(ATTACH_PARENT_PROCESS);
        freopen("CONOUT$", "w", stdout);
    }
    int fail = 0;
    auto check = [&](bool ok, const char* what) {
        printf("[%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) ++fail;
    };
    auto dirs = find_game_dirs();
    printf("game dirs found: %zu\n", dirs.size());
    for (auto& d : dirs) printf("  %s  version=%s writable=%d\n", path_u8(d).c_str(), read_game_version(d).c_str(), (int)dir_writable(d / L"Wolfpack_Data"));
    if (!dirs.empty()) {
        fs::path root;
        check(normalize_game_dir(dirs[0] / L"Wolfpack.exe", root) && root == dirs[0], "normalize from Wolfpack.exe");
        check(normalize_game_dir(dirs[0] / L"Wolfpack_Data", root) && root == dirs[0], "normalize from Wolfpack_Data");
        check(!read_game_version(dirs[0]).empty(), "game version readable");
    }
    Index idx;
    std::string err;
    check(fetch_index(idx, err), ("fetch index " + err).c_str());
    if (!idx.packs.empty()) {
        printf("index: game %s, %zu packs\n", idx.game_version.c_str(), idx.packs.size());
        fs::path tmp = fs::temp_directory_path() / L"wolfpack_l10n_selftest";
        std::error_code ec;
        fs::remove_all(tmp, ec);
        for (auto& p : idx.packs) {
            InstallResult r = install_pack(p, tmp, true, [](float, const std::string&) {});
            bool ok = r.error.empty() && r.missing.empty() && r.installed == (int)(p.files.size() + p.addon_files.size());
            check(ok, ("install pack " + p.code).c_str());
            for (auto& m : r.missing) printf("    missing: %s\n", m.c_str());
            // round trip of the language file
            for (auto& f : p.files) {
                std::vector<char> data;
                if (f.path.size() > 5 && f.path.compare(f.path.size() - 5, 5, ".lang") == 0 && read_file(tmp / u8_path(install_rel(p, f)), data)) {
                    LangFile lf; std::string e;
                    bool pok = parse_lang(std::string(data.begin(), data.end()), lf, e);
                    LangFile lf2; std::string e2;
                    bool rt = pok && parse_lang(dump_lang(lf), lf2, e2) && lf2.entries.size() == lf.entries.size() && lf2.header == lf.header;
                    check(rt && lf.entries.size() == 320, ("lang parse/dump " + p.code).c_str());
                } else if (f.path.find("English.xml") != std::string::npos && read_file(tmp / u8_path(install_rel(p, f)), data)) {
                    XmlFile xf; std::string e;
                    bool pok = parse_xml(std::string(data.begin(), data.end()), xf, e);
                    XmlFile xf2; std::string e2;
                    bool rt = pok && parse_xml(dump_xml(xf), xf2, e2) && xf2.entries.size() == xf.entries.size() && xf.entries.size() == 31;
                    check(rt, ("xml parse/dump " + p.code).c_str());
                }
            }
        }
        fs::remove_all(tmp, ec);
    }
    printf(fail ? "SELFTEST FAILED (%d)\n" : "SELFTEST OK\n", fail);
    return fail ? 1 : 0;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdline, int) {
    if (const wchar_t* st = wcsstr(cmdline, L"--selftest")) {
        st += 10;  // optional log file path after the flag
        while (*st == L' ') ++st;
        return selftest(st);
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof wc, CS_CLASSDC, wnd_proc, 0, 0, inst, nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"WolfpackL10nTool", nullptr};
    RegisterClassExW(&wc);
    float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Wolfpack Language Tool", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, (int)(1180 * dpi), (int)(800 * dpi),
                              nullptr, nullptr, inst, nullptr);
    if (!create_device(hwnd)) {
        MessageBoxW(nullptr, L"Direct3D 11 is not available.", L"Wolfpack Language Tool", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // portable: never write files next to the exe
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    float scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    ImFont* bold = load_fonts();
    ui_init(bold);

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsIconic(hwnd)) { Sleep(50); continue; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        if (!ui_frame(scale)) running = false;
        ImGui::Render();

        const float clear[4] = {0.055f, 0.075f, 0.09f, 1.f};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    destroy_device();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, inst);
    return 0;
}
