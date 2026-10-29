"""Bring packs in line with reference/English.lang after a game update.

For every section (pairs, suffixPairs, morsePairs) missing keys are added with the English text
(so the game never shows a blank), unknown keys are removed and keys are written in reference order.
Usage: python tools/sync.py [pack-code ...]
"""
import sys

from common import *


def main():
    ref = load_lang_full(REFERENCE_LANG)
    wanted = set(sys.argv[1:])
    for pack in pack_dirs():
        if wanted and pack.name not in wanted:
            continue
        f = lang_file(pack)
        data = load_lang_full(f)
        report = []
        for sec in SECTIONS:
            have = {p["key"]: p["value"] for p in data.get(sec, [])}
            added = [p["key"] for p in ref[sec] if p["key"] not in have]
            removed = [k for k in have if k not in {p["key"] for p in ref[sec]}]
            data[sec] = [{"key": p["key"], "value": have.get(p["key"], p["value"])} for p in ref[sec]]
            if added or removed:
                report.append(f"{sec} +{len(added)} -{len(removed)}")
        save_lang_full(f, data)
        print(f"{pack.name}: {', '.join(report) or 'in sync'}")


if __name__ == "__main__":
    main()
