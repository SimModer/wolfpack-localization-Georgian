"""Validate all language packs against the reference files.

Usage: python tools/validate.py [pack-code ...]
Exit code 1 if any error was found (warnings do not fail).
"""
import json
import re
import sys
import unicodedata
import xml.etree.ElementTree as ET

from common import *


def check_pack(pack, ref):
    errors, warnings = [], []
    meta = json.loads((pack / "pack.json").read_text(encoding="utf-8"))
    f = lang_file(pack)
    if f is None:
        return ["no .lang file in Wolfpack_Data/Languages"], warnings
    try:
        data = load_lang_full(f)
        header, entries = load_lang(f)
    except Exception as e:
        return [f"{f.name}: invalid JSON ({e})"], warnings
    if not header.strip():
        errors.append("empty header")
    # all sections must exist with the same keys as the reference, otherwise the game hangs on start
    full_ref = load_lang_full(REFERENCE_LANG)
    for sec in SECTIONS[1:]:
        if sec not in data:
            errors.append(f"missing section '{sec}' (the game hangs on 'Loading languages')")
            continue
        keys = {p["key"] for p in data[sec]}
        ref_keys = {p["key"] for p in full_ref[sec]}
        if keys != ref_keys:
            errors.append(f"{sec}: keys differ from the reference (missing {sorted(ref_keys - keys)}, unknown {sorted(keys - ref_keys)})")
    for sec in SECTIONS:
        for p in data.get(sec, []):
            bad = [hex(ord(c)) for c in p["value"] if unicodedata.category(c) in ("Cc", "Cf", "Co", "Cn", "Zl", "Zp")]
            if bad:
                errors.append(f"{sec} key {p['key']}: invisible/control characters {bad}")
    for k in ref:
        if k not in entries:
            errors.append(f"missing key {k}")
    for k in entries:
        if k not in ref:
            errors.append(f"unknown key {k}")
    same = 0
    for k, v in entries.items():
        if k not in ref:
            continue
        if sorted(PLACEHOLDER.findall(v)) != sorted(PLACEHOLDER.findall(ref[k])):
            warnings.append(f"key {k}: placeholders differ ({ref[k]!r} -> {v!r})")
        if v == ref[k] and re.search(r"[A-Za-z]{3}", v):
            same += 1
    if meta["status"] not in ("complete", "partial", "original") or same > 40:
        warnings.append(f"{same} entries identical to English")
    xml = pack / "Wolfpack_Data" / "Language" / "Official" / "English.xml"
    if xml.exists():
        try:
            tags = [e.tag for e in ET.parse(xml).getroot()]
            ref_tags = [e.tag for e in ET.parse(REFERENCE_XML).getroot()]
            for t in ref_tags:
                if t not in tags:
                    errors.append(f"English.xml: missing <{t}>")
            for t in tags:
                if t not in ref_tags:
                    errors.append(f"English.xml: unknown <{t}>")
        except ET.ParseError as e:
            errors.append(f"English.xml: {e}")
    for txt in pack.glob("addons/autotranslator/BepInEx/Translation/*/Text/*.txt"):
        seen = set()
        for n, line in enumerate(txt.read_text(encoding="utf-8-sig").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            if "=" not in line:
                errors.append(f"{txt.name}:{n}: missing '='")
                continue
            src = line.split("=", 1)[0]
            if src in seen:
                warnings.append(f"{txt.name}:{n}: duplicate source text")
            seen.add(src)
    return errors, warnings


def main():
    _, ref = load_lang(REFERENCE_LANG)
    wanted = set(sys.argv[1:])
    failed = False
    for pack in pack_dirs():
        if wanted and pack.name not in wanted:
            continue
        errors, warnings = check_pack(pack, ref)
        status = "FAIL" if errors else "ok"
        print(f"[{status}] {pack.name} ({len(warnings)} warnings)")
        for e in errors:
            print("   ERROR:", e)
        for w in warnings[:10]:
            print("   warn: ", w)
        failed |= bool(errors)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
