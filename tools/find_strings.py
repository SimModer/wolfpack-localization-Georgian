"""List hard-coded UI strings from the game that are NOT in the .lang file.

These cannot be translated through the language packs.
Reads Wolfpack_Data/il2cpp_data/Metadata/global-metadata.dat.
Usage: python tools/find_strings.py "<path to Wolfpack_Data>" [out.txt]
"""
import re
import struct
import sys

from common import *


def literals(metadata):
    d = open(metadata, "rb").read()
    list_off, list_size, data_off, _ = struct.unpack("<4I", d[8:24])
    for i in range(list_size // 8):
        ln, idx = struct.unpack("<II", d[list_off + i * 8: list_off + i * 8 + 8])
        yield d[data_off + idx: data_off + idx + ln].decode("utf-8", "replace")


def main():
    data = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "untranslated_candidates.txt"
    _, ref = load_lang(REFERENCE_LANG)
    known = set(ref.values())
    noise = re.compile(r"[{}<>_/\=%]|Exception|http|\.cs|serializ|Unity|assembly", re.I)
    found = sorted({s.strip() for s in literals(data / "il2cpp_data/Metadata/global-metadata.dat")
                    if re.search(r"[A-Za-z]{3,} [A-Za-z]{2,}", s) and len(s) < 120
                    and not noise.search(s) and s.strip() not in known})
    out.write_text("\n".join(found), encoding="utf-8")
    print(f"{len(found)} candidates written to {out} (still contains engine/library text - review manually)")


if __name__ == "__main__":
    main()
