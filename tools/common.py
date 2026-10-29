"""Shared helpers for the localization tools (stdlib only)."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REFERENCE_LANG = ROOT / "reference" / "English.lang"
REFERENCE_XML = ROOT / "reference" / "English.xml"
PACKS = ROOT / "packs"
PLACEHOLDER = re.compile(r"\{\d+\}|<[^>]+>|%[sd]")

# A .lang file has a header and three key/value lists. The game hangs on "Loading languages"
# (NullReferenceException in Language.Generate) when suffixPairs or morsePairs is missing.
SECTIONS = ("pairs", "suffixPairs", "morsePairs")


def load_lang_full(path):
    with open(path, encoding="utf-8-sig") as f:
        return json.load(f)


def load_lang(path):
    data = load_lang_full(path)
    return data["header"], {p["key"]: p["value"] for p in data["pairs"]}


def save_lang_full(path, data):
    """Writes the file like the game does: header, the three sections, 4-space indent, no trailing newline."""
    ordered = {"header": data["header"]}
    for sec in SECTIONS:
        ordered[sec] = data.get(sec, [])
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(json.dumps(ordered, ensure_ascii=False, indent=4))


def pack_dirs():
    return sorted(p for p in PACKS.iterdir() if (p / "pack.json").exists())


def lang_file(pack):
    files = list((pack / "Wolfpack_Data" / "Languages").glob("*.lang"))
    return files[0] if files else None
