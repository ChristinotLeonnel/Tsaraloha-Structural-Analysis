#!/usr/bin/env python3
"""Icône de l'exécutable (resources/icons/TSA.ico, docs/ICONS.md) à partir du glyphe officiel du produit.

  python tools/icons/make_app_ico.py            # écrit resources/icons/TSA.ico (et la copie resources/TSA.ico)
  python tools/icons/make_app_ico.py --png DIR  # écrit aussi chaque taille en PNG dans DIR (contrôle visuel)

Style plat : carré arrondi uni (#1F2328), glyphe en blanc plein, sans dégradé ni contour. L'icône s'affiche sur la barre
des tâches et le Bureau, clairs ou sombres selon Windows : le carré la rend visible sur fond clair, le T blanc sur fond
sombre. Avant : carré en dégradé et T gris #D3D7DA à contour noir, flou aux petites tailles.
Rendu sans bibliothèque externe : polygones du glyphe (resources/icons/TSA_glyph.svg, viewBox 512) rastérisés avec un
suréchantillonnage 4 × 4, PNG écrits avec zlib, ICO à images PNG (Windows Vista et suivants). Fichier versionné :
relancer ce script après une modification du glyphe.
"""
import os
import re
import struct
import sys
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GLYPH = os.path.join(ROOT, "resources", "icons", "TSA_glyph.svg")
OUTPUTS = [os.path.join(ROOT, "resources", "icons", "TSA.ico"), os.path.join(ROOT, "resources", "TSA.ico")]
SIZES = [16, 24, 32, 48, 64, 128, 256]
BACKGROUND = (0x1F, 0x23, 0x28)
FOREGROUND = (0xFF, 0xFF, 0xFF)
CORNER = 112.0 / 512.0      # rayon du carré arrondi, comme TSA.svg
SAMPLES = 4                 # suréchantillonnage par axe


def glyph_polygons():
    svg = open(GLYPH, encoding="utf-8").read()
    polys = []
    for pts in re.findall(r'points="([^"]+)"', svg):
        coords = [float(v) for v in re.split(r"[ ,]+", pts.strip())]
        polys.append([(coords[i] / 512.0, coords[i + 1] / 512.0) for i in range(0, len(coords), 2)])
    if not polys:
        sys.exit("aucun polygone dans " + GLYPH)
    return polys


def inside_polygon(x, y, poly):
    hit = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            hit = not hit
        j = i
    return hit


def inside_rounded_square(x, y):
    r = CORNER
    cx = min(max(x, r), 1 - r)
    cy = min(max(y, r), 1 - r)
    return 0 <= x <= 1 and 0 <= y <= 1 and (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def render(size, polys):
    """Pixels RGBA (non prémultipliés) d'une taille donnée."""
    out = bytearray()
    n = SAMPLES * SAMPLES
    # Petites tailles : le glyphe occupe un peu plus de place pour rester lisible (marge réduite autour du T).
    scale = 1.08 if size <= 24 else 1.0
    boxes = [(min(p[0] for p in poly), max(p[0] for p in poly), min(p[1] for p in poly), max(p[1] for p in poly)) for poly in polys]
    for py in range(size):
        for px in range(size):
            bg = fg = 0
            for sy in range(SAMPLES):
                for sx in range(SAMPLES):
                    x = (px + (sx + 0.5) / SAMPLES) / size
                    y = (py + (sy + 0.5) / SAMPLES) / size
                    if not inside_rounded_square(x, y):
                        continue
                    bg += 1
                    gx = 0.5 + (x - 0.5) / scale
                    gy = 0.5 + (y - 0.5) / scale
                    for poly, (x0, x1, y0, y1) in zip(polys, boxes):
                        if x0 <= gx <= x1 and y0 <= gy <= y1 and inside_polygon(gx, gy, poly):
                            fg += 1
                            break
            alpha = bg / n
            if alpha == 0:
                out += b"\x00\x00\x00\x00"
                continue
            t = fg / bg
            rgb = [round(BACKGROUND[k] * (1 - t) + FOREGROUND[k] * t) for k in range(3)]
            out += bytes(rgb + [round(alpha * 255)])
    return bytes(out)


def png(size, rgba):
    rows = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4] for y in range(size))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def ico(images):
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    data = b""
    for size, blob in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def main():
    png_dir = None
    if "--png" in sys.argv:
        png_dir = sys.argv[sys.argv.index("--png") + 1]
        os.makedirs(png_dir, exist_ok=True)
    polys = glyph_polygons()
    images = []
    for size in SIZES:
        blob = png(size, render(size, polys))
        images.append((size, blob))
        if png_dir:
            with open(os.path.join(png_dir, "TSA_%d.png" % size), "wb") as f:
                f.write(blob)
    data = ico(images)
    for path in OUTPUTS:
        with open(path, "wb") as f:
            f.write(data)
        print("écrit :", os.path.relpath(path, ROOT), len(data), "octets,", len(images), "images")


if __name__ == "__main__":
    main()
