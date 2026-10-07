import os
import shutil
from PIL import Image, ImageDraw

# Master Polygons (512x512 coordinate space, Hazel-style T Monogram from TSA Web)
P_TL = [(138.0, 72.5), (188.0, 72.5), (104.0, 148.5), (54.0, 148.5)]
P_BODY = [
    (218.0, 72.5), (458.0, 72.5), (374.0, 148.5), (322.0, 148.5),
    (322.0, 292.5), (246.0, 361.5), (246.0, 148.5), (134.0, 148.5)
]
P_FOOT = [(322.0, 320.5), (322.0, 370.5), (246.0, 439.5), (246.0, 389.5)]
MASTER_POLYS = [P_TL, P_BODY, P_FOOT]

FILL_HAZEL = "#D3D7DA"
STROKE_HAZEL = "#000000"
STROKE_WIDTH = 8.0

BASE_DIR_TSA = r"e:\Book\Dev\TSA"
BASE_DIR_TSA_WEB = r"e:\Book\Dev\TSA Web"

RESOURCES_DIR = os.path.join(BASE_DIR_TSA, "resources")
ICONS_DIR = os.path.join(RESOURCES_DIR, "icons")
PNG_DIR = os.path.join(ICONS_DIR, "png")
BRANDING_DIR = os.path.join(RESOURCES_DIR, "branding")

for d in [RESOURCES_DIR, ICONS_DIR, PNG_DIR, BRANDING_DIR]:
    os.makedirs(d, exist_ok=True)

def render_logo_image(size=512, bg_color=None, fill=FILL_HAZEL, stroke=STROKE_HAZEL, stroke_w=STROKE_WIDTH, squircle=False):
    """Renders the master T symbol at exact size with 4x supersampling."""
    scale = 4
    canvas_size = size * scale
    ratio = size / 512.0
    sw = stroke_w * ratio * scale

    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    if bg_color:
        if squircle:
            rx = int(112 * ratio * scale)
            draw.rounded_rectangle([0, 0, canvas_size - 1, canvas_size - 1], radius=rx, fill=bg_color)
            border_col = (255, 255, 255, 30) if bg_color in ["#0B1120", "#0E162A", (11, 17, 32, 255)] else (0, 0, 0, 25)
            draw.rounded_rectangle([0, 0, canvas_size - 1, canvas_size - 1], radius=rx, outline=border_col, width=max(1, int(2.5 * ratio * scale)))
        else:
            draw.rectangle([0, 0, canvas_size - 1, canvas_size - 1], fill=bg_color)

    for poly in MASTER_POLYS:
        scaled_poly = [(pt[0] * ratio * scale, pt[1] * ratio * scale) for pt in poly]
        draw.polygon(scaled_poly, fill=fill, outline=stroke, width=max(1, int(round(sw))))

    return img.resize((size, size), Image.Resampling.LANCZOS)

