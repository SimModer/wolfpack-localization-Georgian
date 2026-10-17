"""Generates src/app.ico (the logo mark: three slanted bars) with Pillow. Run: python tool/assets/make_icon.py"""
from pathlib import Path
from PIL import Image, ImageDraw

LIME, BG = (199, 255, 0, 255), (11, 11, 13, 255)


def render(size):
    k = 8  # supersampling
    n = size * k
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cut = int(n * 0.22)  # chamfered bottom-right corner, like the UI cards
    d.polygon([(0, 0), (n, 0), (n, n - cut), (n - cut, n), (0, n)], fill=BG)
    # logo mark, same geometry as logo() in ui.cpp
    h = n * 0.50
    w, t, sk = h * 0.9, h * 0.22, h * 0.28
    total = w * 0.84 + sk + t
    ox, oy = (n - total) / 2, (n - h) / 2 - n * 0.01

    def bar(x, y0, y1):
        d.polygon([(ox + x + sk, oy + y0), (ox + x + sk + t, oy + y0), (ox + x + t, oy + y1), (ox + x, oy + y1)], fill=LIME)

    bar(0, 0, h)
    bar(w * 0.42, h * 0.35, h)
    bar(w * 0.84, 0, h)
    return img.resize((size, size), Image.LANCZOS)


sizes = [16, 24, 32, 48, 64, 128, 256]
out = Path(__file__).resolve().parent.parent / "src" / "app.ico"
render(256).save(out, format="ICO", sizes=[(s, s) for s in sizes], append_images=[render(s) for s in sizes if s != 256])
render(256).save(Path(__file__).resolve().parent / "logo.png")
print("wrote", out)
