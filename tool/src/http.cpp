#include "http.h"

#include <windows.h>
#include <winhttp.h>

#include "util.h"

HttpResult http_get(const std::string& url) {
    HttpResult r;
    std::wstring wurl = widen(url);
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof uc;
    uc.dwSchemeLength = uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { r.error = "invalid URL"; return r; }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength + uc.dwExtraInfoLength);

    HINTERNET ses = WinHttpOpen(L"WolfpackL10nTool/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!ses) { r.error = "WinHttpOpen failed"; return r; }
    WinHttpSetTimeouts(ses, 8000, 8000, 15000, 30000);
    HINTERNET con = WinHttpConnect(ses, host.c_str(), uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                          : nullptr;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD code = 0, sz = sizeof code;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
        r.status = (int)code;
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
            size_t old = r.body.size();
            r.body.resize(old + avail);
            DWORD got = 0;
            if (!WinHttpReadData(req, r.body.data() + old, avail, &got)) { r.error = "read failed"; r.status = 0; break; }
            r.body.resize(old + got);
        }
        if (r.status && !r.ok()) r.error = "HTTP " + std::to_string(r.status);
    } else {
        r.error = "connection failed (error " + std::to_string(GetLastError()) + ")";
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return r;
}
