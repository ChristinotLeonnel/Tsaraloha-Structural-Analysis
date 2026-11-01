// Suite « loadaxes » : repères locaux et rotation de section selon Robot Structural Analysis, charges en axes globaux ou locaux.
// Référence : tests/data/robot_rsa2024_reference.json, mesures dans Robot 2024 par son API (voir
// docs/audit/2026-10-31/COORDINATE_SYSTEM_AND_SECTION_ROTATION_REPORT.md).
//   558  repère local d'une barre : horizontales, inclinées, verticales, sens inversé, γ positif et négatif, seuil de verticalité ;
//   559  copie par rotation : autour de Z, X, Y, 30° à 180° : repère et γ de Robot ;
//   560  copie miroir : plans X, Y, Z et diagonal : repère et γ de Robot ;
//   561  rotation et symétrie sans copie, inversion des nœuds ;
//   562  charges : une charge globale garde sa direction, une charge locale suit l'élément ; rotation, symétrie, copies multiples,
//        charges combinées, Annuler, enregistrement ;
//   563  anciens projets : charge « gravité », axes incohérents, surfaces, repère des barres verticales, fichier de version 2 ;
//   564  calcul comparé à Robot : consoles à section tournée, charges locales et globales, réactions et déplacements ;
//   565  transformations mathématiques : matrice orthogonale directe, F_global = R·F_local, F_local = Rᵀ·F_global, y × z = x.

#include "test_common.h"

#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/LoadResolver.h"
#include "Coordinate/CoordinateTransformationService.h"
#include "Grid/GridManager.h"
#include "IO/TSAFileFormat.h"
#include "Model/LegacyBarFrames.h"
#include "Model/Load/LegacyLoads.h"
#include "Model/Load/LoadManager.h"
#include "Model/StructuralClipboard.h"
#include "UndoRedo/EditTransaction.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstddef>

using namespace TSA::Analysis;
using TSA::Coordinate::CoordinateTransformationService;

