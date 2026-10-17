"""Shared helpers for the localization tools (stdlib only)."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REFERENCE_LANG = ROOT / "reference" / "English.lang"
REFERENCE_XML = ROOT / "reference" / "English.xml"
PACKS = ROOT / "packs"
PLACEHOLDER = re.compile(r"\{\d+\}|<[^>]+>|%[sd]")


def load_lang(path):
    with open(path, encoding="utf-8-sig") as f:
        data = json.load(f)
    return data["header"], {p["key"]: p["value"] for p in data["pairs"]}


def save_lang(path, header, entries):
    data = {"header": header, "pairs": [{"key": k, "value": v} for k, v in entries.items()]}
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")


def pack_dirs():
    return sorted(p for p in PACKS.iterdir() if (p / "pack.json").exists())


def lang_file(pack):
    files = list((pack / "Wolfpack_Data" / "Languages").glob("*.lang"))
    return files[0] if files else None
