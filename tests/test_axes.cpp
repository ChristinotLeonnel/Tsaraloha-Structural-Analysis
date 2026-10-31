// Suite « axes » : repères de la vue 3D, global et locaux (src/Viewer/ElementAxisFrames, AxisTriad, OccView_Axes.cpp).
//   541  barres : repère local affiché identique à celui que reçoit OpenSees (horizontale, verticale, inclinée, 3D
//        quelconque, nœuds inversés, rotations de section) ; orthonormé, direct ;
//   542  autres éléments : dalle et voile identiques à MeshGeometry, directs ; treillis et câble : axe x seul ; aucun
//        repère pour une fondation ou un nœud ; plafond de la sélection multiple ;
//   543  point sélectionné : point de l'élément visé par le rayon, borné à la barre, centre si le rayon est parallèle ;
//   544  réglages : bornes, signal, réinitialisation, enregistrement relu ;
//   545  vue : un trièdre par élément sélectionné, lettres et couleurs, position et orientation réelles, trièdres
//        réutilisés, vidage, sélection multiple, élément modifié, non sélectionnables, réglages appliqués ;
//   546  stabilité : rotation de caméra, zoom, ajustement de la vue insensible à l'origine lointaine, plan de travail ;
//   547  image rendue : rouge, vert et bleu visibles sur fond sombre et clair, taille constante à l'écran, lettres ;
//   548  point cliqué dans la vue réelle : le repère se place sous le curseur.

#include "test_common.h"

#include "Analysis/AnalysisTypes.h"
#include "Analysis/CalculationSnapshot.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"
#include "Analysis/OpenSeesModelMap.h"
#include "Mesh/MeshGeometry.h"
#include "Viewer/AxisDisplayStyle.h"
#include "Viewer/ElementAxisFrames.h"
#include "Viewer/OccView.h"
#include "Viewer/SelectionManager.h"

#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <Prs3d_Drawer.hxx>
#include <V3d_View.hxx>

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QRect>
#include <QSettings>
#include <QThread>

using namespace TSA::Analysis;
using TSA::Viewer::AxisDisplaySettings;
using TSA::Viewer::AxisDisplayStyle;
using TSA::Viewer::ElementAxisFrame;
using TSA::Viewer::LocalAxisAnchor;
using TSA::Viewer::kMaxElementAxisFrames;

