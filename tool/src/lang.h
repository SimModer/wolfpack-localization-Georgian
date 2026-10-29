#pragma once
#include <string>
#include <vector>

struct LangEntry {
    int key = 0;
    std::string value;
};
// A .lang file: header + three key/value lists. All three must be present, otherwise the game
// hangs on "Loading languages" (NullReferenceException in Language.Generate).
struct LangFile {
    std::string header;
    std::vector<LangEntry> entries;  // "pairs": UI texts
    std::vector<LangEntry> suffix;   // "suffixPairs": unit suffixes (" kts", " meters", ...)
    std::vector<LangEntry> morse;    // "morsePairs": radio/morse phrases
};

bool parse_lang(const std::string& text, LangFile& out, std::string& err);
// Same format as the game writes: header, pairs, suffixPairs, morsePairs, 4-space indent, no trailing newline.
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
