import os
import math
import numpy as np
from PIL import Image, ImageDraw

def create_linear_gradient_mask(width, height, p1, p2, color1, color2):
    """Génère un gradient linéaire 2D RGBA interpolé entre p1(x,y) et p2(x,y)."""
    x1, y1 = p1
    x2, y2 = p2
    dx = x2 - x1
    dy = y2 - y1
    len_sq = dx * dx + dy * dy
    if len_sq == 0:
        len_sq = 1.0

    y_coords, x_coords = np.mgrid[0:height, 0:width]
    # Projection scalaire orthogonale
    proj = ((x_coords - x1) * dx + (y_coords - y1) * dy) / len_sq
    proj = np.clip(proj, 0.0, 1.0)

    r = color1[0] + proj * (color2[0] - color1[0])
    g = color1[1] + proj * (color2[1] - color1[1])
    b = color1[2] + proj * (color2[2] - color1[2])
    a = color1[3] + proj * (color2[3] - color1[3]) if len(color1) > 3 else 255.0

    grad_arr = np.dstack((r, g, b, a)).astype(np.uint8)
    return Image.fromarray(grad_arr, mode="RGBA")

def render_tsa_master(size=1024, mode="dark_badge", transparent_bg=False):
    """
    Rendu master supersamplé (4x) du symbole TSA.
    Modes disponibles :
    - 'dark_badge' : Fond sombre squircle navy titane + gradient cyan/cobalt
    - 'light_badge': Fond clair squircle platinum + gradient cobalt/sapphire
    - 'glyph_tech' : Sans fond (transparent) + gradient cyan/cobalt vibrant
    - 'monochrome_white': Sans fond + blanc pur
    - 'monochrome_black': Sans fond + noir/ardoise profonde
    """
    scale = 4
    canvas_size = size * scale
    s = canvas_size / 512.0

    master = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))

    # 1. Fond Squircle (si badge)
    if not transparent_bg:
        rx = int(112 * s)
        bg_layer = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
        bg_draw = ImageDraw.Draw(bg_layer)

        if mode == "dark_badge":
            # Gradient fond sombre profond (Navy tech #0C1222 -> #060913)
            bg_grad = create_linear_gradient_mask(
                canvas_size, canvas_size,
                (0, 0), (canvas_size, canvas_size),
                (14, 22, 42, 255), (6, 9, 19, 255)
            )
            # Masque squircle
            mask = Image.new("L", (canvas_size, canvas_size), 0)
            ImageDraw.Draw(mask).rounded_rectangle(
                [0, 0, canvas_size - 1, canvas_size - 1], radius=rx, fill=255
            )
            master.paste(bg_grad, (0, 0), mask)

            # Liseré subtil de bordure technologique
            border_mask = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
            ImageDraw.Draw(border_mask).rounded_rectangle(
                [0, 0, canvas_size - 1, canvas_size - 1],
                radius=rx, outline=(255, 255, 255, 30), width=max(1, int(2.5 * s))
            )
            master.alpha_composite(border_mask)

        elif mode == "light_badge":
            bg_grad = create_linear_gradient_mask(
                canvas_size, canvas_size,
                (0, 0), (canvas_size, canvas_size),
                (255, 255, 255, 255), (241, 245, 249, 255)
            )
            mask = Image.new("L", (canvas_size, canvas_size), 0)
            ImageDraw.Draw(mask).rounded_rectangle(
                [0, 0, canvas_size - 1, canvas_size - 1], radius=rx, fill=255
            )
            master.paste(bg_grad, (0, 0), mask)

            border_mask = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
            ImageDraw.Draw(border_mask).rounded_rectangle(
                [0, 0, canvas_size - 1, canvas_size - 1],
                radius=rx, outline=(0, 0, 0, 25), width=max(1, int(2.5 * s))
            )
            master.alpha_composite(border_mask)

    # 2. Coordonnées géométriques du symbole T-Vector Triad
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

    # Fût Central (Vecteur Z / Stabilité & Ancrage)
    poly_stem = [
        (226 * s, 254 * s),
        (286 * s, 254 * s),
        (276 * s, 400 * s),
        (256 * s, 424 * s),
        (236 * s, 400 * s)
    ]

    # Nœud Central Nexus (Point nodal losange)
    poly_nexus = [
        (256 * s, 222 * s),
        (266 * s, 236 * s),
        (256 * s, 250 * s),
        (246 * s, 236 * s)
    ]

    # Rendu de chaque facette polygonale avec son gradient dédié
    def draw_faceted_poly(poly, col_start, col_end, p_start, p_end):
        mask = Image.new("L", (canvas_size, canvas_size), 0)
        ImageDraw.Draw(mask).polygon(poly, fill=255)
        grad = create_linear_gradient_mask(
            canvas_size, canvas_size,
            (p_start[0] * s, p_start[1] * s),
            (p_end[0] * s, p_end[1] * s),
            col_start, col_end
        )
        master.paste(grad, (0, 0), mask)

    if mode in ["dark_badge", "glyph_tech"]:
        # Aile gauche : Cyan vibrant (#00F2FE -> #0072FF)
        draw_faceted_poly(poly_left, (0, 242, 254, 255), (0, 114, 255, 255), (96, 140), (244, 230))
        # Aile droite : Cobalt royal (#2979FF -> #1565C0)
        draw_faceted_poly(poly_right, (41, 121, 255, 255), (21, 101, 192, 255), (268, 140), (416, 230))
        # Fût central : Indigo technique (#00B0FF -> #0D47A1)
        draw_faceted_poly(poly_stem, (0, 176, 255, 255), (13, 71, 161, 255), (256, 254), (256, 424))
        # Nœud central : Blanc éclatant avec lueur (#FFFFFF)
        nexus_mask = Image.new("L", (canvas_size, canvas_size), 0)
        ImageDraw.Draw(nexus_mask).polygon(poly_nexus, fill=255)
        nexus_color = Image.new("RGBA", (canvas_size, canvas_size), (255, 255, 255, 250))
        master.paste(nexus_color, (0, 0), nexus_mask)

    elif mode == "light_badge":
        # Couleurs adaptées au fond blanc/clair
        draw_faceted_poly(poly_left, (0, 160, 255, 255), (0, 80, 210, 255), (96, 140), (244, 230))
        draw_faceted_poly(poly_right, (0, 100, 230, 255), (10, 50, 170, 255), (268, 140), (416, 230))
        draw_faceted_poly(poly_stem, (0, 130, 245, 255), (5, 40, 150, 255), (256, 254), (256, 424))
        nexus_mask = Image.new("L", (canvas_size, canvas_size), 0)
        ImageDraw.Draw(nexus_mask).polygon(poly_nexus, fill=255)
        nexus_color = Image.new("RGBA", (canvas_size, canvas_size), (15, 23, 42, 255))
        master.paste(nexus_color, (0, 0), nexus_mask)

    elif mode == "monochrome_white":
        for poly in [poly_left, poly_right, poly_stem]:
            m = Image.new("L", (canvas_size, canvas_size), 0)
            ImageDraw.Draw(m).polygon(poly, fill=255)
            master.paste(Image.new("RGBA", (canvas_size, canvas_size), (255, 255, 255, 255)), (0, 0), m)
        # Nœud évidé
        nm = Image.new("L", (canvas_size, canvas_size), 0)
        ImageDraw.Draw(nm).polygon(poly_nexus, fill=255)
        master.paste(Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0)), (0, 0), nm)

    elif mode == "monochrome_black":
        for poly in [poly_left, poly_right, poly_stem]:
            m = Image.new("L", (canvas_size, canvas_size), 0)
            ImageDraw.Draw(m).polygon(poly, fill=255)
            master.paste(Image.new("RGBA", (canvas_size, canvas_size), (15, 23, 42, 255)), (0, 0), m)
        nm = Image.new("L", (canvas_size, canvas_size), 0)
        ImageDraw.Draw(nm).polygon(poly_nexus, fill=255)
        master.paste(Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0)), (0, 0), nm)

    # Réduction Lanczos pour anti-aliasing parfait
    return master.resize((size, size), Image.Resampling.LANCZOS)

