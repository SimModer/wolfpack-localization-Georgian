"""Bring packs in line with reference/English.lang after a game update.

Missing keys are added with the English text (so the game never shows a blank),
unknown keys are removed and keys are written in reference order.
Usage: python tools/sync.py [pack-code ...]
"""
import sys

from common import *


def main():
    _, ref = load_lang(REFERENCE_LANG)
    wanted = set(sys.argv[1:])
    for pack in pack_dirs():
        if wanted and pack.name not in wanted:
            continue
        f = lang_file(pack)
        header, entries = load_lang(f)
        added = [k for k in ref if k not in entries]
        removed = [k for k in entries if k not in ref]
        save_lang(f, header, {k: entries.get(k, ref[k]) for k in ref})
        print(f"{pack.name}: +{len(added)} -{len(removed)}", added[:10])


if __name__ == "__main__":
    main()
