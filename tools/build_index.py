"""Generate packs/index.json, the manifest read by the installer/editor tool.

Usage: python tools/build_index.py          write packs/index.json
       python tools/build_index.py --check  exit 1 if the file is out of date (CI)
Re-run after every change below packs/ or reference/.
"""
import hashlib
import json
import sys

from common import *


def entry(path):
    data = path.read_bytes()
    return {"path": path.relative_to(ROOT).as_posix(), "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest()}


def build():
    ref = json.loads((ROOT / "reference" / "VERSION.json").read_text(encoding="utf-8"))
    packs = []
    for pack in pack_dirs():
        meta = json.loads((pack / "pack.json").read_text(encoding="utf-8"))
        files = [entry(p) for p in sorted((pack / "Wolfpack_Data").rglob("*")) if p.is_file()]
        addon = pack / "addons" / "autotranslator"
        extras = [entry(p) for p in sorted(addon.rglob("*")) if p.is_file() and p.name != ".gitkeep"] if addon.exists() else []
        packs.append({**{k: meta[k] for k in ("code", "name", "native_name", "pack_version", "game_version", "status", "notes")},
                      "authors": meta.get("authors", []), "files": files, "addon_files": extras})
    return {"schema": 1, "game_version": ref["game_version"],
            "reference": [entry(REFERENCE_LANG), entry(REFERENCE_XML)], "packs": packs}


def main():
    text = json.dumps(build(), ensure_ascii=False, indent=1) + "\n"
    out = PACKS / "index.json"
    if "--check" in sys.argv:
        if not out.exists() or out.read_text(encoding="utf-8") != text:
            print("packs/index.json is out of date: run python tools/build_index.py")
            sys.exit(1)
        print("index.json up to date")
        return
    out.write_text(text, encoding="utf-8", newline="\n")
    print("wrote", out.relative_to(ROOT), f"({len(text) // 1024} KiB)")


if __name__ == "__main__":
    main()
