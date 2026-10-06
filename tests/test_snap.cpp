// Suite « snap » : moteur d'accrochage 3D en espace écran (tests 181-187).
// Vues synthétiques (orthographiques, dessus et isométrique, zooms extrêmes) : le moteur reçoit
// la projection exactement comme OccView la lui fournit (rayon de visée + projection monde → pixels).

#include "test_common.h"

#include "Grid/GridManager.h"
#include "Grid/SnapEngine.h"

using namespace TSA::Model;
using namespace TSA::Grid;

namespace
{
/// Vue orthographique : centre, direction de visée, haut, échelle (pixels par mètre).
struct OrthoView
{
    gp_Pnt center { 0, 0, 0 };
    gp_Dir dir { 0, 0, -1 };
    gp_Dir up { 0, 1, 0 };
    double scale = 50.0;
    double w = 1200.0, h = 800.0;

    gp_Dir right() const { return up.Crossed(dir.Reversed()); }
    void project(const gp_Pnt& p, double& sx, double& sy) const
    {
        const gp_Vec v(center, p);
        sx = w / 2 + v.Dot(gp_Vec(right())) * scale;
        sy = h / 2 - v.Dot(gp_Vec(up)) * scale;
    }
    /// Requête pour un curseur placé à (dx, dy) pixels de la projection du point p.
    SnapQuery query(const gp_Pnt& p, double dx = 0.0, double dy = 0.0) const
    {
        SnapQuery q;
        project(p, q.cursorX, q.cursorY);
        q.cursorX += dx;
        q.cursorY += dy;
        const gp_Pnt onPlane = center.Translated(gp_Vec(right()) * ((q.cursorX - w / 2) / scale) + gp_Vec(up) * ((h / 2 - q.cursorY) / scale));
        q.rayOrigin = onPlane.Translated(gp_Vec(dir) * -1000.0);
        q.rayDir = dir;
        q.project = [this](const gp_Pnt& pt, double& sx, double& sy) { project(pt, sx, sy); return true; };
        return q;
    }
};

OrthoView isometric(double scale)
{
    OrthoView v;
    v.dir = gp_Dir(-1, -1, -1);
    const gp_Vec z(0, 0, 1);
    v.up = gp_Dir(z - gp_Vec(v.dir) * z.Dot(gp_Vec(v.dir)));
    v.scale = scale;
    return v;
}

GridSnapResult snapAt(const Model& m, const SnapQuery& q, const std::vector<const GridSystem*>& grids = {})
{
    SnapEngine e(q);
    e.collectModel(m);
    e.collectGrids(grids);
    return e.result();
}

int bar(Model& m, int a, int b) { return m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam); }
} // namespace

