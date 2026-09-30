#include "test_common.h"
#include "UI/Widgets/PointSelector.h"
#include "Interaction/InteractionManager.h"

bool runSuite_NodeSystem(int& passed)
{
    // =========================================================================
    // TEST 53: Centralized Node Repository & Node Lifecycle (Model)
    // =========================================================================
    {
        std::cout << "\n--- TEST 53: Centralized Node Repository & Node Lifecycle ---" << std::endl;

        Model model;
        TEST_CHECK(model.nodes().empty(), "Subtest 53.1: Initially empty node repository");

        // 1. Ajout de nœuds avec nom auto et personnalisé
        int n1 = model.addNode(0.0, 0.0, 0.0);
        TEST_CHECK(n1 == 1, "Subtest 53.1: First node ID is 1");
        auto* node1 = model.getNode(n1);
        TEST_CHECK(node1 != nullptr, "Subtest 53.1: Node 1 retrieved");
        TEST_CHECK(approxEqual(node1->x(), 0.0) && approxEqual(node1->y(), 0.0) && approxEqual(node1->z(), 0.0), "Subtest 53.1: Node 1 coordinates match");
        TEST_CHECK(node1->formattedName() == "N001", "Subtest 53.1: Node 1 auto-formatted name N001");
        TEST_CHECK(node1->supportType() == TSA::Model::SupportType::Free, "Subtest 53.1: Default support is Free");

        // 2. Nœud avec nom et support personnalisé
        int n2 = model.addNode(5.0, 0.0, 3.0, "L1", "Poteau_Base");
        TEST_CHECK(n2 == 2, "Subtest 53.2: Second node ID is 2");
        auto* node2 = model.getNode(n2);
        TEST_CHECK(node2 != nullptr, "Subtest 53.2: Node 2 retrieved");
        TEST_CHECK(node2->name() == "Poteau_Base", "Subtest 53.2: Custom name preserved");
        TEST_CHECK(node2->levelId() == "L1", "Subtest 53.2: Level ID preserved");

        node2->setSupportType(TSA::Model::SupportType::Fixed);
        TEST_CHECK(node2->supportType() == TSA::Model::SupportType::Fixed, "Subtest 53.2: Support type changed to Fixed");

        // 3. Récupération par ID inexistant
        TEST_CHECK(model.getNode(999) == nullptr, "Subtest 53.3: Inexistent node returns nullptr");

        // 4. Déplacement de nœud et Point3D
        node1 = model.getNode(n1);
        if (node1)
        {
            node1->setCoordinates(1.0, 2.0, 3.0);
        }
        node1 = model.getNode(n1);
        TEST_CHECK(node1 && approxEqual(node1->x(), 1.0) && approxEqual(node1->y(), 2.0) && approxEqual(node1->z(), 3.0), "Subtest 53.4: Node coordinates updated");
        TEST_CHECK(node1 && approxEqual(node1->point3D().x, 1.0) && approxEqual(node1->point3D().y, 2.0) && approxEqual(node1->point3D().z, 3.0), "Subtest 53.4: point3D() matches");

        // 5. Suppression de nœud
        bool removed = model.removeNode(n1);
        TEST_CHECK(removed, "Subtest 53.5: removeNode succeeded");
        TEST_CHECK(model.getNode(n1) == nullptr, "Subtest 53.5: Deleted node cannot be retrieved");
        TEST_CHECK(model.nodes().size() == 1, "Subtest 53.5: Total nodes count is 1");

        std::cout << "[PASS] Test 53: Centralized Node Repository & Lifecycle Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 54: PointSelector Syntax Parsing & Compact Formatting (Modes A & C)
    // =========================================================================
    {
        std::cout << "\n--- TEST 54: PointSelector Syntax Parsing & Compact Formatting ---" << std::endl;

        Model model;
        int id1 = model.addNode(10.0, 20.0, 30.0);
        int id2 = model.addNode(0.0, 0.0, 5.0, "", "Sommet");

        gp_Pnt pt;
        int outId = -1;

        // Subtest 54.1: Format avec parenthèses "(X, Y, Z)"
        bool ok1 = TSA::UI::PointSelector::parsePointString("(10.5, 20.25, 30.0)", &model, pt, outId);
        TEST_CHECK(ok1, "Subtest 54.1: (X, Y, Z) parsed successfully");
        TEST_CHECK(approxEqual(pt.X(), 10.5) && approxEqual(pt.Y(), 20.25) && approxEqual(pt.Z(), 30.0), "Subtest 54.1: Coordinates exact");
        TEST_CHECK(outId == -1, "Subtest 54.1: Free point ID is -1");

        // Subtest 54.2: Format point-virgule "X; Y; Z" avec virgules décimales
        bool ok2 = TSA::UI::PointSelector::parsePointString("10,5; 20,25; 30,0", &model, pt, outId);
        TEST_CHECK(ok2, "Subtest 54.2: X; Y; Z with comma decimals parsed successfully");
        TEST_CHECK(approxEqual(pt.X(), 10.5) && approxEqual(pt.Y(), 20.25) && approxEqual(pt.Z(), 30.0), "Subtest 54.2: Coordinates exact");

        // Subtest 54.3: Format séparé par espaces "X Y Z"
        bool ok3 = TSA::UI::PointSelector::parsePointString("100.0 200.0 300.0", &model, pt, outId);
        TEST_CHECK(ok3, "Subtest 54.3: Space-separated coordinates parsed");
        TEST_CHECK(approxEqual(pt.X(), 100.0) && approxEqual(pt.Y(), 200.0) && approxEqual(pt.Z(), 300.0), "Subtest 54.3: Coordinates exact");

        // Subtest 54.4: Syntaxe directe de nœud "N1", "n1"
        bool ok4 = TSA::UI::PointSelector::parsePointString("N1", &model, pt, outId);
        TEST_CHECK(ok4, "Subtest 54.4: N1 parsed successfully");
        TEST_CHECK(outId == id1, "Subtest 54.4: Resolved to node id 1");
        TEST_CHECK(approxEqual(pt.X(), 10.0) && approxEqual(pt.Y(), 20.0) && approxEqual(pt.Z(), 30.0), "Subtest 54.4: Retrieved exact node coordinates");

        bool ok4_lower = TSA::UI::PointSelector::parsePointString("n1", &model, pt, outId);
        TEST_CHECK(ok4_lower && outId == id1, "Subtest 54.4: Lowercase n1 parsed");

        // Subtest 54.5: Syntaxe entier "2" pour nœud N2
        bool ok5 = TSA::UI::PointSelector::parsePointString("2", &model, pt, outId);
        TEST_CHECK(ok5, "Subtest 54.5: Number 2 resolved to existing node N2");
        TEST_CHECK(outId == id2, "Subtest 54.5: Out ID is 2");
        TEST_CHECK(approxEqual(pt.Z(), 5.0), "Subtest 54.5: Z is 5.0");

        // Subtest 54.6: Coordonnées correspondant exactement à un nœud existant
        bool ok6 = TSA::UI::PointSelector::parsePointString("(10.0, 20.0, 30.0)", &model, pt, outId);
        TEST_CHECK(ok6, "Subtest 54.6: Exact node coordinates detected");
        TEST_CHECK(outId == id1, "Subtest 54.6: Automatically matched to existing node N1");

        // Subtest 54.7: Chaînes invalides
        TEST_CHECK(!TSA::UI::PointSelector::parsePointString("", &model, pt, outId), "Subtest 54.7: Empty string invalid");
        TEST_CHECK(!TSA::UI::PointSelector::parsePointString("texte", &model, pt, outId), "Subtest 54.7: Random text invalid");
        TEST_CHECK(!TSA::UI::PointSelector::parsePointString("(1.0, 2.0)", &model, pt, outId), "Subtest 54.7: 2D coordinates rejected (needs 3D)");
        TEST_CHECK(!TSA::UI::PointSelector::parsePointString("N999", &model, pt, outId), "Subtest 54.7: Non-existent node ID rejected");

        // Subtest 54.8: Formatage compact
        QString fmtFree = TSA::UI::PointSelector::formatPointString(gp_Pnt(1.5, 2.5, 3.5), -1);
        TEST_CHECK(fmtFree.contains("1.50") && fmtFree.contains("2.50") && fmtFree.contains("3.50"), "Subtest 54.8: Free point formatted with 2 decimals");

        QString fmtNode = TSA::UI::PointSelector::formatPointString(gp_Pnt(10.0, 20.0, 30.0), id1, &model);
        TEST_CHECK(fmtNode.startsWith("N1"), "Subtest 54.8: Node format starts with N1");

        QString fmtNamed = TSA::UI::PointSelector::formatPointString(gp_Pnt(0.0, 0.0, 5.0), id2, &model);
        TEST_CHECK(fmtNamed.contains("N2") && fmtNamed.contains("Sommet"), "Subtest 54.8: Named node includes custom name");

        std::cout << "[PASS] Test 54: PointSelector Syntax Parsing & Compact Formatting Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 55: 3D Point Picking Non-Destructive Behavior & Snapping Priority
    // =========================================================================
    {
        std::cout << "\n--- TEST 55: 3D Point Picking Non-Destructive Behavior & Snapping ---" << std::endl;

        Model model;
        int nA = model.addNode(0.0, 0.0, 0.0);
        model.addNode(5.0, 0.0, 0.0);
        size_t initialCount = model.nodes().size();
        TEST_CHECK(initialCount == 2, "Subtest 55.1: Initial count is 2 nodes");

        TSA::Interaction::InteractionManager interactMgr;

        // 1. Simulation d'un pick 3D sur un point libre : NE DOIT PAS CRÉER DE NŒUD
        bool pickedCallback = false;
        gp_Pnt capturedPoint;
        int capturedNodeId = 0;

        TSA::Interaction::SelectionRequest req;
        req.mode = TSA::Interaction::SelectionMode::SelectPoint;
        req.targetField = "Point Libre";
        req.onSelected = [&](const TSA::Interaction::SelectedEntity& entity) {
            pickedCallback = true;
            capturedPoint = entity.point;
            capturedNodeId = entity.entityId;
        };
        interactMgr.requestSelection(req);

        TSA::Interaction::SelectedEntity freeEntity;
        freeEntity.mode = TSA::Interaction::SelectionMode::SelectPoint;
        freeEntity.point = gp_Pnt(12.0, 4.0, 2.5);
        freeEntity.entityId = -1; // Point libre (aucun nœud accroché)
        interactMgr.completeSelection(freeEntity);

        TEST_CHECK(pickedCallback, "Subtest 55.1: Pick callback executed");
        TEST_CHECK(approxEqual(capturedPoint.X(), 12.0), "Subtest 55.1: Point X captured");
        TEST_CHECK(capturedNodeId == -1, "Subtest 55.1: Entity ID is -1 (free point)");
        TEST_CHECK(model.nodes().size() == initialCount, "Subtest 55.1: CRITICAL - Picking 3D point did NOT create any node in model");

        // 2. Simulation d'un pick 3D avec Node Snap sur nA
        pickedCallback = false;
        TSA::Interaction::SelectionRequest reqSnap;
        reqSnap.mode = TSA::Interaction::SelectionMode::SelectPoint;
        reqSnap.onSelected = [&](const TSA::Interaction::SelectedEntity& entity) {
            pickedCallback = true;
            capturedPoint = entity.point;
            capturedNodeId = entity.entityId;
        };
        interactMgr.requestSelection(reqSnap);

        TSA::Interaction::SelectedEntity nodeSnapEntity;
        nodeSnapEntity.mode = TSA::Interaction::SelectionMode::SelectPoint;
        nodeSnapEntity.point = gp_Pnt(0.0, 0.0, 0.0);
        nodeSnapEntity.entityId = nA; // Nœud accroché
        interactMgr.completeSelection(nodeSnapEntity);

        TEST_CHECK(pickedCallback, "Subtest 55.2: Node snap pick callback executed");
        TEST_CHECK(capturedNodeId == nA, "Subtest 55.2: Node snap correctly detected node nA");
        TEST_CHECK(model.nodes().size() == initialCount, "Subtest 55.2: CRITICAL - Model node count unchanged");

        // 3. Annulation de la sélection 3D (Échap)
        bool cancelCallback = false;
        TSA::Interaction::SelectionRequest reqCancel;
        reqCancel.mode = TSA::Interaction::SelectionMode::SelectPoint;
        reqCancel.onCancelled = [&]() {
            cancelCallback = true;
        };
        interactMgr.requestSelection(reqCancel);
        TEST_CHECK(interactMgr.hasActiveSelectionRequest(), "Subtest 55.3: Selection request active");
        interactMgr.cancelSelectionRequest();
        TEST_CHECK(cancelCallback, "Subtest 55.3: onCancelled callback executed on cancel");
        TEST_CHECK(!interactMgr.hasActiveSelectionRequest(), "Subtest 55.3: No active selection request after cancel");

        std::cout << "[PASS] Test 55: Non-Destructive 3D Picking & Snapping Hierarchy Validated!" << std::endl;
        passed++;
    }

    return true;
}
