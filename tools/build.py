"""Build installable zip files into dist/.

Usage: python tools/build.py [pack-code ...]
Each zip contains a Wolfpack_Data/ folder that is copied into the game directory.
The German autotranslator add-on (packs/<code>/addons/autotranslator) is zipped separately.
"""
import json
import sys
import zipfile

from common import *


def zip_tree(zf, base, prefix=""):
    for p in sorted(base.rglob("*")):
        if p.is_file():
            zf.write(p, prefix + p.relative_to(base).as_posix())


def main():
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    wanted = set(sys.argv[1:])
    for pack in pack_dirs():
        if wanted and pack.name not in wanted:
            continue
        meta = json.loads((pack / "pack.json").read_text(encoding="utf-8"))
        name = f"Wolfpack-L10n_{pack.name}_game-{meta['game_version']}_v{meta['pack_version']}"
        with zipfile.ZipFile(dist / f"{name}.zip", "w", zipfile.ZIP_DEFLATED) as zf:
            zip_tree(zf, pack / "Wolfpack_Data", "Wolfpack_Data/")
        print("built", name + ".zip")
        addon = pack / "addons" / "autotranslator"
        if addon.exists():
            with zipfile.ZipFile(dist / f"{name}_autotranslator-addon.zip", "w", zipfile.ZIP_DEFLATED) as zf:
                zip_tree(zf, addon)
            print("built", name + "_autotranslator-addon.zip")


if __name__ == "__main__":
    main()