namespace
{
constexpr double kTol = 1e-6;

bool same(double a, double b, double tol = 1e-9)
{
    return std::abs(a - b) <= tol;
}

QJsonObject reference()
{
    QFile file(QStringLiteral(TSA_SOURCE_DIR "/tests/data/robot_rsa2024_reference.json"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

gp_Pnt point(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    return gp_Pnt(a[0].toDouble(), a[1].toDouble(), a[2].toDouble());
}

gp_Dir direction(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    return gp_Dir(a[0].toDouble(), a[1].toDouble(), a[2].toDouble());
}

gp_Ax3 frameOf(const gp_Pnt& a, const gp_Pnt& b, double gamma)
{
    return CoordinateTransformationService::computeElementLocalFrame(a, b, gamma);
}

bool matches(const gp_Ax3& f, const QJsonObject& bar)
{
    return f.XDirection().IsEqual(direction(bar["x"]), kTol) && f.YDirection().IsEqual(direction(bar["y"]), kTol) && f.Direction().IsEqual(direction(bar["z"]), kTol);
}

/// Écart d'angles modulo 360°
double angleGap(double a, double b)
{
    return std::abs(std::remainder(a - b, 360.0));
}

/// Barres de référence dans un modèle : poteaux (verticaux) comme Column, le reste comme Beam, pour vérifier les deux familles.
struct Built
{
    int id = 0;
    bool column = false;
    gp_Pnt a, b;
};

std::vector<Built> build(Model& m, const QJsonArray& bars, ElementSet& set)
{
    std::vector<Built> out;
    for (const auto& v : bars)
    {
        const QJsonObject o = v.toObject();
        const gp_Pnt a = point(o["start"]), b = point(o["end"]);
        const int na = m.addNode(a.X(), a.Y(), a.Z()), nb = m.addNode(b.X(), b.Y(), b.Z());
        const bool column = o["name"].toString().startsWith("poteau");
        Built bar { 0, column, a, b };
        if (column)
        {
            bar.id = m.addColumn(na, nb, Section::heb(200), Material::steelS235(), o["gamma"].toDouble());
            set.columns.insert(bar.id);
        }
        else
        {
            bar.id = m.addBar(na, nb, Section::ipe(300), Material::steelS235(), BarRole::Beam);
            m.getBeam(bar.id)->setRotation(o["gamma"].toDouble());
            set.beams.insert(bar.id);
        }
        out.push_back(bar);
    }
    return out;
}

/// Barre créée (absente de `skip`) d'extrémités a, b ; rotation : angle lu. Une copie peut coïncider avec son original.
bool findBar(const Model& m, const gp_Pnt& a, const gp_Pnt& b, double* rotation, const ElementSet& skip = {})
{
    auto at = [&](int node, const gp_Pnt& p) {
        const Node* n = m.getNode(node);
        return n && gp_Pnt(n->x(), n->y(), n->z()).Distance(p) < 1e-6;
    };
    for (const auto& [id, e] : m.beams())
        if (!skip.beams.count(id) && at(e.startNodeId(), a) && at(e.endNodeId(), b)) return *rotation = e.rotation(), true;
    for (const auto& [id, e] : m.columns())
        if (!skip.columns.count(id) && at(e.startNodeId(), a) && at(e.endNodeId(), b)) return *rotation = e.rotation(), true;
    return false;
}

gp_Pnt nodePoint(const Model& m, int id)
{
    const Node* n = m.getNode(id);
    return gp_Pnt(n->x(), n->y(), n->z());
}

gp_Vec linear(const gp_Vec& v, const gp_Trsf& t)
{
    return v.Transformed(t);
}

gp_Vec vectorOnBeam(const Model& m, const MemberLoad& l)
{
    const Beam* b = m.getBeam(l.elementId());
    return LoadResolver::memberLoadVector(l, l.q1(), nodePoint(m, b->startNodeId()), nodePoint(m, b->endNodeId()), b->rotation());
}

int copyOfBeam(const Model& m, int source)
{
    int copy = 0;
    for (const auto& [id, e] : m.beams())
        copy = id != source ? id : copy;
    return copy;
}

std::vector<MemberLoad> loadsOn(const Model& m, int beam)
{
    std::vector<MemberLoad> loads;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        if (l.elementId() == beam && l.targetType() == MemberTargetType::Beam) loads.push_back(l);
    return loads;
}

MemberLoad barLoad(int bar, bool column, int lc, LoadType type, double q, LoadDirection dir, double x = 0.0, bool relative = false)
{
    const double x2 = type == LoadType::MemberPoint ? x : 0.0;
    return MemberLoad(0, bar, lc, type, q, q, dir, x, x2, relative, "", column ? MemberTargetType::Column : MemberTargetType::Beam);
}
} // namespace

bool runSuite_LoadAxes(int& passed)
{
    const QJsonObject ref = reference();
    TEST_CHECK(!ref.isEmpty(), "Référence Robot lue");

    // -------------------------------------------------------------------------
    // TEST 558 : repères de référence
    // -------------------------------------------------------------------------
    {
        int count = 0;
        for (const char* model : { "mirror", "rotation" })
            for (const auto& v : ref["models"].toObject()[model].toArray())
            {
                const QJsonObject b = v.toObject();
                const gp_Ax3 f = frameOf(point(b["start"]), point(b["end"]), b["gamma"].toDouble());
                TEST_CHECK(matches(f, b), ("Test 558: repère de Robot, " + b["name"].toString().toStdString()).c_str());
                ++count;
            }
        int tilts = 0;
        for (const auto& v : ref["verticality"].toArray())
        {
            const QJsonObject t = v.toObject();
            const double d = 3.0 * std::tan(t["tiltDegrees"].toDouble() * M_PI / 180.0);
            const QString dir = t["direction"].toString();
            const double ux = dir.contains("-X") ? -1.0 : 0.0, uy = dir.contains("+Y") ? 1.0 : 0.0;
            const gp_Pnt lower(0, 0, 0), upper(ux * d, uy * d, 3.0);
            const bool up = t["sense"].toInt() > 0;
            const gp_Ax3 f = frameOf(up ? lower : upper, up ? upper : lower, 0.0);
            const QJsonArray y = t["y"].toArray();
            TEST_CHECK(std::abs(f.YDirection().X() - y[0].toDouble()) < 2e-3 && std::abs(f.YDirection().Y() - y[1].toDouble()) < 2e-3,
                       ("Test 558: seuil de verticalité, " + dir.toStdString() + ", " + std::to_string(t["tiltDegrees"].toDouble()) + "°").c_str());
            ++tilts;
        }
        std::cout << "[PASS] Test 558: Repère local égal à celui de Robot (" << count << " barres, " << tilts << " inclinaisons)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 559, 560 : copie par rotation, copie miroir
    // -------------------------------------------------------------------------
    {
        const QJsonObject operations = ref["operations"].toObject();
        int rotated = 0, mirrored = 0;
        for (auto it = operations.begin(); it != operations.end(); ++it)
        {
            const QJsonObject op = it.value().toObject();
            const bool rotation = op["kind"].toString() == "rotation";
            Model m;
            ElementSet set;
            build(m, ref["models"].toObject()[rotation ? "rotation" : "mirror"].toArray(), set);
            std::vector<std::string> warnings;
            if (rotation)
                m.copyAndRotateElements(set, gp_Pnt(0, 0, 0), direction(op["axis"]), op["degrees"].toDouble() * M_PI / 180.0, 1, ClipboardOptions {}, &warnings);
            else
                m.mirrorCopyElements(set, point(op["point"]), direction(op["normal"]), ClipboardOptions {}, &warnings);
            const std::string name = it.key().toStdString();
            for (const auto& v : op["bars"].toArray())
            {
                const QJsonObject b = v.toObject();
                const gp_Pnt a = point(b["start"]), e = point(b["end"]);
                double rotationValue = 0.0;
                if (!findBar(m, a, e, &rotationValue, set))
                    TEST_CHECK(findBar(m, e, a, &rotationValue, set), ("Test 559: barre copiée trouvée, " + name).c_str());
                TEST_CHECK(angleGap(rotationValue, b["gamma"].toDouble()) < 1e-3,
                           ("Test 559: angle de section de Robot, " + name + " : TSA " + std::to_string(rotationValue) + ", Robot " + std::to_string(b["gamma"].toDouble())).c_str());
                TEST_CHECK(matches(frameOf(a, e, rotationValue), b), ("Test 559: repère de Robot, " + name).c_str());
                (rotation ? rotated : mirrored) += 1;
            }
        }
        std::cout << "[PASS] Test 559: Copie par rotation conforme à Robot (" << rotated << " barres)" << std::endl;
        ++passed;
        std::cout << "[PASS] Test 560: Copie miroir conforme à Robot (" << mirrored << " barres)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 561 : sans copie, inversion des nœuds
    // -------------------------------------------------------------------------
    {
        const QJsonObject moves = ref["move"].toObject();
        const auto run = [&](const char* name, auto&& transform) {
            Model m;
            const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(0, 8, 0), d = m.addNode(4, 11, 2), e = m.addNode(16, 0, 0), f = m.addNode(16, 0, 3);
            const int beam = m.addBeam(a, b), inclined = m.addBeam(c, d), column = m.addColumn(e, f, Section::heb(200), Material::steelS235(), 30.0);
            m.getBeam(beam)->setRotation(30.0);
            m.getBeam(inclined)->setRotation(30.0);
            transform(m, std::set<int> { a, b, c, d, e, f });
            const QJsonArray expected = moves[name].toArray();
            const double got[3] = { m.getBeam(beam)->rotation(), m.getBeam(inclined)->rotation(), m.getColumn(column)->rotation() };
            bool ok = true;
            for (int k = 0; k < 3; ++k) ok = ok && angleGap(got[k], expected[k].toObject()["gamma"].toDouble()) < 1e-3;
            return ok;
        };
        TEST_CHECK(run("rotation Z 30 déplacée", [](Model& m, const std::set<int>& nodes) {
                       gp_Trsf t;
                       t.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), M_PI / 6.0);
                       m.transformNodes(nodes, t);
                   }), "Test 561: rotation sans copie, γ de Robot");
        TEST_CHECK(run("miroir x=50 déplacé", [](Model& m, const std::set<int>& nodes) {
                       m.mirrorElements(nodes, { 1, 2 }, { 1 }, {}, gp_Pnt(50, 0, 0), gp_Dir(1, 0, 0), false);
                   }), "Test 561: symétrie sans copie, γ de Robot");

        for (const auto& v : ref["reversal"].toArray())
        {
            const QJsonObject before = v.toObject()["before"].toObject(), after = v.toObject()["after"].toObject();
            const gp_Ax3 reversed = frameOf(point(before["end"]), point(before["start"]), -before["gamma"].toDouble());
            TEST_CHECK(matches(reversed, after), "Test 561: nœuds inversés avec γ opposé, repère de Robot");
        }
        std::cout << "[PASS] Test 561: Rotation et symétrie sans copie, inversion des nœuds" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 562 : charges globales et locales
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Exploitation", LoadCaseCategory::Live));
        const int a = m.addNode(1, 2, 3), b = m.addNode(7, 2, 5);
        const int beam = m.addBeam(a, b);
        m.getBeam(beam)->setRotation(20.0);
        auto& lm = m.loadManager();
        lm.addMemberLoad(MemberLoad::uniform(beam, lc, -10.0, LoadDirection::GlobalZ));
        lm.addMemberLoad(MemberLoad::uniform(beam, lc, 4.0, LoadDirection::GlobalX));
        lm.addMemberLoad(MemberLoad::uniform(beam, lc, -3.0, LoadDirection::LocalZ));
        lm.addMemberLoad(MemberLoad::pointOnMember(beam, lc, 7.0, 0.5, LoadDirection::LocalY, true));
        lm.addMemberLoad(barLoad(beam, false, lc, LoadType::MemberMoment, 2.0, LoadDirection::GlobalY));
        lm.addMemberLoad(barLoad(beam, false, lc, LoadType::MemberMoment, 1.5, LoadDirection::LocalX));
        NodalLoad nodal(0, b, lc, 1.0, 2.0, 3.0);
        nodal.setMx(4.0), nodal.setMy(5.0), nodal.setMz(6.0);
        lm.addNodalLoad(nodal);
        const auto source = loadsOn(m, beam);
        ElementSet set;
        set.beams = { beam };

        const auto compare = [&](Model& mm, const gp_Trsf& t, bool mirror) {
            const int copy = copyOfBeam(mm, beam);
            const auto loads = loadsOn(mm, copy);
            if (loads.size() != source.size()) return false;
            for (std::size_t i = 0; i < loads.size(); ++i)
            {
                if (loads[i].q1() != source[i].q1() || loads[i].direction() != source[i].direction()) return false;   // enregistrement identique
                const gp_Vec before = vectorOnBeam(mm, source[i]), after = vectorOnBeam(mm, loads[i]);
                if (source[i].coordSystem() == LoadCoordSystem::Global)
                {
                    if (!after.IsEqual(before, 1e-9, 1e-9)) return false;   // direction globale fixe
                }
                else if (!mirror && (after - linear(before, t)).Magnitude() > 1e-9)
                    return false;   // la charge locale tourne avec la barre
            }
            return true;
        };

        for (double degrees : { 90.0, 180.0, 30.0, -135.0 })
        {
            Model copy;
            copy.restoreSnapshot(m.createSnapshot("source"));
            gp_Trsf t;
            t.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), degrees * M_PI / 180.0);
            copy.copyAndRotateElements(set, gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), degrees * M_PI / 180.0, 1, ClipboardOptions {});
            TEST_CHECK(compare(copy, t, false), "Test 562: rotation, charges globales fixes, charges locales tournées");
            for (const auto& [id, l] : copy.loadManager().nodalLoads())
                TEST_CHECK(l.fx() == 1.0 && l.fy() == 2.0 && l.fz() == 3.0 && l.mx() == 4.0, "Test 562: charge nodale recopiée à l'identique (repère global)");
        }

        Model mirrored;
        mirrored.restoreSnapshot(m.createSnapshot("source"));
        gp_Trsf t;
        t.SetMirror(gp_Ax2(gp_Pnt(-5, 0, 0), gp_Dir(1, 0, 0)));
        mirrored.mirrorCopyElements(set, gp_Pnt(-5, 0, 0), gp_Dir(1, 0, 0), ClipboardOptions {});
        TEST_CHECK(compare(mirrored, t, true), "Test 562: symétrie, enregistrements identiques, charges globales fixes");
        const auto mirroredLoads = loadsOn(mirrored, copyOfBeam(mirrored, beam));
        const gp_Vec localZ = vectorOnBeam(mirrored, source[2]), imageZ = vectorOnBeam(mirrored, mirroredLoads[2]);
        TEST_CHECK((imageZ - linear(localZ, t)).Magnitude() < 1e-9, "Test 562: symétrie, charge locale z : image de la charge");
        const gp_Vec localY = vectorOnBeam(mirrored, source[3]), imageY = vectorOnBeam(mirrored, mirroredLoads[3]);
        TEST_CHECK((imageY + linear(localY, t)).Magnitude() < 1e-9, "Test 562: symétrie, charge locale y : opposée à l'image, comme le repère");

        Model multiple;
        multiple.restoreSnapshot(m.createSnapshot("source"));
        multiple.copyAndRotateElements(set, gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), M_PI / 2, 3, ClipboardOptions {});
        TEST_CHECK(multiple.beams().size() == 4 && multiple.loadManager().memberLoads().size() == 4 * source.size(), "Test 562: duplication multiple, charges sur chaque copie");

