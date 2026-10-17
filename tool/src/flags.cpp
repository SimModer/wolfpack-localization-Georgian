#include "flags.h"

#include <windows.h>
#include <d3d11.h>

#include <map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "util.h"

namespace {
ID3D11Device* g_device = nullptr;
struct Flag {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0, h = 0;
    bool tried = false;
};
std::map<std::string, Flag> g_flags;

std::wstring resource_name(const std::string& code) {
    std::string n = "FLAG_";
    for (char c : code) n += c == '-' ? '_' : (char)toupper((unsigned char)c);
    return widen(n);
}

Flag& load(const std::string& code) {
    Flag& f = g_flags[code];
    if (f.tried) return f;
    f.tried = true;
    HRSRC res = FindResourceW(nullptr, resource_name(code).c_str(), RT_RCDATA);
    if (!res || !g_device) return f;
    HGLOBAL mem = LoadResource(nullptr, res);
    const unsigned char* data = (const unsigned char*)LockResource(mem);
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)SizeofResource(nullptr, res), &w, &h, &n, 4);
    if (!px) return f;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{px, (UINT)(w * 4), 0};
    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(g_device->CreateTexture2D(&td, &sd, &tex))) {
        g_device->CreateShaderResourceView(tex, nullptr, &f.srv);
        tex->Release();
        f.w = w;
        f.h = h;
    }
    stbi_image_free(px);
    return f;
}
}  // namespace

void flags_init(ID3D11Device* device) { g_device = device; }

float flag_draw(ImDrawList* dl, const std::string& code, ImVec2 p, float height, float slot) {
    Flag& f = load(code);
    float w = f.srv ? height * (float)f.w / (float)f.h : height * 1.6f;
    float used = slot > w ? slot : w;
    if (slot > w) p.x += (slot - w) * 0.5f;
    ImVec2 q(p.x + w, p.y + height);
    if (f.srv) {
        dl->AddImage((ImTextureID)(intptr_t)f.srv, p, q);
    } else {
        dl->AddRectFilled(p, q, IM_COL32(40, 40, 46, 255));
        std::string c = code.substr(0, 2);
        for (char& ch : c) ch = (char)toupper((unsigned char)ch);
        ImVec2 ts = ImGui::CalcTextSize(c.c_str());
        dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f), IM_COL32(150, 150, 160, 255), c.c_str());
    }
    dl->AddRect(p, q, IM_COL32(255, 255, 255, 40));
    return used;
}
