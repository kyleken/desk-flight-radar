#!/usr/bin/env python3
"""Build docs/QuickStart-Card.pdf: a two-sided 5 x 7 in card for the gift box.

Usage: python3 tools/make_card.py   (needs reportlab and Pillow)
Print at 100 % scale, double-sided (flip on long edge), on card stock.
"""
import os

from reportlab.lib.colors import HexColor
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "fonts")
OUT = os.path.join(ROOT, "docs", "QuickStart-Card.pdf")
SHOT = os.path.join(ROOT, "docs", "screens", "01_radar_home.png")

W, H = 5 * inch, 7 * inch
M = 0.32 * inch  # margin

NAVY = HexColor("#071423")
PANEL = HexColor("#0E1C2D")
BORDER = HexColor("#24405E")
TEXT = HexColor("#E8EEF4")
DIM = HexColor("#9DB0C3")
AMBER = HexColor("#FFB547")
CYAN = HexColor("#5CD6FF")
LAVENDER = HexColor("#BE96FF")
RED = HexColor("#FF5C5C")
GREEN = HexColor("#3DDC84")

for name, file in [("Inter", "Inter-Medium.ttf"), ("Inter-Semi", "Inter-SemiBold.ttf"),
                   ("Inter-Bold", "Inter-Bold.ttf"), ("Mono", "JetBrainsMono-Bold.ttf")]:
    pdfmetrics.registerFont(TTFont(name, os.path.join(FONTS, file)))


def background(c):
    c.setFillColor(NAVY)
    c.rect(0, 0, W, H, fill=1, stroke=0)
    # faint radar rings in the corner
    c.setStrokeColor(HexColor("#123A2C"))
    c.setLineWidth(0.8)
    for r in (40, 80, 120):
        c.circle(W - 18, H - 18, r, fill=0, stroke=1)


def wrap(c, text, font, size, width):
    words, lines, line = text.split(), [], ""
    for w in words:
        t = (line + " " + w).strip()
        if c.stringWidth(t, font, size) <= width:
            line = t
        else:
            lines.append(line)
            line = w
    if line:
        lines.append(line)
    return lines


def para(c, x, y, text, font="Inter", size=9.5, color=TEXT, width=None, leading=None):
    width = width or (W - x - M)
    leading = leading or size * 1.32
    c.setFont(font, size)
    c.setFillColor(color)
    for ln in wrap(c, text, font, size, width):
        c.drawString(x, y, ln)
        y -= leading
    return y


def step(c, y, n, title, body):
    c.setFillColor(AMBER)
    c.circle(M + 9, y - 4, 9, fill=1, stroke=0)
    c.setFillColor(NAVY)
    c.setFont("Inter-Bold", 10)
    c.drawCentredString(M + 9, y - 7.6, str(n))
    c.setFont("Inter-Semi", 10.5)
    c.setFillColor(TEXT)
    c.drawString(M + 26, y - 7, title)
    return para(c, M + 26, y - 21, body, size=8.8, color=DIM) - 4


def front(c):
    background(c)
    y = H - M - 6
    c.setFont("Inter-Bold", 22)
    c.setFillColor(TEXT)
    c.drawString(M, y - 14, "Desk Flight Radar")
    c.setFont("Inter", 10)
    c.setFillColor(AMBER)
    c.drawString(M, y - 32, "Happy holidays! The planes over your head, live on your desk.")

    # screenshot of the real screen
    sw = 1.85 * inch
    sh = sw * 320 / 240
    sx, sy = (W - sw) / 2, y - 46 - sh
    c.setFillColor(HexColor("#000000"))
    c.roundRect(sx - 7, sy - 7, sw + 14, sh + 14, 9, fill=1, stroke=0)
    c.drawImage(SHOT, sx, sy, sw, sh)

    y = sy - 26
    c.setFont("Inter-Bold", 12.5)
    c.setFillColor(TEXT)
    c.drawString(M, y, "Set it up in 2 minutes")
    y -= 12
    y = step(c, y, 1, "Plug it in", "Any USB-C phone charger or computer port.")
    y = step(c, y, 2, "Scan the QR code on its screen",
             "Use your phone's camera. Your phone joins the radar's setup Wi-Fi and a page opens "
             "(if not, browse to 192.168.4.1).")
    y = step(c, y, 3, "Enter your Wi-Fi and ZIP code",
             "Tap Save & connect, then switch your phone back to your normal Wi-Fi. "
             "Planes appear within seconds.")
    para(c, M, y - 2, "No phone handy? Tap “Pick a network on this screen” instead. "
         "Needs a 2.4 GHz Wi-Fi network.", size=8, color=DIM)


