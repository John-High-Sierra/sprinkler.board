#!/usr/bin/env python3
"""Generate PWA icons and embed all PWA assets into pwa_content.h.
Run from repo root: python tools/gen_icons.py"""
try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Run: pip install Pillow")
    raise SystemExit(1)

import math, os

DATA_DIR = os.path.join("esp32_firmware", "sprinkler_controller", "data")
FW_DIR   = os.path.join("esp32_firmware", "sprinkler_controller")
TEAL     = (13, 148, 136, 255)
WHITE    = (255, 255, 255, 255)

def teardrop(cx, cy, rx, ry):
    pts = []
    for i in range(31):
        a = math.pi + math.pi * i / 30
        pts.append((cx + rx * math.cos(a),
                    cy + ry * 0.3 + ry * 0.7 * math.sin(a)))
    pts.append((cx, cy - ry * 0.7))
    return pts

def make_icon(size):
    img  = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    m    = max(1, size // 12)
    draw.ellipse([m, m, size - m, size - m], fill=TEAL)
    draw.polygon(teardrop(size / 2, size * 0.54,
                          size * 0.22, size * 0.30), fill=WHITE)
    return img

os.makedirs(DATA_DIR, exist_ok=True)
for sz in (192, 512):
    path = os.path.join(DATA_DIR, f"icon-{sz}.png")
    make_icon(sz).save(path, "PNG")
    print(f"  Wrote {path}")

def text_progmem(name, text):
    return f'static const char {name}[] PROGMEM = R"====(\n{text}\n)====";'

def bytes_progmem(name, data):
    cols = 16
    rows = []
    for i in range(0, len(data), cols):
        rows.append('  ' + ', '.join(f'0x{b:02x}' for b in data[i:i+cols]))
    return (f"static const uint8_t {name}[] PROGMEM = {{\n"
            + ',\n'.join(rows)
            + f"\n}};\n")

with open(os.path.join(DATA_DIR, "manifest.json"), "r", encoding="utf-8") as f:
    manifest = f.read()
with open(os.path.join(DATA_DIR, "sw.js"), "r", encoding="utf-8") as f:
    swjs = f.read()
with open(os.path.join(DATA_DIR, "icon-192.png"), "rb") as f:
    icon192 = f.read()
with open(os.path.join(DATA_DIR, "icon-512.png"), "rb") as f:
    icon512 = f.read()

out  = "// Auto-generated — do not edit. Run tools/gen_icons.py to regenerate.\n\n"
out += text_progmem("MANIFEST_JSON", manifest) + "\n\n"
out += text_progmem("SW_JS", swjs) + "\n\n"
out += bytes_progmem("ICON_192_PNG", icon192) + "\n"
out += bytes_progmem("ICON_512_PNG", icon512) + "\n"

header_path = os.path.join(FW_DIR, "pwa_content.h")
with open(header_path, "w", encoding="utf-8") as f:
    f.write(out)
print(f"  Wrote {header_path}  ({len(icon192)}+{len(icon512)} bytes of PNG)")
print("Done.")
