// Suite « drawing » : service de création des éléments dessinés, modes Dessin de la vue, métré, palette et scénario d'intégration, voile à base inclinée, outils avancés, palette complète, arbre BIM, voile incliné cohérent, types de produits (tests 410-432). Les points sont
// fournis comme par la vue 3D (position accrochée, nœud visé) ; le modèle, l'historique et la couche
// BIM sont vérifiés après chaque création ou refus.

#include "test_common.h"

#include "BIM/Core/ProductTypes.h"
#include "BIM/Core/QuantityTakeoff.h"
#include "BIM/IFC/IfcExporter.h"
#include "BIM/IFC/IfcImporter.h"
#include "IO/TSAFile.h"
#include "Mesh/MeshGeometry.h"
#include "Interaction/ElementCreation.h"
#include "Interaction/PropertyAssignment.h"
#include "Model/ModelDiff.h"
#include "Interaction/Tools/ModelingTool.h"
#include "UndoRedo/EditTransaction.h"
#include "UI/Properties/ElementBimPanel.h"
#include "UI/ModelTree/ModelTreeWidget.h"
#include "UI/Properties/PropertyPanel.h"
#include "Viewer/OccView.h"

#include <QApplication>
#include <QGroupBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QThread>
#include <QTreeWidget>

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

    // TEST 424 : voile dont les deux points de base ne sont pas à la même cote — accepté et signalé au-delà de
    // GeometryTolerance::nodeCoincidence (1 mm), sans avertissement en deçà ; coordonnées jamais retouchées
    {
        WallPreset wp;
        auto hasSlopeWarning = [](const CreationResult& r) {
            for (const auto& w : r.warnings)
                if (w.find("Base inclinée") != std::string::npos) return true;
            return false;
        };

        // Même cote : créé, aucun avertissement ; Annuler retire voile et nœuds.
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, 0), wp);
            TEST_CHECK(r.success && r.warnings.empty(), "Test 424: même cote : voile créé sans avertissement");
            TEST_CHECK(m.undo() && m.walls().empty() && m.nodes().empty() && !m.canUndo(), "Test 424: Annuler retire le voile et ses 2 nœuds");
        }

        // Écart inférieur à la tolérance (0,5 mm) : créé sans avertissement, cote conservée telle quelle.
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, 0.0005), wp);
            const auto* nb = r.success ? m.getNode(r.nodeIds[1]) : nullptr;
            TEST_CHECK(r.success && !hasSlopeWarning(r) && nb && nb->z() == 0.0005, "Test 424: écart de 0,5 mm : voile créé, sans avertissement, cote conservée");
        }

        // Écart supérieur à la tolérance, dans les deux sens : créé, avertissement avec l'écart, cotes conservées.
        for (const auto& [dz, shown] : { std::pair { 0.40, "0.400" }, { -0.40, "0.400" }, { 0.002, "0.002" } })
        {
            Model m;
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0), pt(5, 0, dz), wp);
            bool gapShown = false;
            for (const auto& w : r.warnings) gapShown = gapShown || w.find(std::string("diffèrent de ") + shown + " m") != std::string::npos;
            const auto* nb = r.success ? m.getNode(r.nodeIds[1]) : nullptr;
            TEST_CHECK(r.success && hasSlopeWarning(r) && gapShown && nb && nb->z() == dz,
                       "Test 424: écart de " + std::to_string(dz) + " m : voile créé, avertissement avec l'écart, cote conservée");
            TEST_CHECK(m.undo() && m.walls().empty() && m.nodes().empty(), "Test 424: Annuler retire le voile incliné et ses nœuds");
        }

        // Pieds accrochés à des nœuds existants de cotes différentes : nœuds réutilisés, non modifiés.
        {
            Model m;
            const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0.4);
            ElementCreation c(m);
            auto r = c.createWall(pt(0, 0, 0, n1), pt(5, 0, 0.4, n2), wp);
            TEST_CHECK(r.success && r.createdNodeIds.empty() && m.nodes().size() == 2 && m.getNode(n2)->z() == 0.4,
                       "Test 424: nœuds existants à cotes différentes : réutilisés, non modifiés");
        }
        std::cout << "[PASS] Test 424: Voile à base inclinée accepté et signalé, tolérance de 1 mm" << std::endl;
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

    // TEST 427 : arbre du modèle — branche « Structure BIM » (site › bâtiment › étages › produits), produits classés
    // par étage, clic sur un produit = signal de son élément de calcul, branche à jour après Annuler
    {
        Model m;
        auto* lm = m.levelManager();
        TEST_CHECK(lm != nullptr, "Test 427: niveaux disponibles");
        if (lm->levels().empty()) { lm->addLevel("RDC", 0.0); lm->addLevel("R+1", 3.0); }
        ElementCreation c(m);
        auto col = beamProps();
        col.role = BarRole::Column;
        auto rc = c.createBar(pt(0, 0, 0), pt(0, 0, 3), col);                              // pied à Z = 0
        SlabPreset sp;
        auto rs = c.createSlab({ pt(0, 0, 3, rc.nodeIds[1]), pt(6, 0, 3), pt(6, 5, 3), pt(0, 5, 3) }, sp);   // à Z = 3

        TSA::UI::ModelTreeWidget tree(&m);
        tree.refreshAll();
        auto* qtree = tree.findChild<QTreeWidget*>();
        TEST_CHECK(qtree != nullptr, "Test 427: QTreeWidget de l'arbre");
        std::function<QTreeWidgetItem*(QTreeWidgetItem*, const QString&)> find = [&](QTreeWidgetItem* from, const QString& prefix) -> QTreeWidgetItem* {
            for (int i = 0; i < from->childCount(); ++i)
            {
                if (from->child(i)->text(0).startsWith(prefix)) return from->child(i);
                if (auto* r = find(from->child(i), prefix)) return r;
            }
            return nullptr;
        };
        auto* bimRoot = find(qtree->invisibleRootItem(), "Structure BIM");
        TEST_CHECK(bimRoot && bimRoot->text(1) == "[2]", "Test 427: branche « Structure BIM » avec 2 produits");
        auto* building = bimRoot ? find(bimRoot, QString::fromStdString(m.bim().spatial().building.name)) : nullptr;
        TEST_CHECK(building && building->text(1) == "IfcBuilding", "Test 427: site › bâtiment");

        // Étage de chaque produit = BimModel::resolvedStorey (niveau du nœud le plus bas, sinon cote)
        const auto* colProduct = m.bim().physicalOf({ ElementKind::Column, rc.elementId });
        const auto* slabProduct = m.bim().physicalOf({ ElementKind::Slab, rs.elementId });
        auto storeyItemOf = [&](const TSA::BIM::PhysicalElement* p) -> QTreeWidgetItem* {
            const std::string st = m.bim().resolvedStorey(m, *p);
            const auto* level = lm->getLevel(st);
            return level && building ? find(building, QString::fromStdString(level->name)) : nullptr;
        };
        auto* colStorey = storeyItemOf(colProduct);
        auto* slabStorey = storeyItemOf(slabProduct);
        auto productUnder = [&](QTreeWidgetItem* storey, const char* ifcClass) {
            for (int i = 0; storey && i < storey->childCount(); ++i)
                if (storey->child(i)->text(1) == ifcClass) return storey->child(i);
            return static_cast<QTreeWidgetItem*>(nullptr);
        };
        auto* colItem = productUnder(colStorey, "IfcColumn");
        auto* slabItem = productUnder(slabStorey, "IfcSlab");
        TEST_CHECK(colItem && slabItem && colStorey != slabStorey, "Test 427: poteau et dalle sous leurs étages respectifs");

        int selectedSlab = -1;
        QObject::connect(&tree, &TSA::UI::ModelTreeWidget::slabSelected, [&selectedSlab](int id) { selectedSlab = id; });
        qtree->setCurrentItem(slabItem);
        TEST_CHECK(selectedSlab == rs.elementId, "Test 427: clic sur le produit IfcSlab : dalle sélectionnée (signal de l'élément)");

        TEST_CHECK(m.undo(), "Test 427: Annuler la dalle");
        QApplication::processEvents();   // reconstruction regroupée de la branche
        bimRoot = find(qtree->invisibleRootItem(), "Structure BIM");
        TEST_CHECK(bimRoot && bimRoot->text(1) == "[1]", "Test 427: branche à jour après Annuler (1 produit)");
        std::cout << "[PASS] Test 427: Arbre du modèle, branche Structure BIM" << std::endl;
        ++passed;
    }

    // TEST 428 : voile à base inclinée — même géométrie pour le calcul (surface de calcul orthonormée, aire), les
    // ouvertures, le métré et l'export IFC relu par IfcOpenShell (le calcul lui-même : test 429, suite surfaces)
    {
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(4, 0, 3);   // longueur en plan 4 m, dénivelé 3 m
        const int w = m.addWall(n1, n2, 3.0, 0.2, "Voile incliné");
        const auto* wall = m.getWall(w);
        TEST_CHECK(wall && approxEqual(wall->length(m), 4.0, 1e-12) && approxEqual(wall->baseRise(m), 3.0, 1e-12)
                       && approxEqual(wall->grossArea(m), 12.0, 1e-12), "Test 428: longueur en plan 4 m, dénivelé 3 m, aire brute 12 m²");

        TSA::Mesh::SurfaceGeom sg;
        TEST_CHECK(TSA::Mesh::wallSurface(m, *wall, &sg), "Test 428: surface de calcul");
        TEST_CHECK(std::abs(sg.frame.U.dot(sg.frame.V)) < 1e-12 && approxEqual(sg.area, 12.0, 1e-12) && std::abs(sg.frame.U.z) < 1e-12,
                   "Test 428: repère orthonormé (U horizontal, V vertical), aire 12 m² (et non 15 m²)");
        bool roundTrip = true;
        for (const auto& [u, v] : std::vector<std::pair<double, double>> { { 0, 0 }, { 4, 3 }, { 4, 6 }, { 0, 3 }, { 1.3, 2.2 } })
        {
            double ru = 0, rv = 0;
            sg.frame.toLocal(sg.frame.toGlobal(u, v), &ru, &rv);
            roundTrip = roundTrip && approxEqual(ru, u, 1e-12) && approxEqual(rv, v, 1e-12);
        }
        double cu = 0, cv = 0;
        sg.frame.toLocal(sg.outer[2], &cu, &cv);
        TEST_CHECK(roundTrip && approxEqual(cu, 4.0, 1e-12) && approxEqual(cv, 6.0, 1e-12),
                   "Test 428: coordonnées locales cohérentes (sommet haut droit en (4 ; 6))");

        // Ouvertures : fenêtre intérieure acceptée et déduite partout ; porte au pied refusée (pied non horizontal)
        SurfaceOpening win;
        win.name = "fenêtre";
        win.polygon = { { 1, 2 }, { 2, 2 }, { 2, 3 }, { 1, 3 } };   // entre la base (v = 0,75 u ≤ 1,5) et le haut (v = 3 + 0,75 u ≥ 3,75)
        TEST_CHECK(TSA::Mesh::validateWallOpening(m, *wall, win.polygon, -1).empty(), "Test 428: fenêtre intérieure acceptée");
        SurfaceOpening door;
        door.polygon = { { 0.5, 0 }, { 1.4, 0 }, { 1.4, 2.1 }, { 0.5, 2.1 } };
        TEST_CHECK(TSA::Mesh::validateWallOpening(m, *wall, door.polygon, -1).find("porte au pied") != std::string::npos,
                   "Test 428: porte au pied d'un voile à base inclinée refusée, motif donné");
        SurfaceOpening outside;
        outside.polygon = { { 3, 0.5 }, { 3.8, 0.5 }, { 3.8, 1.5 }, { 3, 1.5 } };   // sous la base inclinée (v < 0,75 u)
        TEST_CHECK(!TSA::Mesh::validateWallOpening(m, *wall, outside.polygon, -1).empty(), "Test 428: ouverture sous la base inclinée refusée");
        m.getWall(w)->setOpenings({ win });
        TEST_CHECK(TSA::Mesh::wallSurface(m, *m.getWall(w), &sg) && approxEqual(sg.area, 11.0, 1e-12) && sg.holes.size() == 1,
                   "Test 428: surface de calcul trouée (11 m²)");
        const auto q = TSA::BIM::elementQuantities(m, TSA::BIM::AnalyticalRef { ElementKind::Wall, w });
        TEST_CHECK(q && approxEqual(q->length, 4.0, 1e-12) && approxEqual(q->grossArea, 12.0, 1e-12) && approxEqual(q->area, 11.0, 1e-12)
                       && approxEqual(q->volume, 11.0 * 0.2, 1e-12), "Test 428: métré = calcul (longueur en plan 4 m, 12 − 1 = 11 m², 2,2 m³)");

        // Export IFC : élévation en parallélogramme extrudée dans l'épaisseur ; relue par IfcOpenShell (volume du solide)
        m.getWall(w)->setOpenings({});
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-18T12:00:00";
        std::string doc;
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, doc).ok, "Test 428: export IFC");
        TEST_CHECK(doc.find("IFCARBITRARYCLOSEDPROFILEDEF") != std::string::npos && doc.find("IFCPOLYLINE") != std::string::npos,
                   "Test 428: profil en parallélogramme (IfcArbitraryClosedProfileDef)");
        // Aller-retour par le modèle analytique IFC (import par défaut) : même voile, mêmes cotes de pied
        Model rt;
        const auto rr = TSA::BIM::Ifc::IfcImporter::importString(doc, rt);
        const TSA::Model::Wall* rw = rt.walls().empty() ? nullptr : &rt.walls().begin()->second;
        TEST_CHECK(rr.ok && rt.walls().size() == 1 && rw && approxEqual(rw->length(rt), 4.0, 1e-6) && approxEqual(std::abs(rw->baseRise(rt)), 3.0, 1e-6)
                       && approxEqual(rw->height(), 3.0, 1e-6) && approxEqual(rw->thickness(), 0.2, 1e-6),
                   "Test 428: aller-retour IFC (modèle analytique) : voile de 4 m en plan, 3 m de dénivelé, H 3 m, e 0,2 m");

        // Géométrie relue par le noyau d'IfcOpenShell (importFile : importString ne lit que le modèle) : volume du solide
        const auto ifcPath = std::filesystem::temp_directory_path() / "tsa_test428.ifc";
        { std::ofstream f(ifcPath, std::ios::binary); f << doc; }
        Model back;
        const auto ir = TSA::BIM::Ifc::IfcImporter::importFile(ifcPath.string(), back);
        std::filesystem::remove(ifcPath);
        double volume = -1.0;
        for (const auto& [id, p] : back.bim().elements())
        {
            if (!p.geometry || p.category != TSA::BIM::BimCategory::Wall) continue;
            const auto& pos = p.geometry->positions;
            const auto& idx = p.geometry->indices;
            double v6 = 0.0;
            for (std::size_t k = 0; k + 2 < idx.size(); k += 3)
            {
                const float* A = &pos[3 * idx[k]];
                const float* B = &pos[3 * idx[k + 1]];
                const float* C = &pos[3 * idx[k + 2]];
                v6 += A[0] * (B[1] * C[2] - B[2] * C[1]) - A[1] * (B[0] * C[2] - B[2] * C[0]) + A[2] * (B[0] * C[1] - B[1] * C[0]);
            }
            volume = std::abs(v6) / 6.0;
        }
        std::cout << "  voile incliné relu par IfcOpenShell : volume " << volume << " m³ (attendu 2.4)" << std::endl;
        for (const auto& wmsg : ir.warnings) std::cout << "  avertissement d'import : " << wmsg << std::endl;
        if (!TSA::BIM::Ifc::IfcImporter::fullEngineAvailable())
            std::cout << "[SKIP] Test 428 (partie géométrie) : IfcOpenShell absent de cette compilation" << std::endl;
        else
            TEST_CHECK(ir.ok && approxEqual(volume, 4.0 * 3.0 * 0.2, 1e-3), "Test 428: IFC relu par IfcOpenShell : volume du voile 2,4 m³ (12 m² × 0,2 m)");
        std::cout << "[PASS] Test 428: Voile à base inclinée, calcul / ouvertures / métré / IFC cohérents" << std::endl;
        ++passed;
    }

    // TEST 430 : types de produits partagés — création depuis le modèle (migration), persistance .tsa, ancien bloc BIM
    // sans types lu tel quel, schéma du bloc inchangé
    {
        Model m;
        ElementCreation c(m);
        auto b1 = c.createBar(pt(0, 0, 3), pt(5, 0, 3), beamProps());
        auto b2 = c.createBar(pt(0, 4, 3), pt(5, 4, 3), beamProps());
        auto steel = beamProps();
        steel.section = Section::ipe(200);
        steel.material = Material::steelS235();
        auto b3 = c.createBar(pt(0, 8, 3), pt(5, 8, 3), steel);
        auto col = beamProps();
        col.role = BarRole::Column;
        col.section = Section::rectangular(0.30, 0.30, "R30x30");
        auto c1 = c.createBar(pt(0, 0, 0), pt(0, 0, 3, b1.nodeIds[0]), col);
        SlabPreset s20, s25;
        s25.thickness = 0.25;
        auto sl1 = c.createSlab({ pt(10, 0, 0), pt(14, 0, 0), pt(14, 4, 0), pt(10, 4, 0) }, s20);
        auto sl2 = c.createSlab({ pt(20, 0, 0), pt(24, 0, 0), pt(24, 4, 0), pt(20, 4, 0) }, s25);
        WallPreset wp;
        c.createWall(pt(30, 0, 0), pt(35, 0, 0), wp);
        FoundationPreset fp;
        c.createFoundation(pt(0, 0, 0, c1.nodeIds[0]), fp);
        TEST_CHECK(b2.success && b3.success && sl1.success && sl2.success, "Test 430: modèle (3 poutres, poteau, 2 dalles, voile, semelle)");

        const int created = TSA::BIM::createTypesFromModel(m);
        const auto& bim = m.bim();
        auto typeOf = [&](ElementKind k, int id) { const auto* p = bim.physicalOf({ k, id }); return p ? bim.type(p->typeId) : nullptr; };
        const auto* tb1 = typeOf(ElementKind::Beam, b1.elementId);
        TEST_CHECK(created == 7 && bim.types().size() == 7, "Test 430: 7 types (poutre R30x50, poutre IPE 200, poteau, dalle 0,20, dalle 0,25, voile, semelle)");
        TEST_CHECK(tb1 && tb1 == typeOf(ElementKind::Beam, b2.elementId) && tb1 != typeOf(ElementKind::Beam, b3.elementId)
                       && bim.occurrences(tb1->id).size() == 2 && tb1->section.name == "R30x50" && tb1->globalId.size() == 22,
                   "Test 430: deux poutres R30x50 = un type à 2 occurrences, la poutre IPE 200 a le sien");
        TEST_CHECK(typeOf(ElementKind::Slab, sl1.elementId) != typeOf(ElementKind::Slab, sl2.elementId)
                       && std::abs(typeOf(ElementKind::Slab, sl2.elementId)->thickness - 0.25) < 1e-12, "Test 430: dalles d'épaisseurs différentes : types distincts");
        TEST_CHECK(TSA::BIM::createTypesFromModel(m) == 0, "Test 430: relancé : aucun type de plus");
        TEST_CHECK(m.bim().toJson()["schema"].toInt() == 1, "Test 430: schéma du bloc BIM inchangé (champs facultatifs ajoutés)");

        // .tsa : types, GlobalId, occurrences, section et matériau (apparence comprise) relus
        TSA::Model::Material tinted = m.getBeam(b3.elementId)->material();
        const std::string color = tinted.visual.baseColor;
        const auto filePath = std::filesystem::temp_directory_path() / "tsa_test430.tsa";
        const QString file = QString::fromStdWString(filePath.wstring());
        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 430: enregistrement .tsa (" + err + ")");
        Model r;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, r, nullptr, &err), "Test 430: relecture .tsa (" + err + ")");
        std::filesystem::remove(filePath);
        bool same = r.bim().types().size() == 7;
        for (const auto& [tid, t] : m.bim().types())
        {
            const auto* rt = r.bim().type(tid);
            same = same && rt && rt->globalId == t.globalId && rt->name == t.name && rt->section.name == t.section.name
                   && rt->material.name == t.material.name && rt->material.visual.baseColor == t.material.visual.baseColor
                   && std::abs(rt->thickness - t.thickness) < 1e-12 && r.bim().occurrences(tid).size() == m.bim().occurrences(tid).size();
        }
        TEST_CHECK(same, "Test 430: .tsa relu : mêmes types, GlobalId, définitions et occurrences");

        // Bloc BIM d'une version antérieure (sans « types » ni « type ») : lu, produits sans type
        QJsonObject old = m.bim().toJson();
        old.remove("types");
        old.remove("nextTypeId");
        TSA::BIM::BimModel legacy;
        bool untyped = TSA::BIM::BimModel::fromJson(old, legacy) && legacy.types().empty();
        for (const auto& [id, p] : legacy.elements()) untyped = untyped && p.typeId == 0;
        TEST_CHECK(untyped && legacy.elements().size() == m.bim().elements().size(), "Test 430: ancien bloc BIM sans types : lu, produits sans type");
        std::cout << "[PASS] Test 430: Types de produits — création, persistance, compatibilité" << std::endl;
        ++passed;
    }

    // TEST 431 : occurrence modifiée → écart affiché ; propagation au type (une entrée Annuler), réapplication,
    // résultats de calcul périmés
    {
        Model m;
        ElementCreation c(m);
        auto b1 = c.createBar(pt(0, 0, 3), pt(5, 0, 3), beamProps());
        auto b2 = c.createBar(pt(0, 4, 3), pt(5, 4, 3), beamProps());
        auto b3 = c.createBar(pt(0, 8, 3), pt(5, 8, 3), beamProps());
        TSA::UI::ModelTreeWidget tree(&m);
        tree.refreshAll();
        QString lastMessage;
        QObject::connect(&tree, &TSA::UI::ModelTreeWidget::modelEditedByTree, [&lastMessage](const QString& msg) { lastMessage = msg; });
        TEST_CHECK(tree.createProductTypes() == 1 && m.canUndo(), "Test 431: type créé depuis l'arbre (une entrée Annuler)");
        const int p2 = m.bim().physicalOf({ ElementKind::Beam, b2.elementId })->id;
        const int typeId = m.bim().element(p2)->typeId;

        // Occurrence modifiée seule (comme dans la palette) : écart signalé (fonction et panneau BIM)
        m.getBeam(b2.elementId)->setSection(Section::rectangular(0.30, 0.60, "R30x60"));
        m.notifyBeamModified(b2.elementId);
        TEST_CHECK(TSA::BIM::typeDeviation(m, *m.bim().element(p2)) == "section R30x60 au lieu de R30x50", "Test 431: écart de la poutre modifiée");
        TSA::UI::ElementBimPanel panel;
        panel.setModel(&m);
        panel.showElement(TSA::BIM::AnalyticalRef { ElementKind::Beam, b2.elementId });
        TEST_CHECK(panel.text().contains("Type : Poutre R30x50") && panel.text().contains("diffère du type"), "Test 431: palette : type et écart affichés");

        auto results = std::make_shared<TSA::Analysis::ResultsModel>();
        results->setValid(true);
        TSA::Analysis::ResultsValidityGuard guard(&m);
        guard.trackResults(results);
        m.clearUndoRedo();
        TEST_CHECK(tree.propagateProductType(p2) == 2 && lastMessage.contains("propagé"), "Test 431: propagation au type : les 2 autres poutres modifiées");
        bool all60 = true;
        for (int id : { b1.elementId, b2.elementId, b3.elementId }) all60 = all60 && m.getBeam(id)->section().name == "R30x60";
        TEST_CHECK(all60 && m.bim().type(typeId)->section.name == "R30x60" && !guard.resultsUpToDate(),
                   "Test 431: type et 3 occurrences en R30x60, résultats de calcul périmés");
        TEST_CHECK(m.undo() && m.getBeam(b1.elementId)->section().name == "R30x50" && m.getBeam(b3.elementId)->section().name == "R30x50"
                       && m.bim().type(typeId)->section.name == "R30x50" && !m.canUndo(),
                   "Test 431: une seule entrée Annuler : type et occurrences rétablis");
        TEST_CHECK(tree.reapplyProductType(typeId) == 1 && m.getBeam(b2.elementId)->section().name == "R30x50",
                   "Test 431: réapplication du type : la poutre modifiée revient à R30x50");
        TEST_CHECK(tree.reapplyProductType(typeId) == 0 && lastMessage.contains("conformes"), "Test 431: déjà conforme : rien à faire, message");
        std::cout << "[PASS] Test 431: Types de produits — écart, propagation, réapplication" << std::endl;
        ++passed;
    }

    // TEST 432 : types de produits en IFC — Ifc*Type + IfcRelDefinesByType (fichier validé par IfcOpenShell), réimport :
    // mêmes types (GlobalId), occurrences, définitions reprises des occurrences
    {
        Model m;
        ElementCreation c(m);
        c.createBar(pt(0, 0, 3), pt(5, 0, 3), beamProps());
        c.createBar(pt(0, 4, 3), pt(5, 4, 3), beamProps());
        SlabPreset sp;
        c.createSlab({ pt(0, 0, 3), pt(5, 0, 3), pt(5, 4, 3), pt(0, 4, 3) }, sp);
        TSA::BIM::createTypesFromModel(m);
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-18T12:00:00";
        std::string doc;
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, doc).ok, "Test 432: export IFC");
        TEST_CHECK(doc.find("IFCBEAMTYPE") != std::string::npos && doc.find("IFCSLABTYPE") != std::string::npos
                       && doc.find("IFCRELDEFINESBYTYPE") != std::string::npos, "Test 432: IfcBeamType, IfcSlabType, IfcRelDefinesByType écrits");
        const auto ifcPath = std::filesystem::temp_directory_path() / "tsa_test432.ifc";
        const auto er = TSA::BIM::Ifc::IfcExporter::exportToFile(m, ifcPath.string(), opt);
        std::filesystem::remove(ifcPath);
        TEST_CHECK(er.ok, "Test 432: fichier écrit après relecture de contrôle (IfcOpenShell)");

        Model back;
        TEST_CHECK(TSA::BIM::Ifc::IfcImporter::importString(doc, back).ok, "Test 432: réimport");
        bool same = back.bim().types().size() == m.bim().types().size();
        for (const auto& [tid, t] : m.bim().types())
        {
            const TSA::BIM::ProductType* bt = nullptr;
            for (const auto& [id, x] : back.bim().types()) if (x.globalId == t.globalId) bt = &x;
            same = same && bt && bt->name == t.name && bt->category == t.category && bt->material.name == t.material.name
                   && back.bim().occurrences(bt->id).size() == m.bim().occurrences(tid).size()
                   && (t.category != TSA::BIM::BimCategory::Beam || bt->section.name == t.section.name);
        }
        TEST_CHECK(same, "Test 432: réimport : mêmes types (GlobalId, nom), mêmes occurrences, définitions reprises");
        std::cout << "[PASS] Test 432: Types de produits en IFC" << std::endl;
        ++passed;
    }

    // TEST 494 : affectation d'une section (ruban : Profilés I/H, Rectangulaire, Circulaire) — une entrée Annuler,
    // section d'origine rendue par l'annulation, rien d'enregistré sans sélection. Auparavant : aucune entrée Annuler.
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(0, 0, 3);
        const int beam = m.addBar(a, b, Section::rectangular(0.30, 0.50, "R30x50"), Material::concreteC25_30());
        const int col = m.addColumn(a, c);
        const std::string beamBefore = m.getBeam(beam)->section().name, colBefore = m.getColumn(col)->section().name;
        const auto entries = m.undoManager() ? m.undoManager()->undoCount() : 0;
        const int n = TSA::Interaction::assignSection(m, { beam }, { col, 999 }, Section::ipe(300), "Profilé IPE 300");
        TEST_CHECK(n == 2 && m.getBeam(beam)->section().name == "IPE 300" && m.getColumn(col)->section().name == "IPE 300",
                   "Test 494: section affectée à la poutre et au poteau (id inexistant ignoré)");
        TEST_CHECK(m.undoManager() && m.undoManager()->undoCount() == entries + 1, "Test 494: une seule entrée Annuler");
        TEST_CHECK(m.undo() && m.getBeam(beam)->section().name == beamBefore && m.getColumn(col)->section().name == colBefore,
                   "Test 494: Annuler rend les sections d'origine");
        const auto afterUndo = m.undoManager()->undoCount();
        TEST_CHECK(TSA::Interaction::assignSection(m, {}, {}, Section::ipe(300), "vide") == 0 && m.undoManager()->undoCount() == afterUndo,
                   "Test 494: sélection vide : aucune entrée Annuler");
        std::cout << "[PASS] Test 494: Affectation de section annulable (ruban)" << std::endl;
        ++passed;
    }

    // TEST 497 : Annuler / Rétablir des dalles et voiles — tout champ modifié est signalé aux vues (ModelDiff).
    // Auparavant, type, formulation, ouvertures, appuis de bord (et décalage du voile) n'étaient pas comparés :
    // les données étaient restaurées mais ni la vue 3D, ni l'arbre, ni le panneau Propriétés n'étaient prévenus.
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
        const int slab = m.addSlab({ a, b, c, d }, 0.20);
        const int e = m.addNode(0, 0, 3);
        const int wall = m.addWall(a, b, 3.0, 0.20);
        (void)e;
        auto slabChanged = [&](const std::function<void(Slab&)>& change) {
            const auto before = m.createSnapshot();
            change(*m.getSlab(slab));
            const auto diff = TSA::Model::ModelDiff::compute(before, m.createSnapshot());
            m.restoreSnapshot(before);
            return std::find(diff.modifiedSlabIds.begin(), diff.modifiedSlabIds.end(), slab) != diff.modifiedSlabIds.end();
        };
        auto wallChanged = [&](const std::function<void(Wall&)>& change) {
            const auto before = m.createSnapshot();
            change(*m.getWall(wall));
            const auto diff = TSA::Model::ModelDiff::compute(before, m.createSnapshot());
            m.restoreSnapshot(before);
            return std::find(diff.modifiedWallIds.begin(), diff.modifiedWallIds.end(), wall) != diff.modifiedWallIds.end();
        };
        SlabOpening hole;
        hole.name = "Trémie";
        hole.polygon = { { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 2 } };
        SurfaceEdgeSupport edge;
        edge.edge = 0;
        edge.fixed[2] = true;
        TEST_CHECK(slabChanged([](Slab& s) { s.setSlabType(SlabType::OneWay); }), "Test 497: dalle — type signalé");
        TEST_CHECK(slabChanged([](Slab& s) { s.setFormulation(s.formulation() == SurfaceFormulation::Shell ? SurfaceFormulation::Plate : SurfaceFormulation::Shell); }),
                   "Test 497: dalle — formulation signalée");
        TEST_CHECK(slabChanged([&](Slab& s) { s.setOpenings({ hole }); }), "Test 497: dalle — ouverture ajoutée signalée");
        TEST_CHECK(slabChanged([&](Slab& s) { s.setEdgeSupports({ edge }); }), "Test 497: dalle — appui de bord signalé");
        TEST_CHECK(wallChanged([](Wall& w) { w.setOffset(0.1); }), "Test 497: voile — décalage signalé");
        TEST_CHECK(wallChanged([](Wall& w) { w.setFormulation(w.formulation() == SurfaceFormulation::Shell ? SurfaceFormulation::Plate : SurfaceFormulation::Shell); }),
                   "Test 497: voile — formulation signalée");
        TEST_CHECK(wallChanged([&](Wall& w) { w.setOpenings({ hole }); }), "Test 497: voile — ouverture signalée");
        TEST_CHECK(wallChanged([&](Wall& w) { w.setEdgeSupports({ edge }); }), "Test 497: voile — appui de bord signalé");
        // Ouverture déplacée (même nombre de sommets) : le polygone est comparé, pas seulement le nombre
        m.getSlab(slab)->setOpenings({ hole });
        SlabOpening moved = hole;
        moved.polygon[0].first = 1.5;
        TEST_CHECK(slabChanged([&](Slab& s) { s.setOpenings({ moved }); }), "Test 497: dalle — sommet d'ouverture déplacé signalé");
        TEST_CHECK(!slabChanged([](Slab&) {}), "Test 497: dalle inchangée non signalée (pas de mise à jour inutile)");

        // Bout en bout : Annuler une ouverture ajoutée sous transaction notifie la dalle
        struct Observer : TSA::Model::IModelObserver
        {
            std::vector<int> slabs;
            void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override { slabs.insert(slabs.end(), diff.modifiedSlabIds.begin(), diff.modifiedSlabIds.end()); }
        } obs;
        m.addObserver(&obs);
        m.pushUndoState("Trémie");
        m.getSlab(slab)->setOpenings({});
        m.notifySlabModified(slab);
        TEST_CHECK(m.undo() && m.getSlab(slab)->openings().size() == 1 && std::count(obs.slabs.begin(), obs.slabs.end(), slab) == 1,
                   "Test 497: Annuler restaure l'ouverture et prévient les vues");
        m.removeObserver(&obs);
        std::cout << "[PASS] Test 497: Annuler / Rétablir des dalles et voiles : tous les champs signalés aux vues" << std::endl;
        ++passed;
    }

    // TEST 502 : ouvertures d'une dalle ou d'un voile (Maillage > Ouvertures) — une entrée Annuler, une seule
    // notification de l'élément (vue 3D, arbre, panneau Propriétés), modèle marqué modifié. Auparavant (BUG-113) : la
    // dalle n'était pas notifiée (forme 3D seule, mise à jour directe de la vue) ; la forme du voile était reconstruite
    // deux fois (notification + appel direct à la vue).
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
        const int slab = m.addSlab({ a, b, c, d }, 0.20);
        const int wall = m.addWall(a, b, 3.0, 0.20);
        struct Observer : TSA::Model::IModelObserver
        {
            int slabs = 0, walls = 0, edits = 0, other = 0;
            void onSlabModified(const Slab&) override { ++slabs; }
            void onWallModified(const Wall&) override { ++walls; }
            void onBeamModified(const Beam&) override { ++other; }
            void onNodeModified(const Node&) override { ++other; }
            void onModelEdited() override { ++edits; }
        } obs;
        m.addObserver(&obs);
        m.setModified(false);
        SlabOpening hole;
        hole.name = "Trémie";
        hole.polygon = { { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 2 } };
        const auto entries = m.undoManager()->undoCount();
        TEST_CHECK(TSA::Interaction::setSlabOpenings(m, slab, { hole }, "Ouvertures de la dalle"), "Test 502: ouverture de dalle affectée");
        TEST_CHECK(m.getSlab(slab)->openings().size() == 1 && obs.slabs == 1 && obs.walls == 0 && obs.other == 0 && obs.edits >= 1,
                   "Test 502: dalle notifiée une fois (" << obs.slabs << "), rien d'autre");
        TEST_CHECK(m.undoManager()->undoCount() == entries + 1 && m.isModified(), "Test 502: une entrée Annuler, projet modifié");
        TEST_CHECK(TSA::Interaction::setWallOpenings(m, wall, { hole }, "Ouvertures du voile") && m.getWall(wall)->openings().size() == 1
                       && obs.walls == 1 && obs.slabs == 1 && m.undoManager()->undoCount() == entries + 2,
                   "Test 502: voile : une notification (" << obs.walls << "), une entrée Annuler");
        TEST_CHECK(m.undo() && m.getWall(wall)->openings().empty() && m.undo() && m.getSlab(slab)->openings().empty(),
                   "Test 502: Annuler retire les ouvertures du voile puis de la dalle");
        const auto afterUndo = m.undoManager()->undoCount();
        TEST_CHECK(!TSA::Interaction::setSlabOpenings(m, 999, { hole }, "x") && !TSA::Interaction::setWallOpenings(m, 999, { hole }, "x")
                       && m.undoManager()->undoCount() == afterUndo,
                   "Test 502: élément inexistant : aucune entrée Annuler");
        m.removeObserver(&obs);
        std::cout << "[PASS] Test 502: Ouvertures de dalle et de voile : une entrée Annuler, une notification" << std::endl;
        ++passed;
    }

    return true;
}
