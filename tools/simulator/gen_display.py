#!/usr/bin/env python3
"""Make host versions of display.h / display.cpp from the firmware's.

The LCD device class becomes an in-memory 240x320 sprite; everything else
(Canvas, fonts, fitText) is kept exactly as in the firmware.
Usage: gen_display.py <firmware src dir> <build dir>
"""
import sys

src, out = sys.argv[1], sys.argv[2]

h = open(f"{src}/display.h").read()
a, b = h.index("// ST7789 with"), h.index("enum FontId")
h = h[:a] + "class LGFX : public lgfx::LGFX_Sprite {\n public:\n  LGFX() : lgfx::LGFX_Sprite() {}\n};\n\n" + h[b:]
open(f"{out}/display.h", "w").write(h)

c = open(f"{src}/display.cpp").read()
a, b = c.index("// Init sequence"), c.index("namespace display {")
c = c[:a] + c[b:]
begin_old = "  lcd.init();\n  lcd.setRotation(flip ? 2 : 0);\n  lcd.fillScreen(TFT_BLACK);"
if begin_old not in c:
    sys.exit("gen_display.py: display::begin() changed; update this script")
c = c.replace(begin_old, "  lcd.setColorDepth(16);\n  lcd.createSprite(240, 320);")
c = c.replace("void setFlip(bool flip) { lcd.setRotation(flip ? 2 : 0); }", "void setFlip(bool) {}")
open(f"{out}/display.cpp", "w").write(c)
