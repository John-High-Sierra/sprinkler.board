#!/usr/bin/env python3
"""Generate PWA icons for SprinKlr-8. Run from repo root: python tools/gen_icons.py"""
try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Run: pip install Pillow")
    raise SystemExit(1)

import math, os

OUT_DIR = os.path.join("esp32_firmware", "sprinkler_controller", "data")
TEAL    = (13, 148, 136, 255)   # #0d9488
WHITE   = (255, 255, 255, 255)

def teardrop(cx, cy, rx, ry):
    """Polygon points: teardrop with tip at top, round base at bottom."""
    pts = []
    for i in range(31):                              # bottom arc, left to right
        a = math.pi + math.pi * i / 30
        pts.append((cx + rx * math.cos(a),
                    cy + ry * 0.3 + ry * 0.7 * math.sin(a)))
    pts.append((cx, cy - ry * 0.7))                 # tip
    return pts

def make_icon(size):
    img  = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    m    = max(1, size // 12)
    draw.ellipse([m, m, size - m, size - m], fill=TEAL)
    draw.polygon(teardrop(size / 2, size * 0.54,
                          size * 0.22, size * 0.30), fill=WHITE)
    return img

os.makedirs(OUT_DIR, exist_ok=True)
for sz in (192, 512):
    path = os.path.join(OUT_DIR, f"icon-{sz}.png")
    make_icon(sz).save(path, "PNG")
    print(f"  Wrote {path}")
print("Done.")