def legend_row(c, x, y, color, label):
    c.setFillColor(color)
    c.circle(x + 4, y + 3, 4, fill=1, stroke=0)
    c.setFillColor(TEXT)
    c.setFont("Inter", 8.8)
    c.drawString(x + 13, y, label)


def back(c):
    background(c)
    y = H - M - 20
    c.setFont("Inter-Bold", 15)
    c.setFillColor(TEXT)
    c.drawString(M, y, "Using your radar")
    y -= 12

    rows = [
        ("Tap a plane", "Photo, tail number, type, airline and route"),
        ("+  /  −", "Zoom from 3 to 50 miles"),
        ("Gear", "Settings: location, Wi-Fi, colors, units, brightness"),
        ("Tap the weather", "Refresh it now"),
        ("Bottom-right", "Green dot = live. Tap it if it says No Wi-Fi"),
    ]
    for k, v in rows:
        y -= 23
        c.setFillColor(PANEL)
        c.roundRect(M, y - 6, W - 2 * M, 19, 4, fill=1, stroke=0)
        c.setFont("Inter-Semi", 9)
        c.setFillColor(AMBER)
        c.drawString(M + 8, y, k)
        c.setFont("Inter", 8.8)
        c.setFillColor(TEXT)
        c.drawString(M + 1.25 * inch, y, v)

    y -= 32
    c.setFont("Inter-Bold", 11)
    c.setFillColor(TEXT)
    c.drawString(M, y, "What the colors mean")
    y -= 17
    half = (W - 2 * M) / 2
    legend_row(c, M, y, AMBER, "Airliners and other traffic")
    legend_row(c, M + half, y, LAVENDER, "Military (color is adjustable)")
    y -= 15
    legend_row(c, M, y, RED, "Emergency squawk")
    legend_row(c, M + half, y, CYAN, "The plane you tapped")

    y -= 28
    c.setFont("Inter-Bold", 11)
    c.setFillColor(TEXT)
    c.drawString(M, y, "Good to know")
    y -= 15
    tips = [
        "The radar is centered on your ZIP code, north is up, and the outer ring is the range "
        "in the top-left corner.",
        "Too bright at bedtime? Settings → Display → Night dimming.",
        "On Christmas Eve, keep an eye out for a very special sleigh.",
        "Make it yours: Classic, Amber or Holiday themes (with snow), metric or aviation units, "
        "flight trails, and the color of military planes.",
        "Turn on the alert chime to hear when a plane flies low overhead or squawks an emergency.",
        "To start over (e.g. a new home or Wi-Fi), hold the board's BOOT button for 5 seconds.",
    ]
    for t in tips:
        c.setFillColor(AMBER)
        c.circle(M + 3, y + 3, 1.8, fill=1, stroke=0)
        y = para(c, M + 11, y, t, size=8.8, color=TEXT, width=W - 2 * M - 11) - 7

    # footer credits
    c.setStrokeColor(BORDER)
    c.setLineWidth(0.6)
    c.line(M, M + 26, W - M, M + 26)
    para(c, M, M + 15, "Aircraft data: adsb.fi · Weather: Open-Meteo.com · Photos: planespotters.net "
         "· Routes: adsbdb.com", size=6.6, color=DIM, leading=8.5)


def main():
    c = canvas.Canvas(OUT, pagesize=(W, H))
    c.setTitle("Desk Flight Radar: quick start")
    c.setAuthor("Desk Flight Radar")
    front(c)
    c.showPage()
    back(c)
    c.showPage()
    c.save()
    print("wrote", OUT)


if __name__ == "__main__":
    main()