        {
            TSA::UndoRedo::EditTransaction tx(m, "copie");
            m.copyElements(set, 0.0, 5.0, 0.0, 1, ClipboardOptions {});
            tx.commit();
        }
        TEST_CHECK(m.undo() && lm.memberLoads().size() == source.size(), "Test 562: Annuler");
        TEST_CHECK(m.redo() && lm.memberLoads().size() == 2 * source.size(), "Test 562: Rétablir");

        const QString file = QDir::temp().filePath(QStringLiteral("tsa_load_axes_%1.tsa").arg(QCoreApplication::applicationPid()));
        std::string error;
        TEST_CHECK(TSAProjectIO::saveToFile(file, m, nullptr, &error), "Test 562: enregistrement");
        Model reloaded;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, reloaded, nullptr, &error), "Test 562: relecture");
        QFile::remove(file);
        for (const auto& [id, l] : lm.memberLoads())
        {
            const MemberLoad* r = reloaded.loadManager().getMemberLoad(id);
            TEST_CHECK(r && r->direction() == l.direction() && r->coordSystem() == l.coordSystem() && r->q1() == l.q1(), "Test 562: repère et axe relus");
        }
        TEST_CHECK(same(reloaded.getBeam(beam)->rotation(), 20.0), "Test 562: angle de section relu, sans migration");
        std::cout << "[PASS] Test 562: Charges globales fixes, charges locales suivant l'élément" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 563 : anciens projets
    // -------------------------------------------------------------------------
    {
        const auto stored = [](std::uint8_t dir, LoadCoordSystem cs, double q1, double q2) {
            return memberLoadFromStored(1, 2, 3, LoadType::MemberLinear, q1, q2, dir, cs, 0.0, 1.0, true, "ancienne", MemberTargetType::Beam);
        };
        MemberLoad g = stored(kLegacyGravityDirection, LoadCoordSystem::Global, 10.0, -4.0);
        TEST_CHECK(g.direction() == LoadDirection::GlobalZ && g.q1() == -10.0 && g.q2() == -4.0, "Test 563: ancienne gravité, Z global d'intensité −|q|");
        TEST_CHECK(g.isRelativePosition() && g.name() == "ancienne", "Test 563: position et nom conservés");
        g = stored(static_cast<std::uint8_t>(LoadDirection::LocalY), LoadCoordSystem::Global, 5.0, 5.0);
        TEST_CHECK(g.direction() == LoadDirection::GlobalZ && g.q1() == -5.0, "Test 563: axe local en repère global, lu vers le bas comme avant");
        g = stored(static_cast<std::uint8_t>(LoadDirection::GlobalX), LoadCoordSystem::Local, 5.0, 5.0);
        TEST_CHECK(g.direction() == LoadDirection::LocalZ && g.q1() == -5.0, "Test 563: axe global en repère local, lu selon −z local comme avant");
        g = stored(static_cast<std::uint8_t>(LoadDirection::LocalY), LoadCoordSystem::Local, -5.0, 2.0);
        TEST_CHECK(g.direction() == LoadDirection::LocalY && g.q1() == -5.0 && g.q2() == 2.0, "Test 563: charge locale cohérente inchangée");

        SurfaceLoad s;
        s.value = 3.0, s.gradV = 0.5;
        applyStoredSurfaceDirection(s, "gravity");
        TEST_CHECK(s.direction == SurfaceLoadDirection::GlobalZ && s.value == -3.0 && s.gradV == -0.5, "Test 563: surface, ancienne gravité");
        SurfaceLoad n;
        n.value = 1.5;
        applyStoredSurfaceDirection(n, "normal");
        TEST_CHECK(n.direction == SurfaceLoadDirection::LocalZ && n.value == -1.5, "Test 563: surface, ancienne normale −N");
        SurfaceLoad x;
        x.value = 2.0;
        applyStoredSurfaceDirection(x, "localX");
        TEST_CHECK(x.direction == SurfaceLoadDirection::LocalX && x.value == 2.0, "Test 563: surface, axe actuel inchangé");

        // Repère des barres verticales : l'orientation physique est conservée
        for (double beta : { 0.0, 30.0, -45.0, 90.0 })
            for (double dz : { 3.0, -3.0 })
            {
                Model m;
                const int a = m.addNode(0, 0, 3 - dz), b = m.addNode(0, 0, 3);
                const int column = m.addColumn(a, b, Section::heb(200), Material::steelS235(), beta);
                TEST_CHECK(migrateLegacyBarFrames(m) == 1, "Test 563: poteau converti");
                const gp_Dir x0 = dz > 0 ? gp_Dir(0, 0, 1) : gp_Dir(0, 0, -1);
                // Ancienne convention : y0 = Y × x, z0 = x × y0, puis rotation β
                const gp_Vec y0 = gp_Vec(0, 1, 0).Crossed(gp_Vec(x0)), z0 = gp_Vec(x0).Crossed(y0);
                const double r = beta * M_PI / 180.0;
                const gp_Vec oldY = y0 * std::cos(r) + z0 * std::sin(r), oldZ = z0 * std::cos(r) - y0 * std::sin(r);
                const gp_Ax3 f = frameOf(nodePoint(m, a), nodePoint(m, b), m.getColumn(column)->rotation());
                TEST_CHECK(gp_Vec(f.YDirection()).IsEqual(oldY, 1e-9, 1e-9) && gp_Vec(f.Direction()).IsEqual(oldZ, 1e-9, 1e-9), "Test 563: axes y et z physiquement inchangés");
            }

        Model m;
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(6, 0, 0));
        m.getBeam(beam)->setRotation(30.0);
        TEST_CHECK(migrateLegacyBarFrames(m) == 0 && m.getBeam(beam)->rotation() == 30.0, "Test 563: barre non verticale inchangée");

        // Barre sans angle de section : charge locale réexprimée
        Model t;
        const int lc = t.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int truss = t.addTrussMember(t.addNode(0, 0, 0), t.addNode(0, 0, 4));
        t.loadManager().addMemberLoad(MemberLoad(0, truss, lc, LoadType::MemberUniform, 5.0, 5.0, LoadDirection::LocalY, 0.0, 0.0, false, "", MemberTargetType::Truss));
        TEST_CHECK(migrateLegacyBarFrames(t) == 1 && t.loadManager().memberLoads().size() == 1, "Test 563: treillis vertical, charge locale convertie");
        const MemberLoad& converted = t.loadManager().memberLoads().begin()->second;
        TEST_CHECK(converted.direction() == LoadDirection::LocalZ && same(converted.q1(), -5.0), "Test 563: ancien y (+X) = −z actuel");

        // Fichier de version 2 : conversion à la lecture ; version courante : aucune
        Model column;
        const int colId = column.addColumn(column.addNode(0, 0, 0), column.addNode(0, 0, 3), Section::heb(200), Material::steelS235(), 0.0);
        const QString file = QDir::temp().filePath(QStringLiteral("tsa_legacy_%1.tsa").arg(QCoreApplication::applicationPid()));
        std::string error;
        TEST_CHECK(TSAProjectIO::saveToFile(file, column, nullptr, &error), "Test 563: enregistrement");
        Model current;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, current, nullptr, &error) && current.getColumn(colId)->rotation() == 0.0, "Test 563: format actuel, angle lu tel quel");
        QFile f(file);
        TEST_CHECK(f.open(QIODevice::ReadWrite), "Test 563: fichier ouvert");
        f.seek(offsetof(TSA::IO::TSAFileHeader, versionMajor));
        const quint16 two = 2;
        f.write(reinterpret_cast<const char*>(&two), sizeof(two));
        f.close();
        Model legacy;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, legacy, nullptr, &error), ("Test 563: fichier de version 2 lu : " + error).c_str());
        QFile::remove(file);
        TEST_CHECK(same(legacy.getColumn(colId)->rotation(), -90.0), "Test 563: fichier de version 2, angle converti (−90°)");
        std::cout << "[PASS] Test 563: Anciens projets, charges et repères convertis sans changer l'action physique" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 564 : calcul comparé à Robot
    // -------------------------------------------------------------------------
    {
        const QJsonObject analysis = ref["analysis"].toObject();
        const QJsonObject table = analysis["results"].toObject();
        Model m;
        struct Bar
        {
            QString name;
            int id = 0, base = 0, tip = 0;
            bool column = false;
        };
        std::vector<Bar> bars;
        for (const auto& v : analysis["bars"].toArray())
        {
            const QJsonObject o = v.toObject();
            const gp_Pnt a = point(o["start"]), b = point(o["end"]);
            Bar bar { o["name"].toString() };
            bar.base = m.addNode(a.X(), a.Y(), a.Z());
            bar.tip = m.addNode(b.X(), b.Y(), b.Z());
            m.getNode(bar.base)->setSupport(SupportDefinition::fixed());
            bar.column = bar.name.startsWith("poteau");
            if (bar.column) bar.id = m.addColumn(bar.base, bar.tip, Section::ipe(300), Material::steelS235(), o["gamma"].toDouble());
            else
            {
                bar.id = m.addBar(bar.base, bar.tip, Section::ipe(300), Material::steelS235(), BarRole::Beam);
                m.getBeam(bar.id)->setRotation(o["gamma"].toDouble());
            }
            bars.push_back(bar);
        }
        auto& lm = m.loadManager();
        std::vector<int> cases;
        for (const char* name : { "uniforme global", "uniforme local", "ponctuelle locale", "ponctuelle globale", "moment réparti global", "moment réparti local", "nodale" })
            cases.push_back(lm.addLoadCase(LoadCase(0, name, LoadCaseCategory::Live)));
        for (const Bar& b : bars)
        {
            auto add = [&](int k, LoadType type, double q, LoadDirection dir, double x = 0.0, bool rel = false) { lm.addMemberLoad(barLoad(b.id, b.column, cases[k], type, q, dir, x, rel)); };
            add(0, LoadType::MemberUniform, -10.0, LoadDirection::GlobalZ);
            add(1, LoadType::MemberUniform, 3.0, LoadDirection::LocalY);
            add(1, LoadType::MemberUniform, -7.0, LoadDirection::LocalZ);
            add(2, LoadType::MemberPoint, 2.0, LoadDirection::LocalY, 0.5, true);
            add(2, LoadType::MemberPoint, -5.0, LoadDirection::LocalZ, 0.5, true);
            add(3, LoadType::MemberPoint, 1.0, LoadDirection::GlobalX, 0.5, true);
            add(3, LoadType::MemberPoint, 2.0, LoadDirection::GlobalY, 0.5, true);
            add(3, LoadType::MemberPoint, -3.0, LoadDirection::GlobalZ, 0.5, true);
            add(4, LoadType::MemberMoment, 2.0, LoadDirection::GlobalY);
            add(5, LoadType::MemberMoment, 1.0, LoadDirection::LocalX);
            add(5, LoadType::MemberMoment, 0.5, LoadDirection::LocalZ);
            NodalLoad nodal(0, b.tip, cases[6], 1.0, 2.0, 3.0);
            nodal.setMx(0.4), nodal.setMy(0.5), nodal.setMz(0.6);
            lm.addNodalLoad(nodal);
        }

        AnalysisEngineRegistry registry;
        registerBuiltInEngines(registry);
        AnalysisManager manager(registry);
        GridManager grids;
        double worstReaction = 0.0, worstDisplacement = 0.0;
        for (std::size_t k = 0; k < cases.size(); ++k)
        {
            AnalysisContext ctx;
            ctx.engineId = "opensees";
            ctx.common.includeSelfWeight = false;
            ctx.loadCaseIds = { cases[k] };
            const auto prepared = manager.prepare(m, &grids, ctx);
            TEST_CHECK(prepared.canRun(), "Test 564: calcul préparé");
            const auto run = manager.run(ctx, prepared);
            TEST_CHECK(run.success, "Test 564: calcul réussi");
            for (const Bar& b : bars)
            {
                const QJsonObject robot = table[QString("%1|%2").arg(k + 1).arg(b.name)].toObject();
                const QJsonArray R = robot["R"].toArray(), U = robot["U"].toArray();
                const auto r = run.results.nodeReaction(b.base);
                const auto d = run.results.nodeDisplacement(b.tip);
                const double reaction[6] = { r.rx, r.ry, r.rz, r.mx, r.my, r.mz };
                const double displacement[6] = { d.ux, d.uy, d.uz, d.rx, d.ry, d.rz };
                double scale = 0.0, scaleU = 0.0, scaleR = 0.0;
                for (int i = 0; i < 6; ++i)
                {
                    scale = std::max(scale, std::abs(R[i].toDouble()) / 1000.0);
                    (i < 3 ? scaleU : scaleR) = std::max(i < 3 ? scaleU : scaleR, std::abs(U[i].toDouble()));
                }
                const std::string label = QString("cas %1, %2").arg(k + 1).arg(b.name).toStdString();
                for (int i = 0; i < 6; ++i)
                {
                    // Convention : Robot rend la réaction d'appui, TSA aussi ; même signe attendu
                    const double gapR = std::abs(reaction[i] - R[i].toDouble() / 1000.0) / std::max(scale, 1e-9);
                    const double gapU = std::abs(displacement[i] - U[i].toDouble()) / std::max(i < 3 ? scaleU : scaleR, 1e-12);
                    worstReaction = std::max(worstReaction, gapR);
                    worstDisplacement = std::max(worstDisplacement, gapU);
                    TEST_CHECK(gapR < 1e-6, ("Test 564: réaction " + std::to_string(i) + " égale à Robot, " + label + " : TSA " + std::to_string(reaction[i]) + ", Robot " + std::to_string(R[i].toDouble() / 1000.0)).c_str());
                    TEST_CHECK(gapU < 0.03, ("Test 564: déplacement " + std::to_string(i) + " proche de Robot, " + label + " : TSA " + std::to_string(displacement[i]) + ", Robot " + std::to_string(U[i].toDouble())).c_str());
                }
            }
        }
        std::cout << "  écart relatif maximal : réactions " << worstReaction << ", déplacements " << worstDisplacement << std::endl;
        std::cout << "[PASS] Test 564: Calcul conforme à Robot (7 cas, 5 barres)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 565 : transformations
    // -------------------------------------------------------------------------
    {
        for (const auto& v : ref["models"].toObject()["mirror"].toArray())
        {
            const QJsonObject b = v.toObject();
            const gp_Pnt a = point(b["start"]), e = point(b["end"]);
            const gp_Ax3 f = frameOf(a, e, b["gamma"].toDouble());
            const gp_Vec x(f.XDirection()), y(f.YDirection()), z(f.Direction());
            TEST_CHECK(std::abs(x.Dot(y)) < 1e-12 && std::abs(y.Dot(z)) < 1e-12 && std::abs(x.Dot(z)) < 1e-12, "Test 565: matrice orthogonale");
            TEST_CHECK(std::abs(x.DotCross(y, z) - 1.0) < 1e-12 && y.Crossed(z).IsEqual(x, 1e-12, 1e-12), "Test 565: déterminant +1, y × z = x");
            // R a pour colonnes x, y, z : F_global = R·F_local ; F_local = Rᵀ·F_global
            const double fx = 1.5, fy = -2.0, fz = 4.0;
            const gp_Vec global = LoadResolver::localVectorToGlobal(fx, fy, fz, a, e, b["gamma"].toDouble());
            TEST_CHECK(global.IsEqual(x * fx + y * fy + z * fz, 1e-9, 1e-9), "Test 565: F_global = R·F_local");
            const auto local = LoadResolver::decomposeGlobalVectorToLocal(global, a, e, b["gamma"].toDouble());
            TEST_CHECK(same(local.wx, fx) && same(local.wy, fy) && same(local.wz, fz), "Test 565: F_local = Rᵀ·F_global");
        }
        std::cout << "[PASS] Test 565: Matrices de passage, orthogonales et directes" << std::endl;
        ++passed;
    }

    return true;
}
