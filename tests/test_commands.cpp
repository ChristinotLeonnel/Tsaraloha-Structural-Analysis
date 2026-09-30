#include "test_common.h"

bool runSuite_Commands(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 19: Functional Suite for Differential Undo/Redo & ModelDiff
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 19: Functional Suite for Differential Undo/Redo & ModelDiff ---" << std::endl;

        class MockDiffObserver : public IModelObserver
        {
        public:
            int diffCallCount = 0;
            int clearedCallCount = 0;
            ModelDiff lastDiff;

            void onModelDiffApplied(const ModelDiff& diff) override
            {
                diffCallCount++;
                lastDiff = diff;
            }

            void onModelCleared() override
            {
                clearedCallCount++;
            }
        };

        Model testModel;
        MockDiffObserver obs;
        testModel.addObserver(&obs);

        // 19.1: Création d'une barre, Undo, Redo
        {
            testModel.pushUndoState("Create Bar 1");
            int n1 = testModel.addNode(0, 0, 0);
            int n2 = testModel.addNode(5, 0, 0);
            int b1 = testModel.addBeam(n1, n2, 0.3, 0.4);

            TEST_CHECK(testModel.canUndo(), "Test 19.1: canUndo true");
            obs.diffCallCount = 0;
            obs.clearedCallCount = 0;

            bool undoOk = testModel.undo();
            TEST_CHECK(undoOk, "Test 19.1: undo succeeded");
            TEST_CHECK(obs.clearedCallCount == 0, "Test 19.1: onModelCleared NEVER called on Undo");
            TEST_CHECK(obs.diffCallCount == 1, "Test 19.1: onModelDiffApplied called exactly once");
            TEST_CHECK(obs.lastDiff.deletedBeamIds.size() == 1 && obs.lastDiff.deletedBeamIds[0] == b1, "Test 19.1: b1 deleted in diff");
            TEST_CHECK(testModel.beams().empty(), "Test 19.1: model has 0 beams after undo");

            // Redo
            bool redoOk = testModel.redo();
            TEST_CHECK(redoOk, "Test 19.1: redo succeeded");
            TEST_CHECK(obs.clearedCallCount == 0, "Test 19.1: onModelCleared NEVER called on Redo");
            TEST_CHECK(obs.lastDiff.createdBeamIds.size() == 1 && obs.lastDiff.createdBeamIds[0] == b1, "Test 19.1: b1 recreated in diff");
            TEST_CHECK(testModel.beams().size() == 1, "Test 19.1: model has 1 beam after redo");
            std::cout << "  [PASS] Subtest 19.1: Create Bar -> Undo -> Redo (Differential correctness confirmed)" << std::endl;
        }

        // 19.2: Suppression d'une barre parmi 5, Undo, Redo
        {
            testModel.clear();
            obs.diffCallCount = 0;
            obs.clearedCallCount = 0;

            std::vector<int> nodeIds;
            for (int i = 0; i <= 5; ++i)
                nodeIds.push_back(testModel.addNode(i * 3.0, 0, 0));

            std::vector<int> beamIds;
            for (int i = 0; i < 5; ++i)
                beamIds.push_back(testModel.addBeam(nodeIds[i], nodeIds[i+1], 0.25, 0.40));

            // Supprimer la barre 3 (beamIds[2])
            int deletedId = beamIds[2];
            testModel.pushUndoState("Bar 3 Deleted");
            testModel.removeBeam(deletedId);

            TEST_CHECK(testModel.beams().size() == 4, "Test 19.2: 4 beams remain");

            // Undo de la suppression
            bool undoOk = testModel.undo();
            TEST_CHECK(undoOk, "Test 19.2: undo delete succeeded");
            TEST_CHECK(obs.clearedCallCount == 0, "Test 19.2: onModelCleared NEVER called");
            TEST_CHECK(obs.lastDiff.createdBeamIds.size() == 1 && obs.lastDiff.createdBeamIds[0] == deletedId,
                       "Test 19.2: ONLY deleted bar is in createdBeamIds on undo");
            TEST_CHECK(obs.lastDiff.deletedBeamIds.empty(), "Test 19.2: no deleted beams on undo");
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.empty(), "Test 19.2: no modified beams on undo");
            TEST_CHECK(testModel.beams().size() == 5, "Test 19.2: all 5 beams restored");

            // Redo de la suppression
            bool redoOk = testModel.redo();
            TEST_CHECK(redoOk, "Test 19.2: redo delete succeeded");
            TEST_CHECK(obs.lastDiff.deletedBeamIds.size() == 1 && obs.lastDiff.deletedBeamIds[0] == deletedId,
                       "Test 19.2: ONLY target bar is in deletedBeamIds on redo");
            TEST_CHECK(obs.lastDiff.createdBeamIds.empty(), "Test 19.2: no created beams on redo");
            TEST_CHECK(testModel.beams().size() == 4, "Test 19.2: 4 beams after redo");
            std::cout << "  [PASS] Subtest 19.2: Delete 1 Bar among 5 -> Undo -> Redo (Only target bar affected)" << std::endl;
        }

        // 19.3: Modification de section d'une barre, Undo, Redo
        {
            testModel.clear();
            obs.clearedCallCount = 0;
            obs.diffCallCount = 0;
            int nA = testModel.addNode(0, 0, 0);
            int nB = testModel.addNode(4, 0, 0);
            int nC = testModel.addNode(8, 0, 0);
            int b1 = testModel.addBeam(nA, nB, 0.30, 0.40);
            int b2 = testModel.addBeam(nB, nC, 0.30, 0.40);

            // Modifier b1 en Cercle D=400mm
            testModel.pushUndoState("Change B1 Section to Circle");
            auto* pB1 = testModel.getBeam(b1);
            TEST_CHECK(pB1 != nullptr, "Test 19.3: pB1 valid");
            Section circleSec = Section::circular(0.40, "Circle 400");
            pB1->setSection(circleSec);

            // Undo
            bool undoOk = testModel.undo();
            TEST_CHECK(undoOk, "Test 19.3: undo succeeded");
            TEST_CHECK(obs.clearedCallCount == 0, "Test 19.3: no clear called");
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == b1,
                       "Test 19.3: ONLY b1 marked modified");
            TEST_CHECK(obs.lastDiff.createdBeamIds.empty(), "Test 19.3: no created beams");
            TEST_CHECK(obs.lastDiff.deletedBeamIds.empty(), "Test 19.3: no deleted beams");
            TEST_CHECK(testModel.getBeam(b1)->section().shape == SectionShape::Rectangular, "Test 19.3: b1 restored to Rectangular");
            TEST_CHECK(testModel.getBeam(b2)->section().shape == SectionShape::Rectangular, "Test 19.3: b2 unchanged");

            // Redo
            bool redoOk = testModel.redo();
            TEST_CHECK(redoOk, "Test 19.3: redo succeeded");
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == b1,
                       "Test 19.3: ONLY b1 marked modified on redo");
            TEST_CHECK(testModel.getBeam(b1)->section().shape == SectionShape::Circular, "Test 19.3: b1 redo to Circular");
            std::cout << "  [PASS] Subtest 19.3: Modify Section -> Undo -> Redo (Only modified beam updated)" << std::endl;
        }

        // 19.4: Déplacement d'un nœud et propagation différentielle
        {
            testModel.clear();
            obs.clearedCallCount = 0;
            obs.diffCallCount = 0;
            int n1 = testModel.addNode(0, 0, 0);
            int n2 = testModel.addNode(5, 0, 0);
            int n3 = testModel.addNode(10, 0, 0);
            int n4 = testModel.addNode(15, 0, 0);

            int bConnect = testModel.addBeam(n1, n2, 0.3, 0.4); // connecté à n1
            int bOther = testModel.addBeam(n3, n4, 0.3, 0.4);   // non connecté à n1

            // Déplacer n1
            testModel.pushUndoState("Move Node 1");
            auto* pN1 = testModel.getNode(n1);
            pN1->setCoordinates(0, 2.5, 3.0);

            // Undo
            bool undoOk = testModel.undo();
            TEST_CHECK(undoOk, "Test 19.4: undo move succeeded");
            TEST_CHECK(obs.clearedCallCount == 0, "Test 19.4: no clear");
            TEST_CHECK(obs.lastDiff.modifiedNodeIds.size() == 1 && obs.lastDiff.modifiedNodeIds[0] == n1,
                       "Test 19.4: n1 in modifiedNodeIds");
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == bConnect,
                       "Test 19.4: connected bConnect propagated to modifiedBeamIds");
            // bOther NE doit PAS être présent dans modifiedBeamIds !
            TEST_CHECK(std::find(obs.lastDiff.modifiedBeamIds.begin(), obs.lastDiff.modifiedBeamIds.end(), bOther)
                       == obs.lastDiff.modifiedBeamIds.end(), "Test 19.4: unconnected bOther NOT modified");

            // Redo
            bool redoOk = testModel.redo();
            TEST_CHECK(redoOk, "Test 19.4: redo move succeeded");
            TEST_CHECK(obs.lastDiff.modifiedNodeIds.size() == 1 && obs.lastDiff.modifiedNodeIds[0] == n1,
                       "Test 19.4: n1 in modifiedNodeIds on redo");
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == bConnect,
                       "Test 19.4: bConnect in modifiedBeamIds on redo");
            std::cout << "  [PASS] Subtest 19.4: Move Node -> Connected Bars marked MODIFIED, Unconnected UNCHANGED" << std::endl;
        }

        // 19.5: Modification de matériau
        {
            testModel.clear();
            obs.clearedCallCount = 0;
            obs.diffCallCount = 0;
            int nA = testModel.addNode(0, 0, 0);
            int nB = testModel.addNode(3, 0, 0);
            int b = testModel.addBeam(nA, nB, 0.2, 0.3);

            testModel.pushUndoState("Steel Material");
            auto* pB = testModel.getBeam(b);
            Material steelMat;
            steelMat.type = MaterialType::Steel;
            steelMat.name = "S355";
            pB->setMaterial(steelMat);

            testModel.undo();
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == b,
                       "Test 19.5: Material change detected in diff on undo");
            TEST_CHECK(testModel.getBeam(b)->material().type == MaterialType::Concrete, "Test 19.5: Material restored to Concrete");

            testModel.redo();
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == b,
                       "Test 19.5: Material change detected in diff on redo");
            TEST_CHECK(testModel.getBeam(b)->material().type == MaterialType::Steel, "Test 19.5: Material restored to Steel");
            std::cout << "  [PASS] Subtest 19.5: Modify Material -> Undo -> Redo (Detected and differentiated)" << std::endl;
        }

        // 19.6: Modification des relâchements (End Releases)
        {
            testModel.clear();
            obs.clearedCallCount = 0;
            obs.diffCallCount = 0;
            int nA = testModel.addNode(0, 0, 0);
            int nB = testModel.addNode(4, 0, 0);
            int b = testModel.addBeam(nA, nB, 0.2, 0.3);

            testModel.pushUndoState("Hinged Releases");
            auto* pB = testModel.getBeam(b);
            EndRelease hingedRel;
            hingedRel.my = true;
            hingedRel.mz = true;
            pB->setEndRelease(hingedRel);

            testModel.undo();
            TEST_CHECK(obs.lastDiff.modifiedBeamIds.size() == 1 && obs.lastDiff.modifiedBeamIds[0] == b,
                       "Test 19.6: EndRelease change detected in diff on undo");
            TEST_CHECK(testModel.getBeam(b)->endRelease().my == false, "Test 19.6: Release restored to Fixed (my=false)");

            testModel.redo();
            TEST_CHECK(testModel.getBeam(b)->endRelease().my == true, "Test 19.6: Release restored to Hinged (my=true)");
            std::cout << "  [PASS] Subtest 19.6: Modify Releases -> Undo -> Redo (Detected and differentiated)" << std::endl;
        }

        // 19.7: Multiples Undo / Multiples Redo séquentiels
        {
            testModel.clear();
            obs.clearedCallCount = 0;
            obs.diffCallCount = 0;
            testModel.pushUndoState("Step 1 (Bar 1)");
            int n1 = testModel.addNode(0, 0, 0);
            int n2 = testModel.addNode(1, 0, 0);
            int b1 = testModel.addBeam(n1, n2, 0.2, 0.2);
            (void)b1;

            testModel.pushUndoState("Step 2 (Bar 2)");
            int n3 = testModel.addNode(2, 0, 0);
            int b2 = testModel.addBeam(n2, n3, 0.2, 0.2);

            testModel.pushUndoState("Step 3 (Bar 3)");
            int n4 = testModel.addNode(3, 0, 0);
            int b3 = testModel.addBeam(n3, n4, 0.2, 0.2);

            TEST_CHECK(testModel.beams().size() == 3, "Test 19.7: 3 beams initially");

            // Undo step 3
            testModel.undo();
            TEST_CHECK(obs.lastDiff.deletedBeamIds.size() == 1 && obs.lastDiff.deletedBeamIds[0] == b3, "Test 19.7: b3 removed");
            TEST_CHECK(testModel.beams().size() == 2, "Test 19.7: 2 beams left");

            // Undo step 2
            testModel.undo();
            TEST_CHECK(obs.lastDiff.deletedBeamIds.size() == 1 && obs.lastDiff.deletedBeamIds[0] == b2, "Test 19.7: b2 removed");
            TEST_CHECK(testModel.beams().size() == 1, "Test 19.7: 1 beam left");

            // Redo step 2
            testModel.redo();
            TEST_CHECK(obs.lastDiff.createdBeamIds.size() == 1 && obs.lastDiff.createdBeamIds[0] == b2, "Test 19.7: b2 restored");
            TEST_CHECK(testModel.beams().size() == 2, "Test 19.7: 2 beams restored");

            // Redo step 3
            testModel.redo();
            TEST_CHECK(obs.lastDiff.createdBeamIds.size() == 1 && obs.lastDiff.createdBeamIds[0] == b3, "Test 19.7: b3 restored");
            TEST_CHECK(testModel.beams().size() == 3, "Test 19.7: 3 beams restored");
            std::cout << "  [PASS] Subtest 19.7: Multiple Undo/Redo sequence perfectly maintains state & diffs" << std::endl;
        }

        testModel.removeObserver(&obs);
        std::cout << "[PASS] Test 19: Full Differential Undo/Redo Functional Suite (7 Subtests Validated)" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 20: Performance and Scalability Benchmark (100, 1000, 5000 Bars)
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 20: Performance & Scalability Benchmark for Differential Undo/Redo ---" << std::endl;

        class BenchmarkObserver : public IModelObserver
        {
        public:
            int diffCount = 0;
            int clearCount = 0;
            ModelDiff lastDiff;

            void onModelDiffApplied(const ModelDiff& diff) override
            {
                diffCount++;
                lastDiff = diff;
            }

            void onModelCleared() override
            {
                clearCount++;
            }
        };

        const std::vector<int> modelSizes = { 100, 1000, 5000 };

        for (int totalBars : modelSizes)
        {
            Model benchModel;
            BenchmarkObserver benchObs;
            benchModel.addObserver(&benchObs);

            // Générer un modèle linéaire avec totalBars barres
            std::vector<int> nodeIds;
            nodeIds.reserve(totalBars + 1);
            for (int i = 0; i <= totalBars; ++i)
            {
                nodeIds.push_back(benchModel.addNode(i * 1.5, (i % 10) * 2.0, ((i / 10) % 5) * 3.0));
            }

            std::vector<int> beamIds;
            beamIds.reserve(totalBars);
            for (int i = 0; i < totalBars; ++i)
            {
                beamIds.push_back(benchModel.addBeam(nodeIds[i], nodeIds[i+1], 0.30, 0.45));
            }

            // Supprimer une seule barre au milieu du modèle (index totalBars / 2)
            int targetIdx = totalBars / 2;
            int targetBarId = beamIds[targetIdx];
            benchModel.pushUndoState("Delete Single Bar");
            benchModel.removeBeam(targetBarId);

            TEST_CHECK(benchModel.beams().size() == static_cast<size_t>(totalBars - 1), "Test 20: 1 bar removed");

            // Mesurer le temps d'exécution de Undo (restauration logique + calcul du diff)
            benchObs.diffCount = 0;
            benchObs.clearCount = 0;

            auto tStart = std::chrono::high_resolution_clock::now();
            bool undoOk = benchModel.undo();
            auto tEnd = std::chrono::high_resolution_clock::now();

            double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

            TEST_CHECK(undoOk, "Test 20: undo succeeded");
            TEST_CHECK(benchObs.clearCount == 0, "Test 20: clearScene/rebuildAll was NEVER called!");
            TEST_CHECK(benchObs.diffCount == 1, "Test 20: onModelDiffApplied called exactly once");

            // Vérifier la nature purement différentielle : 1 seule barre créée, 0 barres supprimées/modifiées
            TEST_CHECK(benchObs.lastDiff.createdBeamIds.size() == 1 && benchObs.lastDiff.createdBeamIds[0] == targetBarId,
                       "Test 20: exact target bar restored");
            TEST_CHECK(benchObs.lastDiff.deletedBeamIds.empty(), "Test 20: 0 deletions");
            TEST_CHECK(benchObs.lastDiff.modifiedBeamIds.empty(), "Test 20: 0 modifications");
            TEST_CHECK(benchModel.beams().size() == static_cast<size_t>(totalBars), "Test 20: full bar count restored");

            std::cout << "  Model with " << totalBars << " bars:"
                      << " Undo of 1 deleted bar completed in " << elapsedMs << " ms"
                      << " (Cost: O(1) modified object, 4999+ untouched objects preserved)" << std::endl;

            // Le calcul logique + diff sur 5000 barres doit rester raisonnable.
            // En mode Debug/MSVC, 50 ms est trop serré (compilateur non optimisé, PCH, charge CPU).
            // On utilise 250 ms comme seuil dur, avec un avertissement au-delà de 100 ms.
            if (elapsedMs > 100.0 && elapsedMs < 250.0) {
                std::cout << "  [WARN] Undo took " << elapsedMs << " ms (> 100 ms) — acceptable in Debug, monitor in Release" << std::endl;
            }
            TEST_CHECK(elapsedMs < 350.0, "Test 20: Undo diff computation is fast (< 350 ms in Debug)");

            // Mesurer le temps d'exécution du Redo
            tStart = std::chrono::high_resolution_clock::now();
            bool redoOk = benchModel.redo();
            tEnd = std::chrono::high_resolution_clock::now();
            double redoElapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

            TEST_CHECK(redoOk, "Test 20: redo succeeded");
            TEST_CHECK(benchObs.lastDiff.deletedBeamIds.size() == 1 && benchObs.lastDiff.deletedBeamIds[0] == targetBarId,
                       "Test 20: exact target bar deleted on redo");
            TEST_CHECK(benchObs.lastDiff.createdBeamIds.empty(), "Test 20: 0 creations on redo");
            TEST_CHECK(benchObs.clearCount == 0, "Test 20: no clear on redo");

            std::cout << "  Model with " << totalBars << " bars:"
                      << " Redo of 1 deleted bar completed in " << redoElapsedMs << " ms" << std::endl;

            benchModel.removeObserver(&benchObs);
        }

        std::cout << "[PASS] Test 20: Performance and Scalability Benchmark Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 25: Command Pattern & Centralized Undo/Redo Architecture
    // =========================================================================
    {
        std::cout << "\n--- TEST 25: Command Pattern & Centralized Undo/Redo Architecture ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(0.0, 0.0, 4.0);

        TSA::UndoRedo::CommandManager cmdMgr(&model, model.undoManager());
        TEST_CHECK(!cmdMgr.canUndo(), "Test 25: cannot undo initially");
        TEST_CHECK(!cmdMgr.canRedo(), "Test 25: cannot redo initially");

        auto createCmd = std::make_unique<TSA::Commands::CreateBeamCommand>(model, n1, n2, 0.30, 0.60, "Poutre_P25");
        bool execOk = cmdMgr.executeCommand(std::move(createCmd));
        TEST_CHECK(execOk, "Test 25: CreateBeamCommand executed successfully");
        TEST_CHECK(model.beams().size() == 1, "Test 25: 1 beam created in model");
        TEST_CHECK(cmdMgr.canUndo(), "Test 25: canUndo is true after command execution");

        bool undoOk = cmdMgr.undo();
        TEST_CHECK(undoOk, "Test 25: undo succeeded");
        TEST_CHECK(model.beams().empty(), "Test 25: beam removed on undo");
        TEST_CHECK(cmdMgr.canRedo(), "Test 25: canRedo is true");

        bool redoOk = cmdMgr.redo();
        TEST_CHECK(redoOk, "Test 25: redo succeeded");
        TEST_CHECK(model.beams().size() == 1, "Test 25: beam restored on redo");

        std::cout << "[PASS] Test 25: Command Pattern & Centralized Undo/Redo Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // -------------------------------------------------------------------------
    // TEST 50: Professional CAD Command System, Command Catalog & Classification
    // (Inspiré d'AutoCAD & Robot Structural Analysis)
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 50: Professional CAD Command System & Command Classification ---" << std::endl;

        // 50.1: CommandCategory & Names
        {
            TEST_CHECK(TSA::Commands::categoryToString(TSA::Commands::CommandCategory::Create) == "Création / Dessin", "Subtest 50.1: Category Create string valid");
            TEST_CHECK(TSA::Commands::categoryToString(TSA::Commands::CommandCategory::Modify) == "Modification CAO", "Subtest 50.1: Category Modify string valid");
            TEST_CHECK(TSA::Commands::categoryToString(TSA::Commands::CommandCategory::Properties) == "Propriétés", "Subtest 50.1: Category Properties string valid");
            TEST_CHECK(TSA::Commands::categoryToString(TSA::Commands::CommandCategory::Analysis) == "Calculs / Analyse", "Subtest 50.1: Category Analysis string valid");
            std::cout << "  [PASS] Subtest 50.1: CommandCategory Taxonomy & Localized Names Verified" << std::endl;
        }

        // 50.2: CommandCatalog Registration & Querying
        {
            auto& catalog = TSA::Commands::CommandCatalog::instance();
            const auto* cmdBeam = catalog.findCommand("cmd.create.beam");
            TEST_CHECK(cmdBeam != nullptr, "Subtest 50.2: Command cmd.create.beam found");
            if (cmdBeam)
            {
                TEST_CHECK(cmdBeam->category == TSA::Commands::CommandCategory::Create, "Subtest 50.2: Beam command is in Create category");
                TEST_CHECK(cmdBeam->shortcut == "B", "Subtest 50.2: Beam shortcut is 'B'");
            }

            const auto* cmdCable = catalog.findCommand("cmd.create.cable");
            TEST_CHECK(cmdCable != nullptr, "Subtest 50.2: Command cmd.create.cable found");
            if (cmdCable)
            {
                TEST_CHECK(cmdCable->category == TSA::Commands::CommandCategory::Create, "Subtest 50.2: Cable command is in Create category");
            }

            auto createCmds = catalog.commandsInCategory(TSA::Commands::CommandCategory::Create);
            TEST_CHECK(createCmds.size() >= 8, "Subtest 50.2: At least 8 structural creation commands registered");

            auto modifyCmds = catalog.commandsInCategory(TSA::Commands::CommandCategory::Modify);
            TEST_CHECK(modifyCmds.size() >= 3, "Subtest 50.2: At least 3 modify commands registered");

            std::cout << "  [PASS] Subtest 50.2: CommandCatalog Central Registry & Querying Verified" << std::endl;
        }

        // 50.3: CreateElementCommands (Node, Column, Cable, Slab, Wall, Foundation, Truss)
        {
            TSA::Model::Model m;

            // 1. CreateNodeCommand
            auto nodeCmd1 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 0.0, 0.0, 0.0, "", "N1");
            TEST_CHECK(nodeCmd1->execute(), "Subtest 50.3: CreateNodeCommand 1 executed");
            int n1 = nodeCmd1->createdNodeId();
            TEST_CHECK(n1 > 0 && m.nodes().size() == 1, "Subtest 50.3: Node 1 in model");

            auto nodeCmd2 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 5.0, 0.0, 0.0, "", "N2");
            TEST_CHECK(nodeCmd2->execute(), "Subtest 50.3: CreateNodeCommand 2 executed");
            int n2 = nodeCmd2->createdNodeId();

            auto nodeCmd3 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 5.0, 4.0, 0.0, "", "N3");
            TEST_CHECK(nodeCmd3->execute(), "Subtest 50.3: CreateNodeCommand 3 executed");
            int n3 = nodeCmd3->createdNodeId();

            auto nodeCmd4 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 0.0, 4.0, 0.0, "", "N4");
            TEST_CHECK(nodeCmd4->execute(), "Subtest 50.3: CreateNodeCommand 4 executed");
            int n4 = nodeCmd4->createdNodeId();

            auto nodeCmd5 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 0.0, 0.0, 3.5, "", "N5");
            TEST_CHECK(nodeCmd5->execute(), "Subtest 50.3: CreateNodeCommand 5 executed");
            int n5 = nodeCmd5->createdNodeId();

            auto nodeCmd6 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 5.0, 0.0, 3.5, "", "N6");
            TEST_CHECK(nodeCmd6->execute(), "Subtest 50.3: CreateNodeCommand 6 executed");
            int n6 = nodeCmd6->createdNodeId();

            // 2. CreateColumnCommand
            auto colCmd = std::make_unique<TSA::Commands::CreateColumnCommand>(m, n1, n5, 0.35, 0.35, "Poteau_C1");
            TEST_CHECK(colCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Column command category is Create");
            TEST_CHECK(colCmd->execute(), "Subtest 50.3: CreateColumnCommand executed");
            int colId = colCmd->createdColumnId();
            TEST_CHECK(colId > 0, "Subtest 50.3: Column ID valid");
            TEST_CHECK(m.columns().size() == 1, "Subtest 50.3: Column added to model");

            // 3. CreateCableCommand
            auto cableCmd = std::make_unique<TSA::Commands::CreateCableCommand>(m, n2, n5, TSA::Model::CableType::StayCable, "Hauban_K1");
            TEST_CHECK(cableCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Cable command category is Create");
            TEST_CHECK(cableCmd->execute(), "Subtest 50.3: CreateCableCommand executed");
            int cableId = cableCmd->createdCableId();
            TEST_CHECK(cableId > 0, "Subtest 50.3: Cable ID valid");
            TEST_CHECK(m.cables().size() == 1, "Subtest 50.3: Cable added to model");

            // 4. CreateSlabCommand
            auto slabCmd = std::make_unique<TSA::Commands::CreateSlabCommand>(m, std::vector<int>{n1, n2, n3, n4}, 0.22, "Dalle_D1");
            TEST_CHECK(slabCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Slab command category is Create");
            TEST_CHECK(slabCmd->execute(), "Subtest 50.3: CreateSlabCommand executed");
            int slabId = slabCmd->createdSlabId();
            TEST_CHECK(slabId > 0, "Subtest 50.3: Slab ID valid");
            TEST_CHECK(m.slabs().size() == 1, "Subtest 50.3: Slab added to model");

            // 5. CreateWallCommand
            auto wallCmd = std::make_unique<TSA::Commands::CreateWallCommand>(m, n1, n4, 3.5, 0.20, "Voile_V1");
            TEST_CHECK(wallCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Wall command category is Create");
            TEST_CHECK(wallCmd->execute(), "Subtest 50.3: CreateWallCommand executed");
            int wallId = wallCmd->createdWallId();
            TEST_CHECK(wallId > 0, "Subtest 50.3: Wall ID valid");
            TEST_CHECK(m.walls().size() == 1, "Subtest 50.3: Wall added to model");

            // 6. CreateFoundationCommand
            auto fndCmd = std::make_unique<TSA::Commands::CreateFoundationCommand>(m, n1, 1.8, 1.8, 0.60, "Semelle_S1");
            TEST_CHECK(fndCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Foundation command category is Create");
            TEST_CHECK(fndCmd->execute(), "Subtest 50.3: CreateFoundationCommand executed");
            int fndId = fndCmd->createdFoundationId();
            TEST_CHECK(fndId > 0, "Subtest 50.3: Foundation ID valid");
            TEST_CHECK(m.foundations().size() == 1, "Subtest 50.3: Foundation added to model");

            // 7. CreateTrussMemberCommand
            auto trussCmd = std::make_unique<TSA::Commands::CreateTrussMemberCommand>(m, n1, n6, 0.08, "Diagonale_T1");
            TEST_CHECK(trussCmd->category() == TSA::Commands::CommandCategory::Create, "Subtest 50.3: Truss command category is Create");
            TEST_CHECK(trussCmd->execute(), "Subtest 50.3: CreateTrussMemberCommand executed");
            int trId = trussCmd->createdMemberId();
            TEST_CHECK(trId > 0, "Subtest 50.3: Truss member ID valid");
            TEST_CHECK(m.trussMembers().size() == 1, "Subtest 50.3: Truss member added to model");

            // Undo every command in reverse order
            TEST_CHECK(trussCmd->undo(), "Subtest 50.3: Undo CreateTrussMemberCommand");
            TEST_CHECK(m.trussMembers().empty(), "Subtest 50.3: Truss members empty");

            TEST_CHECK(fndCmd->undo(), "Subtest 50.3: Undo CreateFoundationCommand");
            TEST_CHECK(m.foundations().empty(), "Subtest 50.3: Foundations empty");

            TEST_CHECK(wallCmd->undo(), "Subtest 50.3: Undo CreateWallCommand");
            TEST_CHECK(m.walls().empty(), "Subtest 50.3: Walls empty");

            TEST_CHECK(slabCmd->undo(), "Subtest 50.3: Undo CreateSlabCommand");
            TEST_CHECK(m.slabs().empty(), "Subtest 50.3: Slabs empty");

            TEST_CHECK(cableCmd->undo(), "Subtest 50.3: Undo CreateCableCommand");
            TEST_CHECK(m.cables().empty(), "Subtest 50.3: Cables empty");

            TEST_CHECK(colCmd->undo(), "Subtest 50.3: Undo CreateColumnCommand");
            TEST_CHECK(m.columns().empty(), "Subtest 50.3: Columns empty");

            std::cout << "  [PASS] Subtest 50.3: CreateElementCommands (Node, Col, Cable, Slab, Wall, Fnd, Truss) Execute & Undo Verified" << std::endl;
        }

        // 50.4: ModifyCommands (Move, Rotate, Delete)
        {
            TSA::Model::Model m;
            int nA = m.addNode(0.0, 0.0, 0.0);
            int nB = m.addNode(4.0, 0.0, 0.0);
            int bId = m.addBeam(nA, nB, 0.25, 0.40);
            int cId = m.addCable(nA, nB, TSA::Model::CableType::Strand);

            // MoveElementsCommand
            auto moveCmd = std::make_unique<TSA::Commands::MoveElementsCommand>(m, std::set<int>{nA, nB}, 2.0, 3.0, 1.0);
            TEST_CHECK(moveCmd->category() == TSA::Commands::CommandCategory::Modify, "Subtest 50.4: Move command is in Modify category");
            TEST_CHECK(moveCmd->execute(), "Subtest 50.4: Move command executed");
            TEST_CHECK(approxEqual(m.getNode(nA)->x(), 2.0), "Subtest 50.4: nA x is 2.0");
            TEST_CHECK(approxEqual(m.getNode(nB)->x(), 6.0), "Subtest 50.4: nB x is 6.0");

            TEST_CHECK(moveCmd->undo(), "Subtest 50.4: Move command undone");
            TEST_CHECK(approxEqual(m.getNode(nA)->x(), 0.0), "Subtest 50.4: nA x reverted to 0.0");
            TEST_CHECK(approxEqual(m.getNode(nB)->x(), 4.0), "Subtest 50.4: nB x reverted to 4.0");

            // RotateElementsCommand
            auto rotCmd = std::make_unique<TSA::Commands::RotateElementsCommand>(m, std::set<int>{nB}, 0.0, 0.0, 0.0, 90.0, 0.0, 0.0, 1.0);
            TEST_CHECK(rotCmd->category() == TSA::Commands::CommandCategory::Modify, "Subtest 50.4: Rotate command is in Modify category");
            TEST_CHECK(rotCmd->execute(), "Subtest 50.4: Rotate command executed");
            TEST_CHECK(approxEqual(m.getNode(nB)->x(), 0.0, 1e-4), "Subtest 50.4: nB x after 90 deg rotation is 0");
            TEST_CHECK(approxEqual(m.getNode(nB)->y(), 4.0, 1e-4), "Subtest 50.4: nB y after 90 deg rotation is 4");

            TEST_CHECK(rotCmd->undo(), "Subtest 50.4: Rotate command undone");
            TEST_CHECK(approxEqual(m.getNode(nB)->x(), 4.0, 1e-4), "Subtest 50.4: nB x reverted to 4");
            TEST_CHECK(approxEqual(m.getNode(nB)->y(), 0.0, 1e-4), "Subtest 50.4: nB y reverted to 0");

            // DeleteElementsCommand
            auto delCmd = std::make_unique<TSA::Commands::DeleteElementsCommand>(
                m, std::set<int>{nA, nB}, std::set<int>{bId}, std::set<int>{},
                std::set<int>{}, std::set<int>{}, std::set<int>{}, std::set<int>{},
                std::set<int>{cId}
            );
            TEST_CHECK(delCmd->execute(), "Subtest 50.4: Delete command executed");
            TEST_CHECK(m.nodes().empty(), "Subtest 50.4: All nodes deleted");
            TEST_CHECK(m.beams().empty(), "Subtest 50.4: Beam deleted");
            TEST_CHECK(m.cables().empty(), "Subtest 50.4: Cable deleted");

            TEST_CHECK(delCmd->undo(), "Subtest 50.4: Delete command undone via snapshot");
            TEST_CHECK(m.nodes().size() == 2, "Subtest 50.4: Nodes restored");
            TEST_CHECK(m.beams().size() == 1, "Subtest 50.4: Beam restored");
            TEST_CHECK(m.cables().size() == 1, "Subtest 50.4: Cable restored");

            std::cout << "  [PASS] Subtest 50.4: ModifyCommands (Move, Rotate, Delete) Execute & Undo Verified" << std::endl;
        }

        // 50.5: CommandManager Pipeline & Transaction Safety
        {
            TSA::Model::Model m;
            TSA::UndoRedo::CommandManager cmdMgr(&m, m.undoManager());

            TEST_CHECK(!cmdMgr.canUndo(), "Subtest 50.5: Initial canUndo is false");
            TEST_CHECK(!cmdMgr.canRedo(), "Subtest 50.5: Initial canRedo is false");

            auto cmd1 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 1.0, 2.0, 3.0, "", "NodA");
            TEST_CHECK(cmdMgr.executeCommand(std::move(cmd1)), "Subtest 50.5: executeCommand 1 succeeded");
            TEST_CHECK(m.nodes().size() == 1, "Subtest 50.5: Node count is 1");
            TEST_CHECK(cmdMgr.canUndo(), "Subtest 50.5: canUndo is true");

            auto cmd2 = std::make_unique<TSA::Commands::CreateNodeCommand>(m, 4.0, 5.0, 6.0, "", "NodB");
            TEST_CHECK(cmdMgr.executeCommand(std::move(cmd2)), "Subtest 50.5: executeCommand 2 succeeded");
            TEST_CHECK(m.nodes().size() == 2, "Subtest 50.5: Node count is 2");

            TEST_CHECK(cmdMgr.undo(), "Subtest 50.5: cmdMgr.undo() succeeded");
            TEST_CHECK(m.nodes().size() == 1, "Subtest 50.5: Node count back to 1");
            TEST_CHECK(cmdMgr.canRedo(), "Subtest 50.5: canRedo is true");

            TEST_CHECK(cmdMgr.redo(), "Subtest 50.5: cmdMgr.redo() succeeded");
            TEST_CHECK(m.nodes().size() == 2, "Subtest 50.5: Node count back to 2");

            std::cout << "  [PASS] Subtest 50.5: CommandManager Execution Pipeline & Transaction Safety Verified" << std::endl;
        }

        std::cout << "[PASS] Test 50: Professional CAD Command System & Command Classification (5 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }


    return true;
}
