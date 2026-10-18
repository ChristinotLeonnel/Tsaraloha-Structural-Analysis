// Suite « drawing » : service de création des éléments dessinés, modes Dessin de la vue, métré, palette et scénario d'intégration, voile à base inclinée, outils avancés, palette complète (tests 410-426). Les points sont
// fournis comme par la vue 3D (position accrochée, nœud visé) ; le modèle, l'historique et la couche
// BIM sont vérifiés après chaque création ou refus.

#include "test_common.h"

#include "BIM/Core/QuantityTakeoff.h"
#include "BIM/IFC/IfcExporter.h"
#include "BIM/IFC/IfcImporter.h"
#include "IO/TSAFile.h"
#include "Mesh/MeshGeometry.h"
#include "Interaction/ElementCreation.h"
#include "Interaction/Tools/ModelingTool.h"
#include "UndoRedo/EditTransaction.h"
#include "UI/Properties/ElementBimPanel.h"
#include "UI/Properties/PropertyPanel.h"
#include "Viewer/OccView.h"

#include <QApplication>
#include <QGroupBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QThread>

#include <set>

using namespace TSA::Interaction;

namespace
{
DrawPoint pt(double x, double y, double z, int node = -1)
{
    return DrawPoint(gp_Pnt(x, y, z), node);
}

BarProperties beamProps()
{
    BarProperties p;
    p.id = 0;
    p.name = "";
    p.role = BarRole::Beam;
    p.section = Section::rectangular(0.30, 0.50, "R30x50");
    p.material = Material::concreteC25_30();
    return p;
}

/// Aucune trace d'une création refusée : ni élément, ni nœud, ni entrée Annuler.
bool untouched(const Model& m, size_t nodes, size_t beams)
{
    return m.nodes().size() == nodes && m.beams().size() == beams && !m.canUndo();
}

bool hasWarning(const CreationResult& r, const std::string& needle)
{
    for (const auto& w : r.warnings)
        if (w.find(needle) != std::string::npos) return true;
    return false;
}
} // namespace

