import math
import os
from PIL import Image, ImageDraw

def render_tsa_icon(size=512, background=True, dark_mode=True, monochrome=False):
    # Rendu haute définition par supersampling 4x pour un anti-aliasing parfait
    scale = 4
    canvas_size = size * scale
    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    s = canvas_size / 512.0

    # 1. Fond squircle (optionnel)
    if background:
        rx = int(112 * s)
        if monochrome:
            bg_color = (0, 0, 0, 255) if dark_mode else (255, 255, 255, 255)
        elif dark_mode:
            # Fond sombre dégradé bleu nuit high-tech
            bg_color = (8, 13, 26, 255)
        else:
            # Fond clair platinum / aluminium
            bg_color = (248, 250, 252, 255)
        
        # Dessin du rectangle arrondi
        draw.rounded_rectangle([0, 0, canvas_size - 1, canvas_size - 1], radius=rx, fill=bg_color)
        
        # Fin liseré de contour (subtile border)
        border_color = (255, 255, 255, 25) if dark_mode else (0, 0, 0, 20)
        draw.rounded_rectangle([0, 0, canvas_size - 1, canvas_size - 1], radius=rx, outline=border_color, width=max(1, int(2 * s)))

    # Coordonnées des polygones (T-Vector Triad)
    # Aile Gauche (Vecteur X / Cisaillement)
    poly_left = [
        (96 * s, 140 * s),
        (244 * s, 140 * s),
        (244 * s, 230 * s),
        (154 * s, 230 * s),
        (96 * s, 172 * s)
    ]

    # Aile Droite (Vecteur Y / Moment)
    poly_right = [
        (268 * s, 140 * s),
        (416 * s, 140 * s),
        (416 * s, 172 * s),
        (358 * s, 230 * s),
        (268 * s, 230 * s)
    ]

    # Fût Central (Vecteur Z / Ancrage & Normal)
    poly_stem = [
        (226 * s, 254 * s),
        (286 * s, 254 * s),
        (276 * s, 400 * s),
        (256 * s, 424 * s),
        (236 * s, 400 * s)
    ]

    # Nœud Central Nexus (Losange d'articulation)
    poly_nexus = [
        (256 * s, 222 * s),
        (266 * s, 236 * s),
        (256 * s, 250 * s),
        (246 * s, 236 * s)
    ]

    if monochrome:
        c_fill = (255, 255, 255, 255) if dark_mode else (15, 23, 42, 255)
        draw.polygon(poly_left, fill=c_fill)
        draw.polygon(poly_right, fill=c_fill)
        draw.polygon(poly_stem, fill=c_fill)
        # Nœud inversé
        c_nexus = (8, 13, 26, 255) if dark_mode else (255, 255, 255, 255)
        draw.polygon(poly_nexus, fill=c_nexus)
    else:
        if dark_mode:
            # Couleurs riches tech : Cyan électrique vers Cobalt
            # On applique des dégradés ou teintes contrastées pour chaque facette 3D
            # Aile gauche : Cyan vibrant
            draw.polygon(poly_left, fill=(0, 210, 255, 255))
            # Aile droite : Cobalt profond
            draw.polygon(poly_right, fill=(0, 102, 255, 255))
            # Fût vertical : Indigo technique
            draw.polygon(poly_stem, fill=(0, 140, 255, 255))
            # Nœud central : Blanc pur éclatant
            draw.polygon(poly_nexus, fill=(255, 255, 255, 255))
        else:
            # Couleurs mode clair
            draw.polygon(poly_left, fill=(0, 150, 240, 255))
            draw.polygon(poly_right, fill=(0, 80, 210, 255))
            draw.polygon(poly_stem, fill=(0, 110, 230, 255))
            draw.polygon(poly_nexus, fill=(15, 23, 42, 255))

    # Réduction Lanczos pour anti-aliasing haute qualité
    final_img = img.resize((size, size), Image.Resampling.LANCZOS)
    return final_img

# Test de génération
out_dir = r"e:\Book\Dev\TSA\docs\identity\renders"
os.makedirs(out_dir, exist_ok=True)

# 1. Dark App Icon (Badge)
img_dark = render_tsa_icon(512, background=True, dark_mode=True)
img_dark.save(os.path.join(out_dir, "tsa_icon_dark_512.png"))

# 2. Light App Icon
img_light = render_tsa_icon(512, background=True, dark_mode=False)
img_light.save(os.path.join(out_dir, "tsa_icon_light_512.png"))

# 3. Transparent Glyph
img_glyph = render_tsa_icon(512, background=False, dark_mode=True)
img_glyph.save(os.path.join(out_dir, "tsa_glyph_transparent_512.png"))

# 4. Monochrome
img_mono = render_tsa_icon(512, background=False, dark_mode=False, monochrome=True)
img_mono.save(os.path.join(out_dir, "tsa_glyph_monochrome_512.png"))

print("Renders generated successfully in", out_dir)