namespace
{
void pump(int n = 10)
{
    for (int k = 0; k < n; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

bool sameDir(const gp_Dir& a, const Vec3& b, double tol = 1e-9)
{
    return std::abs(a.X() - b[0]) < tol && std::abs(a.Y() - b[1]) < tol && std::abs(a.Z() - b[2]) < tol;
}

bool sameDir(const gp_Dir& a, const TSA::Mesh::P3& b, double tol = 1e-9)
{
    return std::abs(a.X() - b.x) < tol && std::abs(a.Y() - b.y) < tol && std::abs(a.Z() - b.z) < tol;
}

/// Orthonormé et direct : les gp_Dir sont unitaires par construction.
bool rightHanded(const ElementAxisFrame& f, double tol = 1e-9)
{
    return std::abs(f.x.Dot(f.y)) < tol && std::abs(f.y.Dot(f.z)) < tol && std::abs(f.x.Dot(f.z)) < tol &&
           f.x.Crossed(f.y).IsEqual(f.z, tol) && std::abs(f.x.DotCross(f.y, f.z) - 1.0) < tol;
}

const OpsElementEntry* entryFor(const OpenSeesModelMap& map, StructuralElementKind kind, int id)
{
    for (const auto& e : map.elements())
        if (e.key.kind == kind && e.key.id == id) return &e;
    return nullptr;
}

/// Réglages d'origine rétablis en fin de test : les essais passent par les paramètres réels de l'utilisateur.
struct AxisSettingsGuard
{
    AxisDisplaySettings saved = AxisDisplayStyle::instance().settings();
    AxisSettingsGuard() { AxisDisplayStyle::instance().reset(); }
    ~AxisSettingsGuard() { AxisDisplayStyle::instance().setSettings(saved); }
};

struct AxisView
{
    TSA::Viewer::SelectionManager selection;   // détruit après la vue
    OccView view;
    explicit AxisView(Model& m)
    {
        view.resize(800, 600);
        view.show();
        pump(20);
        view.setSelectionManager(&selection);
        view.setModel(&m);
        view.setGridSnapEnabled(false);
        view.setObjectSnapEnabled(false);
        view.setViewTransitionMs(0);
        view.rebuildAllShapes();
        pump();
    }
    ~AxisView()
    {
        view.setModel(nullptr);
        pump();
    }
    const Handle(AIS_InteractiveContext)& context() const { return view.context(); }
};

int displayedTriads(const AxisView& v)
{
    int count = 0;
    for (std::size_t i = 0; i < v.view.localAxisTriadPoolSize(); ++i)
        count += v.context()->IsDisplayed(v.view.localAxisTriad(i)) ? 1 : 0;
    return count;
}

/// Trièdre posé en pixels : ancrage = origine monde, transformation locale = rotation x, y, z en colonnes.
bool triadShows(const Handle(AIS_Trihedron)& triad, const ElementAxisFrame& f, double tol = 1e-9)
{
    const gp_Trsf t = triad->LocalTransformation();
    const Handle(Graphic3d_TransformPers)& pers = triad->TransformPersistence();
    if (pers.IsNull() || pers->Mode() != Graphic3d_TMF_ZoomPers) return false;
    const auto column = [&t](int c) { return gp_Dir(t.Value(1, c), t.Value(2, c), t.Value(3, c)); };
    return pers->AnchorPoint().Distance(f.origin) < tol && column(1).IsEqual(f.x, tol) && column(2).IsEqual(f.y, tol) && column(3).IsEqual(f.z, tol);
}

/// Images à contrôler à l'œil : dossier TSA_AXES_IMAGES, rien d'écrit sinon.
void keepImage(const QImage& image, const QString& name)
{
    const QString folder = qEnvironmentVariable("TSA_AXES_IMAGES");
    if (!folder.isEmpty()) image.save(QDir(folder).filePath(name));
}

struct ColourCount
{
    int red = 0, green = 0, blue = 0;
    QRect redBox;
};

/// Pixels nettement rouges, verts, bleus : ni le fond, ni l'élément gris, ni le ViewCube (coin haut droit) ne comptent.
ColourCount countAxisColours(const QImage& image)
{
    ColourCount c;
    const QRect cube(image.width() - 190, 0, 190, 190);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            if (cube.contains(x, y)) continue;
            const QColor p = image.pixelColor(x, y);
            const int r = p.red(), g = p.green(), b = p.blue();
            if (r > 150 && g < 110 && b < 110)
            {
                ++c.red;
                c.redBox |= QRect(x, y, 1, 1);
            }
            if (g > 130 && r < 110 && b < 110) ++c.green;
            if (b > 200 && r < 110 && g < 150) ++c.blue;
        }
    return c;
}
} // namespace

bool runSuite_Axes(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 541 : barres, repère affiché = repère de OpenSees
    // -------------------------------------------------------------------------
    {
        struct Case { const char* name; gp_Pnt a, b; double rotation; bool column; };
        std::vector<Case> cases;
        for (double rotation : { 0.0, 30.0, 90.0, -45.0, 137.0 })
        {
            cases.push_back({ "horizontale X", gp_Pnt(0, 0, 3), gp_Pnt(6, 0, 3), rotation, false });
            cases.push_back({ "horizontale Y", gp_Pnt(0, 0, 3), gp_Pnt(0, 5, 3), rotation, false });
            cases.push_back({ "horizontale inversée", gp_Pnt(6, 0, 3), gp_Pnt(0, 0, 3), rotation, false });
            cases.push_back({ "inclinée", gp_Pnt(1, 2, 0), gp_Pnt(5, 2, 3), rotation, false });
            cases.push_back({ "3D quelconque", gp_Pnt(1, -2, 0.5), gp_Pnt(4, 3, 2.25), rotation, false });
            cases.push_back({ "3D inversée", gp_Pnt(4, 3, 2.25), gp_Pnt(1, -2, 0.5), rotation, false });
            cases.push_back({ "poteau vertical", gp_Pnt(2, 1, 0), gp_Pnt(2, 1, 3), rotation, true });
            cases.push_back({ "poteau inversé", gp_Pnt(2, 1, 3), gp_Pnt(2, 1, 0), rotation, true });
            cases.push_back({ "poteau incliné", gp_Pnt(0, 0, 0), gp_Pnt(0.5, 0.2, 3), rotation, true });
        }

        int compared = 0;
        for (const Case& c : cases)
        {
            Model m;
            const int a = m.addNode(c.a.X(), c.a.Y(), c.a.Z());
            const int b = m.addNode(c.b.X(), c.b.Y(), c.b.Z());
            const int id = c.column ? m.addColumn(a, b, Section::heb(200), Material::steelS235(), c.rotation)
                                    : m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
            if (!c.column) m.getBeam(id)->setRotation(c.rotation);
            const CalculationSnapshot snap = CalculationSnapshot::capture(m);
            AnalysisParameters params;
            const OpenSeesModelMap map = OpenSeesModelMap::build(snap, params);

            TSA::Model::ElementSet selection;
            (c.column ? selection.columns : selection.beams).insert(id);
            const auto frames = TSA::Viewer::elementAxisFrames(m, selection);
            const auto* entry = entryFor(map, c.column ? StructuralElementKind::Column : StructuralElementKind::Beam, id);
            const std::string name = std::string(c.name) + " à " + std::to_string(c.rotation) + "°";
            TEST_CHECK(frames.size() == 1 && entry && entry->axes.valid, ("Test 541: repère et entrée OpenSees présents, " + name).c_str());
            const ElementAxisFrame& f = frames[0];
            TEST_CHECK(sameDir(f.x, entry->axes.x) && sameDir(f.y, entry->axes.y) && sameDir(f.z, entry->axes.z),
                       ("Test 541: x, y, z égaux à ceux de OpenSees, " + name).c_str());
            TEST_CHECK(rightHanded(f), ("Test 541: orthonormé et direct, " + name).c_str());
            TEST_CHECK(f.transverseAxes && !f.surface(), ("Test 541: axes transverses affichés, " + name).c_str());
            const gp_Vec expected(c.a, c.b);
            TEST_CHECK(gp_Dir(expected).IsEqual(f.x, 1e-9), ("Test 541: x va du nœud de départ au nœud d'arrivée, " + name).c_str());
            TEST_CHECK(f.origin.Distance(c.a.Translated(0.5 * expected)) < 1e-9, ("Test 541: origine au milieu, " + name).c_str());
            ++compared;
        }
        std::cout << "[PASS] Test 541: Repère de barre identique à OpenSees (" << compared << " cas, orthonormé, direct)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 542 : surfaces, treillis, câbles, éléments sans repère, plafond
    // -------------------------------------------------------------------------
    {
        Model m;
        // Dalle inclinée et voile oblique : aucun axe confondu avec le repère global.
        const int s1 = m.addNode(0, 0, 3), s2 = m.addNode(4, 1, 3.5), s3 = m.addNode(3, 5, 4), s4 = m.addNode(-1, 4, 3.5);
        const int slab = m.addSlab({ s1, s2, s3, s4 }, 0.2);
        const int w1 = m.addNode(10, 0, 0), w2 = m.addNode(13, 4, 0);
        const int wall = m.addWall(w1, w2, 3.0, 0.2);
        const int t1 = m.addNode(0, 10, 0), t2 = m.addNode(3, 12, 2);
        const int truss = m.addTrussMember(t1, t2, 0.05);
        const int cable = m.addCable(t2, m.addNode(5, 10, 4), 0.02);
        const int f1 = m.addNode(20, 0, 0);
        const int foundation = m.addFoundation(f1);

        const auto geometry = TSA::Mesh::MeshGeometry::extract(m);
        TSA::Model::ElementSet selection;
        selection.slabs.insert(slab);
        selection.walls.insert(wall);
        const auto surfaces = TSA::Viewer::elementAxisFrames(m, selection);
        TEST_CHECK(surfaces.size() == 2 && surfaces[0].kind == ElementAxisFrame::Kind::Slab && surfaces[1].kind == ElementAxisFrame::Kind::Wall,
                   "Test 542: un repère par dalle et voile");
        const auto* slabGeometry = geometry.surface("SL" + std::to_string(slab));
        const auto* wallGeometry = geometry.surface("W" + std::to_string(wall));
        TEST_CHECK(slabGeometry && wallGeometry, "Test 542: surfaces de MeshGeometry trouvées");
        TEST_CHECK(sameDir(surfaces[0].x, slabGeometry->frame.U) && sameDir(surfaces[0].y, slabGeometry->frame.V) && sameDir(surfaces[0].z, slabGeometry->frame.N),
                   "Test 542: dalle, x, y, z = U, V, N de MeshGeometry");
        TEST_CHECK(sameDir(surfaces[1].x, wallGeometry->frame.U) && sameDir(surfaces[1].y, wallGeometry->frame.V) && sameDir(surfaces[1].z, wallGeometry->frame.N),
                   "Test 542: voile, x, y, z = U, V, N de MeshGeometry");
        TEST_CHECK(rightHanded(surfaces[0]) && rightHanded(surfaces[1]) && surfaces[0].surface() && surfaces[1].surface(), "Test 542: surfaces orthonormées et directes");
        TEST_CHECK(std::abs(surfaces[1].origin.Z() - 1.5) < 1e-9 && surfaces[1].origin.Distance(gp_Pnt(11.5, 2, 1.5)) < 1e-9, "Test 542: origine du voile au centre");

        const CalculationSnapshot snap = CalculationSnapshot::capture(m);
        AnalysisParameters params;
        const OpenSeesModelMap map = OpenSeesModelMap::build(snap, params);
        TSA::Model::ElementSet axial;
        axial.trussMembers.insert(truss);
        axial.cables.insert(cable);
        const auto bars = TSA::Viewer::elementAxisFrames(m, axial);
        const auto* trussEntry = entryFor(map, StructuralElementKind::Truss, truss);
        const auto* cableEntry = entryFor(map, StructuralElementKind::Cable, cable);
        TEST_CHECK(bars.size() == 2 && !bars[0].transverseAxes && !bars[1].transverseAxes, "Test 542: treillis et câble, axe x seul, formulation axiale");
        TEST_CHECK(trussEntry && cableEntry && sameDir(bars[0].x, trussEntry->axes.x) && sameDir(bars[1].x, cableEntry->axes.x), "Test 542: x égal à celui de OpenSees");

        TSA::Model::ElementSet none;
        none.foundations.insert(foundation);
        none.nodes.insert(f1);
        TEST_CHECK(TSA::Viewer::elementAxisFrames(m, none).empty(), "Test 542: fondation et nœud sans repère fictif");

        Model many;
        TSA::Model::ElementSet crowd;
        for (std::size_t i = 0; i <= kMaxElementAxisFrames; ++i)
        {
            const double y = double(i);
            const int beam = many.addBeam(many.addNode(0, y, 0), many.addNode(1, y, 0));
            if (i < kMaxElementAxisFrames) crowd.beams.insert(beam);
            else
            {
                bool tooMany = true;
                TEST_CHECK(TSA::Viewer::elementAxisFrames(many, crowd, &tooMany).size() == kMaxElementAxisFrames && !tooMany, "Test 542: plafond atteint, repères affichés");
                crowd.beams.insert(beam);
                TEST_CHECK(TSA::Viewer::elementAxisFrames(many, crowd, &tooMany).empty() && tooMany, "Test 542: au-delà du plafond, aucun repère et avis");
            }
        }
        std::cout << "[PASS] Test 542: Surfaces, treillis, câbles, éléments sans repère, plafond" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 543 : point sélectionné
    // -------------------------------------------------------------------------
    {
        ElementAxisFrame f;
        TEST_CHECK(TSA::Viewer::barAxisFrame(gp_Pnt(0, 0, 0), gp_Pnt(8, 0, 0), 0.0, &f), "Test 543: barre valide");
        ElementAxisFrame degenerate;
        TEST_CHECK(!TSA::Viewer::barAxisFrame(gp_Pnt(1, 1, 1), gp_Pnt(1, 1, 1), 0.0, &degenerate), "Test 543: barre de longueur nulle sans repère");
        const auto anchor = [&](const gp_Pnt& from, const gp_Dir& d, const ElementAxisFrame& e) { return TSA::Viewer::axisAnchorOnElement(e, gp_Lin(from, d)); };
        TEST_CHECK(anchor(gp_Pnt(2, 5, 3), gp_Dir(0, -5, -3), f).Distance(gp_Pnt(2, 0, 0)) < 1e-9, "Test 543: point de la barre visé par le rayon");
        TEST_CHECK(anchor(gp_Pnt(12, 5, 0), gp_Dir(0, -1, 0), f).Distance(gp_Pnt(8, 0, 0)) < 1e-9, "Test 543: borné à l'extrémité de la barre");
        TEST_CHECK(anchor(gp_Pnt(-3, 5, 0), gp_Dir(0, -1, 0), f).Distance(gp_Pnt(0, 0, 0)) < 1e-9, "Test 543: borné au début de la barre");
        TEST_CHECK(anchor(gp_Pnt(0, 2, 0), gp_Dir(1, 0, 0), f).Distance(f.origin) < 1e-9, "Test 543: rayon parallèle, centre conservé");

        ElementAxisFrame plate;
        plate.kind = ElementAxisFrame::Kind::Slab;
        plate.origin = gp_Pnt(1, 1, 3);
        plate.x = gp_Dir(1, 0, 0);
        plate.y = gp_Dir(0, 1, 0);
        plate.z = gp_Dir(0, 0, 1);
        TEST_CHECK(anchor(gp_Pnt(2, 4, 10), gp_Dir(0, 0, -1), plate).Distance(gp_Pnt(2, 4, 3)) < 1e-9, "Test 543: point du plan de la surface");
        TEST_CHECK(anchor(gp_Pnt(2, 4, 10), gp_Dir(1, 0, 0), plate).Distance(plate.origin) < 1e-9, "Test 543: rayon dans la direction du plan, centre conservé");
        std::cout << "[PASS] Test 543: Point sélectionné sur l'élément" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 544 : réglages
    // -------------------------------------------------------------------------
    {
        AxisSettingsGuard guard;
        auto& style = AxisDisplayStyle::instance();
        const AxisDisplaySettings defaults;
        TEST_CHECK(style.settings() == defaults, "Test 544: valeurs initiales après réinitialisation");
        int emitted = 0;
        const auto connection = QObject::connect(&style, &AxisDisplayStyle::changed, [&emitted] { ++emitted; });
        style.setAxisPixels(5.0);
        TEST_CHECK(style.settings().axisPixels == AxisDisplayStyle::kMinAxisPixels && emitted == 1, "Test 544: longueur bornée au minimum, signal émis");
        style.setAxisPixels(5000.0);
        TEST_CHECK(style.settings().axisPixels == AxisDisplayStyle::kMaxAxisPixels, "Test 544: longueur bornée au maximum");
        style.setLabelPixels(1.0);
        TEST_CHECK(style.settings().labelPixels == AxisDisplayStyle::kMinLabelPixels, "Test 544: lettres bornées au minimum");
        style.setLabelPixels(100.0);
        TEST_CHECK(style.settings().labelPixels == AxisDisplayStyle::kMaxLabelPixels, "Test 544: lettres bornées au maximum");
        const int count = emitted;
        style.setLabelPixels(100.0);
        TEST_CHECK(emitted == count, "Test 544: valeur inchangée, aucun signal");
        style.setLetters(false);
        style.setGlobalAxes(false);
        style.setCornerIndicator(false);
        style.setLocalAxes(false);
        style.setAnchor(LocalAxisAnchor::SelectedLocation);
        const AxisDisplaySettings written = style.settings();
        style.reset();
        TEST_CHECK(style.settings() == defaults, "Test 544: réinitialisation");
        // Domaine de paramètres propre au test : ceux de l'application ne sont pas touchés.
        const QString savedOrganization = QCoreApplication::organizationName(), savedApplication = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName("TSAAxesTest");
        QCoreApplication::setApplicationName("TSAAxesTest");
        style.setSettings(written);
        style.reset();
        style.setSettings(written);
        style.load();
        const bool reloaded = style.settings() == written;
        QSettings().clear();
        QCoreApplication::setOrganizationName(savedOrganization);
        QCoreApplication::setApplicationName(savedApplication);
        TEST_CHECK(reloaded, "Test 544: réglages enregistrés puis relus");
        QObject::disconnect(connection);
        std::cout << "[PASS] Test 544: Réglages des repères (bornes, signal, enregistrement)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 545 : vue, sélection
    // -------------------------------------------------------------------------
    {
        AxisSettingsGuard guard;
        Model m;
        const int a = m.addNode(0, 0, 3), b = m.addNode(5, 2, 4), c = m.addNode(0, 0, 0);
        const int beam = m.addBeam(a, b);
        const int column = m.addColumn(c, a);
        const int truss = m.addTrussMember(c, b, 0.05);
        const int footing = m.addFoundation(c);
        m.getBeam(beam)->setRotation(30.0);

        AxisView v(m);
        TEST_CHECK(!v.view.globalAxisTriad().IsNull() && v.context()->IsDisplayed(v.view.globalAxisTriad()), "Test 545: repère global affiché à l'ouverture");
        TEST_CHECK(v.view.localAxisFrames().empty() && displayedTriads(v) == 0, "Test 545: aucun repère local sans sélection");

        v.selection.selectBeam(beam);
        TEST_CHECK(v.view.localAxisFrames().size() == 1 && displayedTriads(v) == 1, "Test 545: un repère à la sélection d'une poutre");
        ElementAxisFrame expected;
        TSA::Viewer::barAxisFrame(gp_Pnt(0, 0, 3), gp_Pnt(5, 2, 4), 30.0, &expected);
        TEST_CHECK(triadShows(v.view.localAxisTriad(0), expected), "Test 545: trièdre au milieu de la poutre, orientation du calcul à 30°");
        const Handle(AIS_Trihedron) first = v.view.localAxisTriad(0);
        TEST_CHECK(first->Label(Prs3d_DatumParts_XAxis) == TCollection_ExtendedString("x") && first->Label(Prs3d_DatumParts_ZAxis) == TCollection_ExtendedString("z"),
                   "Test 545: lettres minuscules du repère local");
        TEST_CHECK(v.view.globalAxisTriad()->Label(Prs3d_DatumParts_XAxis) == TCollection_ExtendedString("X") &&
                       v.view.globalAxisTriad()->Label(Prs3d_DatumParts_YAxis) == TCollection_ExtendedString("Y"),
                   "Test 545: lettres majuscules du repère global");
        NCollection_List<int> modes;
        v.context()->ActivatedModes(first, modes);
        TEST_CHECK(modes.IsEmpty(), "Test 545: repère local non sélectionnable");
        modes.Clear();
        v.context()->ActivatedModes(v.view.globalAxisTriad(), modes);
        TEST_CHECK(modes.IsEmpty(), "Test 545: repère global non sélectionnable");

        v.selection.selectColumn(column);
        TEST_CHECK(v.view.localAxisFrames().size() == 1 && v.view.localAxisFrames()[0].kind == ElementAxisFrame::Kind::Column && displayedTriads(v) == 1,
                   "Test 545: changement de sélection, repère du nouvel élément");
        TEST_CHECK(v.view.localAxisTriadPoolSize() == 1 && v.view.localAxisTriad(0) == first, "Test 545: trièdre réutilisé, aucune recréation");

        v.selection.selectBeam(beam);
        v.selection.selectColumn(column, true);
        v.selection.selectTrussMember(truss, true);
        TEST_CHECK(v.view.localAxisFrames().size() == 3 && displayedTriads(v) == 3, "Test 545: sélection multiple, un repère par élément");
        for (std::size_t i = 0; i < 3; ++i) TEST_CHECK(triadShows(v.view.localAxisTriad(i), v.view.localAxisFrames()[i]), "Test 545: chaque trièdre sur son élément");
        TEST_CHECK(!v.view.localAxisFrames()[2].transverseAxes && v.view.localAxisTriad(2)->Attributes()->DatumAspect()->DatumAxes() == Prs3d_DatumAxes_XAxis,
                   "Test 545: treillis, axe x seul dessiné");

        v.selection.selectFoundation(footing);
        TEST_CHECK(v.view.localAxisFrames().empty() && displayedTriads(v) == 0, "Test 545: fondation, aucun repère fictif");
        v.selection.selectBeam(beam);
        TEST_CHECK(displayedTriads(v) == 1 && v.view.localAxisTriad(0)->Attributes()->DatumAspect()->DatumAxes() == Prs3d_DatumAxes_XYZAxes,
                   "Test 545: trièdre réutilisé, axes transverses rétablis");

        m.getNode(b)->setCoordinates(5, -2, 6);
        m.notifyNodeModified(b);
        pump();
        TSA::Viewer::barAxisFrame(gp_Pnt(0, 0, 3), gp_Pnt(5, -2, 6), 30.0, &expected);
        TEST_CHECK(triadShows(v.view.localAxisTriad(0), expected), "Test 545: élément déplacé, repère à jour");
        m.getBeam(beam)->setRotation(-45.0);
        m.notifyBeamModified(beam);
        pump();
        TSA::Viewer::barAxisFrame(gp_Pnt(0, 0, 3), gp_Pnt(5, -2, 6), -45.0, &expected);
        TEST_CHECK(triadShows(v.view.localAxisTriad(0), expected), "Test 545: section tournée, repère à jour");

        auto& style = AxisDisplayStyle::instance();
        style.setLocalAxes(false);
        TEST_CHECK(displayedTriads(v) == 0, "Test 545: repères locaux masqués par le réglage");
        style.setLocalAxes(true);
        TEST_CHECK(displayedTriads(v) == 1, "Test 545: repères locaux rétablis, sélection conservée");
        style.setGlobalAxes(false);
        TEST_CHECK(!v.context()->IsDisplayed(v.view.globalAxisTriad()), "Test 545: repère global masqué par le réglage");
        style.setGlobalAxes(true);
        TEST_CHECK(v.context()->IsDisplayed(v.view.globalAxisTriad()), "Test 545: repère global rétabli");
        style.setLetters(false);
        TEST_CHECK(!v.view.localAxisTriad(0)->Attributes()->DatumAspect()->ToDrawLabels() && !v.view.globalAxisTriad()->Attributes()->DatumAspect()->ToDrawLabels(),
                   "Test 545: lettres masquées par le réglage");
        style.setLetters(true);
        style.setAxisPixels(120.0);
        TEST_CHECK(std::abs(v.view.localAxisTriad(0)->Size() - v.view.globalAxisTriad()->Size()) < 1e-9 && v.view.globalAxisTriad()->Size() >= 120.0,
                   "Test 545: taille appliquée aux trièdres");

        v.selection.clearSelection();
        TEST_CHECK(v.view.localAxisFrames().empty() && displayedTriads(v) == 0, "Test 545: sélection vidée, repère masqué");

        TSA::Model::ElementSet crowd;
        for (int i = 0; i <= int(kMaxElementAxisFrames); ++i) crowd.beams.insert(m.addBeam(m.addNode(i, 20, 0), m.addNode(i, 21, 0)));
        v.selection.selectElements(crowd, false);
        TEST_CHECK(v.view.localAxesTooMany() && v.view.localAxisFrames().empty() && displayedTriads(v) == 0, "Test 545: sélection au-delà du plafond, aucun repère");
        v.selection.clearSelection();
        TEST_CHECK(!v.view.localAxesTooMany(), "Test 545: avis retiré au vidage");
        std::cout << "[PASS] Test 545: Repères à la sélection dans la vue réelle" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 546 : stabilité, caméra, ajustement, plan de travail
    // -------------------------------------------------------------------------
    {
        AxisSettingsGuard guard;
        Model m;
        // Modèle à 5 km de l'origine : le repère global ne doit pas entrer dans l'ajustement de la vue.
        const int beam = m.addBeam(m.addNode(5000, 5000, 3), m.addNode(5006, 5000, 3));
        AxisView v(m);
        v.selection.selectBeam(beam);

        const auto triad = v.view.localAxisTriad(0);
        const ElementAxisFrame frame = v.view.localAxisFrames()[0];
        v.view.setWorkPlaneAxesVisible(false);   // panneau du plan de travail recentré sur le modèle
        pump(3);
        v.view.fitAll();
        pump();
        std::cout << "  hauteur de vue ajustée : " << v.view.view()->Camera()->Scale() << " m" << std::endl;
        TEST_CHECK(v.view.view()->Camera()->Scale() < 100.0, "Test 546: l'origine lointaine n'entre pas dans l'ajustement de la vue");
        TEST_CHECK(v.context()->IsDisplayed(v.view.globalAxisTriad()) && displayedTriads(v) == 1, "Test 546: repères de nouveau affichés après l'ajustement");
        v.view.view()->SetProj(V3d_XposYposZpos);
        v.view.view()->Rotate(0.4, 0.2, 0.1, true);
        v.view.view()->SetZoom(3.0);
        v.view.view()->Redraw();
        pump();
        TEST_CHECK(v.view.localAxisTriadPoolSize() == 1 && v.view.localAxisTriad(0) == triad && triadShows(triad, frame), "Test 546: rotation et zoom, repère local inchangé");
        TEST_CHECK(v.view.localAxisFrames()[0].x.IsEqual(frame.x, 1e-12), "Test 546: orientation indépendante de la caméra");
        const Handle(Graphic3d_TransformPers)& global = v.view.globalAxisTriad()->TransformPersistence();
        TEST_CHECK(!global.IsNull() && global->AnchorPoint().Distance(gp_Pnt(0, 0, 0)) < 1e-12, "Test 546: repère global ancré à l'origine");

        v.view.setWorkPlaneAxesVisible(true);
        TEST_CHECK(v.view.workPlaneAxisTriad().IsNull(), "Test 546: plan XY confondu avec le repère global, un seul dessin");
        AxisDisplayStyle::instance().setGlobalAxes(false);
        TEST_CHECK(!v.view.workPlaneAxisTriad().IsNull() && v.context()->IsDisplayed(v.view.workPlaneAxisTriad()), "Test 546: repère global masqué, trièdre du plan dessiné");
        TEST_CHECK(v.view.workPlaneAxisTriad()->Label(Prs3d_DatumParts_XAxis) == TCollection_ExtendedString("Xwp"), "Test 546: lettres du plan de travail distinctes");
        AxisDisplayStyle::instance().setGlobalAxes(true);
        TEST_CHECK(v.view.workPlaneAxisTriad().IsNull(), "Test 546: repère global rétabli, un seul dessin");
        v.selection.clearSelection();
        std::cout << "[PASS] Test 546: Repères stables en rotation, zoom et ajustement" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 547 : image rendue
    // -------------------------------------------------------------------------
    {
        AxisSettingsGuard guard;
        auto& style = AxisDisplayStyle::instance();
        style.setCornerIndicator(false);
        style.setGlobalAxes(false);
        Model m;
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(8, 3, 2));
        m.getBeam(beam)->setRotation(25.0);
        AxisView v(m);
        v.view.setWorkPlaneAxesVisible(false);
        v.view.fitAll();
        v.selection.selectBeam(beam);
        pump();
        const bool wasDark = v.view.isDarkMode();

        for (bool dark : { true, false })
        {
            v.view.setDarkMode(dark);
            pump();
            const std::string theme = dark ? "fond sombre" : "fond clair";
            const QImage on = v.view.captureViewImage(800, 600, false);
            keepImage(on, QString("repere_local_%1.png").arg(dark ? "sombre" : "clair"));
            const ColourCount shown = countAxisColours(on);
            style.setLocalAxes(false);
            pump(3);
            const ColourCount hidden = countAxisColours(v.view.captureViewImage(800, 600, false));
            style.setLocalAxes(true);
            pump(3);
            std::cout << "  " << theme << " : rouge " << shown.red << "/" << hidden.red << ", vert " << shown.green << "/" << hidden.green << ", bleu " << shown.blue << "/" << hidden.blue << std::endl;
            TEST_CHECK(shown.red > hidden.red + 80, ("Test 547: axe x rouge, " + theme).c_str());
            TEST_CHECK(shown.green > hidden.green + 80, ("Test 547: axe y vert, " + theme).c_str());
            TEST_CHECK(shown.blue > hidden.blue + 80, ("Test 547: axe z bleu, " + theme).c_str());

            style.setLetters(false);
            pump(3);
            const QImage plain = v.view.captureViewImage(800, 600, false);
            style.setLetters(true);
            pump(3);
            TEST_CHECK(plain != v.view.captureViewImage(800, 600, false), ("Test 547: lettres visibles, " + theme).c_str());
        }

        v.view.setDarkMode(true);
        pump(3);
        // Élément au centre de l'image : le zoom le garde dans le cadre.
        const gp_Pnt anchor = v.view.localAxisFrames()[0].origin;
        int anchorX = 0, anchorY = 0;
        v.view.worldToPixel(anchor.X(), anchor.Y(), anchor.Z(), anchorX, anchorY);
        v.view.view()->SetCenter(anchorX, anchorY);
        pump(3);
        const QRect before = countAxisColours(v.view.captureViewImage(800, 600, false)).redBox;
        const double diagonalBefore = std::hypot(before.width(), before.height());
        v.view.view()->SetZoom(4.0);
        pump(3);
        const QRect zoomed = countAxisColours(v.view.captureViewImage(800, 600, false)).redBox;
        const double diagonalZoomed = std::hypot(zoomed.width(), zoomed.height());
        std::cout << "  axe x : " << diagonalBefore << " px, " << diagonalZoomed << " px après un zoom de 4" << std::endl;
        TEST_CHECK(diagonalBefore > 40.0 && std::abs(diagonalZoomed - diagonalBefore) < 6.0, "Test 547: taille constante à l'écran au zoom");

        style.setGlobalAxes(true);
        v.selection.clearSelection();
        v.view.fitAll();
        pump(3);
        const QImage globalImage = v.view.captureViewImage(800, 600, false);
        keepImage(globalImage, "repere_global.png");
        const ColourCount global = countAxisColours(globalImage);
        TEST_CHECK(global.red > 80 && global.green > 80 && global.blue > 80, "Test 547: repère global en rouge, vert, bleu");
        v.view.setDarkMode(wasDark);
        std::cout << "[PASS] Test 547: Couleurs, lettres et taille sur l'image rendue" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 548 : point cliqué dans la vue réelle
    // -------------------------------------------------------------------------
    {
        AxisSettingsGuard guard;
        AxisDisplayStyle::instance().setAnchor(LocalAxisAnchor::SelectedLocation);
        Model m;
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(8, 0, 0));
        AxisView v(m);
        v.view.setWorkPlaneAxesVisible(false);
        v.view.fitAll();
        pump();

        const gp_Pnt target(2, 0, 0);
        int px = 0, py = 0, winW = 0, winH = 0;
        v.view.worldToPixel(target.X(), target.Y(), target.Z(), px, py);
        v.view.view()->Window()->Size(winW, winH);
        const QPointF local(px * double(v.view.width()) / winW, py * double(v.view.height()) / winH);
        QMouseEvent press(QEvent::MouseButtonPress, local, v.view.mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&v.view, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, local, v.view.mapToGlobal(local), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&v.view, &release);
        pump(3);

        TEST_CHECK(v.selection.selectedBeams().count(beam) == 1, "Test 548: clic réel, poutre sélectionnée");
        TEST_CHECK(v.view.localAxisFrames().size() == 1, "Test 548: repère affiché après le clic");
        const double pixel = v.view.view()->Camera()->Scale() / std::max(1, winH);
        TEST_CHECK(v.view.localAxisFrames()[0].origin.Distance(target) < 4.0 * pixel, "Test 548: origine sous le curseur");

        v.selection.clearSelection();
        v.selection.selectBeam(beam);
        TEST_CHECK(v.view.localAxisFrames()[0].origin.Distance(target) < 4.0 * pixel, "Test 548: même élément resélectionné, point conservé");
        AxisDisplayStyle::instance().setAnchor(LocalAxisAnchor::ElementCenter);
        TEST_CHECK(v.view.localAxisFrames()[0].origin.Distance(gp_Pnt(4, 0, 0)) < 1e-9, "Test 548: retour au centre de l'élément");
        std::cout << "[PASS] Test 548: Repère au point cliqué" << std::endl;
        ++passed;
    }

    return true;
}