def generate_svg_files():
    print("--- 1. Écriture des SVG officiels TSA depuis TSA Web ---")

    # 1.1 TSA_glyph.svg : Monogramme T transparent officiel pur
    svg_glyph = f'''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <g stroke-linejoin="round" stroke-linecap="round">
    <polygon points="138.0,72.5 188.0,72.5 104.0,148.5 54.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="218.0,72.5 458.0,72.5 374.0,148.5 322.0,148.5 322.0,292.5 246.0,361.5 246.0,148.5 134.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="322.0,320.5 322.0,370.5 246.0,439.5 246.0,389.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
  </g>
</svg>'''
    with open(os.path.join(ICONS_DIR, "TSA_glyph.svg"), "w", encoding="utf-8") as f:
        f.write(svg_glyph)
    with open(os.path.join(BRANDING_DIR, "TSA_Glyph.svg"), "w", encoding="utf-8") as f:
        f.write(svg_glyph)

    # 1.2 TSA.svg : Badge officiel squircle sombre (Navy/Titane)
    svg_dark_badge = f'''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="tsa_bg_grad" x1="0" y1="0" x2="512" y2="512" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#0E162A"/>
      <stop offset="100%" stop-color="#060913"/>
    </linearGradient>
  </defs>
  <!-- Fond squircle tech -->
  <rect width="512" height="512" rx="112" fill="url(#tsa_bg_grad)"/>
  <rect x="1" y="1" width="510" height="510" rx="111" stroke="#FFFFFF" stroke-opacity="0.1" stroke-width="2"/>
  <!-- Symbole T Hazel officiel -->
  <g stroke-linejoin="round" stroke-linecap="round">
    <polygon points="138.0,72.5 188.0,72.5 104.0,148.5 54.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="218.0,72.5 458.0,72.5 374.0,148.5 322.0,148.5 322.0,292.5 246.0,361.5 246.0,148.5 134.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="322.0,320.5 322.0,370.5 246.0,439.5 246.0,389.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
  </g>
</svg>'''
    with open(os.path.join(ICONS_DIR, "TSA.svg"), "w", encoding="utf-8") as f:
        f.write(svg_dark_badge)
    with open(os.path.join(BRANDING_DIR, "TSA_Logo.svg"), "w", encoding="utf-8") as f:
        f.write(svg_dark_badge)

    # 1.3 TSA_light.svg : Badge officiel squircle clair (Platinum/Blanc)
    svg_light_badge = f'''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="tsa_bg_light" x1="0" y1="0" x2="512" y2="512" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="100%" stop-color="#F1F5F9"/>
    </linearGradient>
  </defs>
  <rect width="512" height="512" rx="112" fill="url(#tsa_bg_light)"/>
  <rect x="1" y="1" width="510" height="510" rx="111" stroke="#000000" stroke-opacity="0.08" stroke-width="2"/>
  <g stroke-linejoin="round" stroke-linecap="round">
    <polygon points="138.0,72.5 188.0,72.5 104.0,148.5 54.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="218.0,72.5 458.0,72.5 374.0,148.5 322.0,148.5 322.0,292.5 246.0,361.5 246.0,148.5 134.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
    <polygon points="322.0,320.5 322.0,370.5 246.0,439.5 246.0,389.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="{STROKE_WIDTH}"/>
  </g>
</svg>'''
    with open(os.path.join(ICONS_DIR, "TSA_light.svg"), "w", encoding="utf-8") as f:
        f.write(svg_light_badge)

    # 1.4 TSA_monochrome.svg : Variante monochrome
    svg_mono = '''<svg width="512" height="512" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
  <g stroke-linejoin="round" stroke-linecap="round" fill="currentColor" stroke="currentColor">
    <polygon points="138.0,72.5 188.0,72.5 104.0,148.5 54.0,148.5"/>
    <polygon points="218.0,72.5 458.0,72.5 374.0,148.5 322.0,148.5 322.0,292.5 246.0,361.5 246.0,148.5 134.0,148.5"/>
    <polygon points="322.0,320.5 322.0,370.5 246.0,439.5 246.0,389.5"/>
  </g>
</svg>'''
    with open(os.path.join(ICONS_DIR, "TSA_monochrome.svg"), "w", encoding="utf-8") as f:
        f.write(svg_mono)

    # 1.5 file_tsa.svg : Icône officielle document .TSA issue de TSA Web
    src_ext_svg = os.path.join(BASE_DIR_TSA_WEB, "TSA-File-Extension-512.svg")
    dst_ext_svg = os.path.join(ICONS_DIR, "file_tsa.svg")
    if os.path.exists(src_ext_svg):
        shutil.copy2(src_ext_svg, dst_ext_svg)
        print("Copié file_tsa.svg depuis TSA Web:", dst_ext_svg)

    # 1.6 TSA_Banner.svg : Bannière modernisée intégrant le monogramme T officiel Hazel
    svg_banner = f'''<svg width="880" height="220" viewBox="0 0 880 220" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <!-- Background Gradient -->
    <linearGradient id="banner_bg" x1="0" y1="0" x2="880" y2="220" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#0B1120"/>
      <stop offset="50%" stop-color="#070B14"/>
      <stop offset="100%" stop-color="#04060A"/>
    </linearGradient>

    <!-- Logo Box Gradient -->
    <linearGradient id="logo_bg" x1="40" y1="30" x2="200" y2="190" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#111B33"/>
      <stop offset="100%" stop-color="#070B14"/>
    </linearGradient>

    <!-- Text Title Gradient -->
    <linearGradient id="title_grad" x1="230" y1="50" x2="520" y2="100" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#38BDF8"/>
      <stop offset="40%" stop-color="#00F2FE"/>
      <stop offset="100%" stop-color="#3B82F6"/>
    </linearGradient>

    <!-- Accent Line Gradient -->
    <linearGradient id="accent_line" x1="230" y1="0" x2="820" y2="0" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#00F2FE" stop-opacity="0.8"/>
      <stop offset="50%" stop-color="#3B82F6" stop-opacity="0.4"/>
      <stop offset="100%" stop-color="#1E293B" stop-opacity="0"/>
    </linearGradient>
  </defs>

  <!-- Banner Background Card -->
  <rect width="880" height="220" rx="16" fill="url(#banner_bg)"/>
  <rect x="1" y="1" width="878" height="218" rx="15" stroke="#1E293B" stroke-width="1.5"/>

  <!-- Subtle Perspective Grid / CAD Texture -->
  <g opacity="0.07" stroke="#38BDF8" stroke-width="1">
    <line x1="230" y1="20" x2="860" y2="20" />
    <line x1="230" y1="60" x2="860" y2="60" />
    <line x1="230" y1="100" x2="860" y2="100" />
    <line x1="230" y1="140" x2="860" y2="140" />
    <line x1="230" y1="180" x2="860" y2="180" />
    <line x1="330" y1="10" x2="330" y2="210" />
    <line x1="450" y1="10" x2="450" y2="210" />
    <line x1="570" y1="10" x2="570" y2="210" />
    <line x1="690" y1="10" x2="690" y2="210" />
    <line x1="810" y1="10" x2="810" y2="210" />
  </g>

  <!-- Logo Squircle Base (Left) -->
  <rect x="40" y="30" width="160" height="160" rx="36" fill="url(#logo_bg)"/>
  <rect x="41" y="31" width="158" height="158" rx="35" stroke="#FFFFFF" stroke-opacity="0.12" stroke-width="1.5"/>

  <!-- Official Hazel T Symbol (Scale 0.25, centered in 160x160 box: translate 56, 46) -->
  <g transform="translate(56, 46) scale(0.25)" stroke-linejoin="round" stroke-linecap="round">
    <polygon points="138.0,72.5 188.0,72.5 104.0,148.5 54.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="10.0"/>
    <polygon points="218.0,72.5 458.0,72.5 374.0,148.5 322.0,148.5 322.0,292.5 246.0,361.5 246.0,148.5 134.0,148.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="10.0"/>
    <polygon points="322.0,320.5 322.0,370.5 246.0,439.5 246.0,389.5" fill="{FILL_HAZEL}" stroke="{STROKE_HAZEL}" stroke-width="10.0"/>
  </g>

  <!-- Typography / Content (Right Side) -->
  <!-- Main Title "TSA" -->
  <text x="230" y="82" fill="url(#title_grad)" font-family="system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif" font-size="52" font-weight="900" letter-spacing="-1">TSA</text>

  <!-- Subtitle / Full Name -->
  <text x="355" y="65" fill="#F8FAFC" font-family="system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif" font-size="21" font-weight="700" letter-spacing="0.5">Tsaraloha Structural Analysis</text>
  <text x="355" y="85" fill="#94A3B8" font-family="system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif" font-size="12" font-weight="600" letter-spacing="2.5">ENGINEERING CAD &amp; FEA PLATFORM</text>

  <!-- Glowing Accent Line -->
  <line x1="230" y1="108" x2="840" y2="108" stroke="url(#accent_line)" stroke-width="2"/>

  <!-- Description Line -->
  <text x="230" y="136" fill="#CBD5E1" font-family="system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif" font-size="14.5" font-weight="500">
    Logiciel de modélisation 3D exacte B-Rep et d'analyse structurelle par éléments finis
  </text>

  <!-- Tech Pill Tags -->
  <g transform="translate(230, 156)">
    <rect width="66" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="33" y="16" fill="#38BDF8" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">C++20</text>
  </g>
  <g transform="translate(304, 156)">
    <rect width="56" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="28" y="16" fill="#4ADE80" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">Qt 6</text>
  </g>
  <g transform="translate(368, 156)">
    <rect width="118" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="59" y="16" fill="#60A5FA" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">OpenCASCADE</text>
  </g>
  <g transform="translate(494, 156)">
    <rect width="112" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="56" y="16" fill="#F472B6" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">OpenSees FEA</text>
  </g>
  <g transform="translate(614, 156)">
    <rect width="90" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="45" y="16" fill="#FBBF24" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">Eurocodes</text>
  </g>
  <g transform="translate(712, 156)">
    <rect width="102" height="24" rx="6" fill="#1E293B" stroke="#334155" stroke-width="1"/>
    <text x="51" y="16" fill="#94A3B8" font-family="system-ui, sans-serif" font-size="11" font-weight="700" text-anchor="middle">Windows x64</text>
  </g>
</svg>'''
    with open(os.path.join(BRANDING_DIR, "TSA_Banner.svg"), "w", encoding="utf-8") as f:
        f.write(svg_banner)

