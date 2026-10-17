#include "lang.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

bool parse_lang(const std::string& text, LangFile& out, std::string& err) {
    try {
        size_t skip = (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) ? 3 : 0;
        json j = json::parse(text.begin() + skip, text.end());
        out.header = j.at("header").get<std::string>();
        out.entries.clear();
        for (auto& p : j.at("pairs")) out.entries.push_back({p.at("key").get<int>(), p.at("value").get<std::string>()});
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

std::string dump_lang(const LangFile& f) {
    json pairs = json::array();
    for (auto& e : f.entries) pairs.push_back({{"key", e.key}, {"value", e.value}});
    json j = {{"header", f.header}, {"pairs", pairs}};
    return j.dump(2, ' ', false) + "\n";
}

static std::string unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            struct { const char* from; char to; } map[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            bool hit = false;
            for (auto& m : map) {
                size_t n = strlen(m.from);
                if (s.compare(i, n, m.from) == 0) { o += m.to; i += n - 1; hit = true; break; }
            }
            if (hit) continue;
        }
        o += s[i];
    }
    return o;
}

static std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

bool parse_xml(const std::string& text, XmlFile& out, std::string& err) {
    out.entries.clear();
    size_t pos = text.find("<LanguageString");
    if (pos == std::string::npos) { err = "no <LanguageString> element"; return false; }
    pos = text.find('>', pos);
    if (pos == std::string::npos) { err = "broken root element"; return false; }
    ++pos;
    for (;;) {
        size_t lt = text.find('<', pos);
        if (lt == std::string::npos || text.compare(lt, 2, "</") == 0) break;
        size_t gt = text.find('>', lt);
        if (gt == std::string::npos) { err = "unterminated tag"; return false; }
        std::string tag = text.substr(lt + 1, gt - lt - 1);
        if (!tag.empty() && tag.back() == '/') {  // <tag />
            tag.pop_back();
            while (!tag.empty() && tag.back() == ' ') tag.pop_back();
            out.entries.push_back({tag, ""});
            pos = gt + 1;
            continue;
        }
        std::string close = "</" + tag + ">";
        size_t end = text.find(close, gt);
        if (end == std::string::npos) { err = "missing " + close; return false; }
        out.entries.push_back({tag, unescape(text.substr(gt + 1, end - gt - 1))});
        pos = end + close.size();
    }
    return true;
}

std::string dump_xml(const XmlFile& f) {
    std::string o =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<LanguageString xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n";
    for (auto& e : f.entries) o += "  <" + e.tag + ">" + escape(e.text) + "</" + e.tag + ">\n";
    o += "</LanguageString>\n";
    return o;
}