bool runSuite_Drawing(int& passed)
{
    // TEST 410 : nœud — création, nœud existant non dupliqué, annulation
    {
        Model m;
        ElementCreation c(m);
        auto r = c.createNode(pt(1, 2, 3));
        TEST_CHECK(r.success && m.nodes().size() == 1 && r.createdNodeIds.size() == 1, "Test 410: nœud créé");
        auto dup = c.createNode(pt(1.0005, 2, 3));
        TEST_CHECK(!dup.success && m.nodes().size() == 1 && dup.message.find("N" + std::to_string(r.elementId)) != std::string::npos,
                   "Test 410: point confondu (< 1 mm) : refus motivé, pas de doublon");
        auto other = c.createNode(pt(1.002, 2, 3));
        TEST_CHECK(other.success && m.nodes().size() == 2, "Test 410: à 2 mm : nouveau nœud (aucune fusion par proximité)");
        TEST_CHECK(m.undo() && m.nodes().size() == 1, "Test 410: annulable");
        std::cout << "[PASS] Test 410: Création de nœud" << std::endl;
        ++passed;
    }

    // TEST 411 : poutre entre deux points libres — nœuds et poutre dans UNE entrée Annuler (D2)
    {
        Model m;
        ElementCreation c(m);
        auto r = c.createBar(pt(0, 0, 0), pt(5, 0, 0), beamProps());
        TEST_CHECK(r.success && m.beams().size() == 1 && m.nodes().size() == 2, "Test 411: poutre et 2 nœuds créés");
        TEST_CHECK(r.createdNodeIds.size() == 2 && r.nodeIds.size() == 2, "Test 411: nœuds créés signalés");
        const auto* b = m.getBeam(r.elementId);
        TEST_CHECK(b && b->section().name == "R30x50" && b->role() == BarRole::Beam, "Test 411: section et rôle du préréglage");
        TEST_CHECK(m.undo() && m.beams().empty() && m.nodes().empty() && !m.canUndo(),
                   "Test 411: Annuler retire la poutre ET ses nœuds (une seule entrée)");
        TEST_CHECK(m.redo() && m.beams().size() == 1 && m.nodes().size() == 2, "Test 411: Rétablir");

        // Seconde poutre accrochée au nœud d'arrivée : nœud réutilisé, un seul nœud créé.
        auto r2 = c.createBar(pt(5, 0, 0, r.nodeIds[1]), pt(5, 4, 0), beamProps());
        TEST_CHECK(r2.success && r2.nodeIds[0] == r.nodeIds[1] && r2.createdNodeIds.size() == 1 && m.nodes().size() == 3,
                   "Test 411: nœud accroché réutilisé");
        // Point libre confondu avec un nœud (sans accrochage) : réutilisé aussi.
        auto r3 = c.createBar(pt(0, 0, 0.0004), pt(0, 4, 0), beamProps());
        TEST_CHECK(r3.success && r3.nodeIds[0] == r.nodeIds[0] && m.nodes().size() == 4, "Test 411: nœud confondu réutilisé");
        std::cout << "[PASS] Test 411: Poutre transactionnelle" << std::endl;
        ++passed;
    }

    // TEST 412 : refus avant toute modification (D5)
    {
        Model m;
        ElementCreation c(m);
        auto zero = c.createBar(pt(1, 1, 0), pt(1, 1, 0.0005), beamProps());
        TEST_CHECK(!zero.success && !zero.message.empty() && untouched(m, 0, 0), "Test 412: longueur nulle refusée, modèle intact");
        auto badSection = beamProps();
        badSection.section = Section::rectangular(0.0, 0.5, "vide");
        auto sec = c.createBar(pt(0, 0, 0), pt(3, 0, 0), badSection);
        TEST_CHECK(!sec.success && sec.message.find("section") != std::string::npos && untouched(m, 0, 0), "Test 412: section sans aire refusée");
        auto badMat = beamProps();
        badMat.material.E = 0.0;
        TEST_CHECK(!c.createBar(pt(0, 0, 0), pt(3, 0, 0), badMat).success && untouched(m, 0, 0), "Test 412: matériau sans module refusé");

        auto ok = c.createBar(pt(0, 0, 0), pt(3, 0, 0), beamProps());
        TEST_CHECK(ok.success, "Test 412: première poutre");
        auto dup = c.createBar(pt(3, 0, 0, ok.nodeIds[1]), pt(0, 0, 0, ok.nodeIds[0]), beamProps());
        TEST_CHECK(!dup.success && dup.message.find("double") != std::string::npos && m.beams().size() == 1,
                   "Test 412: poutre en double (sens inverse) refusée");
        auto col = beamProps();
        col.role = BarRole::Column;
        TEST_CHECK(!c.createBar(pt(0, 0, 0), pt(3, 0, 0), col).success && m.columns().empty(),
                   "Test 412: poteau sur une poutre existante refusé (même famille de barres)");
        std::cout << "[PASS] Test 412: Validation avant création" << std::endl;
        ++passed;
    }

    // TEST 413 : poteau, treillis (préréglage explicite, D4), câble
    {
        Model m;
        ElementCreation c(m);
        auto col = beamProps();
        col.role = BarRole::Column;
        col.section = Section::rectangular(0.30, 0.30, "R30x30");
        col.color = "#808080";
        auto rc = c.createBar(pt(0, 0, 0), pt(0, 0, 3.2), col);
        const auto* column = m.getColumn(rc.elementId);
        TEST_CHECK(rc.success && column && m.beams().empty() && column->color() == "#808080", "Test 413: poteau (rôle Column) et couleur");

        TrussPreset tp;
        tp.section = Section::circular(0.08, "C80");
        tp.material = Material::steelS355();
        tp.role = TrussMemberRole::TopChord;
        auto rt = c.createTrussMember(pt(0, 0, 3.2, rc.nodeIds[1]), pt(4, 0, 3.2), tp);
        const auto* tr = m.getTrussMember(rt.elementId);
        TEST_CHECK(rt.success && tr && tr->section().name == "C80" && tr->material().name == Material::steelS355().name
                       && tr->role() == TrussMemberRole::TopChord, "Test 413: treillis selon le préréglage (plus de 0,10 m imposé)");

        int setupId = -1;
        auto rk = c.createCable(pt(4, 0, 3.2, rt.nodeIds[1]), pt(8, 0, 0), CableType::StayCable,
                                [&setupId](Model& model, int id) { setupId = id; model.getCable(id)->setName("Hauban 1"); });
        TEST_CHECK(rk.success && setupId == rk.elementId && m.getCable(rk.elementId)->name() == "Hauban 1",
                   "Test 413: câble, réglages du dialogue appliqués");
        TEST_CHECK(m.undo() && m.cables().empty() && m.nodes().size() == 3, "Test 413: réglages compris dans la même entrée Annuler");
        std::cout << "[PASS] Test 413: Poteau, treillis, câble" << std::endl;
        ++passed;
    }

    // TEST 414 : dalle — contour valide, refus (alignés, non plan, croisé, confondus, doublon)
    {
        Model m;
        ElementCreation c(m);
        SlabPreset sp;
        sp.thickness = 0.25;
        // Refus motivé, sans aucune trace dans le modèle ni dans l'historique.
        auto refused = [&](std::vector<DrawPoint> contour, const std::string& needle) {
            auto r = c.createSlab(contour, sp);
            return !r.success && r.message.find(needle) != std::string::npos && m.nodes().empty() && m.slabs().empty() && !m.canUndo();
        };
        TEST_CHECK(refused({ pt(0, 0, 0), pt(1, 0, 0) }, "au moins 3"), "Test 414: 2 points refusés");
        TEST_CHECK(refused({ pt(0, 0, 0), pt(1, 0, 0), pt(2, 0, 0) }, "alignés"), "Test 414: points alignés refusés");
        TEST_CHECK(refused({ pt(0, 0, 0), pt(4, 0, 0), pt(4, 4, 0.3), pt(0, 4, 0) }, "pas plan"), "Test 414: contour non plan refusé");
        TEST_CHECK(refused({ pt(0, 0, 0), pt(4, 4, 0), pt(4, 0, 0), pt(0, 4, 0) }, "recoupe"), "Test 414: contour croisé (nœud papillon) refusé");
        TEST_CHECK(refused({ pt(0, 0, 0), pt(4, 0, 0), pt(4, 0, 0.0002), pt(0, 4, 0) }, "confondus"), "Test 414: points confondus refusés");
        TEST_CHECK(refused({ pt(0, 0, 0), pt(4, 0, 0), pt(2, 0, 0), pt(2, 3, 0) }, "recoupe"), "Test 414: côté qui revient sur un autre refusé");

        auto r = c.createSlab({ pt(0, 0, 3), pt(6, 0, 3), pt(6, 5, 3), pt(0, 5, 3) }, sp, "Plancher R+1");
        const auto* s = m.getSlab(r.elementId);
        TEST_CHECK(r.success && s && s->nodeIds().size() == 4 && std::abs(s->thickness() - 0.25) < 1e-12 && s->name() == "Plancher R+1",
                   "Test 414: dalle créée (épaisseur, nom)");
        std::vector<DrawPoint> again;
        for (int id : r.nodeIds) again.push_back(pt(0, 0, 0, id));
        auto dup = c.createSlab(again, sp);
        TEST_CHECK(!dup.success && dup.message.find("double") != std::string::npos && m.slabs().size() == 1, "Test 414: dalle en double refusée");
        TEST_CHECK(m.undo() && m.slabs().empty() && m.nodes().empty(), "Test 414: Annuler retire dalle et nœuds");
        std::cout << "[PASS] Test 414: Dalle validée" << std::endl;
        ++passed;
    }

    // TEST 415 : voile et fondation (préréglages, doublons)
    {
        Model m;
        ElementCreation c(m);
        WallPreset wp;
        wp.height = 2.8;
        wp.thickness = 0.18;
        wp.offset = 0.05;
        auto rw = c.createWall(pt(0, 0, 0), pt(5, 0, 0), wp, "V1");
        const auto* w = m.getWall(rw.elementId);
        TEST_CHECK(rw.success && w && std::abs(w->height() - 2.8) < 1e-12 && std::abs(w->thickness() - 0.18) < 1e-12
                       && std::abs(w->offset() - 0.05) < 1e-12, "Test 415: voile selon le préréglage");
        WallPreset flat = wp;
        flat.height = 0.0;
        TEST_CHECK(!c.createWall(pt(0, 2, 0), pt(5, 2, 0), flat).success && m.walls().size() == 1 && m.nodes().size() == 2,
                   "Test 415: voile de hauteur nulle refusé");
        TEST_CHECK(!c.createWall(pt(5, 0, 0, rw.nodeIds[1]), pt(0, 0, 0, rw.nodeIds[0]), wp).success, "Test 415: voile en double refusé");

        FoundationPreset fp;
        fp.widthA = 2.0;
        fp.lengthB = 1.2;
        fp.heightH = 0.6;
        auto rf = c.createFoundation(pt(0, 0, 0, rw.nodeIds[0]), fp);
        const auto* f = m.getFoundation(rf.elementId);
        TEST_CHECK(rf.success && f && std::abs(f->widthA() - 2.0) < 1e-12 && std::abs(f->lengthB() - 1.2) < 1e-12
                       && std::abs(f->heightH() - 0.6) < 1e-12 && rf.createdNodeIds.empty(),
                   "Test 415: semelle selon le préréglage (plus de 1,50 × 1,50 × 0,50 imposé), sur le nœud existant");
        auto twice = c.createFoundation(pt(0, 0, 0), fp);
        TEST_CHECK(!twice.success && twice.message.find("F" + std::to_string(rf.elementId)) != std::string::npos && m.foundations().size() == 1,
                   "Test 415: deuxième fondation sur le même nœud refusée");
        std::cout << "[PASS] Test 415: Voile et fondation" << std::endl;
        ++passed;
    }

    // TEST 416 : croisement ≠ connexion (D6) — avertissement, rien n'est divisé ni fusionné
    {
        Model m;
        ElementCreation c(m);
        auto base = c.createBar(pt(0, 0, 0), pt(6, 0, 0), beamProps());
        auto tee = c.createBar(pt(3, 0, 0), pt(3, 4, 0), beamProps());
        TEST_CHECK(tee.success && hasWarning(tee, "B" + std::to_string(base.elementId)) && hasWarning(tee, "aucune connexion"),
                   "Test 416: extrémité sur l'intérieur d'une poutre : avertissement");
        const auto* b0 = m.getBeam(base.elementId);
        TEST_CHECK(m.beams().size() == 2 && b0 && b0->startNodeId() == base.nodeIds[0] && b0->endNodeId() == base.nodeIds[1],
                   "Test 416: poutre existante non divisée");
        auto cross = c.createBar(pt(1, -2, 0), pt(1, 2, 0), beamProps());
        TEST_CHECK(cross.success && hasWarning(cross, "croise") && m.nodes().size() == 6, "Test 416: croisement signalé, aucun nœud au croisement");
        auto above = c.createBar(pt(2, -2, 1), pt(2, 2, 1), beamProps());
        TEST_CHECK(above.success && above.warnings.empty(), "Test 416: croisement à distance (1 m au-dessus) non signalé");
        auto shared = c.createBar(pt(6, 0, 0, base.nodeIds[1]), pt(6, 4, 0), beamProps());
        TEST_CHECK(shared.success && shared.warnings.empty(), "Test 416: nœud commun : connexion réelle, aucun avertissement");
        std::cout << "[PASS] Test 416: Croisement non connecté signalé" << std::endl;
        ++passed;
    }

    // TEST 417 : produit BIM de l'élément créé (identité persistante), retiré par Annuler
    {
        Model m;
        ElementCreation c(m);
        auto col = beamProps();
        col.role = BarRole::Column;
        auto r = c.createBar(pt(0, 0, 0), pt(0, 0, 3), col);
        const TSA::BIM::AnalyticalRef ref { ElementKind::Column, r.elementId };
        const auto* prod = m.bim().physicalOf(ref);
        TEST_CHECK(prod && prod->globalId.size() == 22 && prod->category == TSA::BIM::BimCategory::Column && !prod->imported(),
                   "Test 417: produit physique IfcColumn avec GlobalId");
        const std::string guid = prod ? prod->globalId : std::string();
        TEST_CHECK(m.undo() && m.bim().physicalOf(ref) == nullptr, "Test 417: Annuler retire le produit");
        TEST_CHECK(m.redo() && m.bim().physicalOf(ref) && m.bim().physicalOf(ref)->globalId == guid, "Test 417: Rétablir rend le même GlobalId");
        std::cout << "[PASS] Test 417: Identité BIM des éléments dessinés" << std::endl;
        ++passed;
    }

    // TEST 418 : résultats de calcul périmés après une création, pas après un refus
    {
        Model m;
        ElementCreation c(m);
        c.createBar(pt(0, 0, 0), pt(4, 0, 0), beamProps());
        auto results = std::make_shared<TSA::Analysis::ResultsModel>();
        results->setValid(true);
        TSA::Analysis::ResultsValidityGuard guard(&m);
        int staleCalls = 0;
        guard.setStaleCallback([&]() { ++staleCalls; });
        guard.trackResults(results);
        const auto rev = m.revision();
        c.createBar(pt(0, 0, 0), pt(0, 0, 0), beamProps());
        TEST_CHECK(m.revision() == rev && guard.resultsUpToDate() && staleCalls == 0, "Test 418: un refus laisse les résultats valides");
        c.createBar(pt(0, 0, 0), pt(0, 4, 0), beamProps());
        TEST_CHECK(!guard.resultsUpToDate() && !results->isValid() && staleCalls == 1, "Test 418: la création rend les résultats obsolètes");
        std::cout << "[PASS] Test 418: Résultats périmés signalés" << std::endl;
        ++passed;
    }

    // TEST 419 : modes Dessin de la vue 3D réelle, pilotés par clics : aucun nœud avant la validation,
    // Échap ne laisse rien (D1), Entrée valide une dalle, une poutre = une entrée Annuler avec ses nœuds (D2), aucun poteau inventé (D4).
    // Vue OCCT : contexte OpenGL sur une vraie fenêtre ; la plateforme Qt « offscreen » n'en fournit pas
    // (BUG-076) : test annoncé comme sauté, jamais compté réussi.
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        std::cout << "[SKIP] Test 419: vue 3D indisponible sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
    else
    {
        auto pump = [](int n = 10) {
            for (int k = 0; k < n; ++k)
            {
                QApplication::processEvents();
                QThread::msleep(5);
            }
        };
        Model m;
        OccView view;
        view.resize(800, 600);
        view.show();
        pump(20);
        view.setModel(&m);
        pump();
        auto click = [&](int x, int y) {
            const QPointF local(x, y);
            QMouseEvent press(QEvent::MouseButtonPress, local, view.mapToGlobal(local), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&view, &press);
            QMouseEvent release(QEvent::MouseButtonRelease, local, view.mapToGlobal(local), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&view, &release);
            pump(3);
        };
        QStringList prompts;
        QObject::connect(&view, &OccView::drawingPromptChanged, [&prompts](const QString& p) { prompts << p; });

        view.setInteractionMode(OccView::InteractionMode::DrawSlab);
        click(320, 260);
        click(470, 260);
        click(470, 390);
        TEST_CHECK(m.nodes().empty() && prompts.size() >= 3, "Test 419: dalle en cours (3 clics) : aucun nœud créé avant la validation");
        view.cancelCurrentDrawing();
        TEST_CHECK(m.nodes().empty() && m.slabs().empty() && !m.canUndo(), "Test 419: Échap : rien dans le modèle ni dans l'historique");
        click(320, 260);
        click(470, 260);
        click(470, 390);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&view, &enter);
        pump(3);
        TEST_CHECK(m.slabs().size() == 1 && m.nodes().size() == 3, "Test 419: Entrée valide la dalle (3 nœuds créés avec elle)");
        TEST_CHECK(m.undo() && m.slabs().empty() && m.nodes().empty() && !m.canUndo(), "Test 419: Annuler retire la dalle et ses nœuds");

        view.setInteractionMode(OccView::InteractionMode::DrawBeam);
        click(320, 260);
        TEST_CHECK(m.nodes().empty(), "Test 419: 1er point de poutre : aucun nœud");
        click(470, 260);
        TEST_CHECK(m.beams().size() == 1 && m.nodes().size() == 2, "Test 419: poutre et ses 2 nœuds créés au 2nd clic");
        view.cancelCurrentDrawing();   // fin de l'enchaînement « Étirer »
        TEST_CHECK(m.undo() && m.beams().empty() && m.nodes().empty() && !m.canUndo(),
                   "Test 419: Annuler retire la poutre ET ses nœuds (une seule entrée)");

        view.setInteractionMode(OccView::InteractionMode::DrawColumn);
        click(320, 260);
        prompts.clear();
        click(320, 260);
        TEST_CHECK(m.columns().empty() && m.nodes().empty() && !prompts.isEmpty() && prompts.back().startsWith("Non créé"),
                   "Test 419: 2nd clic au même point : refus motivé, aucun poteau de 3 m inventé");
        view.cancelCurrentDrawing();
        view.setModel(nullptr);
        pump();
        std::cout << "[PASS] Test 419: Modes Dessin de la vue 3D (clics, Échap, Annuler)" << std::endl;
        ++passed;
    }

    // TEST 420 : métré dérivé du modèle — longueurs, surfaces nettes (ouvertures déduites), volumes, masses ;
    // cumuls par matériau, section, famille et produit physique
    {
        Model m;
        ElementCreation c(m);
        auto beam = c.createBar(pt(0, 0, 3), pt(5, 0, 3), beamProps());                         // R30x50, 5 m
        SlabPreset sp;
        sp.thickness = 0.25;
        auto slab = c.createSlab({ pt(0, 0, 3), pt(6, 0, 3), pt(6, 5, 3), pt(0, 5, 3) }, sp);   // 30 m²
        SlabOpening hole;
        hole.polygon = { { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 2 } };                               // 1 m²
        m.getSlab(slab.elementId)->setOpenings({ hole });
        WallPreset wp;
        wp.height = 2.8;
        wp.thickness = 0.18;
        auto wall = c.createWall(pt(0, 8, 0), pt(5, 8, 0), wp);                                  // 14 m²
        SurfaceOpening door;
        door.polygon = { { 1, 0 }, { 2, 0 }, { 2, 2 }, { 1, 2 } };                               // 2 m²
        m.getWall(wall.elementId)->setOpenings({ door });
        FoundationPreset fp;
        fp.widthA = 2.0;
        fp.lengthB = 1.2;
        fp.heightH = 0.6;
        auto footing = c.createFoundation(pt(10, 0, 0), fp);
        TrussPreset tp;
        tp.section = Section::circular(0.08, "C80");
        auto truss = c.createTrussMember(pt(0, 12, 0), pt(4, 12, 0), tp);

        using TSA::BIM::AnalyticalRef;
        const double rho = Material::concreteC25_30().density;
        auto qb = TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::Beam, beam.elementId });
        TEST_CHECK(qb && approxEqual(qb->length, 5.0, 1e-9) && approxEqual(qb->volume, 0.75, 1e-9) && approxEqual(qb->mass, 0.75 * rho, 1e-6)
                       && qb->section == "R30x50" && qb->label == "B" + std::to_string(beam.elementId), "Test 420: poutre 5 m R30x50 : 0,75 m³");
        auto qs = TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::Slab, slab.elementId });
        TEST_CHECK(qs && approxEqual(qs->grossArea, 30.0, 1e-9) && approxEqual(qs->openingArea, 1.0, 1e-9) && approxEqual(qs->area, 29.0, 1e-9)
                       && approxEqual(qs->volume, 29.0 * 0.25, 1e-9), "Test 420: dalle 30 m² − trémie 1 m² = 29 m², 7,25 m³");
        auto qw = TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::Wall, wall.elementId });
        TEST_CHECK(qw && approxEqual(qw->length, 5.0, 1e-9) && approxEqual(qw->grossArea, 14.0, 1e-9) && approxEqual(qw->area, 12.0, 1e-9)
                       && approxEqual(qw->volume, 12.0 * 0.18, 1e-9), "Test 420: voile 14 m² − porte 2 m² = 12 m²");
        auto qf = TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::Foundation, footing.elementId });
        TEST_CHECK(qf && approxEqual(qf->volume, 2.0 * 1.2 * 0.6, 1e-9), "Test 420: semelle 1,44 m³");
        auto qt = TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::TrussMember, truss.elementId });
        TEST_CHECK(qt && approxEqual(qt->volume, 4.0 * Section::circular(0.08).area(), 1e-12)
                       && approxEqual(qt->mass, qt->volume * Material::steelS235().density, 1e-9), "Test 420: treillis C80 en acier");
        TEST_CHECK(!TSA::BIM::elementQuantities(m, AnalyticalRef { ElementKind::Beam, 999 }), "Test 420: élément inexistant : aucune quantité");

        auto report = TSA::BIM::takeoff(m);
        TEST_CHECK(report.elements.size() == 5 && report.total.count == 5, "Test 420: 5 éléments métrés");
        const double concrete = 0.75 + 7.25 + 2.16 + 1.44;
        TEST_CHECK(approxEqual(report.byMaterial[Material::concreteC25_30().name].volume, concrete, 1e-9)
                       && report.byMaterial[Material::concreteC25_30().name].count == 4, "Test 420: cumul par matériau (béton)");
        TEST_CHECK(report.bySection["R30x50"].count == 1 && report.byFamily["Dalle"].count == 1, "Test 420: cumuls par section et par famille");

        // Deux poutres regroupées en un produit physique : quantités cumulées sur le produit.
        auto beam2 = c.createBar(pt(5, 0, 3, beam.nodeIds[1]), pt(9, 0, 3), beamProps());
        const AnalyticalRef r1 { ElementKind::Beam, beam.elementId }, r2 { ElementKind::Beam, beam2.elementId };
        const int productId = m.bimForEdit().group({ r1, r2 });
        const auto* product = m.bim().element(productId);
        TEST_CHECK(product && approxEqual(TSA::BIM::productQuantities(m, *product).length, 9.0, 1e-9)
                       && approxEqual(TSA::BIM::takeoff(m).byProduct[productId].volume, 9.0 * 0.15, 1e-9),
                   "Test 420: produit de 2 poutres : 9 m, 1,35 m³");
        std::cout << "[PASS] Test 420: Métré dérivé du modèle" << std::endl;
        ++passed;
    }

    // TEST 421 : les quantités suivent le modèle (aucune valeur figée) et l'historique
    {
        Model m;
        ElementCreation c(m);
        auto beam = c.createBar(pt(0, 0, 0), pt(4, 0, 0), beamProps());
        const TSA::BIM::AnalyticalRef ref { ElementKind::Beam, beam.elementId };
        TEST_CHECK(approxEqual(TSA::BIM::elementQuantities(m, ref)->length, 4.0, 1e-9), "Test 421: 4 m");
        m.pushUndoState("Déplacement");
        m.getNode(beam.nodeIds[1])->setX(6.0);
        m.notifyNodeModified(beam.nodeIds[1]);
        TEST_CHECK(approxEqual(TSA::BIM::elementQuantities(m, ref)->length, 6.0, 1e-9), "Test 421: nœud déplacé : 6 m");
        TEST_CHECK(m.undo() && approxEqual(TSA::BIM::elementQuantities(m, ref)->length, 4.0, 1e-9), "Test 421: Annuler : 4 m");
        std::cout << "[PASS] Test 421: Quantités recalculées" << std::endl;
        ++passed;
    }

    // TEST 422 : palette de propriétés — identité BIM et quantités de l'élément dessiné, à jour après modification
    {
        Model m;
        ElementCreation c(m);
        auto beam = c.createBar(pt(0, 0, 3), pt(5, 0, 3), beamProps());
        TSA::UI::ElementBimPanel panel;
        panel.setModel(&m);
        panel.showElement(TSA::BIM::AnalyticalRef { ElementKind::Beam, beam.elementId });
        const auto* prod = m.bim().physicalOf(TSA::BIM::AnalyticalRef { ElementKind::Beam, beam.elementId });
        QString t = panel.text();
        TEST_CHECK(prod && t.contains("IfcBeam") && t.contains(QString::fromStdString(prod->globalId)) && t.contains("Longueur : 5.000 m")
                       && t.contains("Volume : 0.7500"), "Test 422: classe IFC, GlobalId, longueur et volume affichés");
        m.getNode(beam.nodeIds[1])->setX(8.0);
        m.notifyNodeModified(beam.nodeIds[1]);
        panel.refresh();
        TEST_CHECK(panel.text().contains("Longueur : 8.000 m"), "Test 422: quantités recalculées après modification");
        panel.showElement(TSA::BIM::AnalyticalRef { ElementKind::Node, beam.nodeIds[0] });
        TEST_CHECK(panel.text().isEmpty(), "Test 422: nœud : panneau vide (pas de produit physique)");
        std::cout << "[PASS] Test 422: Identité BIM et quantités dans la palette" << std::endl;
        ++passed;
    }

    // TEST 423 : scénario d'intégration — une travée complète tracée comme dans la vue (points accrochés aux
    // nœuds existants), contrôlée (aucun orphelin, un produit BIM par élément, GlobalId uniques), métrée,
    // enregistrée en .tsa et relue, exportée en IFC et réimportée, puis entièrement annulée.
    {
        Model m;
        ElementCreation c(m);
        auto col = beamProps();
        col.role = BarRole::Column;
        col.section = Section::rectangular(0.30, 0.30, "R30x30");
        const double X[2] = { 0.0, 6.0 }, Y[2] = { 0.0, 5.0 };
        int base[2][2] = {}, top[2][2] = {};
        bool ok = true;
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
            {
                auto r = c.createBar(pt(X[i], Y[j], 0), pt(X[i], Y[j], 3), col);   // 1-2 : poteaux
                ok = ok && r.success;
                base[i][j] = r.success ? r.nodeIds[0] : -1;
                top[i][j] = r.success ? r.nodeIds[1] : -1;
            }
        TEST_CHECK(ok && m.columns().size() == 4 && m.nodes().size() == 8, "Test 423: 4 poteaux, 8 nœuds");
        auto at = [&](int node) {
            const auto* n = m.getNode(node);
            return pt(n->x(), n->y(), n->z(), node);
        };
        const int ring[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        for (int k = 0; k < 4; ++k)                                                      // 3 : poutres en tête
        {
            const auto& a = ring[k];
            const auto& b = ring[(k + 1) % 4];
            ok = ok && c.createBar(at(top[a[0]][a[1]]), at(top[b[0]][b[1]]), beamProps()).success;
        }
        SlabPreset sp;
        std::vector<DrawPoint> contour;
        for (const auto& r : ring) contour.push_back(at(top[r[0]][r[1]]));
        auto slab = c.createSlab(contour, sp);                                            // 4 : dalle sur les têtes
        WallPreset wp;
        auto wall = c.createWall(at(base[0][0]), at(base[1][0]), wp);                     // 5 : voile entre deux pieds
        FoundationPreset fp;
        for (int i = 0; i < 2; ++i)                                                       // 6 : semelles sous les poteaux
            for (int j = 0; j < 2; ++j) ok = ok && c.createFoundation(at(base[i][j]), fp).success;
        TEST_CHECK(ok && slab.success && wall.success && m.beams().size() == 4 && m.foundations().size() == 4 && m.nodes().size() == 8,
                   "Test 423: poutres, dalle, voile, semelles sur les nœuds existants (aucun nœud ajouté)");

        // 7 : cohérence du modèle — tout nœud porte un élément, un produit par élément, GlobalId uniques
        std::set<int> used;
        auto useLinear = [&used](const auto& map) { for (const auto& [id, e] : map) { (void)id; used.insert(e.startNodeId()); used.insert(e.endNodeId()); } };
        useLinear(m.columns());
        useLinear(m.beams());
        useLinear(m.walls());
        for (const auto& [id, s] : m.slabs()) used.insert(s.nodeIds().begin(), s.nodeIds().end());
        TEST_CHECK(used.size() == m.nodes().size(), "Test 423: aucun nœud orphelin");
        std::set<std::string> guids;
        for (const auto& [id, p] : m.bim().elements()) guids.insert(p.globalId);
        const size_t elements = m.columns().size() + m.beams().size() + m.slabs().size() + m.walls().size() + m.foundations().size();
        TEST_CHECK(m.bim().elements().size() == elements && guids.size() == elements, "Test 423: un produit BIM par élément, GlobalId uniques");

        // 8 : métré
        const auto q = TSA::BIM::takeoff(m);
        const double expectedConcrete = 4 * 3.0 * 0.09 + (2 * 6.0 + 2 * 5.0) * 0.15 + 30.0 * 0.20 + 6.0 * 3.0 * 0.20 + 4 * 1.5 * 1.5 * 0.5;
        TEST_CHECK(q.total.count == static_cast<int>(elements) && approxEqual(q.total.volume, expectedConcrete, 1e-9),
                   "Test 423: métré total (béton) conforme aux dimensions");

        // 9 : .tsa — enregistrement, relecture, mêmes identités et mêmes quantités
        const auto filePath = std::filesystem::temp_directory_path() / "tsa_test423.tsa";
        const QString file = QString::fromStdWString(filePath.wstring());
        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 423: enregistrement .tsa (" + err + ")");
        Model reloaded;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, reloaded, nullptr, &err), "Test 423: relecture .tsa (" + err + ")");
        std::filesystem::remove(filePath);
        bool sameGuids = reloaded.bim().elements().size() == m.bim().elements().size();
        for (const auto& [id, p] : m.bim().elements()) sameGuids = sameGuids && reloaded.bim().elementByGlobalId(p.globalId) != nullptr;
        TEST_CHECK(sameGuids && approxEqual(TSA::BIM::takeoff(reloaded).total.volume, q.total.volume, 1e-9),
                   "Test 423: .tsa relu : mêmes produits (GlobalId) et mêmes quantités");

        // 10 : IFC — export puis réimport : mêmes produits
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-17T12:00:00";
        std::string doc;
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, doc).ok, "Test 423: export IFC");
        Model fromIfc;
        TEST_CHECK(TSA::BIM::Ifc::IfcImporter::importString(doc, fromIfc).ok, "Test 423: réimport IFC");
        bool sameIfc = fromIfc.bim().elements().size() == m.bim().elements().size();
        for (const auto& [id, p] : m.bim().elements()) sameIfc = sameIfc && fromIfc.bim().elementByGlobalId(p.globalId) != nullptr;
        TEST_CHECK(sameIfc && fromIfc.columns().size() == 4 && fromIfc.slabs().size() == 1 && fromIfc.walls().size() == 1,
                   "Test 423: IFC réimporté : mêmes produits (GlobalId), poteaux, dalle, voile");

        int undos = 0;
        while (m.canUndo() && m.undo()) ++undos;
        TEST_CHECK(undos == 14 && m.nodes().empty() && m.bim().elements().empty(), "Test 423: 14 créations = 14 entrées Annuler, modèle vide après annulation");
        std::cout << "[PASS] Test 423: Scénario d'intégration (tracé, contrôle, métré, .tsa, IFC, annulation)" << std::endl;
        ++passed;
    }

    // TEST 424 : voile dont les deux points de base ne sont pas à la même cote — refusé au-delà de
    // GeometryTolerance::nodeCoincidence (1 mm), accepté en deçà sans retouche des coordonnées.
    {
        WallPreset wp;
        auto hasSlopeWarning = [](const CreationResult& r) {
            for (const auto& w : r.warnings)
                if (w.find("cote") != std::string::npos) return true;
            return false;
        };

        // Même cote : créé, aucun avertissement de cote ; Annuler retire voile et nœuds.
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, 0), wp);
            TEST_CHECK(r.success && !hasSlopeWarning(r) && r.warnings.empty(), "Test 424: même cote : voile créé sans avertissement");
            TEST_CHECK(m.undo() && m.walls().empty() && m.nodes().empty() && !m.canUndo(), "Test 424: Annuler retire le voile et ses 2 nœuds");
        }

        // Écart inférieur à la tolérance (0,5 mm) : créé, coordonnées conservées telles quelles.
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, 0.0005), wp);
            const auto* nb = r.success ? m.getNode(r.nodeIds[1]) : nullptr;
            TEST_CHECK(r.success && !hasSlopeWarning(r) && nb && nb->z() == 0.0005,
                       "Test 424: écart de 0,5 mm (< 1 mm) : voile créé, cote du point d'origine conservée");
        }

        // Écart supérieur à la tolérance, dans les deux sens : refus motivé, rien de créé.
        for (const auto& [dz, shown] : { std::pair { 0.40, "0.400" }, { -0.40, "0.400" }, { 0.002, "0.002" } })
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, dz), wp);
            TEST_CHECK(!r.success && r.message.find("même cote") != std::string::npos && r.message.find(std::string("écart de ") + shown + " m") != std::string::npos,
                       "Test 424: écart de " + std::to_string(dz) + " m : refus motivé (écart indiqué)");
            TEST_CHECK(m.nodes().empty() && m.walls().empty() && !m.canUndo() && r.createdNodeIds.empty(),
                       "Test 424: refus : aucun nœud ni voile abandonné, historique intact");
        }

        // Points accrochés à des nœuds existants de cotes différentes : nœuds intacts, rien d'ajouté.
        {
            Model m;
            const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0.4);
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0, n1), pt(5, 0, 0.4, n2), wp);
            TEST_CHECK(!r.success && m.nodes().size() == 2 && m.walls().empty() && m.getNode(n2)->z() == 0.4,
                       "Test 424: nœuds existants à cotes différentes : refus, nœuds non modifiés");
        }

        // Motif du refus : la surface de calcul d'un voile à base inclinée (créé sans le service) n'a pas un repère
        // orthogonal et surestime son aire. Si ce garde échoue, wallSurface a été corrigée : la règle peut être revue.
        {
            Model m;
            const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(4, 0, 3);
            const int w = m.addWall(n1, n2, 3.0, 0.2);
            TSA::Mesh::SurfaceGeom sg;
            TEST_CHECK(w > 0 && TSA::Mesh::wallSurface(m, *m.getWall(w), &sg), "Test 424: surface de calcul du voile incliné");
            const double trueArea = 4.0 * 3.0;   // parallélogramme : longueur horizontale × hauteur
            TEST_CHECK(std::abs(sg.frame.U.dot(sg.frame.V)) > 0.5 && sg.area > trueArea + 1.0,
                       "Test 424: limite constatée (repère non orthogonal, aire 15 m² au lieu de 12 m²) : motif du refus");
        }
        std::cout << "[PASS] Test 424: Voile à base inclinée refusé, tolérance de 1 mm" << std::endl;
        ++passed;
    }

    // TEST 425 : outils de dessin avancés (ModelingTool) par ElementCreation — outil entier refusé au premier
    // élément invalide, une entrée Annuler par outil, préréglage Treillis, croisements signalés
    {
        TSA::Interaction::ModelingToolRegistry reg;
        TSA::Interaction::registerBuiltInModelingTools(reg);
        auto pick = [](double x, double y, double z) {
            TSA::Interaction::ToolPick p;
            p.point = gp_Pnt(x, y, z);
            return p;
        };
        TSA::Interaction::ToolContext ctx;

        // Portique dont le poteau gauche existe déjà : refusé entièrement, même dans la transaction de la fenêtre.
        {
            Model m;
            ElementCreation c(m);
            auto col = beamProps();
            col.role = BarRole::Column;
            TEST_CHECK(c.createBar(pt(0, 0, 0), pt(0, 0, 4), col).success, "Test 425: poteau existant");
            m.clearUndoRedo();
            auto portal = reg.create("draw_portal");
            portal->acceptValue(4.0, ctx);
            portal->addPick(pick(0, 0, 0), ctx);
            portal->addPick(pick(6, 0, 0), ctx);
            TSA::Interaction::ToolResult r;
            {
                TSA::UndoRedo::EditTransaction tx(m, portal->name());   // comme MainWindow::applyModelingTool
                r = portal->apply(m, ctx);
                if (!r.success) tx.rollback();
                else tx.commit();
            }
            TEST_CHECK(!r.success && r.message.find("Portique non créé") != std::string::npos && r.message.find("double") != std::string::npos,
                       "Test 425: portique sur un poteau existant : refus motivé");
            TEST_CHECK(m.columns().size() == 1 && m.beams().empty() && m.nodes().size() == 2 && !m.canUndo(),
                       "Test 425: rien de créé (ni 2ᵉ poteau, ni traverse, ni nœud), historique intact");
        }

        // Portique accepté, outil appelé seul : une entrée Annuler retire 2 poteaux, la traverse et 4 nœuds.
        {
            Model m;
            auto portal = reg.create("draw_portal");
            portal->addPick(pick(0, 0, 0), ctx);
            portal->addPick(pick(6, 0, 0), ctx);
            const auto r = portal->apply(m, ctx);
            TEST_CHECK(r.success && m.columns().size() == 2 && m.beams().size() == 1 && m.nodes().size() == 4 && r.created.columns.size() == 2,
                       "Test 425: portique créé, nœuds de tête partagés par la traverse");
            TEST_CHECK(m.undo() && m.columns().empty() && m.beams().empty() && m.nodes().empty() && !m.canUndo(),
                       "Test 425: une seule entrée Annuler pour tout l'outil");
        }

        // Contreventement en X : section et matériau du préréglage Treillis ; les diagonales se croisent sans nœud.
        {
            Model m;
            TSA::Interaction::ToolContext tctx;
            tctx.presets.truss.section = Section::circular(0.06, "C60");
            tctx.presets.truss.material = Material::steelS355();
            auto xb = reg.create("draw_x_bracing");
            for (const auto& p : { pick(0, 0, 0), pick(6, 0, 0), pick(6, 0, 4), pick(0, 0, 4) }) xb->addPick(p, tctx);
            const auto r = xb->apply(m, tctx);
            bool presetUsed = m.trussMembers().size() == 2;
            for (const auto& [id, t] : m.trussMembers())
                presetUsed = presetUsed && t.section().name == "C60" && t.material().name == Material::steelS355().name
                             && t.role() == TrussMemberRole::Diagonal;
            TEST_CHECK(r.success && presetUsed, "Test 425: diagonales selon le préréglage Treillis (plus de 0,10 m imposé)");
            TEST_CHECK(r.message.find("croise") != std::string::npos && m.nodes().size() == 4,
                       "Test 425: croisement des diagonales signalé, aucun nœud ajouté au croisement");
        }

        // Arc relancé à l'identique : toutes ses poutres seraient en double, refus et modèle inchangé.
        {
            Model m;
            auto arc = reg.create("draw_beam_arc");
            arc->setParam("segments", 4);
            for (const auto& p : { pick(-5, 0, 0), pick(0, 0, 5), pick(5, 0, 0) }) arc->addPick(p, ctx);
            TEST_CHECK(arc->apply(m, ctx).success && m.beams().size() == 4 && m.nodes().size() == 5, "Test 425: arc de 4 poutres");
            const auto again = arc->apply(m, ctx);
            TEST_CHECK(!again.success && m.beams().size() == 4 && m.nodes().size() == 5, "Test 425: arc en double refusé, modèle inchangé");
        }
        std::cout << "[PASS] Test 425: Outils de dessin avancés par le service de création" << std::endl;
        ++passed;
    }

    // TEST 426 : palette complète (PropertyPanel construit avec son modèle, comme dans l'application) — le bloc
    // « Identité BIM et quantités » s'affiche pour l'élément sélectionné. Régression trouvée par l'essai de l'interface
    // réelle (tools/ui_tests/drawing_scenario.ps1) : le bloc ne recevait pas le modèle et restait masqué.
    {
        Model m;
        ElementCreation c(m);
        SlabPreset sp;
        auto slab = c.createSlab({ pt(0, 0, 0), pt(6, 0, 0), pt(6, 5, 0), pt(0, 5, 0) }, sp);
        TSA::UI::PropertyPanel panel(&m);
        panel.showSlabProperties(slab.elementId);
        TSA::UI::ElementBimPanel* bimPanel = nullptr;   // pas de Q_OBJECT : recherche parmi les QGroupBox
        for (auto* g : panel.findChildren<QGroupBox*>())
            if (auto* candidate = dynamic_cast<TSA::UI::ElementBimPanel*>(g)) bimPanel = candidate;
        const QString t = bimPanel ? bimPanel->text() : QString();
        TEST_CHECK(bimPanel && !bimPanel->isHidden() && t.contains("IfcSlab") && t.contains("Surface : 30.000"),
                   "Test 426: palette de la dalle : identité BIM et surface affichées");
        panel.showNodeProperties(slab.nodeIds[0]);
        TEST_CHECK(bimPanel->isHidden(), "Test 426: nœud : bloc BIM masqué");
        std::cout << "[PASS] Test 426: Palette complète, bloc identité BIM et quantités" << std::endl;
        ++passed;
    }

    return true;
}
