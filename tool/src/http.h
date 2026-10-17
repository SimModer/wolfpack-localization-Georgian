#pragma once
#include <string>
#include <vector>

struct HttpResult {
    int status = 0;  // 0 = connection error
    std::vector<char> body;
    std::string error;
    bool ok() const { return status >= 200 && status < 300; }
};

HttpResult http_get(const std::string& url);
