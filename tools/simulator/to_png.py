#!/usr/bin/env python3
"""Convert the simulator's .ppm frames to .png (real size) and a 2x contact sheet."""
import glob
import os
import sys

from PIL import Image, ImageDraw

out = sys.argv[1]
frames = []
for f in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    im = Image.open(f).convert("RGB")
    png = f[:-4] + ".png"
    im.save(png)
    os.remove(f)
    frames.append((os.path.basename(png)[:-4], im))

cols, W, H = 4, 480, 640
rows = (len(frames) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (W + 20) + 20, rows * (H + 40) + 20), (28, 30, 34))
d = ImageDraw.Draw(sheet)
for i, (name, im) in enumerate(frames):
    x, y = 20 + (i % cols) * (W + 20), 20 + (i // cols) * (H + 40)
    d.text((x, y), name, fill=(220, 220, 220))
    sheet.paste(im.resize((W, H), Image.NEAREST), (x, y + 16))
sheet.save(os.path.join(out, "contact_sheet.png"))
print(f"{len(frames)} screens -> {out}/*.png and contact_sheet.png")
