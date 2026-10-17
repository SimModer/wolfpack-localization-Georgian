#pragma once
#include <string>
#include <vector>

struct LangEntry {
    int key = 0;
    std::string value;
};
struct LangFile {
    std::string header;
    std::vector<LangEntry> entries;
};

bool parse_lang(const std::string& text, LangFile& out, std::string& err);
std::string dump_lang(const LangFile& f);

// Control tooltips (Language/Official/English.xml): flat list of <tag>text</tag> below the root element.
struct XmlEntry {
    std::string tag, text;
};
struct XmlFile {
    std::vector<XmlEntry> entries;
};

bool parse_xml(const std::string& text, XmlFile& out, std::string& err);
std::string dump_xml(const XmlFile& f);
