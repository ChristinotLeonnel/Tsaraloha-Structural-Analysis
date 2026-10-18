// Suite « views » : vues 2D / 3D, ViewCube, plans de travail et dessin selon la vue active.
// Convention : Z vertical ; direction de caméra = de l'œil vers la cible ; droite de l'écran = direction ^ haut.
// Vue OCCT réelle (contexte OpenGL sur une vraie fenêtre) : sous la plateforme Qt « offscreen », les tests de vue sont
// annoncés sautés et jamais comptés réussis (BUG-076). Le test 434 (orientations calculées) tourne partout.

#include "test_common.h"
#include "IO/TSAFile.h"
#include "UI/Dialogs/NamedViewsDialog.h"
#include "Viewer/OccView.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QThread>

#include <Graphic3d_Camera.hxx>

#include <filesystem>
#include <map>

using namespace TSA::Model;
using TSA::Viewer::CameraFrame;
using TSA::Viewer::StandardCameraView;
using TSA::Viewer::ViewManager;

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

bool sameDir(const gp_Dir& a, const gp_Dir& b, double tol = 1e-9) { return a.IsEqual(b, tol); }

bool orthonormal(const CameraFrame& f)
{
    return std::abs(f.direction.Dot(f.up)) < 1e-12 && std::abs(gp_Vec(f.right()).Magnitude() - 1.0) < 1e-12;
}

bool cameraIs(const Handle(Graphic3d_Camera)& cam, const CameraFrame& f, double tol = 1e-9)
{
    return sameDir(cam->Direction(), f.direction, tol) && sameDir(cam->Up(), f.up, tol);
}

const char* faceName(StandardCameraView v)
{
    switch (v)
    {
    case StandardCameraView::Top: return "dessus";
    case StandardCameraView::Bottom: return "dessous";
    case StandardCameraView::Front: return "avant";
    case StandardCameraView::Back: return "arrière";
    case StandardCameraView::Left: return "gauche";
    case StandardCameraView::Right: return "droite";
    default: return "3D";
    }
}

/// Vue de test : fenêtre réelle, modèle attaché, accrochages coupés (points cliqués reproductibles).
struct ViewHarness
{
    OccView view;
    explicit ViewHarness(Model& m)
    {
        view.resize(800, 600);
        view.show();
        pump(20);
        view.setModel(&m);
        view.setGridSnapEnabled(false);
        view.setObjectSnapEnabled(false);
        view.setViewTransitionMs(0);
        pump();
    }
    ~ViewHarness()
    {
        view.cancelCurrentDrawing();
        view.setModel(nullptr);
        pump();
    }
    const Handle(Graphic3d_Camera)& cam() const { return view.view()->Camera(); }
    /// Point monde → position souris (coordonnées logiques du widget, comme un vrai clic).
    QPointF toMouse(const gp_Pnt& p) const
    {
        int px = 0, py = 0, winW = 0, winH = 0;
        view.worldToPixel(p.X(), p.Y(), p.Z(), px, py);
        view.view()->Window()->Size(winW, winH);
        return QPointF(px * double(view.width()) / winW, py * double(view.height()) / winH);
    }
    void click(const QPointF& local)
    {
        QMouseEvent press(QEvent::MouseButtonPress, local, view.mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, local, view.mapToGlobal(local), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &release);
        pump(3);
    }
    /// Taille d'un pixel dans le monde (projection orthographique) : tolérance d'un point cliqué.
    double pixelSize() const
    {
        int winW = 0, winH = 0;
        view.view()->Window()->Size(winW, winH);
        return cam()->Scale() / std::max(1, winH);
    }
};

std::map<int, gp_Pnt> nodeSnapshot(const Model& m)
{
    std::map<int, gp_Pnt> s;
    for (const auto& [id, n] : m.nodes())
        s[id] = gp_Pnt(n.x(), n.y(), n.z());
    return s;
}

bool sameSnapshot(const std::map<int, gp_Pnt>& a, const std::map<int, gp_Pnt>& b)
{
    if (a.size() != b.size())
        return false;
    for (const auto& [id, p] : a)
    {
        const auto it = b.find(id);
        if (it == b.end() || p.X() != it->second.X() || p.Y() != it->second.Y() || p.Z() != it->second.Z())
            return false;   // égalité exacte : un changement de vue ne touche jamais la géométrie
    }
    return true;
}
} // namespace

