#!/usr/bin/env python3
"""Icônes composées de TSA (docs/ICONS.md) → resources/icons/<catégorie>/<nom>.svg

  python tools/icons/generate_icons.py

Une icône = un glyphe de base (grille, plan, nœud, maillage…) + éventuellement un badge en bas à droite qui précise la
variante (œil = afficher, + = créer, curseurs = paramètres, flèches = importer / exporter, coche = vérifier, flèche
circulaire = régénérer, × = supprimer…). Deux commandes proches ont ainsi la même famille de forme et un badge distinct.
Style commun : 24 × 24, traits de 1,75 px à extrémités arrondies, palette ci-dessous (thème sombre ; le thème clair est
dérivé par tools/icons/make_palette.py). Les icônes générées sont versionnées : relancer ce script après modification.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "resources", "icons")

SKY, SLATE, RED, GREEN, AMBER, VIOLET, ROSE = "#38BDF8", "#94A3B8", "#EF4444", "#10B981", "#F59E0B", "#A78BFA", "#F43F5E"
W = 'stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"'
W15 = 'stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"'


def path(d, color, width=W, fill="none"):
    return f'<path d="{d}" stroke="{color}" {width} fill="{fill}"/>'


def dot(x, y, r, color):
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>'


def ring(x, y, r, color, width=W):
    return f'<circle cx="{x}" cy="{y}" r="{r}" stroke="{color}" {width} fill="none"/>'


# --- Glyphes de base (zone 2..18 quand un badge occupe le coin 14..23) -------------------------------------------------
def g_grid(c=SLATE, s=1.0):
    e = 2 + 15 * s
    m1, m2 = 2 + 5 * s, 2 + 10 * s
    return path(f"M2 {m1}H{e}M2 {m2}H{e}M{m1} 2V{e}M{m2} 2V{e}", c) + path(f"M2 2H{e}V{e}H2Z", c, W15)


def g_plane(c=SKY):
    return path("M2 15L7 9H19L14 15Z", c)


def g_node(c=SKY, x=9, y=9):
    return ring(x, y, 3, c) + path(f"M{x} {y-6.5}V{y-4.5}M{x} {y+4.5}V{y+6.5}M{x-6.5} {y}H{x-4.5}M{x+4.5} {y}H{x+6.5}", SLATE, W15)


def g_support(c=GREEN):
    return path("M9 4L4 12H14Z", c) + path("M2 15H16M4 15L2.5 17.5M8 15L6.5 17.5M12 15L10.5 17.5", SLATE, W15)


def g_levels(c=SKY):
    return path("M2 5H17M2 10H17M2 15H17", c) + path("M4 3V17", SLATE, W15)


def g_mesh(c=VIOLET, size=16):
    e = 2 + size
    h = 2 + size / 2
    return path(f"M2 2H{e}V{e}H2ZM2 {h}H{e}M{h} 2V{e}M2 2L{e} {e}M2 {h}L{h} {e}M{h} 2L{e} {h}", c, W15)


def g_cube(c=SKY):
    return path("M10 2L17 6V14L10 18L3 14V6Z", c) + path("M3 6L10 10L17 6M10 10V18", c, W15)


def g_document(c=SLATE):
    return path("M4 2H12L16 6V18H4Z", c) + path("M12 2V6H16", c, W15)


def g_building(c=SKY):
    return path("M3 18V4L10 1.5L17 4V18Z", c) + path("M3 9H17M3 13.5H17M10 4V18", c, W15)


def g_camera(c=SKY):
    return path("M2 6H6L8 3.5H12L14 6H18V16H2Z", c) + ring(10, 11, 3, c, W15)


def g_beam(c=SKY):
    return path("M3 10H17", c, 'stroke-width="3" stroke-linecap="round"') + dot(3, 10, 2.2, SLATE) + dot(17, 10, 2.2, SLATE)


def g_load_arrow(c=RED, x=9):
    return path(f"M{x} 2V13", c) + f'<path d="M{x-4} 10L{x} 15L{x+4} 10Z" fill="{c}"/>'


def g_slab(c=SKY):
    return path("M2 13L7 6H19L14 13Z", c) + path("M2 13V15L14 15V13M14 15L19 8V6", SLATE, W15)


def g_house(c=SKY):
    return path("M3 11L12 3L21 11M5 9.5V20H10V14H14V20H19V9.5", c)


def g_dashed_box(c=SLATE):
    return f'<rect x="3" y="3" width="18" height="18" rx="1.5" stroke="{c}" stroke-width="1.5" stroke-dasharray="3 2.5" fill="none"/>'


def g_clipboard(c=SLATE):
    return path("M6 4H4V20H16V4H14", c) + path("M7 2.5H13V5.5H7Z", c, W15)


def g_puzzle(c=SKY):
    return path("M4 7H8A2 2 0 1 1 12 7H16V11A2 2 0 1 1 16 15V19H12A2 2 0 1 0 8 19H4V15A2 2 0 1 0 4 11Z", c)


def g_spark(c=VIOLET, x=8, y=8, r=6):
    return path(f"M{x} {y-r}L{x+1.6} {y-1.6}L{x+r} {y}L{x+1.6} {y+1.6}L{x} {y+r}L{x-1.6} {y+1.6}L{x-r} {y}L{x-1.6} {y-1.6}Z", c, W15)


# --- Badges (coin inférieur droit, centre 18 ; 18) ------------------------------------------------------------------
def b_eye(c=SKY):
    return path("M12.5 18C14 15.6 16 14.5 18 14.5S22 15.6 23.5 18C22 20.4 20 21.5 18 21.5S14 20.4 12.5 18Z", c, W15) + dot(18, 18, 1.5, c)


def b_plus(c=GREEN):
    return path("M18 13.5V22.5M13.5 18H22.5", c, 'stroke-width="2" stroke-linecap="round"')


def b_check(c=GREEN):
    return path("M13.5 18L16.5 21L22.5 14.5", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_cross(c=ROSE):
    return path("M14.5 14.5L21.5 21.5M21.5 14.5L14.5 21.5", c, 'stroke-width="2" stroke-linecap="round"')


def b_sliders(c=AMBER):
    return path("M13 15H23M13 20H23", c, W15) + dot(16, 15, 1.8, c) + dot(20.5, 20, 1.8, c)


def b_out(c=GREEN):
    return path("M18 22.5V14M14.5 17.5L18 14L21.5 17.5", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_in(c=SKY):
    return path("M18 13.5V22M14.5 18.5L18 22L21.5 18.5", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_refresh(c=GREEN):
    return path("M22 17A4 4 0 1 1 20.6 14.2", c, W15) + path("M20.5 12.5V14.5H22.5", c, W15)


def b_target(c=AMBER):
    return ring(18, 18, 4, c, W15) + dot(18, 18, 1.4, c)


def b_back(c=AMBER):
    return path("M22.5 21V19A3 3 0 0 0 19.5 16H14M16.5 13.5L14 16L16.5 18.5", c, 'stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"')


def b_magnet(c=RED):
    return path("M14.5 13.5V18.5A3.5 3.5 0 0 0 21.5 18.5V13.5", c, 'stroke-width="2.25" stroke-linecap="butt"') + path("M14.5 13.5V15.5M21.5 13.5V15.5", SLATE, 'stroke-width="2.25"')


def b_arrow_up(c=AMBER):
    return path("M18 22.5V13.5M14.5 17L18 13.5L21.5 17", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_arrow_down(c=AMBER):
    return path("M18 13.5V22.5M14.5 19L18 22.5L21.5 19", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_arrow_right(c=AMBER):
    return path("M13.5 18H22.5M19 14.5L22.5 18L19 21.5", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_arrow_left(c=AMBER):
    return path("M22.5 18H13.5M17 14.5L13.5 18L17 21.5", c, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')


def b_tag(c=AMBER, label="1"):
    if label == "1":
        return path("M14 15L17 13.5V22.5M14.5 22.5H19.5", c, W15) + path("M20.5 15.5H23M20.5 19H23", c, W15)
    return path("M13 14H19L23 18L19 22H13Z", c, W15) + dot(16, 18, 1.2, c)


def letter(ch, x, y, c):
    """Lettre en traits (y, z, x) de 4 × 5 px, coin haut-gauche (x, y)."""
    if ch == "z":
        return path(f"M{x} {y}H{x+4}L{x} {y+5}H{x+4}", c, W15)
    if ch == "y":
        return path(f"M{x} {y}L{x+2} {y+2.5}L{x+4} {y}M{x+2} {y+2.5}V{y+5.5}", c, W15)
    return path(f"M{x} {y}L{x+4} {y+5}M{x+4} {y}L{x} {y+5}", c, W15)


def svg(*parts):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none">\n  '
            + "\n  ".join(parts) + "\n</svg>\n")


# --- Diagrammes d'efforts (poutre grise + courbe ambre + lettre de l'axe) -------------------------------------------------
def diagram(curve, ch=None):
    parts = [path("M2 6H22", SLATE, W15), path(curve, AMBER)]
    if ch:
        parts.append(letter(ch, 18, 16, SKY))
    return svg(*parts)


ICONS = {
    # Commun
    "common/diagnostic.svg": svg(g_document(), path("M5.5 12H8L9.5 9L11 14.5L12.5 11H14.5", RED, W15)),
    "common/extension_manager.svg": svg(g_puzzle()),
    "common/library.svg": svg(path("M3 4V20M7 4V20M11 6L14.5 19.5M18 4V20", SKY), path("M2 20.5H22", SLATE, W15)),
    "common/keyboard_settings.svg": svg(path("M2 5H16V13H2Z", SLATE), path("M4.5 7.5H5M8 7.5H8.5M11.5 7.5H12M5.5 10.5H12.5", SLATE, W15), b_sliders()),
    "common/ai_settings.svg": svg(g_spark(), b_sliders()),
    "common/ai_check.svg": svg(g_spark(), b_check()),
    "common/ai_explain.svg": svg(path("M2 3H16V12H8L4 15.5V12H2Z", VIOLET), g_spark(VIOLET, 18, 18, 5)),
    "common/modules.svg": svg(path("M3 13H11V21H3ZM13 13H21V21H13ZM8 3H16V11H8Z", SKY)),
    "common/report_problem.svg": svg(path("M5 21V3M5 4H17L14.5 8L17 12H5", RED)),
    "common/online_docs.svg": svg(path("M2 4H9A3 3 0 0 1 12 7V20A2.5 2.5 0 0 0 9.5 17.5H2Z", SKY), ring(18, 17, 4.5, SLATE, W15),
                                  path("M13.5 17H22.5M18 12.5C16.5 14 16.5 20 18 21.5C19.5 20 19.5 14 18 12.5", SLATE, W15)),
    "common/named_views.svg": svg(g_camera(), path("M15.5 13V22L18.5 19.5L21.5 22V13Z", AMBER, W15)),
    # Modélisation
    "modeling/modeling_settings.svg": svg(path("M3 6H15M3 6V16M15 6V16", SKY), b_sliders()),
    "modeling/grid_manager.svg": svg(g_grid(), path("M14 14.5H23M14 18H23M14 21.5H23", AMBER, W15)),
    "modeling/grid_visible.svg": svg(g_grid(), b_eye()),
    "modeling/grid_new.svg": svg(g_grid(), b_plus()),
    "modeling/levels_visible.svg": svg(g_levels(), b_eye()),
    "modeling/workplane_level.svg": svg(path("M2 18H20M2 13H20", SLATE, W15), path("M2 9L6 4H20L16 9Z", SKY)),
    "modeling/workplane_custom.svg": svg(path("M2 16L9 3L20 9L13 21Z", SKY), dot(9, 3, 2, AMBER), dot(20, 9, 2, AMBER), dot(13, 21, 2, AMBER)),
    # Plans de travail globaux : plan tramé (on y dessine) dans son orientation, distinct des vues de caméra
    "modeling/workplane_xy.svg": svg(path("M2 17L8 9H22L16 17Z", SKY), path("M6.5 17L12.5 9M11.5 17L17.5 9M5 13H19", SKY, W15)),
    "modeling/workplane_xz.svg": svg(path("M4 3H20V21H4Z", SKY), path("M4 9H20M4 15H20M9.5 3V21M14.5 3V21", SKY, W15)),
    "modeling/workplane_yz.svg": svg(path("M5 7L19 2V17L5 22Z", SKY), path("M5 12L19 7M5 17L19 12M12 4.5V19.5", SKY, W15)),
    "modeling/workplane_visible.svg": svg(g_plane(), b_eye()),
    "modeling/workplane_next.svg": svg(path("M2 17L5 13H15L12 17Z", SLATE, W15), g_plane(), b_arrow_up()),
    "modeling/workplane_prev.svg": svg(path("M2 9L5 5H15L12 9Z", SLATE, W15), path("M2 17L7 11H19L14 17Z", SKY), b_arrow_down()),
    "modeling/intersection_next.svg": svg(path("M2 9H16M9 2V16", SLATE), dot(9, 9, 2.2, SKY), b_arrow_right()),
    "modeling/intersection_prev.svg": svg(path("M8 9H22M15 2V16", SLATE), dot(15, 9, 2.2, SKY), b_arrow_left()),
    "modeling/intersection_align.svg": svg(path("M2 9H16M9 2V16", SLATE), b_target(SKY)),
    "modeling/workplane_back.svg": svg(g_plane(), b_back()),
    "modeling/nodes_visible.svg": svg(g_node(), b_eye()),
    "modeling/node_labels.svg": svg(g_node(), b_tag()),
    "modeling/supports_visible.svg": svg(g_support(), b_eye()),
    "modeling/support_labels.svg": svg(g_support(), b_tag(label="tag")),
    "modeling/surface_supports.svg": svg(path("M3 4L10 4L19 13", SKY), path("M2 17H22M5 17L3 20.5M9 17L7 20.5M13 17L11 20.5M17 17L15 20.5M21 17L19 20.5", GREEN, W15)),
    "modeling/snap_grid.svg": svg(dot(3, 3, 1.3, SLATE), dot(9, 3, 1.3, SLATE), dot(15, 3, 1.3, SLATE), dot(3, 9, 1.3, SLATE), dot(9, 9, 1.3, SKY),
                                  dot(15, 9, 1.3, SLATE), dot(3, 15, 1.3, SLATE), dot(9, 15, 1.3, SLATE), b_magnet()),
    "modeling/snap_object.svg": svg(path("M3 15L13 4", SKY), path("M10 1.5H15.5V7H10Z", AMBER, W15), b_magnet()),
    "modeling/topology.svg": svg(ring(5, 5, 2.5, SKY, W15), ring(5, 17, 2.5, SKY, W15), ring(17, 11, 2.5, SKY, W15),
                                 path("M7.3 6.2L14.7 9.8M7.3 15.8L14.7 12.2", SLATE, W15), path("M20.5 3.5L22 2.5V8", AMBER, W15)),
    "modeling/model_clean.svg": svg(path("M15 3L10 13", AMBER), path("M5 13H14L15 21H4Z", SKY), path("M7 16V21M10 16V21", SKY, W15),
                                    path("M19 5V9M17 7H21", GREEN, W15)),
    "modeling/slab_openings.svg": svg(g_slab(), path("M8 8.5H12L10.5 11H6.5Z", AMBER, W15)),
    # Charges
    "modeling/loads_visible.svg": svg(g_load_arrow(), b_eye()),
    "modeling/load_values.svg": svg(g_load_arrow(), path("M13 14.5H23V21.5H13Z", AMBER, W15), path("M15.5 18H20.5", AMBER, W15)),
    "modeling/surface_loads_visible.svg": svg(path("M4 3V9M9 3V9M14 3V9", RED, W15), path("M2 13L5 9H17L14 13Z", SKY, W15), b_eye()),
    # Édition
    "edit/copy_array.svg": svg(path("M2 2H8V8H2Z", SLATE, W15), path("M7 7H13V13H7Z", SKY, W15), path("M12 12H18V18H12Z", SKY), path("M19 20.5H22.5M20.75 18.75V22.25", GREEN, W15)),
    "edit/paste_in_place.svg": svg(g_clipboard(), b_target()),
    "edit/select_all.svg": svg(g_dashed_box(), path("M8 7L8 17L11 14L13.5 19L15.5 18L13 13L17 13Z", SKY, W15)),
    # Vues
    "view/view_home.svg": svg(g_house()),
    "view/view_reset.svg": svg(g_cube(), b_refresh()),
    "view/view_normal.svg": svg(path("M2 18L8 11H22L16 18Z", SLATE), path("M12 14.5V2M8.5 5.5L12 2L15.5 5.5", SKY)),
    "view/fit_selection.svg": svg(path("M2 7V2H7M17 2H22V7M22 17V22H17M7 22H2V17", SKY), path("M8 8H16V16H8Z", AMBER, W15)),
    "view/view_previous.svg": svg(g_camera(SLATE), b_arrow_left(SKY)),
    "view/view_next.svg": svg(g_camera(SLATE), b_arrow_right(SKY)),
    "view/rotate_ccw.svg": svg(path("M5 12A7 7 0 1 0 8 6.3", SKY), path("M8 2.5V6.5H4", SKY)),
    "view/rotate_cw.svg": svg(path("M19 12A7 7 0 1 1 16 6.3", SKY), path("M16 2.5V6.5H20", SKY)),
    "view/view_bottom.svg": svg(path("M12 22L20 17L12 12L4 17Z", SKY), path("M4 17V7L12 2V12M20 17V7L12 2", SLATE, W15)),
    "view/view_back.svg": svg(path("M4 7L12 2L20 7V17L12 22L4 17Z", SLATE, W15), path("M12 2V12M12 12L20 7M12 12L4 7", SLATE, W15), path("M12 12L20 7V17L12 22Z", SKY)),
    "view/view_left.svg": svg(path("M4 7L12 2L20 7V17L12 22L4 17Z", SLATE, W15), path("M4 7L12 12V22L4 17Z", SKY)),
    # Plans 2D (mode de dessin) : feuille plane et ses deux axes (X rouge, Y vert, Z bleu), distincts des vues de caméra
    "view/plan_xy.svg": svg(path("M3 3H21V21H3Z", SLATE, W15), path("M6 18H17M6 18V7", SLATE), path("M6 18H17", RED), path("M6 18V7", GREEN),
                            letter("x", 17, 14.5, RED), letter("y", 8, 4, GREEN)),
    "view/plan_xz.svg": svg(path("M3 3H21V21H3Z", SLATE, W15), path("M6 18H17", RED), path("M6 18V7", SKY),
                            letter("x", 17, 14.5, RED), letter("z", 8, 4, SKY)),
    "view/plan_yz.svg": svg(path("M3 3H21V21H3Z", SLATE, W15), path("M6 18H17", GREEN), path("M6 18V7", SKY),
                            letter("y", 17, 14.5, GREEN), letter("z", 8, 4, SKY)),
    "modeling/isolate_workplane.svg": svg(g_dashed_box(), path("M5 15L8.5 10H19L15.5 15Z", SKY)),
    "view/display_physical.svg": svg(path("M4 6H14M4 16H14M9 6V16", SKY, 'stroke-width="2.5" stroke-linecap="round"'),
                                     path("M14 6L20 3M14 16L20 13M20 3V13M9 6L15 3", SLATE, W15)),
    "edit/move_origin.svg": svg(path("M4 18V6M4 18H16", SKY), path("M2 8L4 5.5L6 8M14 16L16.5 18L14 20", SKY, W15), dot(4, 18, 2, AMBER),
                                path("M19 9V15M16 12H22", AMBER, W15)),
    # Calcul et résultats
    "analysis/analysis_settings.svg": svg(path("M3 3L15 10L3 17Z", GREEN), b_sliders()),
    "analysis/diagram_mz.svg": diagram("M3 6C6 16 14 16 17 6", "z"),
    "analysis/diagram_my.svg": diagram("M3 6C6 14 14 14 17 6", "y"),
    "analysis/diagram_mx.svg": svg(path("M2 12H22", SLATE, W15), path("M7 6A6 6 0 1 0 12 18", AMBER), path("M8.5 3.5L7 6L9.8 7", AMBER, W15), letter("x", 17, 3, SKY)),
    "analysis/diagram_vz.svg": diagram("M3 6V12H10V3H17V6", "z"),
    "analysis/diagram_vy.svg": diagram("M3 6V10H10V3H17V6", "y"),
    "analysis/diagram_n.svg": svg(path("M2 12H22", SLATE, W15), path("M2 6H22V12H2", AMBER, W15), path("M6 18H18M6 18L9 15.5M6 18L9 20.5M18 18L15 15.5M18 18L15 20.5", AMBER, W15)),
    "analysis/diagram_deflection.svg": svg(path("M2 6H22", SLATE, W15, ), path("M2 6C7 15 17 15 22 6", AMBER), path("M12 6V12.5", SKY, W15), path("M10 10.5L12 12.5L14 10.5", SKY, W15)),
    "analysis/diagram_none.svg": svg(path("M2 6H22", SLATE, W15), path("M3 6C6 14 14 14 17 6", SLATE, W15), b_cross()),
    "analysis/reactions.svg": svg(g_support(), path("M20 22V12M17 15L20 12L23 15", AMBER, 'stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')),
    "analysis/deformed_visible.svg": svg(path("M2 4H16", SLATE, W15), path("M2 4C5 12 13 12 16 4", AMBER), b_eye()),
    "analysis/fit_model.svg": svg(path("M2 7V2H7M17 2H22V7M22 17V22H17M7 22H2V17", SKY), path("M7 17V9L12 6L17 9V17", SLATE, W15)),
    "analysis/fit_results.svg": svg(path("M2 7V2H7M17 2H22V7M22 17V22H17M7 22H2V17", SKY), path("M6 9H18", SLATE, W15), path("M6 9C9 17 15 17 18 9", AMBER)),
    "analysis/fit_deformed.svg": svg(path("M2 7V2H7M17 2H22V7M22 17V22H17M7 22H2V17", SKY), path("M6 8V16M18 8V16", SLATE, W15), path("M6 8C9 12 15 12 18 8", AMBER)),
    # Maillage
    "analysis/mesh_settings.svg": svg(g_mesh(size=14), b_sliders()),
    "analysis/mesh_preview.svg": svg(f'<path d="M2 2H16V16H2ZM2 9H16M9 2V16M2 2L16 16" stroke="{VIOLET}" stroke-width="1.5" stroke-dasharray="2 2" fill="none"/>', b_eye()),
    "analysis/mesh_show.svg": svg(g_mesh(size=14), b_eye()),
    "analysis/mesh_quality.svg": svg(g_mesh(size=14), b_check()),
    "analysis/mesh_regenerate.svg": svg(g_mesh(size=14), b_refresh()),
    "analysis/mesh_local.svg": svg(g_mesh(size=14), b_target()),
    "analysis/mesh_zone.svg": svg(path("M2 2H18V18H2Z", VIOLET, W15), ring(10, 10, 5, AMBER, W15), path("M10 5V15M5 10H15M6.5 6.5L13.5 13.5M13.5 6.5L6.5 13.5", VIOLET, 'stroke-width="1" stroke-linecap="round"')),
    "analysis/mesh_export.svg": svg(g_mesh(size=14), b_out()),
    "analysis/mesh_import.svg": svg(g_mesh(size=14), b_in()),
    "analysis/mesh_clear.svg": svg(g_mesh(size=14), b_cross()),
    # Rapports, BIM
    "file/ndc_report.svg": svg(g_document(), path("M6.5 9.5H13.5", SLATE, W15), path("M6.5 9.5C8 14.5 12 14.5 13.5 9.5", AMBER, W15), path("M6.5 16.5H11", SKY, W15)),
    "file/report_templates.svg": svg(path("M7 2H17L21 6V18H7Z", SLATE, W15), path("M3 6H13L17 10V22H3Z", SKY), path("M6 14H14M6 17.5H11", SKY, W15)),
    "file/ifc_import.svg": svg(g_building(), b_in()),
    "file/ifc_export.svg": svg(g_building(), b_out()),
    "file/tsa3d_import.svg": svg(g_cube(), b_in()),
    "file/tsa3d_export.svg": svg(g_cube(), b_out()),
    "file/module_import.svg": svg(path("M3 6H6A1.5 1.5 0 1 1 9 6H12V9A1.5 1.5 0 1 1 12 12V15H9A1.5 1.5 0 1 0 6 15H3Z", SKY), b_in()),
}


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    for rel, content in ICONS.items():
        p = os.path.join(OUT, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
    print(f"{len(ICONS)} icône(s) écrite(s) dans resources/icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
