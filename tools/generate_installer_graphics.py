#!/usr/bin/env python3
"""Génère les images BMP 24 bits officielles pour l'installateur Windows Inno Setup de TSA.
Intègre le véritable logo TSA avec son symbole « T » officiel (issu de resources/branding/TSA_Logo_Dark.png).

- wizard_banner.bmp : 164 x 314 px (panneau gauche des écrans d'accueil et de fin d'installation)
- wizard_small.bmp  : 55 x 58 px (en-tête supérieur droit des pages de l'assistant)
"""

import os
import sys
from PIL import Image, ImageDraw, ImageFont

def get_script_dir():
    return os.path.dirname(os.path.abspath(__file__))

def get_root_dir():
    return os.path.abspath(os.path.join(get_script_dir(), ".."))

def generate_graphics(output_dir=None):
    root = get_root_dir()
    if output_dir is None:
        output_dir = os.path.join(root, "packaging")
    os.makedirs(output_dir, exist_ok=True)

    logo_path = os.path.join(root, "resources", "branding", "TSA_Logo_Dark.png")
    if not os.path.exists(logo_path):
        raise FileNotFoundError(f"Logo officiel TSA introuvable : {logo_path}")

    official_logo = Image.open(logo_path).convert("RGBA")

    # Fonts Windows standard
    font_bold_path = r"C:\Windows\Fonts\segoeuib.ttf"
    font_reg_path = r"C:\Windows\Fonts\segoeui.ttf"

    font_title = ImageFont.truetype(font_bold_path, 26) if os.path.exists(font_bold_path) else ImageFont.load_default()
    font_sub = ImageFont.truetype(font_bold_path, 12) if os.path.exists(font_bold_path) else ImageFont.load_default()
    font_detail = ImageFont.truetype(font_reg_path, 10) if os.path.exists(font_reg_path) else ImageFont.load_default()
    font_pill = ImageFont.truetype(font_bold_path, 9) if os.path.exists(font_bold_path) else ImageFont.load_default()

    # =========================================================================
    # 1. wizard_banner.bmp (164 x 314 px)
    # =========================================================================
    w, h = 164, 314
    banner = Image.new("RGB", (w, h), (8, 13, 26))
    draw = ImageDraw.Draw(banner)

    # Dégradé vertical bleu nuit profond (#0A1124 -> #04060C)
    for y in range(h):
        t = y / float(h)
        r = int(10 * (1 - t) + 4 * t)
        g = int(17 * (1 - t) + 6 * t)
        b = int(36 * (1 - t) + 12 * t)
        draw.line([(0, y), (w, y)], fill=(r, g, b))

    # Grille subtile CAD d'ingénierie (espacement 20 px)
    for x in range(0, w, 20):
        draw.line([(x, 0), (x, h)], fill=(18, 30, 52))
    for y in range(0, h, 20):
        draw.line([(0, y), (w, y)], fill=(18, 30, 52))

    # Ligne verticale d'accent cyan sur le bord droit
    draw.line([(w - 1, 0), (w - 1, h)], fill=(0, 242, 254))

    # Véritable Logo TSA avec symbole « T » (114 x 114 px)
    logo_size = 114
    logo_resized = official_logo.resize((logo_size, logo_size), Image.Resampling.LANCZOS)
    logo_x = (w - logo_size) // 2
    logo_y = 26
    banner.paste(logo_resized, (logo_x, logo_y), logo_resized)

    # Titre "TSA" en cyan officiel (#00F2FE)
    title_text = "TSA"
    bbox_title = draw.textbbox((0, 0), title_text, font=font_title)
    tw = bbox_title[2] - bbox_title[0]
    draw.text(((w - tw) // 2, 148), title_text, fill=(0, 242, 254), font=font_title)

    # Double ligne d'accentuation structurelle
    draw.line([(28, 184), (w - 28, 184)], fill=(0, 114, 255), width=2)
    draw.line([(50, 184), (w - 50, 184)], fill=(0, 242, 254), width=2)

    # Nom complet du logiciel
    name1 = "Tsaraloha"
    bbox1 = draw.textbbox((0, 0), name1, font=font_sub)
    draw.text(((w - (bbox1[2] - bbox1[0])) // 2, 194), name1, fill=(241, 245, 249), font=font_sub)

    name2 = "Structural Analysis"
    bbox2 = draw.textbbox((0, 0), name2, font=font_detail)
    draw.text(((w - (bbox2[2] - bbox2[0])) // 2, 212), name2, fill=(148, 163, 184), font=font_detail)

    # Badges d'ingénierie (3D CAD, FEA SOLVER, BIM IFC)
    pills = ["3D CAD", "FEA SOLVER", "BIM IFC"]
    py = 236
    for p in pills:
        bbox_p = draw.textbbox((0, 0), p, font=font_pill)
        pw = bbox_p[2] - bbox_p[0] + 16
        px = (w - pw) // 2
        draw.rounded_rectangle([(px, py), (px + pw, py + 16)], radius=4, fill=(15, 23, 42), outline=(51, 65, 85))
        draw.text((px + 8, py + 2), p, fill=(56, 189, 248), font=font_pill)
        py += 22

    banner_bmp_path = os.path.join(output_dir, "wizard_banner.bmp")
    banner.save(banner_bmp_path, format="BMP")
    print(f"[OK] Genere : {banner_bmp_path} (164x314, logo TSA 'T' officiel)")

    # =========================================================================
    # 2. wizard_small.bmp (55 x 58 px)
    # =========================================================================
    sw, sh = 55, 58
    # Fond blanc pur (#FFFFFF) adapte a l'en-tete moderne Inno Setup
    small = Image.new("RGB", (sw, sh), (255, 255, 255))
    s_size = 48
    s_logo = official_logo.resize((s_size, s_size), Image.Resampling.LANCZOS)
    sx = (sw - s_size) // 2
    sy = (sh - s_size) // 2
    small.paste(s_logo, (sx, sy), s_logo)

    small_bmp_path = os.path.join(output_dir, "wizard_small.bmp")
    small.save(small_bmp_path, format="BMP")
    print(f"[OK] Genere : {small_bmp_path} (55x58, logo TSA 'T' officiel)")

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else None
    generate_graphics(out)
