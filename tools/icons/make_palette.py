#!/usr/bin/env python3
"""Palettes des icônes par thème (docs/ICONS.md) → resources/icons/palette.json.

  python tools/icons/make_palette.py [--check]

Les icônes SVG sont dessinées pour le thème sombre. Pour chaque couleur employée dans resources/icons/**.svg, le contraste
avec le fond de chaque thème est calculé (WCAG 2.1) ; une couleur de trait sous 3:1 (minimum des éléments graphiques)
est assombrie (thème clair) ou éclaircie (thème sombre) en conservant sa teinte, jusqu'à 3,2:1. Les aplats très clairs
(surfaces) et le blanc pur des remplissages suivent la même règle. Le moteur d'icônes de TSA (UI/Theme/ThemedIcon)
applique la table du thème actif au rendu. --check : code 1 si palette.json n'est plus à jour.
"""
import colorsys
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ICONS = os.path.join(ROOT, "resources", "icons")
# Fonds du ruban et des panneaux (ThemeManager) : normal, survol, bouton actif
BACKGROUNDS = {"dark": ["#161B22", "#212830", "#2D333B", "#1F3A5A"], "light": ["#FFFFFF", "#F6F8FA", "#EAEEF2", "#DDF4FF"]}
TARGET = 3.2
TAG = re.compile(r"<(?:svg|g|path|line|polyline|polygon|circle|ellipse|rect|text)[\s>/][^>]*>", re.S)
COLOR = re.compile(r'(?:stroke|fill|stop-color|color)\s*[=:]\s*"?\s*(#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3})\b')


def rgb(h):
    h = h.lstrip("#")
    if len(h) == 3:
        h = "".join(c * 2 for c in h)
    return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))


def hexa(c):
    return "#" + "".join(f"{round(v * 255):02X}" for v in c)


def luminance(c):
    lin = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in c]
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def worst(color, backgrounds):
    return min(contrast(color, b) for b in backgrounds)


def adjust(color, backgrounds, darken):
    h, l, s = colorsys.rgb_to_hls(*color)
    for _ in range(200):
        if worst(colorsys.hls_to_rgb(h, l, s), backgrounds) >= TARGET:
            break
        l = max(0.0, l - 0.01) if darken else min(1.0, l + 0.01)
    return colorsys.hls_to_rgb(h, l, s)


def build():
    # Couleurs qui portent la forme : traits, et remplissages sans contour (pointes de flèche, points). Un aplat
    # entouré d'un trait n'a pas besoin de contraste avec le fond.
    colors = set()
    for d, _, files in os.walk(ICONS):
        for fn in files:
            if fn.endswith(".svg"):
                text = open(os.path.join(d, fn), encoding="utf-8", errors="replace").read()
                for tag in TAG.findall(text):
                    s = re.search(r'stroke\s*=\s*"(#[0-9a-fA-F]{3,6})"', tag)
                    f = re.search(r'fill\s*=\s*"(#[0-9a-fA-F]{3,6})"', tag)
                    if s:
                        colors.add(hexa(rgb(s.group(1))))
                    if f and not s:
                        colors.add(hexa(rgb(f.group(1))))
    palette = {}
    for theme, bg_hexes in BACKGROUNDS.items():
        bgs = [rgb(b) for b in bg_hexes]
        table = {}
        for c in sorted(colors):
            if worst(rgb(c), bgs) < 3.0:
                table[c] = hexa(adjust(rgb(c), bgs, darken=(theme == "light")))
        palette[theme] = table
    palette["_comment"] = ("Généré par tools/icons/make_palette.py : couleurs des icônes remplacées selon le thème "
                           "(contraste >= 3:1 avec le fond du ruban). Ne pas modifier à la main.")
    return palette


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    palette = build()
    path = os.path.join(ICONS, "palette.json")
    text = json.dumps(palette, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
    if "--check" in sys.argv:
        current = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
        if current != text:
            print("resources/icons/palette.json n'est plus à jour : python tools/icons/make_palette.py")
            return 1
        print("palette.json à jour")
        return 0
    open(path, "w", encoding="utf-8", newline="\n").write(text)
    print(f"palette.json : {len(palette['light'])} couleur(s) adaptée(s) au thème clair, {len(palette['dark'])} au thème sombre")
    return 0


if __name__ == "__main__":
    sys.exit(main())
