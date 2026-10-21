#!/usr/bin/python3
"""Convert the stock EleksTube nixie face (assets/clockface_stock_nixie/0-9.jpg)
into the 8-bit palettized BMPs the firmware loads from LittleFS (data/10.bmp ... data/19.bmp).

Usage: uv run python tools/convert_stock_face.py [src_dir] [data_dir] [preview.png]
The optional preview shows originals (top row) above the converted images (bottom row).
"""

import sys
from PIL import Image

src = sys.argv[1] if len(sys.argv) > 1 else "assets/clockface_stock_nixie"
dst = sys.argv[2] if len(sys.argv) > 2 else "data"
preview = sys.argv[3] if len(sys.argv) > 3 else None

w, h = 135, 240
sheet = Image.new("RGB", (10 * w, 2 * h))
for n in range(10):
    im = Image.open(f"{src}/{n}.jpg").convert("RGB")
    assert im.size == (w, h), im.size
    q = im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(f"{dst}/1{n}.bmp")
    sheet.paste(im, (n * w, 0))
    sheet.paste(Image.open(f"{dst}/1{n}.bmp").convert("RGB"), (n * w, h))
if preview:
    sheet.save(preview)
