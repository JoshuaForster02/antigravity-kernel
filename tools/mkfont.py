#!/usr/bin/env python3
"""Erzeugt src/font_data.c: 8-Bit-Alpha-Glyphen (ASCII 32-126) aus DejaVu-TTFs.
Nur bei Schrift-Aenderungen noetig; das Ergebnis ist eingecheckt, der Kernel-Build braucht kein Python/PIL.
DejaVu-Schriften: Bitstream Vera / DejaVu License (frei, abgeleitete Bitmaps erlaubt)."""
from PIL import Image, ImageDraw, ImageFont
FONTS = [  # C-Name, Datei, Pixelgroesse
    ("font_mono", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14),
    ("font_ui",   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14),
    ("font_bold", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 12),
    ("font_big",  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 44),
]
out = ['/* GENERIERT von tools/mkfont.py — nicht von Hand bearbeiten. Glyphen: DejaVu (Bitstream Vera License). */',
       '#include "font.h"', '']
for name, path, size in FONTS:
    f = ImageFont.truetype(path, size)
    asc, desc = f.getmetrics(); h = asc + desc
    offs, advs, data = [], [], []
    for c in range(32, 127):
        ch = chr(c); adv = max(1, round(f.getlength(ch)))
        im = Image.new("L", (adv, h), 0); ImageDraw.Draw(im).text((0, asc), ch, font=f, fill=255, anchor="ls")
        offs.append(len(data)); advs.append(adv); data += list(im.tobytes())
    out.append(f'static const unsigned char {name}_px[{len(data)}] = {{')
    for i in range(0, len(data), 32): out.append('  ' + ','.join(map(str, data[i:i+32])) + ',')
    out.append('};')
    out.append(f'static const unsigned int {name}_off[95] = {{{",".join(map(str, offs))}}};')
    out.append(f'static const unsigned char {name}_adv[95] = {{{",".join(map(str, advs))}}};')
    out.append(f'const struct font {name} = {{ {h}, {asc}, {name}_adv, {name}_off, {name}_px }};\n')
open("src/font_data.c", "w").write("\n".join(out) + "\n")
print("ok", [(n, s) for n, _, s in FONTS])