def generate_png_and_ico_files():
    print("--- 2. Génération des PNG et du package ICO multi-résolutions ---")
    sizes = [16, 24, 32, 48, 64, 128, 256, 512, 1024]

    # Copier TSA.ico depuis TSA Web directement
    src_ico = os.path.join(BASE_DIR_TSA_WEB, "TSA.ico")
    dst_ico_res = os.path.join(RESOURCES_DIR, "TSA.ico")
    dst_ico_icons = os.path.join(ICONS_DIR, "TSA.ico")
    if os.path.exists(src_ico):
        shutil.copy2(src_ico, dst_ico_res)
        shutil.copy2(src_ico, dst_ico_icons)
        print("Copié TSA.ico vers resources et resources/icons.")

    # Rendu des PNG multi-résolutions
    for sz in sizes:
        img_app = render_logo_image(sz, bg_color="#0B1120", squircle=True)
        img_app.save(os.path.join(PNG_DIR, f"TSA_app_{sz}x{sz}.png"))

        img_glyph = render_logo_image(sz, bg_color=None)
        img_glyph.save(os.path.join(PNG_DIR, f"TSA_glyph_{sz}x{sz}.png"))

    # Rendu des masters 512 dans icons
    render_logo_image(512, bg_color="#0B1120", squircle=True).save(os.path.join(ICONS_DIR, "TSA_dark.png"))
    render_logo_image(512, bg_color="#FFFFFF", squircle=True).save(os.path.join(ICONS_DIR, "TSA_light.png"))
    render_logo_image(512, bg_color=None).save(os.path.join(ICONS_DIR, "TSA_glyph.png"))
    render_logo_image(512, bg_color=None, fill="#FFFFFF", stroke="#FFFFFF").save(os.path.join(ICONS_DIR, "TSA_mono_white.png"))
    render_logo_image(512, bg_color=None, fill="#0F172A", stroke="#0F172A").save(os.path.join(ICONS_DIR, "TSA_mono_dark.png"))

    # Branding masters
    render_logo_image(512, bg_color="#0B1120", squircle=True).save(os.path.join(BRANDING_DIR, "TSA_Logo_Dark.png"))
    render_logo_image(512, bg_color="#FFFFFF", squircle=True).save(os.path.join(BRANDING_DIR, "TSA_Logo_Light.png"))
    render_logo_image(512, bg_color=None).save(os.path.join(BRANDING_DIR, "TSA_Glyph_Transparent.png"))

    src_sq = os.path.join(BASE_DIR_TSA_WEB, "branding", "TSA-Logo-Text-Square.png")
    if os.path.exists(src_sq):
        shutil.copy2(src_sq, os.path.join(BRANDING_DIR, "TSA_Logo_Square.png"))
    else:
        render_logo_image(512, bg_color="#0B1120", squircle=True).save(os.path.join(BRANDING_DIR, "TSA_Logo_Square.png"))

    print("PNG et icônes générés avec succès !")

if __name__ == "__main__":
    generate_svg_files()
    generate_png_and_ico_files()
    print("Toutes les icônes de TSA ont été mises à jour avec celles de TSA Web !")