def generate_all_identity_assets():
    icons_dir = r"e:\Book\Dev\TSA\resources\icons"
    png_dir = os.path.join(icons_dir, "png")
    os.makedirs(png_dir, exist_ok=True)

    print("--- 1. Génération des PNG multi-résolutions ---")
    sizes = [16, 24, 32, 48, 64, 128, 256, 512, 1024]
    
    ico_images = []
    for sz in sizes:
        img_badge = render_tsa_master(sz, mode="dark_badge", transparent_bg=False)
        img_badge.save(os.path.join(png_dir, f"TSA_app_{sz}x{sz}.png"))
        
        # Pour les tailles standards Windows .ico
        if sz <= 256:
            ico_images.append(img_badge)

        # Glyphe transparent
        img_glyph = render_tsa_master(sz, mode="glyph_tech", transparent_bg=True)
        img_glyph.save(os.path.join(png_dir, f"TSA_glyph_{sz}x{sz}.png"))

    # Sauvegarder également les masters 512 dans le dossier icons
    render_tsa_master(512, mode="dark_badge").save(os.path.join(icons_dir, "TSA_dark.png"))
    render_tsa_master(512, mode="light_badge").save(os.path.join(icons_dir, "TSA_light.png"))
    render_tsa_master(512, mode="glyph_tech", transparent_bg=True).save(os.path.join(icons_dir, "TSA_glyph.png"))
    render_tsa_master(512, mode="monochrome_white", transparent_bg=True).save(os.path.join(icons_dir, "TSA_mono_white.png"))
    render_tsa_master(512, mode="monochrome_black", transparent_bg=True).save(os.path.join(icons_dir, "TSA_mono_dark.png"))

    print("--- 2. Génération du package Windows ICO multi-résolution ---")
    # Windows requiert : 16, 24, 32, 48, 64, 128, 256
    ico_path_icons = os.path.join(icons_dir, "TSA.ico")
    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    
    # img256 est l'image principale
    img256 = [img for img in ico_images if img.size == (256, 256)][0]
    other_imgs = [img for img in ico_images if img.size != (256, 256)]
    img256.save(ico_path_icons, format="ICO", sizes=ico_sizes, append_images=other_imgs)
    print(f"ICO généré avec succès : {ico_path_icons}")

    # Synchroniser aussi dans resources/TSA.ico si présent
    root_ico = os.path.join(r"e:\Book\Dev\TSA\resources", "TSA.ico")
    img256.save(root_ico, format="ICO", sizes=ico_sizes, append_images=other_imgs)
    print(f"ICO synchronisé : {root_ico}")

    print("--- 3. Écriture des SVG vectoriels officiels ---")
    write_official_svgs(icons_dir)