bool runSuite_Snap(int& passed)
{
    // TEST 181 : vide, nœud, extrémité / milieu de barre (spec §19, tests 1 à 4)
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(10, 0, 0);
        const int beam = bar(m, a, b);
        OrthoView v;
        auto r = snapAt(m, v.query(gp_Pnt(5, 4, 0)));
        TEST_CHECK(!r.snapped, "Test 181: curseur dans le vide → aucun accrochage");
        r = snapAt(m, v.query(gp_Pnt(10, 0, 0), 6, -5));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Node && r.targetEntityId == b && r.point.IsEqual(gp_Pnt(10, 0, 0), 1e-12),
                   "Test 181: près d'une extrémité de barre → nœud N2, position exacte");
        r = snapAt(m, v.query(gp_Pnt(5, 0, 0), 4, 3));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Midpoint && r.targetEntityId == beam && r.point.IsEqual(gp_Pnt(5, 0, 0), 1e-12),
                   "Test 181: près du milieu → Milieu, point exact (5, 0, 0)");
        r = snapAt(m, v.query(gp_Pnt(2.5, 0, 0), 0, 6));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Nearest && std::abs(r.point.X() - 2.5) < 1e-9 && std::abs(r.point.Y()) < 1e-12,
                   "Test 181: le long de la barre → Proche, point SUR l'axe");
        r = snapAt(m, v.query(gp_Pnt(10, 0, 0), 20, 0));
        TEST_CHECK(!r.snapped, "Test 181: hors de l'ouverture (20 px) → aucun accrochage");
        std::cout << "[PASS] Test 181: Accrochages de base (vide, nœud, milieu, proche)" << std::endl;
        ++passed;
    }

    // TEST 182 : intersection de deux barres sans nœud commun ; face de dalle ; extrémité de voile
    {
        Model m;
        bar(m, m.addNode(0, 0, 0), m.addNode(10, 0, 0));
        bar(m, m.addNode(4, -3, 0), m.addNode(4, 3, 0));
        OrthoView v;
        auto r = snapAt(m, v.query(gp_Pnt(4, 0, 0), 5, 4));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Intersection && r.point.IsEqual(gp_Pnt(4, 0, 0), 1e-9),
                   "Test 182: croisement de deux barres → Intersection (4, 0, 0)");

        Model s;
        const int n1 = s.addNode(0, 0, 3), n2 = s.addNode(6, 0, 3), n3 = s.addNode(6, 4, 3), n4 = s.addNode(0, 4, 3);
        s.addSlab({ n1, n2, n3, n4 }, 0.2);
        r = snapAt(s, v.query(gp_Pnt(1.5, 1.2, 3)));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Face && std::abs(r.point.Z() - 3.0) < 1e-9 && std::abs(r.point.X() - 1.5) < 1e-6,
                   "Test 182: dans la dalle → Face, point sur le plan de la dalle");
        r = snapAt(s, v.query(gp_Pnt(3, 2, 3), 3, 3));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Center && r.point.IsEqual(gp_Pnt(3, 2, 3), 1e-9), "Test 182: centre de dalle → Centre");

        Model w;
        w.addWall(w.addNode(0, 0, 0), w.addNode(5, 0, 0), 3.0, 0.2);
        OrthoView front;
        front.dir = gp_Dir(0, 1, 0);
        front.up = gp_Dir(0, 0, 1);
        r = snapAt(w, front.query(gp_Pnt(5, 0, 3), -4, 4));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Endpoint && r.point.IsEqual(gp_Pnt(5, 0, 3), 1e-9),
                   "Test 182: coin haut d'un voile (vue de face) → Extrémité");
        std::cout << "[PASS] Test 182: Intersection, face, centre, extrémité de voile" << std::endl;
        ++passed;
    }

    // TEST 183 : grilles (intersection, axe) et priorité nœud > grille au même endroit
    {
        GridDefinition def("G", GridType::Cartesian);
        def.setOrigin(0.0, 0.0, 0.0);
        def.setXPositions({ 0.0, 5.0, 10.0 });
        def.setYPositions({ 0.0, 6.0 });
        GridSystem grid(def);
        Model m;
        OrthoView v;
        auto r = snapAt(m, v.query(gp_Pnt(5, 6, 0), -5, 5), { &grid });
        TEST_CHECK(r.snapped && r.type == GridSnapType::Intersection && r.source == SnapSource::Grid && std::abs(r.point.X() - 5.0) < 1e-9 && std::abs(r.point.Y() - 6.0) < 1e-9,
                   "Test 183: près d'un nœud de grille → Grille (5, 6), niveau le plus proche de l'observateur");
        r = snapAt(m, v.query(gp_Pnt(5, 3, 0), 4, 0), { &grid });
        TEST_CHECK(r.snapped && r.type == GridSnapType::AxisLine && std::abs(r.point.X() - 5.0) < 1e-9 && std::abs(r.point.Y() - 3.0) < 1e-6,
                   "Test 183: sur un axe de grille → Axe, point sur l'axe");
        const int n = m.addNode(5, 6, 0);
        r = snapAt(m, v.query(gp_Pnt(5, 6, 0), 1, 1), { &grid });
        TEST_CHECK(r.snapped && r.type == GridSnapType::Node && r.targetEntityId == n, "Test 183: nœud au droit d'un nœud de grille → Nœud prioritaire");
        auto q = v.query(gp_Pnt(5, 6, 0), 1, 1);
        q.grids = false;
        TEST_CHECK(snapAt(Model(), q, { &grid }).snapped == false, "Test 183: magnétisme grille désactivé → aucune grille");
        std::cout << "[PASS] Test 183: Grilles et priorités" << std::endl;
        ++passed;
    }

    // TEST 184 : zooms extrêmes — ouverture constante en pixels (spec §19, tests 7 et 8)
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(0.4, 0, 0);
        bar(m, a, b);
        OrthoView close;
        close.scale = 5000.0;   // 0,4 m = 2000 px
        auto r = snapAt(m, close.query(gp_Pnt(0.2, 0, 0), 5, 5));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Midpoint, "Test 184: zoom très proche → milieu toujours accessible");
        r = snapAt(m, close.query(gp_Pnt(0.0, 0, 0), 30, 0));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Nearest, "Test 184: zoom très proche, 30 px du nœud → proche (pas de nœud à 6 mm)");
        OrthoView far;
        far.scale = 2.0;        // 0,4 m = 0,8 px : nœuds confondus à l'écran
        r = snapAt(m, far.query(gp_Pnt(0.0, 0, 0), 6, 0));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Node, "Test 184: zoom très éloigné → un nœud est retenu, jamais un point arbitraire");
        std::cout << "[PASS] Test 184: Zoom très proche / très éloigné" << std::endl;
        ++passed;
    }

    // TEST 185 : vue isométrique et poteau vertical ; profondeur (structure dense)
    {
        Model m;
        const int foot = m.addNode(2, 2, 0), head = m.addNode(2, 2, 3);
        m.addColumn(foot, head, Section::heb(200), Material::steelS235());
        const OrthoView iso = isometric(80.0);
        auto r = snapAt(m, iso.query(gp_Pnt(2, 2, 1.5), 3, -2));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Midpoint && r.point.IsEqual(gp_Pnt(2, 2, 1.5), 1e-9),
                   "Test 185: vue isométrique → milieu du poteau");
        r = snapAt(m, iso.query(gp_Pnt(2, 2, 3), -4, 3));
        TEST_CHECK(r.snapped && r.type == GridSnapType::Node && r.targetEntityId == head, "Test 185: vue isométrique → tête de poteau");

        // Deux nœuds superposés à l'écran (vue de dessus) : le plus proche de l'observateur
        Model d;
        const int low = d.addNode(1, 1, 0), high = d.addNode(1, 1, 6);
        (void)low;
        OrthoView top;
        r = snapAt(d, top.query(gp_Pnt(1, 1, 0), 2, 2));
        TEST_CHECK(r.snapped && r.targetEntityId == high, "Test 185: nœuds superposés → le plus proche de l'observateur");
        // Deux nœuds proches à l'écran : le plus proche du curseur, jamais l'ordre de création
        Model e;
        e.addNode(0, 0, 0);
        const int near = e.addNode(0.16, 0, 0);
        r = snapAt(e, top.query(gp_Pnt(0.16, 0, 0), 1, 0));
        TEST_CHECK(r.snapped && r.targetEntityId == near, "Test 185: structure dense → candidat le plus proche du curseur");
        std::cout << "[PASS] Test 185: Vue isométrique, profondeur, structure dense" << std::endl;
        ++passed;
    }

    // TEST 186 : réglages utilisateur (types désactivés, objets OFF), perpendiculaire, filtre d'affichage
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(10, 0, 0);
        const int beam = bar(m, a, b);
        OrthoView v;
        auto q = v.query(gp_Pnt(5, 0, 0), 3, 3);
        q.modes = SnapMode::All & ~SnapMode::Midpoint;
        auto r = snapAt(m, q);
        TEST_CHECK(r.snapped && r.type == GridSnapType::Nearest, "Test 186: Milieu désactivé → proche");
        q.objects = false;
        TEST_CHECK(!snapAt(m, q).snapped, "Test 186: accrochage objets désactivé → aucun accrochage");

        q = v.query(gp_Pnt(3, 0, 0), 2, 4);
        q.hasReference = true;
        q.reference = gp_Pnt(3, 7, 0);
        r = snapAt(m, q);
        TEST_CHECK(r.snapped && r.type == GridSnapType::Perpendicular && r.point.IsEqual(gp_Pnt(3, 0, 0), 1e-9),
                   "Test 186: pied de la perpendiculaire depuis le point précédent");

        q = v.query(gp_Pnt(5, 0, 0), 3, 3);
        q.acceptElement = [beam](int kind, int id) { return !(kind == static_cast<int>(ElementKind::Beam) && id == beam); };
        r = snapAt(m, q);
        TEST_CHECK(!r.snapped, "Test 186: élément masqué (calque) → non accrochable");
        std::cout << "[PASS] Test 186: Réglages, perpendiculaire, éléments masqués" << std::endl;
        ++passed;
    }

    // TEST 187 : libellés du marqueur et priorités documentées
    {
        GridSnapResult s;
        s.snapped = true;
        s.type = GridSnapType::Midpoint;
        s.source = SnapSource::Model;
        s.description = "Poutre B3";
        TEST_CHECK(SnapEngine::displayLabel(s) == "Milieu · Poutre B3", "Test 187: libellé « Milieu · Poutre B3 »");
        s.type = GridSnapType::Node;
        s.description = "Nœud N7 (1.000 ; 2.000 ; 0.000 m)";
        TEST_CHECK(SnapEngine::displayLabel(s) == "Nœud N7", "Test 187: libellé « Nœud N7 » (sans coordonnées)");
        s.type = GridSnapType::Intersection;
        s.source = SnapSource::Grid;
        s.description = "Grille A-2 (0.000 ; 0.000 ; 0.000 m)";
        TEST_CHECK(SnapEngine::displayLabel(s) == "Grille A-2", "Test 187: libellé « Grille A-2 »");
        TEST_CHECK(SnapEngine::priorityBias(GridSnapType::Node, SnapSource::Model) < SnapEngine::priorityBias(GridSnapType::Midpoint, SnapSource::Model)
                       && SnapEngine::priorityBias(GridSnapType::Intersection, SnapSource::Model) < SnapEngine::priorityBias(GridSnapType::Intersection, SnapSource::Grid)
                       && SnapEngine::isDiscrete(GridSnapType::Midpoint) && !SnapEngine::isDiscrete(GridSnapType::Nearest),
                   "Test 187: ordre de priorité");
        std::cout << "[PASS] Test 187: Libellés et priorités" << std::endl;
        ++passed;
    }
    return true;
}
