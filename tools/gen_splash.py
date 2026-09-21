#!/usr/bin/env python3
"""Generate the title bitmap (512x252 monochrome) as RLE data in src/splash.h.

The picture is designed on a 1536x1260 canvas (the CRT's 3:5 dot pitch) and
reduced to dots. Motif: the Evoluon, Philips' saucer-shaped 1966 exhibition
building in Eindhoven -- itself a giant disc -- under a night sky in which the
moon is an Othello ring, with the title in the terminal's own character-ROM
glyphs scaled up. Also writes build/splash.png, a preview rendered like the
game screenshots.
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
FONT_SHEET = ROOT.parent / "p2000c-emulator/assets/font/P2000C font mini.png"
W, H = 512, 252
SX, SY = 3, 5                     # dot pitch
CW, CH = W * SX, H * SY


def glyph_rows(sheet, code):
    return [[sheet.getpixel(((code & 15) * 12 + gx, (code >> 4) * 12 + gy))[1] != 0
             for gx in range(8)] for gy in range(12)]


def draw_text(dots, sheet, text, x, y, scale_x, scale_y, spacing=0):
    """Draws ROM glyphs into the dot image (1 = lit), scaled per axis."""
    for ch in text:
        rows = glyph_rows(sheet, ord(ch))
        for gy in range(12):
            for gx in range(8):
                if rows[gy][gx]:
                    for dy in range(scale_y):
                        for dx in range(scale_x):
                            px, py = x + gx * scale_x + dx, y + gy * scale_y + dy
                            if 0 <= px < W and 0 <= py < H:
                                dots.putpixel((px, py), 1)
        x += 8 * scale_x + spacing


DOT = ((0, 1, 1, 1, 0), (1, 1, 1, 1, 1), (0, 1, 1, 1, 0))   # a 5x3-dot disc, round on the CRT


def draw_text_discs(dots, sheet, text, x, y, pitch_x=6, pitch_y=4):
    """Dot-matrix lettering: every lit glyph pixel becomes a small disc."""
    for ch in text:
        rows = glyph_rows(sheet, ord(ch))
        for gy in range(12):
            for gx in range(8):
                if rows[gy][gx]:
                    for dy, row in enumerate(DOT):
                        for dx, on in enumerate(row):
                            if on:
                                px, py = x + gx * pitch_x + dx, y + gy * pitch_y + dy
                                if 0 <= px < W and 0 <= py < H:
                                    dots.putpixel((px, py), 1)
        x += 8 * pitch_x


def D(x, y):
    """Dot coordinates -> canvas coordinates."""
    return (x * SX, y * SY)


def reduce_to_dots(canvas):
    """A dot is lit when enough of its 3x5 canvas block is."""
    dots = Image.new("1", (W, H), 0)
    px = canvas.load()
    for y in range(H):
        for x in range(W):
            s = 0
            for dy in range(SY):
                for dx in range(SX):
                    s += px[x * SX + dx, y * SY + dy]
            if s >= 255 * 5:
                dots.putpixel((x, y), 1)
    return dots


def stars(d, points):
    for (sx_, sy_) in points:
        d.rectangle([*D(sx_, sy_), sx_ * SX + 3, sy_ * SY + 5], fill=255)


def disc(d, cx, cy, rx, ry, ring, lw=5):
    """Othello disc in dot coordinates (rx:ry = 5:3 is round on the CRT)."""
    box = [*D(cx - rx, cy - ry), *D(cx + rx, cy + ry)]
    if ring:
        d.ellipse(box, outline=255, width=lw * 3)
    else:
        d.ellipse(box, fill=255)


def design_evoluon():
    """A: the Evoluon as a wireframe under a night sky, the moon an Othello ring."""
    canvas = Image.new("L", (CW, CH), 0)
    d = ImageDraw.Draw(canvas)
    lw = 5                                       # ~1.7 dots wide, 1 dot tall

    # --- Evoluon (in dot coordinates): flat saucer, window band, ribs, struts
    cx, rim_y = 256, 156
    half_w, dome_h = 200, 34
    top = rim_y - dome_h
    # dome: upper half of an ellipse, with the lantern on top
    d.ellipse([*D(cx - half_w, top), *D(cx + half_w, top + 2 * dome_h)], outline=255, width=lw)
    d.rectangle([*D(cx - half_w - 2, rim_y + 1), *D(cx + half_w + 2, rim_y + 2 * dome_h + 2)], fill=0)
    d.rectangle([*D(cx - 10, top - 9), *D(cx + 10, top)], outline=255, width=lw)
    # a second, inner shell line gives the dome some volume
    d.ellipse([*D(cx - half_w + 30, top + 9), *D(cx + half_w - 30, top + 9 + 2 * (dome_h - 9))], outline=255, width=lw)
    d.rectangle([*D(cx - half_w, rim_y - 1), *D(cx + half_w, rim_y + 2 * dome_h)], fill=0)
    # window band: double rim line
    d.line([D(cx - half_w, rim_y), D(cx + half_w, rim_y)], fill=255, width=lw)
    d.line([D(cx - half_w + 6, rim_y + 5), D(cx + half_w - 6, rim_y + 5)], fill=255, width=lw)
    for x in range(cx - half_w + 12, cx + half_w - 6, 12):
        d.line([D(x, rim_y), D(x, rim_y + 5)], fill=255, width=lw)
    # underside: shallow cone to a flat base
    base_y, base_hw = rim_y + 26, 44
    d.line([D(cx - half_w, rim_y + 5), D(cx - base_hw, base_y)], fill=255, width=lw)
    d.line([D(cx + half_w, rim_y + 5), D(cx + base_hw, base_y)], fill=255, width=lw)
    d.line([D(cx - base_hw, base_y), D(cx + base_hw, base_y)], fill=255, width=lw)
    # V-struts from the underside to the ground, and the central core
    horizon = 214
    for k in range(-4, 5):
        xr = cx + k * 40
        d.line([D(xr - 14, base_y - 2), D(xr, horizon)], fill=255, width=lw)
        d.line([D(xr + 14, base_y - 2), D(xr, horizon)], fill=255, width=lw)
    d.rectangle([*D(cx - 18, base_y), *D(cx + 18, horizon)], outline=255, width=lw)
    # ground and a dashed reflection of the rim in the pond
    d.line([D(0, horizon), D(W, horizon)], fill=255, width=lw)
    for x in range(cx - half_w, cx + half_w, 16):
        d.line([D(x, horizon + 7), D(x + 8, horizon + 7)], fill=255, width=lw)
    for x in range(cx - half_w + 40, cx + half_w - 40, 16):
        d.line([D(x + 4, horizon + 12), D(x + 10, horizon + 12)], fill=255, width=lw)

    # --- moon as an Othello ring, and stars (dot coordinates)
    mx, my, rx, ry = 448, 100, 20, 12                 # 5:3 so the ring is round on the CRT
    d.ellipse([*D(mx - rx, my - ry), *D(mx + rx, my + ry)], outline=255, width=lw * 3)
    for (sx_, sy_) in [(20, 30), (60, 78), (110, 100), (300, 110), (396, 62), (490, 40),
                       (470, 140), (40, 130), (140, 62), (350, 88), (500, 180), (20, 180)]:
        d.rectangle([*D(sx_, sy_), sx_ * SX + 3, sy_ * SY + 5], fill=255)

    dots = reduce_to_dots(canvas)
    sheet = Image.open(FONT_SHEET).convert("RGB")
    draw_text(dots, sheet, "OTHELLO", 88, 10, 6, 4)
    draw_text(dots, sheet, "PHILIPS P2000C", 144, 60, 2, 1)
    draw_text(dots, sheet, "EVOLUON - EINDHOVEN", 8, 236, 1, 1)
    draw_text(dots, sheet, "druk op een toets", 368, 236, 1, 1)
    return dots


def design_skyline():
    """B: Eindhoven at night -- Lichttoren with its beacon, Witte Dame, the
    Evoluon in the distance -- and a board row of discs as the ground."""
    canvas = Image.new("L", (CW, CH), 0)
    d = ImageDraw.Draw(canvas)
    lw = 5
    ground = 196
    d.line([D(0, ground), D(W, ground)], fill=255, width=lw)
    # Evoluon far away on the left
    ex, ey = 60, 176
    # Witte Dame: long low block with rows of windows
    d.rectangle([*D(120, 150), *D(330, ground)], outline=255, width=lw)
    for wy in (158, 170, 182):
        for wx in range(128, 326, 12):
            d.rectangle([*D(wx, wy), *D(wx + 5, wy + 4)], fill=255)
    # Lichttoren: tower with the glass lantern on top
    d.rectangle([*D(340, 96), *D(400, ground)], outline=255, width=lw)
    d.rectangle([*D(336, 84), *D(404, 96)], outline=255, width=lw)
    d.rectangle([*D(352, 70), *D(388, 84)], outline=255, width=lw)
    for wy in range(104, ground - 8, 12):
        for wx in (350, 365, 380):
            d.rectangle([*D(wx, wy), *D(wx + 5, wy + 4)], fill=255)
    for wx in range(356, 386, 8):
        d.rectangle([*D(wx, 74), *D(wx + 3, 80)], fill=255)
    # beacon sweeping up and to the right, clear of the title
    d.line([D(370, 70), D(500, 4)], fill=255, width=lw)
    d.line([D(370, 70), D(511, 30)], fill=255, width=lw)
    # low wing to the right
    d.rectangle([*D(400, 160), *D(500, ground)], outline=255, width=lw)
    for wy in (168, 180):
        for wx in range(408, 496, 12):
            d.rectangle([*D(wx, wy), *D(wx + 5, wy + 4)], fill=255)
    d.ellipse([*D(ex - 44, ey - 12), *D(ex + 44, ey + 12)], outline=255, width=lw)
    d.rectangle([*D(ex - 46, ey), *D(ex + 46, ey + 14)], fill=0)
    d.line([D(ex - 44, ey), D(ex + 44, ey)], fill=255, width=lw)
    d.line([D(ex - 44, ey), D(ex - 8, ey + 10)], fill=255, width=lw)
    d.line([D(ex + 44, ey), D(ex + 8, ey + 10)], fill=255, width=lw)
    for k in (-24, -8, 8, 24):
        d.line([D(ex + k, ey + 10), D(ex + k, ground)], fill=255, width=lw)
    stars(d, [(20, 60), (60, 90), (30, 130), (200, 110), (250, 90), (460, 110), (500, 140), (12, 20), (120, 120), (300, 120)])
    # a board row of discs below the ground
    for i in range(8):
        disc(d, 64 + i * 56, 222, 15, 9, ring=(i % 2 == 0), lw=4)
    dots = reduce_to_dots(canvas)
    sheet = Image.open(FONT_SHEET).convert("RGB")
    draw_text(dots, sheet, "OTHELLO", 88, 10, 6, 4)
    draw_text(dots, sheet, "PHILIPS P2000C", 40, 52, 2, 1)
    draw_text(dots, sheet, "LICHTTOREN - EINDHOVEN", 8, 240, 1, 1)
    draw_text(dots, sheet, "druk op een toets", 368, 240, 1, 1)
    return dots


def design_perspective():
    """C: a board in perspective with discs, title with a drop shadow."""
    canvas = Image.new("L", (CW, CH), 0)
    d = ImageDraw.Draw(canvas)
    lw = 5
    vx, vy = 256, 60                             # vanishing point
    yb, yt = 240, 132                            # bottom and top edge of the board
    xb0, xb1 = 24, 488
    def edge_x(x_bottom, y):                     # x on the line from (x_bottom, yb) to the vanishing point
        return vx + (x_bottom - vx) * (y - vy) / (yb - vy)
    rows = [yt + (yb - yt) * (1 - (1 - t) ** 1.7) for t in [k / 8 for k in range(9)]]
    for y in rows:
        d.line([D(edge_x(xb0, y), y), D(edge_x(xb1, y), y)], fill=255, width=lw)
    for k in range(9):
        xb = xb0 + (xb1 - xb0) * k / 8
        d.line([D(edge_x(xb, yt), yt), D(xb, yb)], fill=255, width=lw)
    def square_center(col, row):
        y0, y1 = rows[row], rows[row + 1]
        yc = (y0 + y1) / 2
        xc = edge_x(xb0 + (xb1 - xb0) * (col + 0.5) / 8, yc)
        return xc, yc, (y1 - y0)
    for (col, row, ring) in [(3, 3, False), (4, 3, True), (3, 4, True), (4, 4, False), (2, 6, True), (5, 6, False), (1, 7, False), (6, 7, True)]:
        xc, yc, hgt = square_center(col, row)
        rx = hgt * 0.62 * 1.66
        disc(d, xc, yc, rx, hgt * 0.36, ring, lw=4)
    stars(d, [(30, 20), (90, 70), (420, 30), (470, 80), (500, 10), (200, 100), (330, 96)])
    dots = reduce_to_dots(canvas)
    sheet = Image.open(FONT_SHEET).convert("RGB")
    from PIL import ImageFilter
    letters = Image.new("1", (W, H), 0)
    draw_text(letters, sheet, "OTHELLO", 88, 10, 6, 4)
    shadow = Image.new("1", (W, H), 0)
    draw_text(shadow, sheet, "OTHELLO", 94, 14, 6, 4)
    halo = letters.convert("L").filter(ImageFilter.MaxFilter(5))
    for y in range(4, 56):
        for x in range(80, 448):
            if letters.getpixel((x, y)) or (shadow.getpixel((x, y)) and not halo.getpixel((x, y))):
                dots.putpixel((x, y), 1)
    draw_text(dots, sheet, "PHILIPS P2000C - EINDHOVEN", 48, 66, 2, 1)
    draw_text(dots, sheet, "druk op een toets", 368, 242, 1, 1)
    return dots


def design_sunset():
    """D: eighties sunset -- striped sun above a perspective grid floor."""
    canvas = Image.new("L", (CW, CH), 0)
    d = ImageDraw.Draw(canvas)
    lw = 5
    horizon = 170
    # striped sun
    sx, sy, rx, ry = 256, horizon, 175, 96
    d.ellipse([*D(sx - rx, sy - ry), *D(sx + rx, sy + ry)], fill=255)
    for k, yy in enumerate(range(horizon - 54, horizon, 6)):
        d.rectangle([*D(0, yy), *D(W, yy + 1 + k // 2)], fill=0)
    d.rectangle([*D(0, horizon), *D(W, H)], fill=0)
    # perspective grid floor
    d.line([D(0, horizon), D(W, horizon)], fill=255, width=lw)
    for k in range(-9, 10):
        d.line([D(256 + k * 24, horizon), D(256 + k * 110, H)], fill=255, width=lw)
    yy, step = horizon + 3, 3
    while yy < H:
        d.line([D(0, yy), D(W, yy)], fill=255, width=lw)
        step = step * 1.45 + 1
        yy += step
    dots = reduce_to_dots(canvas)
    sheet = Image.open(FONT_SHEET).convert("RGB")
    draw_text_discs(dots, sheet, "OTHELLO", 32, 6, 8, 5)
    draw_text(dots, sheet, "PHILIPS P2000C", 144, 58, 2, 1)
    # caption in a cleared strip at the bottom
    for y in range(232, 252):
        for x in range(W):
            dots.putpixel((x, y), 0)
    draw_text(dots, sheet, "druk op een toets", 368, 238, 1, 1)
    return dots


DESIGNS = {"evoluon": design_evoluon, "skyline": design_skyline,
           "perspective": design_perspective, "sunset": design_sunset}
compose = design_sunset


def to_bytes(dots):
    data = bytearray(W // 8 * H)
    for y in range(H):
        for x in range(W):
            if dots.getpixel((x, y)):
                data[y * 64 + x // 8] |= 0x80 >> (x % 8)
    return bytes(data)


def rle(data):
    """(count, value) pairs, count 1..255."""
    out = bytearray()
    i = 0
    while i < len(data):
        j = i
        while j < len(data) and data[j] == data[i] and j - i < 255:
            j += 1
        out += bytes([j - i, data[i]])
        i = j
    return bytes(out)


def preview(data, out):
    img = Image.new("L", (W, H), 0)
    img.putdata([255 if data[y * 64 + x // 8] & (0x80 >> (x % 8)) else 0 for y in range(H) for x in range(W)])
    img = img.resize((W * SX, H * SY), Image.NEAREST)
    Image.merge("RGB", [img.point(lambda v, c=c: c if v else 0) for c in (51, 255, 51)]).save(out)


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "--all":
        sheet_img = Image.new("RGB", (W * SX * 2 + 40, H * SY * 2 + 40), (40, 40, 40))
        for k, (name, fn) in enumerate(DESIGNS.items()):
            d_ = to_bytes(fn())
            preview(d_, ROOT / f"build/splash_{name}.png")
            im = Image.open(ROOT / f"build/splash_{name}.png")
            sheet_img.paste(im, ((k % 2) * (W * SX + 40), (k // 2) * (H * SY + 40)))
            print(f"{name:12s} RLE {len(rle(d_))} bytes, {sum(bin(b).count('1') for b in d_)} dots")
        sheet_img.resize((sheet_img.width // 2, sheet_img.height // 2), Image.LANCZOS).save(ROOT / "build/splash_options.png")
        sys.exit(0)
    if len(sys.argv) > 1:
        compose = DESIGNS[sys.argv[1]]
    data = to_bytes(compose())
    packed = rle(data)
    spans = sum(1 for y in range(H) if any(data[y * 64:(y + 1) * 64]))
    lit = sum(bin(b).count("1") for b in data)
    body = ",".join(f"0x{b:02X}" for b in packed)
    (ROOT / "src/splash.h").write_text(
        "/* Generated by tools/gen_splash.py -- do not edit. Title bitmap, 512x252,\n"
        " * run-length encoded as (count, value) pairs. */\n#ifndef SPLASH_H\n#define SPLASH_H\n\n"
        f"#define SPLASH_RLE_SIZE {len(packed)}\nstatic const unsigned char splash_rle[SPLASH_RLE_SIZE] = {{\n{body}\n}};\n\n#endif\n")
    (ROOT / "build").mkdir(exist_ok=True)
    preview(data, ROOT / "build/splash.png")
    print(f"splash: {lit} dots lit, {spans} non-empty lines, RLE {len(packed)} bytes; preview build/splash.png")