def write_official_svgs(icons_dir):
    # SVG 1 : TSA.svg officiel (Badge sombre haute définition avec squircle)
    svg_badge = '''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="tsa_bg_grad" x1="0" y1="0" x2="512" y2="512" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#0E162A"/>
      <stop offset="100%" stop-color="#060913"/>
    </linearGradient>
    <linearGradient id="tsa_wing_left" x1="96" y1="140" x2="244" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#00F2FE"/>
      <stop offset="100%" stop-color="#0072FF"/>
    </linearGradient>
    <linearGradient id="tsa_wing_right" x1="268" y1="140" x2="416" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#2979FF"/>
      <stop offset="100%" stop-color="#1565C0"/>
    </linearGradient>
    <linearGradient id="tsa_stem_grad" x1="256" y1="254" x2="256" y2="424" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#00B0FF"/>
      <stop offset="50%" stop-color="#0072FF"/>
      <stop offset="100%" stop-color="#0D47A1"/>
    </linearGradient>
    <filter id="tsa_glow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="12" stdDeviation="16" flood-color="#0066FF" flood-opacity="0.35"/>
    </filter>
  </defs>

  <!-- Fond squircle tech -->
  <rect width="512" height="512" rx="112" fill="url(#tsa_bg_grad)"/>
  <rect x="1" y="1" width="510" height="510" rx="111" stroke="#FFFFFF" stroke-opacity="0.1" stroke-width="2"/>

  <!-- Symbole T-Vector Triad -->
  <g filter="url(#tsa_glow)">
    <!-- Aile Gauche (Vecteur X) -->
    <polygon points="96,140 244,140 244,230 154,230 96,172" fill="url(#tsa_wing_left)"/>

    <!-- Aile Droite (Vecteur Y) -->
    <polygon points="268,140 416,140 416,172 358,230 268,230" fill="url(#tsa_wing_right)"/>

    <!-- Fût Central (Vecteur Z / Ancrage) -->
    <polygon points="226,254 286,254 276,400 256,424 236,400" fill="url(#tsa_stem_grad)"/>

    <!-- Nœud Central Nexus (Losange d'articulation) -->
    <polygon points="256,222 266,236 256,250 246,236" fill="#FFFFFF"/>
  </g>
</svg>'''
    with open(os.path.join(icons_dir, "TSA.svg"), "w", encoding="utf-8") as f:
        f.write(svg_badge)

    # SVG 2 : TSA_glyph.svg (Symbole seul transparent pour UI, Ribbon, Boutons)
    svg_glyph = '''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="tsa_glyph_left" x1="96" y1="140" x2="244" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#00F2FE"/>
      <stop offset="100%" stop-color="#0072FF"/>
    </linearGradient>
    <linearGradient id="tsa_glyph_right" x1="268" y1="140" x2="416" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#2979FF"/>
      <stop offset="100%" stop-color="#1565C0"/>
    </linearGradient>
    <linearGradient id="tsa_glyph_stem" x1="256" y1="254" x2="256" y2="424" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#00B0FF"/>
      <stop offset="50%" stop-color="#0072FF"/>
      <stop offset="100%" stop-color="#0D47A1"/>
    </linearGradient>
  </defs>

  <g>
    <polygon points="96,140 244,140 244,230 154,230 96,172" fill="url(#tsa_glyph_left)"/>
    <polygon points="268,140 416,140 416,172 358,230 268,230" fill="url(#tsa_glyph_right)"/>
    <polygon points="226,254 286,254 276,400 256,424 236,400" fill="url(#tsa_glyph_stem)"/>
    <polygon points="256,222 266,236 256,250 246,236" fill="#FFFFFF"/>
  </g>
</svg>'''
    with open(os.path.join(icons_dir, "TSA_glyph.svg"), "w", encoding="utf-8") as f:
        f.write(svg_glyph)

    # SVG 3 : TSA_light.svg (Version claire)
    svg_light = '''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="tsa_bg_light" x1="0" y1="0" x2="512" y2="512" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="100%" stop-color="#F1F5F9"/>
    </linearGradient>
    <linearGradient id="tsa_light_left" x1="96" y1="140" x2="244" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#0284C7"/>
      <stop offset="100%" stop-color="#0369A1"/>
    </linearGradient>
    <linearGradient id="tsa_light_right" x1="268" y1="140" x2="416" y2="230" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#2563EB"/>
      <stop offset="100%" stop-color="#1D4ED8"/>
    </linearGradient>
    <linearGradient id="tsa_light_stem" x1="256" y1="254" x2="256" y2="424" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#0284C7"/>
      <stop offset="100%" stop-color="#1E3A8A"/>
    </linearGradient>
  </defs>

  <rect width="512" height="512" rx="112" fill="url(#tsa_bg_light)"/>
  <rect x="1" y="1" width="510" height="510" rx="111" stroke="#000000" stroke-opacity="0.08" stroke-width="2"/>

  <g>
    <polygon points="96,140 244,140 244,230 154,230 96,172" fill="url(#tsa_light_left)"/>
    <polygon points="268,140 416,140 416,172 358,230 268,230" fill="url(#tsa_light_right)"/>
    <polygon points="226,254 286,254 276,400 256,424 236,400" fill="url(#tsa_light_stem)"/>
    <polygon points="256,222 266,236 256,250 246,236" fill="#0F172A"/>
  </g>
</svg>'''
    with open(os.path.join(icons_dir, "TSA_light.svg"), "w", encoding="utf-8") as f:
        f.write(svg_light)

    # SVG 4 : TSA_monochrome.svg (Noir sur transparent)
    svg_mono = '''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <g fill="currentColor">
    <polygon points="96,140 244,140 244,230 154,230 96,172"/>
    <polygon points="268,140 416,140 416,172 358,230 268,230"/>
    <polygon points="226,254 286,254 276,400 256,424 236,400"/>
  </g>
</svg>'''
    with open(os.path.join(icons_dir, "TSA_monochrome.svg"), "w", encoding="utf-8") as f:
        f.write(svg_mono)

    print("SVG vectoriels officiels écrits avec succès dans", icons_dir)

if __name__ == "__main__":
    generate_all_identity_assets()