bool runSuite_Views(int& passed)
{
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");
    const StandardCameraView faces[] = { StandardCameraView::Top, StandardCameraView::Bottom, StandardCameraView::Front,
                                         StandardCameraView::Back, StandardCameraView::Left, StandardCameraView::Right };

    // -------------------------------------------------------------------------
    // TEST 434 : orientations calculées (ViewManager, sans vue) — tableau des vues, cube, plans, base orthonormée
    // -------------------------------------------------------------------------
    {
        struct Expected { StandardCameraView v; gp_Dir dir, up, right; };
        const Expected table[] = {
            { StandardCameraView::Top, gp_Dir(0, 0, -1), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0) },
            { StandardCameraView::Bottom, gp_Dir(0, 0, 1), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0) },
            { StandardCameraView::Front, gp_Dir(0, 1, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0) },
            { StandardCameraView::Back, gp_Dir(0, -1, 0), gp_Dir(0, 0, 1), gp_Dir(-1, 0, 0) },
            { StandardCameraView::Left, gp_Dir(1, 0, 0), gp_Dir(0, 0, 1), gp_Dir(0, -1, 0) },
            { StandardCameraView::Right, gp_Dir(-1, 0, 0), gp_Dir(0, 0, 1), gp_Dir(0, 1, 0) },
        };
        for (const auto& e : table)
        {
            const CameraFrame f = ViewManager::standardFrame(e.v);
            TEST_CHECK(sameDir(f.direction, e.dir, 1e-15) && sameDir(f.up, e.up, 1e-15) && sameDir(f.right(), e.right, 1e-15) && orthonormal(f),
                       "Test 434: vue " << faceName(e.v) << " : direction, haut et droite de l'écran exacts");
        }
        const CameraFrame iso = ViewManager::standardFrame(StandardCameraView::ThreeD_Axo);
        TEST_CHECK(sameDir(iso.direction, gp_Dir(-1, 1, -1), 1e-12) && orthonormal(iso) && iso.up.Z() > 0.8,
                   "Test 434: isométrique : regard vers (-1, 1, -1), Z vers le haut de l'écran");

        // Les 26 orientations du cube : base orthonormée, Z jamais vers le bas, faces identiques aux vues standard
        int cube = 0;
        for (int o = V3d_Xpos; o <= V3d_XnegYnegZneg; ++o)
        {
            const CameraFrame f = ViewManager::frameForOrientation(static_cast<V3d_TypeOfOrientation>(o));
            const bool vertical = std::abs(f.direction.Z()) > 1.0 - 1e-12;
            TEST_CHECK(orthonormal(f) && (vertical || f.up.Z() > 0.0), "Test 434: orientation " << o << " du cube : base orthonormée, Z vers le haut");
            ++cube;
        }
        TEST_CHECK(cube == 26, "Test 434: 26 orientations du cube (6 faces, 12 arêtes, 8 coins)");
        for (const auto& e : table)
        {
            const CameraFrame a = ViewManager::frameForOrientation(ViewManager::orientationOf(e.v));
            const CameraFrame b = ViewManager::standardFrame(e.v);
            TEST_CHECK(sameDir(a.direction, b.direction, 0.0) && sameDir(a.up, b.up, 0.0), "Test 434: face du cube = vue standard " << faceName(e.v));
        }

        // Plans : globaux = faces (XY = dessus, XZ = avant, YZ = droite), inclinés et presque dégénérés stables
        using TSA::Coordinate::WorkPlane;
        using T = TSA::Coordinate::WorkPlaneType;
        auto planeIsFace = [](const WorkPlane& wp, StandardCameraView v) {
            const CameraFrame p = ViewManager::planeFrame(wp), f = ViewManager::standardFrame(v);
            return sameDir(p.direction, f.direction, 1e-15) && sameDir(p.up, f.up, 1e-15);
        };
        TEST_CHECK(planeIsFace(WorkPlane(T::GlobalXY, "XY", 3.0), StandardCameraView::Top) && planeIsFace(WorkPlane(T::GlobalXZ, "XZ", 2.0), StandardCameraView::Front)
                       && planeIsFace(WorkPlane(T::GlobalYZ, "YZ", -1.0), StandardCameraView::Right),
                   "Test 434: plans globaux vus de face : XY = dessus, XZ = élévation avant, YZ = élévation droite");
        int planes = 0;
        for (double angle : { 0.3, 30.0, 45.0, 60.0, 89.9, 89.999, 135.0, 179.99 })
        {
            const double a = angle * 3.14159265358979323846 / 180.0;
            const WorkPlane wp(gp_Ax3(gp_Pnt(1, -2, 3), gp_Dir(0, -std::sin(a), std::cos(a)), gp_Dir(1, 0, 0)), "Incliné");
            const CameraFrame f = ViewManager::planeFrame(wp), back = ViewManager::planeFrame(wp, true);
            TEST_CHECK(orthonormal(f) && sameDir(f.direction, wp.normal().Reversed(), 1e-12) && sameDir(f.up, wp.yDirection(), 1e-12)
                           && sameDir(back.direction, wp.normal(), 1e-12),
                       "Test 434: plan incliné à " << angle << "° : regard selon -N, haut = V, base orthonormée");
            ++planes;
        }
        TEST_CHECK(planes == 8, "Test 434: 8 inclinaisons, de 0,3° à 179,99°");
        std::cout << "[PASS] Test 434: Orientations des vues, du cube et des plans (calculées)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 439 : grille (pas affiché) et vues nommées (JSON, .tsa, compatibilité) — calculs, sans vue
    // -------------------------------------------------------------------------
    {
        using G = TSA::Viewer::WorkPlaneGrid;
        TEST_CHECK(G::displayedStep(1.0, 0.01, 20.0) == 1.0 && G::displayedStep(1.0, 0.2, 20.0) == 5.0 && G::displayedStep(1.0, 1.0, 20.0) == 20.0,
                   "Test 439: pas affiché = pas du plan × 1, 5, 20 pour 100, 5 et 1 pixel(s) par mètre (au moins 12 pixels entre lignes)");
        TEST_CHECK(G::displayedStep(0.5, 0.1, 20.0) == 2.5 && G::displayedStep(1.0, 1e-6, 1000.0) == 5.0,
                   "Test 439: pas de 0,5 m → 2,5 m ; plan de 2 km zoomé : au plus 400 lignes (pas de 5 m)");
        bool multiples = true;
        for (double base : { 0.25, 0.5, 1.0, 1.5, 3.0 })
            for (double wpp : { 1e-4, 0.003, 0.07, 0.9, 12.0 })
            {
                const double s = G::displayedStep(base, wpp, 50.0), k = s / base;
                multiples = multiples && std::abs(k - std::round(k)) < 1e-9 && s >= base && G::lineCount(50.0, s) <= G::kMaxLinesPerDirection + 1;
            }
        TEST_CHECK(multiples && G::lineCount(10.0, 1.0) == 21 && G::lineCount(10.0, 3.0) == 7,
                   "Test 439: chaque pas affiché est un multiple entier du pas d'accrochage ; nombre de lignes borné");

        using TSA::Model::NamedView;
        using TSA::Model::NamedViewSet;
        NamedView a;
        a.name = "Façade A";
        a.eye = { 10.0, -40.0, 5.0 };
        a.center = { 10.0, 0.0, 5.0 };
        a.up = { 0.0, 0.0, 1.0 };
        a.scale = 22.5;
        a.mode2D = true;
        a.planFace = "front";
        a.workPlaneJson = TSA::Coordinate::WorkPlane(TSA::Coordinate::WorkPlaneType::GlobalXZ, "File A", 0.0).toJson();
        NamedView b = a;
        b.name = "Perspective";
        b.orthographic = false;
        b.mode2D = false;
        b.planFace = "custom";
        b.workPlaneJson.clear();
        NamedViewSet set;
        set.upsert(a);
        set.upsert(b);
        TEST_CHECK(!set.rename("Façade A", "Perspective") && set.rename("Façade A", "Élévation A") && set.indexOf("Élévation A") == 0,
                   "Test 439: renommer : nom déjà pris refusé, nouveau nom accepté");
        const NamedViewSet back = NamedViewSet::fromJson(set.toJson());
        TEST_CHECK(back == set, "Test 439: vues nommées : aller-retour JSON identique (caméra, projection, mode 2D, face, plan de travail)");
        std::string warnings;
        const NamedViewSet tolerant = NamedViewSet::fromJson(
            R"({"schema":1,"views":[{"name":"ok","eye":[0,0,5],"center":[0,0,0],"up":[0,1,0],"scale":3},)"
            R"({"name":"court","eye":[0,0],"center":[0,0,0],"up":[0,1,0],"scale":3},)"
            R"({"name":"ok","eye":[0,0,5],"center":[0,0,0],"up":[0,1,0],"scale":3},)"
            R"({"name":"nulle","eye":[0,0,5],"center":[0,0,0],"up":[0,1,0],"scale":0}]})", &warnings);
        TEST_CHECK(tolerant.views.size() == 1 && warnings.find("court") != std::string::npos && warnings.find("nulle") != std::string::npos,
                   "Test 439: lecture tolérante : vues invalides ou en double ignorées et signalées");

        Model m;
        m.addNode(0.0, 0.0, 0.0);
        m.setModified(false);
        const auto revision = m.revision();
        m.setNamedViews(set);
        TEST_CHECK(m.isModified() && m.revision() == revision,
                   "Test 439: vues enregistrées : projet à enregistrer, révision inchangée (résultats de calcul toujours valides)");
        const auto path = std::filesystem::temp_directory_path() / "tsa_test439.tsa";
        const QString file = QString::fromStdWString(path.wstring());
        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 439: enregistrement .tsa (" << err << ")");
        TSA::IO::TSAFileHeader header;
        const bool headerOk = TSA::IO::TSAFileReader::readHeader(path.string(), header);
        Model reread;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, reread, nullptr, &err) && headerOk, "Test 439: relecture .tsa (" << err << ")");
        std::filesystem::remove(path);
        TEST_CHECK(header.versionMinor == 12, "Test 439: .tsa enregistré au format 1.12 (lu : 1." << header.versionMinor << ")");
        TEST_CHECK(reread.namedViews() == set, "Test 439: .tsa (chunk VIEW) : vues relues à l'identique (" << reread.namedViews().views.size() << " vue(s) relue(s))");
        // (l'état « modifié » après ouverture est remis à zéro par ProjectManager, comme pour tout projet)
        Model old;
        const auto example = std::filesystem::path(TSA_SOURCE_DIR) / "examples" / "TSA_Demo_Batiment_R+5.tsa";
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(QString::fromStdWString(example.wstring()), old, nullptr, &err) && old.namedViews().views.empty()
                       && !old.nodes().empty(),
                   "Test 439: projet antérieur au chunk VIEW : ouvert, aucune vue nommée");
        std::cout << "[PASS] Test 439: Grille (pas affiché) et vues nommées (JSON, .tsa, compatibilité)" << std::endl;
        ++passed;
    }

    if (offscreen)
    {
        for (int t : { 433, 435, 436, 437, 438, 440, 441, 442 })
            std::cout << "[SKIP] Test " << t << ": vue 3D indisponible sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
        return true;
    }

    // -------------------------------------------------------------------------
    // TEST 433 : défauts B1-B4 constatés dans la vue réelle (non-régression)
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        const bool orthoAtStart = h.cam()->IsOrthographic();
        TEST_CHECK(orthoAtStart, "Test 433: caméra orthographique au démarrage");

        view.setViewPlaneMode(OccView::ViewPlaneMode::PlanXY);   // bouton « Plan XY (Vue d'étage) »
        pump();
        TEST_CHECK(view.isMode2D() && h.cam()->IsOrthographic() && cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Top)),
                   "Test 433 (B1): « Plan XY » = vrai plan 2D (mode 2D, orthographique, caméra perpendiculaire au plan XY)");
        view.setViewPlaneMode(OccView::ViewPlaneMode::Perspective3D);
        pump();
        TEST_CHECK(!view.isMode2D() && h.cam()->IsOrthographic() == orthoAtStart, "Test 433 (B2): retour en 3D : projection d'avant le mode 2D (pas de perspective imposée)");

        const gp_Ax3 inclined(gp_Pnt(1, 2, 3), gp_Dir(0, -std::sin(0.5), std::cos(0.5)), gp_Dir(1, 0, 0));
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(inclined, "Incliné"));
        const CameraFrame expected = ViewManager::planeFrame(view.activeWorkPlane());
        bool sameEveryTime = true;
        for (int start = 0; start < 4; ++start)
        {
            if (start == 0) view.viewTop();
            if (start == 1) view.viewFront();
            if (start == 2) view.viewBottom();
            if (start == 3) view.viewIsometric();
            view.viewNormalToWorkPlane();
            pump(2);
            sameEveryTime = sameEveryTime && cameraIs(h.cam(), expected);
        }
        TEST_CHECK(sameEveryTime, "Test 433 (B3): orientation sur le plan incliné identique depuis le dessus, l'avant, le dessous et l'isométrie");

        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane());
        view.setMode2D(true);
        pump();
        view.setViewOrientation(V3d_TypeOfOrientation_Zup_AxoRight);
        pump();
        TEST_CHECK(!view.isMode2D() && cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::ThreeD_Axo)),
                   "Test 433 (B4): coin du cube en mode 2D : retour en 3D, orientation exacte (jamais de 2D incliné)");
        std::cout << "[PASS] Test 433: Défauts B1-B4 corrigés dans la vue réelle" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 435 : vues standard et ViewCube — orientation finale exacte quelle que soit la vue de départ, projection,
    // synchronisation du mode 2D, historique
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        // Départs variés : isométrique, dessous, coin, rotation libre à la souris
        auto startFrom = [&](int k) {
            if (k == 0) view.viewIsometric();
            if (k == 1) view.viewBottom();
            if (k == 2) view.showCubeOrientation(V3d_XnegYposZneg);
            if (k == 3)
            {
                view.viewIsometric();
                view.view()->StartRotation(400, 300);
                view.view()->Rotation(470, 250);
            }
            pump(2);
        };
        for (StandardCameraView v : faces)
        {
            for (int k = 0; k < 4; ++k)
            {
                startFrom(k);
                view.showStandardView(v);
                pump(2);
                TEST_CHECK(cameraIs(h.cam(), ViewManager::standardFrame(v)) && h.cam()->IsOrthographic() && !view.isMode2D(),
                           "Test 435: vue " << faceName(v) << " depuis le départ " << k << " : orientation exacte, orthographique, en 3D");
            }
        }
        // ViewCube : 26 orientations, transition comprise, orientation finale exacte
        view.setViewTransitionMs(60);
        for (int o = V3d_Xpos; o <= V3d_XnegYnegZneg; ++o)
        {
            view.showCubeOrientation(static_cast<V3d_TypeOfOrientation>(o));
            pump(2);
            TEST_CHECK(cameraIs(h.cam(), ViewManager::frameForOrientation(static_cast<V3d_TypeOfOrientation>(o))),
                       "Test 435: ViewCube, orientation " << o << " : caméra exactement sur l'orientation après la transition");
        }
        view.setViewTransitionMs(0);
        // Mode 2D : une face du cube change de plan 2D, une arête ramène en 3D
        view.showPlanView(StandardCameraView::Top);
        view.showCubeOrientation(V3d_Yneg);
        pump(2);
        TEST_CHECK(view.isMode2D() && view.plan2DFace() == StandardCameraView::Front && view.activeWorkPlane().type() == TSA::Coordinate::WorkPlaneType::GlobalXZ
                       && cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Front)),
                   "Test 435: mode 2D, face AVANT du cube : élévation 2D, plan de travail XZ, caméra exacte");
        view.showCubeOrientation(V3d_XposZpos);
        pump(2);
        TEST_CHECK(!view.isMode2D() && cameraIs(h.cam(), ViewManager::frameForOrientation(V3d_XposZpos)),
                   "Test 435: mode 2D, arête du cube : vue 3D à l'orientation exacte");
        // Vrai clic souris (appui + relâchement) sur une arête du cube depuis le plan 2D de dessus : orientation d'arête
        // ou de coin, la même à chaque clic (le relâchement ne relance plus l'animation interne d'OCCT sur la face
        // devenue visible sous le curseur — défaut constaté dans l'application réelle : vue de droite à la place)
        {
            int winW = 0, winH = 0;
            view.view()->Window()->Size(winW, winH);
            const double k = double(view.width()) / winW;
            const QPointF edge((winW - 85 + 33) * k, (85 + 33) * k);   // centre du cube à 85 px des bords ; face ±31 px
            auto clickCubeFromPlan = [&]() {
                view.showPlanView(StandardCameraView::Top);
                pump(3);
                QMouseEvent mv(QEvent::MouseMove, edge, view.mapToGlobal(edge), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&view, &mv);
                pump(3);
                h.click(edge);
                pump(3);
                return h.cam()->Direction();
            };
            const gp_Dir first = clickCubeFromPlan();
            const gp_Dir second = clickCubeFromPlan();
            bool edgeOrCorner = false;
            for (int o = V3d_XposYpos; o <= V3d_XnegYnegZneg; ++o)
                edgeOrCorner = edgeOrCorner || cameraIs(h.cam(), ViewManager::frameForOrientation(static_cast<V3d_TypeOfOrientation>(o)));
            const double maxComponent = std::max({ std::abs(first.X()), std::abs(first.Y()), std::abs(first.Z()) });
            TEST_CHECK(!view.isMode2D() && edgeOrCorner && maxComponent < 0.99 && sameDir(first, second),
                       "Test 435: clic réel sur une arête du ViewCube : orientation d'arête ou de coin exacte (pas une face), identique à chaque clic");
        }
        // Historique : une vue précédente non perpendiculaire ne peut pas rester en mode 2D
        view.showPlanView(StandardCameraView::Top);
        view.previousView();
        pump(2);
        TEST_CHECK(!view.isMode2D() || std::abs(std::abs(h.cam()->Direction().Dot(view.activeWorkPlane().normal())) - 1.0) < 1e-9,
                   "Test 435: vue précédente depuis le mode 2D : jamais de 2D avec une caméra inclinée");
        // Orbite bloquée en mode 2D : glisser au bouton droit = panoramique, direction inchangée
        view.showPlanView(StandardCameraView::Top);
        const gp_Dir before = h.cam()->Direction();
        const QPointF a(400, 300), b(480, 260);
        QMouseEvent press(QEvent::MouseButtonPress, a, view.mapToGlobal(a), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &press);
        QMouseEvent move(QEvent::MouseMove, b, view.mapToGlobal(b), Qt::NoButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, b, view.mapToGlobal(b), Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &release);
        pump(2);
        TEST_CHECK(view.isMode2D() && sameDir(h.cam()->Direction(), before), "Test 435: mode 2D, glisser au bouton droit : panoramique, la vue ne pivote pas");
        view.zoomIn(1.5);
        TEST_CHECK(sameDir(h.cam()->Direction(), before) && view.isMode2D(), "Test 435: mode 2D, zoom : la vue ne s'incline pas");
        view.show3DView();
        std::cout << "[PASS] Test 435: Vues standard et ViewCube (orientations exactes, mode 2D synchronisé)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 436 : dessin dans chaque vue — plan d'étage au niveau actif, élévations XZ et YZ ; coordonnées réelles,
    // accrochage à un nœud existant dans le plan, Annuler / Rétablir, géométrie intacte au retour en 3D
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        view.setWorkPlaneElevation(3.0);   // niveau actif Z = 3 m
        view.showPlanView(StandardCameraView::Top);
        pump();
        TEST_CHECK(view.isMode2D() && std::abs(view.activeWorkPlane().normal().Z()) > 1.0 - 1e-12 && std::abs(view.activeWorkPlane().origin().Z() - 3.0) < 1e-12,
                   "Test 436: plan d'étage : plan de travail horizontal au niveau actif (Z = 3 m)");
        auto drawBeam = [&](const gp_Pnt& p1, const gp_Pnt& p2) {
            view.setInteractionMode(OccView::InteractionMode::DrawBeam);
            h.click(h.toMouse(p1));
            h.click(h.toMouse(p2));
            view.cancelCurrentDrawing();
        };
        auto node = [&](const gp_Pnt& target) -> const Node* {
            const Node* best = nullptr;
            double d = 1e30;
            for (const auto& [id, n] : m.nodes())
            {
                const double dn = gp_Pnt(n.x(), n.y(), n.z()).Distance(target);
                if (dn < d) { d = dn; best = &n; }
            }
            return best;
        };
        drawBeam(gp_Pnt(1, 1, 3), gp_Pnt(6, 1, 3));
        const double tolPlan = 1.5 * h.pixelSize();
        const Node* a = node(gp_Pnt(1, 1, 3));
        const Node* b = node(gp_Pnt(6, 1, 3));
        TEST_CHECK(m.beams().size() == 1 && m.nodes().size() == 2 && a && b && a != b, "Test 436: plan d'étage : poutre tracée (2 nœuds)");
        TEST_CHECK(a->z() == 3.0 && b->z() == 3.0, "Test 436: plan d'étage : nœuds exactement au niveau (Z = 3 m)");
        TEST_CHECK(std::abs(a->x() - 1) < tolPlan && std::abs(a->y() - 1) < tolPlan && std::abs(b->x() - 6) < tolPlan && std::abs(b->y() - 1) < tolPlan,
                   "Test 436: plan d'étage : X et Y du clic à moins de 1,5 pixel (" << tolPlan << " m)");

        // Accrochage au nœud existant dans le plan : nouvelle poutre reliée, aucun nœud en double
        view.setObjectSnapEnabled(true);
        drawBeam(gp_Pnt(b->x(), b->y(), 3), gp_Pnt(6, 5, 3));
        view.setObjectSnapEnabled(false);
        TEST_CHECK(m.beams().size() == 2 && m.nodes().size() == 3, "Test 436: plan d'étage : 2e poutre accrochée au nœud existant (3 nœuds, pas de doublon)");

        // Élévation avant : plan XZ, dessin à Y = constante
        view.showPlanView(StandardCameraView::Front);
        pump();
        TEST_CHECK(view.activeWorkPlane().type() == TSA::Coordinate::WorkPlaneType::GlobalXZ && cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Front)),
                   "Test 436: élévation avant : plan de travail XZ, caméra perpendiculaire");
        const double yPlane = view.activeWorkPlane().origin().Y();
        drawBeam(gp_Pnt(1, yPlane, 0), gp_Pnt(1, yPlane, 4));
        const Node* c = node(gp_Pnt(1, yPlane, 0));
        const Node* d = node(gp_Pnt(1, yPlane, 4));
        TEST_CHECK(m.beams().size() == 3 && c && d && c->y() == yPlane && d->y() == yPlane && std::abs(d->z() - 4) < 1.5 * h.pixelSize(),
                   "Test 436: élévation avant : poutre dans le plan XZ (Y exactement " << yPlane << " m)");

        // Élévation droite : plan YZ
        view.showPlanView(StandardCameraView::Right);
        pump();
        const double xPlane = view.activeWorkPlane().origin().X();
        drawBeam(gp_Pnt(xPlane, 2, 1), gp_Pnt(xPlane, 7, 1));
        const Node* e = node(gp_Pnt(xPlane, 2, 1));
        TEST_CHECK(m.beams().size() == 4 && e && e->x() == xPlane && view.activeWorkPlane().type() == TSA::Coordinate::WorkPlaneType::GlobalYZ,
                   "Test 436: élévation droite : poutre dans le plan YZ (X exactement " << xPlane << " m)");

        const auto snapshot = nodeSnapshot(m);
        view.show3DView();
        view.showCubeOrientation(V3d_XnegYnegZpos);
        view.showPlanView(StandardCameraView::Back);
        view.show3DView();
        pump();
        TEST_CHECK(!view.isMode2D() && sameSnapshot(snapshot, nodeSnapshot(m)), "Test 436: retour en 3D après plusieurs vues : coordonnées de tous les nœuds inchangées");
        TEST_CHECK(m.undo() && m.beams().size() == 3 && m.redo() && m.beams().size() == 4 && sameSnapshot(snapshot, nodeSnapshot(m)),
                   "Test 436: Annuler / Rétablir après les changements de vue : modèle rendu à l'identique");
        std::cout << "[PASS] Test 436: Dessin dans le plan d'étage et les élévations (coordonnées réelles)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 437 : projet existant (.tsa du dépôt) — géométrie jamais modifiée par les vues ; conversion écran ↔ plan
    // -------------------------------------------------------------------------
    {
        const auto file = std::filesystem::path(TSA_SOURCE_DIR) / "examples" / "TSA_Demo_Batiment_R+5.tsa";
        Model m;
        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(QString::fromStdWString(file.wstring()), m, nullptr, &err) && !m.nodes().empty(),
                   "Test 437: projet de démonstration (enregistré le 11/10/2026) ouvert (" << err << ")");
        const auto snapshot = nodeSnapshot(m);
        const size_t beams = m.beams().size(), columns = m.columns().size();
        ViewHarness h(m);
        OccView& view = h.view;
        for (StandardCameraView v : faces)
        {
            view.showStandardView(v);
            view.showPlanView(v);
            pump(2);
            // Conversion écran → plan : un point du plan revient sur lui-même (à un pixel près), exactement dans le plan
            const TSA::Coordinate::WorkPlane& wp = view.activeWorkPlane();
            // Grille : couvre l'emprise de tous les nœuds projetés dans le plan (modèle loin de l'origine du plan)
            const TSA::Viewer::WorkPlaneGrid* g = view.workPlaneGrid();
            bool covered = g != nullptr;
            for (const auto& [id, nd] : m.nodes())
            {
                double u = 0.0, w = 0.0;
                if (!covered) break;
                wp.toLocal(gp_Pnt(nd.x(), nd.y(), nd.z()), u, w);
                covered = std::abs(u - g->layout().centerU) <= g->layout().halfU + 1e-9 && std::abs(w - g->layout().centerV) <= g->layout().halfV + 1e-9;
            }
            TEST_CHECK(covered, "Test 437: vue " << faceName(v) << " : grille du plan couvrant tous les nœuds du projet");
            for (const auto& [u, w] : { std::pair{ 0.0, 0.0 }, std::pair{ 7.5, 3.0 }, std::pair{ -4.0, 10.0 } })
            {
                const gp_Pnt p = wp.origin().Translated(gp_Vec(wp.xDirection()) * u + gp_Vec(wp.yDirection()) * w);
                int px = 0, py = 0;
                view.worldToPixel(p.X(), p.Y(), p.Z(), px, py);
                double x = 0, y = 0, z = 0;
                TEST_CHECK(view.pixelToWorldPlane(px, py, x, y, z), "Test 437: conversion écran → plan (" << faceName(v) << ")");
                const gp_Pnt q(x, y, z);
                TEST_CHECK(std::abs(wp.distanceTo(q)) < 1e-9 && q.Distance(p) < 1.5 * h.pixelSize(),
                           "Test 437: vue " << faceName(v) << " : point du plan retrouvé à moins de 1,5 pixel, exactement dans le plan");
            }
        }
        for (int o = V3d_Xpos; o <= V3d_XnegYnegZneg; ++o)
            view.showCubeOrientation(static_cast<V3d_TypeOfOrientation>(o));
        view.show3DView();
        pump();
        TEST_CHECK(sameSnapshot(snapshot, nodeSnapshot(m)) && m.beams().size() == beams && m.columns().size() == columns && !m.canUndo(),
                   "Test 437: après toutes les vues (6 faces en 3D et en 2D, 26 orientations du cube) : " << snapshot.size()
                       << " nœuds identiques au bit près, aucun élément ni entrée Annuler créés");
        std::cout << "[PASS] Test 437: Projet existant : géométrie intacte, conversion écran ↔ plan" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 438 : plans de travail — adaptation à la vue en 3D, plan nommé conservé, plan incliné en mode 2D
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        view.setWorkPlaneElevation(6.0);
        view.viewFront();   // plan horizontal vu par la tranche : remplacé par un plan XZ
        TEST_CHECK(view.activeWorkPlane().type() == TSA::Coordinate::WorkPlaneType::GlobalXZ && !view.isMode2D(),
                   "Test 438: vue avant en 3D, plan horizontal vu par la tranche : plan de travail XZ à la place");
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(TSA::Coordinate::WorkPlaneType::GlobalXY, "R+2", 6.0));
        view.viewTop();
        TEST_CHECK(view.activeWorkPlane().name() == "R+2" && std::abs(view.activeWorkPlane().origin().Z() - 6.0) < 1e-12,
                   "Test 438: vue de dessus : plan d'étage « R+2 » conservé (déjà face à la vue)");
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(TSA::Coordinate::WorkPlaneType::GlobalXZ, "File A", 4.0));
        view.showPlanView(StandardCameraView::Back);
        TEST_CHECK(view.activeWorkPlane().name() == "File A" && cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Back)),
                   "Test 438: élévation arrière : plan nommé « File A » conservé, vue de l'autre côté");
        view.show3DView();
        view.showPlanView(StandardCameraView::Front);
        TEST_CHECK(std::abs(view.activeWorkPlane().origin().Y() - 4.0) < 1e-12, "Test 438: élévation rouverte sur le dernier plan XZ utilisé (Y = 4 m)");
        view.show3DView();

        // Plan incliné à 30° : mode 2D perpendiculaire, point cliqué exactement dans le plan
        const gp_Ax3 inclined(gp_Pnt(0, 0, 2), gp_Dir(0, -0.5, std::sqrt(3.0) / 2.0), gp_Dir(1, 0, 0));
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(inclined, "Toiture"));
        view.setMode2D(true);
        pump();
        const TSA::Coordinate::WorkPlane& wp = view.activeWorkPlane();
        TEST_CHECK(view.isMode2D() && view.plan2DFace() == StandardCameraView::Custom && cameraIs(h.cam(), ViewManager::planeFrame(wp)),
                   "Test 438: plan incliné en mode 2D : caméra selon -N, haut = V du plan");
        view.setInteractionMode(OccView::InteractionMode::DrawBeam);
        const gp_Pnt p1 = wp.origin().Translated(gp_Vec(wp.xDirection()) * 1.0 + gp_Vec(wp.yDirection()) * 2.0);
        const gp_Pnt p2 = wp.origin().Translated(gp_Vec(wp.xDirection()) * 5.0 + gp_Vec(wp.yDirection()) * 2.0);
        h.click(h.toMouse(p1));
        h.click(h.toMouse(p2));
        view.cancelCurrentDrawing();
        bool onPlane = m.nodes().size() == 2;
        for (const auto& [id, n] : m.nodes())
            onPlane = onPlane && std::abs(wp.distanceTo(gp_Pnt(n.x(), n.y(), n.z()))) < 1e-9;
        TEST_CHECK(onPlane && m.beams().size() == 1, "Test 438: plan incliné : poutre tracée, nœuds exactement dans le plan (écart < 1e-9 m)");
        view.show3DView();
        std::cout << "[PASS] Test 438: Plans de travail adaptés à la vue, plan incliné" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 440 : grille du plan de travail dans la vue — lignes au pas d'accrochage, densité selon le zoom, grille
    // portée par le plan (niveau, élévation), masquable ; accrochage au pas affiché, exactement dans le plan
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        view.setWorkPlaneElevation(3.0);
        view.showPlanView(StandardCameraView::Top);
        pump();
        const TSA::Viewer::WorkPlaneGrid* grid = view.workPlaneGrid();
        auto isMultiple = [](double step, double base) { const double k = step / base; return std::abs(k - std::round(k)) < 1e-9 && k >= 1.0 - 1e-12; };
        TEST_CHECK(grid && isMultiple(grid->layout().stepU, view.activeWorkPlane().gridSpacingX()) && grid->layout().stepU / h.pixelSize() >= 12.0 - 1e-9,
                   "Test 440: plan d'étage : grille affichée, pas multiple du pas d'accrochage, lignes espacées d'au moins 12 pixels");
        const double s0 = grid->layout().stepU;
        for (int k = 0; k < 6; ++k)
            view.zoomOut(2.0);
        const double s1 = view.workPlaneGrid()->layout().stepU;
        TEST_CHECK(s1 > s0 && TSA::Viewer::WorkPlaneGrid::lineCount(view.workPlaneGrid()->layout().halfU, s1, view.workPlaneGrid()->layout().centerU) <= TSA::Viewer::WorkPlaneGrid::kMaxLinesPerDirection + 1
                       && s1 / h.pixelSize() >= 12.0 - 1e-9,
                   "Test 440: dézoom : pas affiché agrandi (" << s0 << " → " << s1 << " m), densité lisible");
        for (int k = 0; k < 12; ++k)
            view.zoomIn(2.0);
        const double s2 = view.workPlaneGrid()->layout().stepU;
        TEST_CHECK(s2 <= s0 && isMultiple(s2, view.activeWorkPlane().gridSpacingX()), "Test 440: zoom avant : pas affiché revenu au pas d'accrochage (" << s2 << " m)");

        auto planePoint = [](const TSA::Viewer::WorkPlaneGrid* g, const gp_Pnt& local) { return local.Transformed(g->LocalTransformation()); };
        TEST_CHECK(planePoint(view.workPlaneGrid(), gp_Pnt(0, 0, 0)).Distance(gp_Pnt(0, 0, 3)) < 1e-12,
                   "Test 440: grille portée par le niveau actif (Z = 3 m)");
        view.showPlanView(StandardCameraView::Front);
        pump();
        const gp_Pnt n = planePoint(view.workPlaneGrid(), gp_Pnt(0, 0, 1));
        const gp_Pnt o = planePoint(view.workPlaneGrid(), gp_Pnt(0, 0, 0));
        TEST_CHECK(gp_Vec(o, n).IsEqual(gp_Vec(0, -1, 0), 1e-12, 1e-12), "Test 440: élévation avant : grille dans le plan XZ (normale -Y)");

        TSA::Coordinate::WorkPlane hidden = view.activeWorkPlane();
        hidden.setIsGridVisible(false);
        view.setActiveWorkPlane(hidden);
        TEST_CHECK(view.workPlaneGrid() == nullptr, "Test 440: grille du plan masquée : aucune ligne affichée");
        hidden.setIsGridVisible(true);
        view.setActiveWorkPlane(hidden);
        TEST_CHECK(view.workPlaneGrid() != nullptr, "Test 440: grille réaffichée");

        // Accrochage à la grille (objets coupés) : point accroché au pas affiché, exactement dans le plan d'étage
        view.showPlanView(StandardCameraView::Top);
        pump();
        view.setGridSnapEnabled(true);
        const double step = view.workPlaneGrid()->layout().stepU;
        const gp_Pnt raw1(2.3 * step, 1.2 * step, 3.0), raw2(5.2 * step, 1.1 * step, 3.0);
        view.setInteractionMode(OccView::InteractionMode::DrawBeam);
        h.click(h.toMouse(raw1));
        h.click(h.toMouse(raw2));
        view.cancelCurrentDrawing();
        view.setGridSnapEnabled(false);
        bool onGrid = m.nodes().size() == 2;
        for (const auto& [id, nd] : m.nodes())
            onGrid = onGrid && isMultiple(std::abs(nd.x()) + step, step) && isMultiple(std::abs(nd.y()) + step, step) && nd.z() == 3.0;
        TEST_CHECK(onGrid && m.beams().size() == 1, "Test 440: accrochage à la grille : nœuds aux multiples du pas affiché (" << step << " m), Z = 3 m exactement");
        view.show3DView();
        std::cout << "[PASS] Test 440: Grille du plan de travail (pas, zoom, plan, accrochage)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 441 : vues nommées dans la vue — capture et restauration exactes (mode 2D, face, plan de travail, caméra),
    // vue 3D libre, vue invalide refusée ; dialogue : enregistrer, renommer, supprimer, restaurer ; projets séparés
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        auto sameCamera = [&](const TSA::Model::NamedView& v) {
            const auto& c = h.cam();
            const double tol = 1e-9 * std::max(1.0, v.scale);
            return c->Eye().Distance(gp_Pnt(v.eye[0], v.eye[1], v.eye[2])) < tol && c->Center().Distance(gp_Pnt(v.center[0], v.center[1], v.center[2])) < tol
                   && c->Up().IsEqual(gp_Dir(v.up[0], v.up[1], v.up[2]), 1e-9) && std::abs(c->Scale() - v.scale) < tol
                   && c->IsOrthographic() == v.orthographic;
        };
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(TSA::Coordinate::WorkPlaneType::GlobalXZ, "File B", 6.0));
        view.showPlanView(StandardCameraView::Front);
        view.zoomIn(1.7);
        const TSA::Model::NamedView v2d = view.captureNamedView("Élévation B");
        view.show3DView();
        view.showCubeOrientation(V3d_XnegYnegZpos);
        const TSA::Model::NamedView v3d = view.captureNamedView("Coin");
        view.showPlanView(StandardCameraView::Top);
        TEST_CHECK(view.restoreNamedView(v2d) && view.isMode2D() && view.plan2DFace() == StandardCameraView::Front,
                   "Test 441: vue 2D restaurée depuis un autre plan : mode 2D, élévation avant");
        TEST_CHECK(view.activeWorkPlane().name() == "File B" && std::abs(view.activeWorkPlane().origin().Y() - 6.0) < 1e-12,
                   "Test 441: plan de travail de la vue restauré (« " << view.activeWorkPlane().name() << " », Y = " << view.activeWorkPlane().origin().Y() << " m)");
        TEST_CHECK(sameCamera(v2d), "Test 441: caméra exacte, zoom compris (échelle " << h.cam()->Scale() << " / " << v2d.scale << ")");
        TEST_CHECK(view.restoreNamedView(v3d) && !view.isMode2D() && sameCamera(v3d), "Test 441: vue 3D libre restaurée depuis le mode 2D : retour en 3D, caméra exacte");
        TSA::Model::NamedView invalid = v3d;
        invalid.scale = 0.0;
        TEST_CHECK(!view.restoreNamedView(invalid) && sameCamera(v3d), "Test 441: vue invalide refusée, caméra inchangée");

        TSA::UI::NamedViewsDialog dialog(&m, &view);
        int changes = 0;
        QObject::connect(&dialog, &TSA::UI::NamedViewsDialog::namedViewsChanged, [&changes] { ++changes; });
        m.setModified(false);
        const auto revision = m.revision();
        TEST_CHECK(dialog.saveCurrentView("A") && dialog.saveCurrentView("B") && !dialog.saveCurrentView("  ") && dialog.renameView("A", "Façade")
                       && !dialog.renameView("Façade", "B") && dialog.deleteView("B") && dialog.viewNames() == QStringList({ "Façade" }) && changes == 4,
                   "Test 441: dialogue : enregistrer, renommer (nom pris refusé), supprimer ; 4 modifications signalées");
        TEST_CHECK(m.isModified() && m.revision() == revision && dialog.restoreView("Façade") && !dialog.restoreView("absente"),
                   "Test 441: projet à enregistrer sans changer la révision ; restauration par le nom, nom absent refusé");
        Model other;
        TSA::UI::NamedViewsDialog otherDialog(&other, &view);
        TEST_CHECK(otherDialog.viewNames().isEmpty() && m.namedViews().views.size() == 1, "Test 441: un autre projet ne voit pas les vues de celui-ci");
        view.show3DView();
        std::cout << "[PASS] Test 441: Vues nommées (capture, restauration, dialogue, projets séparés)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 442 : dessin dans les vues arrière, gauche et dessous, sur plusieurs niveaux, et dalle + voile sur un plan
    // incliné : coordonnées réelles exactement dans le plan actif ; géométrie intacte au retour en 3D
    // -------------------------------------------------------------------------
    {
        Model m;
        ViewHarness h(m);
        OccView& view = h.view;
        auto newNodes = [&](const std::map<int, gp_Pnt>& before) {
            std::vector<gp_Pnt> pts;
            for (const auto& [id, nd] : m.nodes())
                if (!before.count(id)) pts.emplace_back(nd.x(), nd.y(), nd.z());
            return pts;
        };
        auto drawBeam = [&](const gp_Pnt& p1, const gp_Pnt& p2) {
            const auto before = nodeSnapshot(m);
            view.setInteractionMode(OccView::InteractionMode::DrawBeam);
            h.click(h.toMouse(p1));
            h.click(h.toMouse(p2));
            view.cancelCurrentDrawing();
            return newNodes(before);
        };
        for (double z : { 4.5, 9.0 })
        {
            view.setWorkPlaneElevation(z);
            view.showPlanView(StandardCameraView::Top);
            const auto pts = drawBeam(gp_Pnt(1, 1, z), gp_Pnt(5, 1, z));
            TEST_CHECK(pts.size() == 2 && pts[0].Z() == z && pts[1].Z() == z, "Test 442: plan du niveau Z = " << z << " m : nœuds exactement au niveau");
        }
        view.setWorkPlaneElevation(4.5);
        view.showPlanView(StandardCameraView::Bottom);
        TEST_CHECK(cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Bottom)), "Test 442: plan vu de dessous : caméra exacte");
        auto pts = drawBeam(gp_Pnt(2, 6, 4.5), gp_Pnt(6, 6, 4.5));
        const double tol = 1.5 * h.pixelSize();
        TEST_CHECK(pts.size() == 2 && pts[0].Z() == 4.5 && pts[1].Z() == 4.5 && std::abs(pts[0].Y() - 6.0) < tol,
                   "Test 442: vu de dessous : nœuds au niveau (Z = 4,5 m), Y du clic à 1,5 pixel près (X inversé à l'écran compris)");
        view.showPlanView(StandardCameraView::Back);
        const double y = view.activeWorkPlane().origin().Y();
        pts = drawBeam(gp_Pnt(1, y, 1), gp_Pnt(4, y, 1));
        TEST_CHECK(cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Back)) && pts.size() == 2 && pts[0].Y() == y && pts[1].Y() == y,
                   "Test 442: élévation arrière : nœuds dans le plan XZ (Y exactement " << y << " m)");
        view.showPlanView(StandardCameraView::Left);
        const double x = view.activeWorkPlane().origin().X();
        pts = drawBeam(gp_Pnt(x, 2, 2), gp_Pnt(x, 6, 2));
        TEST_CHECK(cameraIs(h.cam(), ViewManager::standardFrame(StandardCameraView::Left)) && pts.size() == 2 && pts[0].X() == x && pts[1].X() == x,
                   "Test 442: élévation gauche : nœuds dans le plan YZ (X exactement " << x << " m)");
        view.show3DView();

        // Plan incliné à 30° : dalle (3 clics + Entrée) et voile (2 clics), nœuds exactement dans le plan
        const gp_Ax3 inclined(gp_Pnt(0, 20, 3), gp_Dir(0, -0.5, std::sqrt(3.0) / 2.0), gp_Dir(1, 0, 0));
        view.setActiveWorkPlane(TSA::Coordinate::WorkPlane(inclined, "Rampe"));
        view.setMode2D(true);
        pump();
        const TSA::Coordinate::WorkPlane& wp = view.activeWorkPlane();
        auto onPlane = [&](const std::vector<gp_Pnt>& p) {
            bool ok = !p.empty();
            for (const gp_Pnt& q : p) ok = ok && std::abs(wp.distanceTo(q)) < 1e-9;
            return ok;
        };
        auto at = [&](double u, double v) { return wp.origin().Translated(gp_Vec(wp.xDirection()) * u + gp_Vec(wp.yDirection()) * v); };
        auto before = nodeSnapshot(m);
        const size_t slabs = m.slabs().size();
        view.setInteractionMode(OccView::InteractionMode::DrawSlab);
        h.click(h.toMouse(at(1, 1)));
        h.click(h.toMouse(at(5, 1)));
        h.click(h.toMouse(at(5, 4)));
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&view, &enter);
        pump(3);
        const auto slabNodes = newNodes(before);
        TEST_CHECK(m.slabs().size() == slabs + 1 && slabNodes.size() == 3 && onPlane(slabNodes), "Test 442: plan incliné : dalle créée, ses 3 nœuds exactement dans le plan");
        view.cancelCurrentDrawing();
        before = nodeSnapshot(m);
        const size_t walls = m.walls().size();
        view.setInteractionMode(OccView::InteractionMode::DrawWall);
        h.click(h.toMouse(at(-4, 1)));
        h.click(h.toMouse(at(-4, 5)));
        view.cancelCurrentDrawing();
        const auto wallNodes = newNodes(before);
        TEST_CHECK(m.walls().size() == walls + 1 && wallNodes.size() == 2 && onPlane(wallNodes),
                   "Test 442: plan incliné : voile créé, ses 2 nœuds de base exactement dans le plan (base inclinée acceptée)");
        const auto snapshot = nodeSnapshot(m);
        view.show3DView();
        view.showCubeOrientation(V3d_XposYposZpos);
        view.showPlanView(StandardCameraView::Right);
        view.show3DView();
        TEST_CHECK(sameSnapshot(snapshot, nodeSnapshot(m)), "Test 442: après les changements de vue : coordonnées de tous les nœuds inchangées");
        std::cout << "[PASS] Test 442: Dessin vu de dessous, arrière, gauche, sur plusieurs niveaux et sur plan incliné" << std::endl;
        ++passed;
    }
    return true;
}
