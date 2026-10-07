#include "test_common.h"
#include "UndoRedo/EditTransaction.h"

bool runSuite_Grids(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 30: Audit et Tests fonctionnels complets du Système de Grille (Grids 1 à 12)
    // -------------------------------------------------------------------------
    {
        std::cout << "--- Test 30: Comprehensive Grid System Audit (Tests 1 to 12) ---" << std::endl;
        GridManager gm;
        gm.clearAllGrids();

        // Test 1: Créer Grid A -> affichage / définition correcte
        GridDefinition defA("Grid A", GridType::Cartesian);
        defA.setOrigin(0.0, 0.0, 0.0);
        defA.setRotationDeg(0.0);
        defA.setXPositions({ 0.0, 5.0, 10.0 });
        defA.setYPositions({ 0.0, 5.0, 10.0 });
        GridSystem* gridA = gm.addGrid(defA);
        TEST_CHECK(gridA != nullptr, "Test 30.1: Grid A created");
        TEST_CHECK(gridA->definition().name() == "Grid A", "Test 30.1: Grid A name");

        // Test 2: Créer Grid B -> les deux visibles simultanément
        GridDefinition defB("Grid B", GridType::Cartesian);
        defB.setOrigin(20.0, 0.0, 0.0);
        defB.setRotationDeg(15.0);
        defB.setXPositions({ 0.0, 3.0, 6.0, 9.0 });
        defB.setYPositions({ 0.0, 4.0, 8.0 });
        GridSystem* gridB = gm.addGrid(defB);
        TEST_CHECK(gridB != nullptr, "Test 30.2: Grid B created");
        TEST_CHECK(gm.grids().size() == 2, "Test 30.2: Both Grid A and Grid B co-exist in GridManager");
        TEST_CHECK(gridA->isVisible() && gridB->isVisible(), "Test 30.2: Both Grid A and Grid B are visible");

        // Test 3: Activer Grid B -> UI = Grid B active, Model = Grid B active
        gm.setActiveGridId(gridB->id());
        TEST_CHECK(gm.activeGridId() == gridB->id(), "Test 30.3: Active Grid ID is Grid B");
        TEST_CHECK(gridB->isActive(), "Test 30.3: Grid B isActive flag is true");
        TEST_CHECK(!gridA->isActive(), "Test 30.3: Grid A isActive flag is false");

        // Test 4: Modifier Grid B -> Grid A inchangée
        GridDefinition newDefB = gridB->definition();
        newDefB.setRotationDeg(30.0);
        gm.updateGrid(gridB->id(), newDefB);
        TEST_CHECK(approxEqual(gridB->definition().rotationDeg(), 30.0), "Test 30.4: Grid B modified rotation to 30 deg");
        TEST_CHECK(approxEqual(gridA->definition().rotationDeg(), 0.0), "Test 30.4: Grid A rotation remains unchanged at 0 deg");
        TEST_CHECK(gridA->definition().xPositions().size() == 3, "Test 30.4: Grid A X positions intact");

        // Test 5: Supprimer Grid B -> Grid A reste fonctionnelle
        std::string gridBId = gridB->id();
        gm.removeGrid(gridBId);
        TEST_CHECK(gm.grids().size() == 1, "Test 30.5: Grid B removed, 1 grid remains");
        TEST_CHECK(gm.getGrid(gridA->id()) != nullptr, "Test 30.5: Grid A still exists and functional");
        TEST_CHECK(gm.activeGridId() == gridA->id(), "Test 30.5: Active grid automatically reassigned to Grid A");
        TEST_CHECK(gridA->isActive(), "Test 30.5: Grid A isActive state set to true");

        // Test 6: Dupliquer Grid A -> A et C indépendantes
        GridSystem* gridC = gm.duplicateGrid(gridA->id());
        TEST_CHECK(gridC != nullptr, "Test 30.6: Grid C duplicated from Grid A");
        TEST_CHECK(gridC->id() != gridA->id(), "Test 30.6: Grid C has unique distinct ID");
        TEST_CHECK(gm.grids().size() == 2, "Test 30.6: GridManager now holds 2 grids");
        // Modification de Grid C ne modifie pas Grid A
        GridDefinition defCMod = gridC->definition();
        defCMod.setOrigin(50.0, 50.0, 0.0);
        gm.updateGrid(gridC->id(), defCMod);
        TEST_CHECK(approxEqual(gridC->definition().origin().X(), 50.0), "Test 30.6: Grid C origin updated to 50");
        TEST_CHECK(approxEqual(gridA->definition().origin().X(), 0.0), "Test 30.6: Grid A origin remains 0");

        // Test 7: Undo via GridCommands
        TSA::Commands::ModifyGridCommand modCmd(gm, gridA->id(), defCMod);
        modCmd.execute();
        TEST_CHECK(approxEqual(gridA->definition().origin().X(), 50.0), "Test 30.7: ModifyGridCommand executed");
        modCmd.undo();
        TEST_CHECK(approxEqual(gridA->definition().origin().X(), 0.0), "Test 30.7: Undo returned Grid A to exact previous state");

        // Test 8: Redo via GridCommands
        modCmd.execute();
        TEST_CHECK(approxEqual(gridA->definition().origin().X(), 50.0), "Test 30.8: Redo restored modification");
        modCmd.undo(); // Remettre à l'état initial pour la suite

        // Test 9: Save / Load (Sérialisation / Désérialisation JSON)
        std::string jsonStr = gm.serializeToJson();
        TEST_CHECK(!jsonStr.empty(), "Test 30.9: GridManager serialized to JSON");
        TEST_CHECK(jsonStr.find("Grid A") != std::string::npos, "Test 30.9: JSON contains Grid A");

        GridManager gm2;
        gm2.deserializeFromJson(jsonStr);
        TEST_CHECK(gm2.grids().size() == 2, "Test 30.9: Deserialized GridManager contains 2 grids");
        const GridSystem* loadedA = gm2.getGrid(gridA->id());
        TEST_CHECK(loadedA != nullptr, "Test 30.9: Loaded Grid A exists");
        TEST_CHECK(loadedA->definition().name() == "Grid A", "Test 30.9: Loaded Grid A name restored");

        // Test 10: Snap avec plusieurs grilles (Multi-Grid Snapping)
        GridSnapManager snapMgr;
        snapMgr.setSnapEnabled(true);
        snapMgr.setSnapTolerance(0.5);

        // rawPoint proche de l'intersection (5, 5, 0) de Grid A
        gp_Pnt nearPt(5.05, 4.95, 0.0);
        GridSnapResult sRes = snapMgr.findSnap(nearPt, &gm, nullptr);
        TEST_CHECK(sRes.snapped, "Test 30.10: Snapped to multi-grid candidate");
        TEST_CHECK(approxEqual(sRes.point.X(), 5.0) && approxEqual(sRes.point.Y(), 5.0), "Test 30.10: Snap point accurate to intersection (5, 5, 0)");

        // Test 11: Modifier une grille pendant qu'elle est visible -> aucun crash
        TEST_CHECK(gridA->isVisible(), "Test 30.11: Grid A is visible");
        GridDefinition liveDef = gridA->definition();
        liveDef.generateCartesian(4, 4.0, 4, 4.0, 3, 3.0);
        bool updatedOk = gm.updateGrid(gridA->id(), liveDef);
        TEST_CHECK(updatedOk, "Test 30.11: Modified live visible grid without crash");

        // Test 12: Modifier une grille active -> Modèle synchronisé
        TEST_CHECK(gridA->isActive(), "Test 30.12: Grid A is active");
        TEST_CHECK(gridA->definition().xPositions().size() == 5, "Test 30.12: Active grid updated with 5 X lines");

        // Test 13: Cas limite d'une nouvelle grille où Y est configuré en premier (1 coordonnée X, 1 coordonnée Y, 1 niveau Z)
        // Vérification de non-dégénérescence géométrique (aucun segment de longueur nulle)
        GridDefinition minimalDef("Minimal Y First Grid", GridType::Cartesian);
        minimalDef.setXPositions({ 0.0 });
        minimalDef.setYPositions({ 0.0 });
        minimalDef.setZLevels({ 0.0 });
        CartesianGrid minimalCartesian(minimalDef);

        TEST_CHECK(minimalCartesian.verticalConnectionLines().empty(),
            "Test 30.13: No vertical connection lines when only 1 Z level (prevents zero-length edges)");
        TEST_CHECK(minimalCartesian.levelBoundaryPlanes().empty(),
            "Test 30.13: No boundary planes when 2D area is zero (prevents zero-length edges)");
        for (const auto& line : minimalCartesian.allLines())
        {
            TEST_CHECK(line.start.Distance(line.end) > 0.1,
                "Test 30.13: All axis lines have strictly positive length even with single point per axis");
        }

        std::cout << "[PASS] Test 30: All 13 Grid System Audit Tests Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 31: Advanced Robot Structural Analysis Grid System Features
    // -------------------------------------------------------------------------
    {
        // 1. Définition et calculateur de lignes arbitraires (ArbitraryGrid)
        GridDefinition arbDef("Arbitrary Construction", GridType::Arbitrary);
        ArbitraryLine line1;
        line1.label = "Axe_1";
        line1.p1 = gp_Pnt(0.0, 0.0, 0.0);
        line1.p2 = gp_Pnt(10.0, 0.0, 0.0);
        line1.type = "droite";
        line1.isBold = true;

        ArbitraryLine line2;
        line2.label = "Axe_2";
        line2.p1 = gp_Pnt(5.0, -5.0, 0.0);
        line2.p2 = gp_Pnt(5.0, 5.0, 0.0);
        line2.type = "droite";
        line2.isBold = false;

        ArbitraryLine line3;
        line3.label = "Diag";
        line3.p1 = gp_Pnt(0.0, 0.0, 0.0);
        line3.p2 = gp_Pnt(10.0, 10.0, 0.0);
        line3.type = "segment";
        line3.isBold = false;

        arbDef.addArbitraryLine(line1);
        arbDef.addArbitraryLine(line2);
        arbDef.addArbitraryLine(line3);

        GridDisplaySettings ds;
        ds.extension = 2.5;
        ds.bubbleRadius = 0.5;
        ds.showBubbles = true;
        ds.lineColor = "#FF5500";
        ds.lineStyle = "dash";
        ds.lineWidth = 1.8;
        arbDef.setDisplaySettings(ds);

        ArbitraryGrid arbCalc(arbDef);

        // Vérifier les lignes de rendu (extension pour droites, exacte pour segments)
        const auto& rLines = arbCalc.renderLines();
        TEST_CHECK(rLines.size() == 3, "Test 31.1: Arbitrary render lines count == 3");
        // line3 est un segment de (0,0,0) à (10,10,0) -> longueur = sqrt(200) ~= 14.142
        TEST_CHECK(approxEqual(rLines[2].start.Distance(rLines[2].end), std::sqrt(200.0)), "Test 31.1: Segment preserves exact length");
        // line1 est une droite avec extension de 50m aux deux bouts -> longueur = 10 + 2*50 = 110.0
        TEST_CHECK(approxEqual(rLines[0].start.Distance(rLines[0].end), 110.0), "Test 31.1: Droite extends across viewport (110m)");

        // 2. Intersections entre lignes arbitraires
        const auto& arbInters = arbCalc.intersections();
        TEST_CHECK(arbInters.size() >= 2, "Test 31.2: Intersections detected between arbitrary lines");
        // Intersection entre Axe_1 et Axe_2 doit être exactement (5, 0, 0)
        bool foundIntersection500 = false;
        for (const auto& inter : arbInters)
        {
            if (approxEqual(inter.X(), 5.0) && approxEqual(inter.Y(), 0.0) && approxEqual(inter.Z(), 0.0))
            {
                foundIntersection500 = true;
                break;
            }
        }
        TEST_CHECK(foundIntersection500, "Test 31.2: Exact intersection (5, 0, 0) found between Axe_1 and Axe_2");

        // 3. Aimantation (Snapping) sur lignes arbitraires
        // Proche de l'intersection (5, 0, 0)
        gp_Pnt nearInter(5.02, 0.04, 0.0);
        GridSnapResult snapInter = arbCalc.findClosestSnap(nearInter, 0.5);
        TEST_CHECK(snapInter.snapped, "Test 31.3: Snapped near arbitrary intersection");
        TEST_CHECK(snapInter.type == GridSnapType::Intersection, "Test 31.3: Snap type is Intersection");
        TEST_CHECK(approxEqual(snapInter.point.X(), 5.0) && approxEqual(snapInter.point.Y(), 0.0), "Test 31.3: Snap point is exactly (5, 0, 0)");

        // Proche de la ligne Diag (segment) à x=3, y=3
        gp_Pnt nearLine(3.04, 2.95, 0.0);
        GridSnapResult snapLine = arbCalc.findClosestSnap(nearLine, 0.5);
        TEST_CHECK(snapLine.snapped, "Test 31.3: Snapped near arbitrary line segment");
        TEST_CHECK(snapLine.type == GridSnapType::AxisLine, "Test 31.3: Snap type is AxisLine");
        TEST_CHECK(approxEqual(snapLine.point.X(), snapLine.point.Y()), "Test 31.3: Snap on diagonal line (X == Y)");

        // 4. Intégration dans GridSystem & GridManager
        GridManager gm;
        gm.clearAllGrids();
        GridSystem* sysArb = gm.addGrid(arbDef);
        TEST_CHECK(sysArb != nullptr, "Test 31.4: Added Arbitrary Grid to GridManager");
        TEST_CHECK(sysArb->type() == GridType::Arbitrary, "Test 31.4: GridSystem type is Arbitrary");
        TEST_CHECK(sysArb->arbitrary() != nullptr, "Test 31.4: GridSystem arbitrary calculator available");

        // Snap unifié via GridSystem
        GridSnapResult sysSnap = sysArb->findClosestSnap(nearInter, 0.5);
        TEST_CHECK(sysSnap.snapped, "Test 31.4: Unified GridSystem findClosestSnap succeeded");

        // 5. Presse-papier de grilles (Copy / Paste / Rename)
        gm.copyGrid(sysArb->id());
        TEST_CHECK(gm.hasCopiedGrid(), "Test 31.5: GridManager clipboard has copied grid");

        GridSystem* pastedSys = gm.pasteGrid();
        TEST_CHECK(pastedSys != nullptr, "Test 31.5: Pasted grid created successfully");
        TEST_CHECK(pastedSys->id() != sysArb->id(), "Test 31.5: Pasted grid has unique independent ID");
        TEST_CHECK(pastedSys->name() == "Arbitrary Construction (Copie)", "Test 31.5: Pasted grid renamed with (Copie)");
        TEST_CHECK(pastedSys->type() == GridType::Arbitrary, "Test 31.5: Pasted grid retains Arbitrary type");

        bool renamedOk = gm.renameGrid(pastedSys->id(), "Grille Rénommée");
        TEST_CHECK(renamedOk, "Test 31.5: Renamed pasted grid");
        TEST_CHECK(pastedSys->name() == "Grille Rénommée", "Test 31.5: Grid name reflects new name");

        // 6. Sauvegarde / Chargement JSON avec lignes arbitraires, gras et displaySettings
        GridDefinition cartWithBold("Cartesian Bold", GridType::Cartesian);
        cartWithBold.generateCartesian(2, 5.0, 2, 5.0, 1, 3.0);
        cartWithBold.setXIsBold({ true, false, true });
        cartWithBold.setYIsBold({ false, true, false });
        gm.addGrid(cartWithBold);

        std::string jsonStr = gm.serializeToJson();
        TEST_CHECK(jsonStr.find("\"Arbitrary\"") != std::string::npos, "Test 31.6: JSON contains Arbitrary type");
        TEST_CHECK(jsonStr.find("\"xIsBold\"") != std::string::npos, "Test 31.6: JSON contains xIsBold");
        TEST_CHECK(jsonStr.find("\"lineColor\"") != std::string::npos, "Test 31.6: JSON contains lineColor");

        GridManager gmLoaded;
        gmLoaded.deserializeFromJson(jsonStr);
        TEST_CHECK(gmLoaded.grids().size() == 3, "Test 31.6: Deserialized all 3 grids correctly");
        const GridSystem* loadedArb = gmLoaded.getGrid(sysArb->id());
        TEST_CHECK(loadedArb != nullptr, "Test 31.6: Loaded arbitrary grid exists");
        TEST_CHECK(loadedArb->definition().arbitraryLines().size() == 3, "Test 31.6: Loaded arbitrary grid has 3 lines");
        TEST_CHECK(loadedArb->definition().displaySettings().lineColor == "#FF5500", "Test 31.6: Display settings color preserved");
        TEST_CHECK(approxEqual(loadedArb->definition().displaySettings().extension, 2.5), "Test 31.6: Display settings extension preserved");

        const GridSystem* loadedCart = gmLoaded.getGrid(cartWithBold.id());
        TEST_CHECK(loadedCart != nullptr, "Test 31.6: Loaded cartesian grid exists");
        TEST_CHECK(loadedCart->definition().xIsBold(0) == true, "Test 31.6: xIsBold[0] == true preserved");
        TEST_CHECK(loadedCart->definition().xIsBold(1) == false, "Test 31.6: xIsBold[1] == false preserved");
        TEST_CHECK(loadedCart->definition().xIsBold(2) == true, "Test 31.6: xIsBold[2] == true preserved");

        std::cout << "[PASS] Test 31: Advanced Robot Structural Analysis Grid Features (Arbitrary, Display Settings, Clipboard, Snapping, JSON) Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 32: Cylindrical Grid Angular Sectors (Robot Structural Analysis)
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 32: Cylindrical Grid Angular Sectors Suite ---" << std::endl;

        // Subtest 32.1: Test 1 - Départ = 0°, Total = 90° -> secteur 0° -> 90°
        {
            GridDefinition def1("Cyl_90", GridType::Cylindrical);
            def1.generateCylindrical(2, 3.0, 5, 18.0, 1, 3.0, 0.0, 90.0);
            TEST_CHECK(approxEqual(def1.startAngleDeg(), 0.0), "Test 32.1: startAngleDeg == 0");
            TEST_CHECK(approxEqual(def1.totalAngleDeg(), 90.0), "Test 32.1: totalAngleDeg == 90");
            TEST_CHECK(def1.angles().size() == 6, "Test 32.1: 5 divisions give 6 angle rays");
            TEST_CHECK(approxEqual(def1.angles().front(), 0.0), "Test 32.1: First angle is 0 deg");
            TEST_CHECK(approxEqual(def1.angles().back(), 90.0), "Test 32.1: Last angle is 90 deg");

            CylindricalGrid cyl1(def1);
            TEST_CHECK(!cyl1.circles().empty(), "Test 32.1: circles not empty");
            TEST_CHECK(cyl1.circles().front().isFullCircle() == false, "Test 32.1: isFullCircle == false for 90 deg");
            TEST_CHECK(approxEqual(cyl1.circles().front().startAngleDeg, 0.0), "Test 32.1: circle startAngle == 0");
            TEST_CHECK(approxEqual(cyl1.circles().front().totalAngleDeg, 90.0), "Test 32.1: circle totalAngle == 90");
            TEST_CHECK(cyl1.radialLines().size() == 12, "Test 32.1: 6 rays * 2 levels == 12 radial lines");

            // OCCT Arc BRep verification
            gp_Circ occtCirc(gp_Ax2(gp_Pnt(0,0,0), gp_Dir(0,0,1)), 3.0);
            BRepBuilderAPI_MakeEdge arcMaker(occtCirc, 0.0, 90.0 * 3.14159265358979323846 / 180.0);
            TEST_CHECK(arcMaker.IsDone(), "Test 32.1: OCCT arc edge created");
            BRepAdaptor_Curve adapt(arcMaker.Edge());
            double arcLen = GCPnts_AbscissaPoint::Length(adapt);
            TEST_CHECK(approxEqual(arcLen, 3.0 * (3.14159265358979323846 / 2.0)), "Test 32.1: Arc length == R * pi / 2");

            // Snapping inside arc (with tolerance 0.2m so it snaps to the arc rather than intersection at 36 deg which is 0.47m away)
            gp_Pnt pInSector(3.0 * std::cos(45.0 * 3.14159265358979323846 / 180.0),
                             3.0 * std::sin(45.0 * 3.14159265358979323846 / 180.0), 0.0);
            GridSnapResult snapIn = cyl1.findClosestSnap(pInSector, 0.2);
            TEST_CHECK(snapIn.snapped, "Test 32.1: Snapped on arc in sector");
            TEST_CHECK(snapIn.type == GridSnapType::Circle, "Test 32.1: Snap type is Circle/Arc");

            // Snapping outside sector (at 180 deg) clamps to sector bound
            gp_Pnt pOutside(-3.0, 0.0, 0.0);
            GridSnapResult snapOut = cyl1.findClosestSnap(pOutside, 10.0);
            TEST_CHECK(snapOut.snapped, "Test 32.1: Snapped with large tolerance");
            TEST_CHECK(snapOut.point.X() >= -1e-4 && snapOut.point.Y() >= -1e-4, "Test 32.1: Clamped point stays within first quadrant [0, 90 deg]");
            std::cout << "  [PASS] Subtest 32.1: Test 1 (0° -> 90° sector) Validated" << std::endl;
        }

        // Subtest 32.2: Test 2 - Départ = 0°, Total = 180° -> demi-cercle
        {
            GridDefinition def2("Cyl_180", GridType::Cylindrical);
            def2.generateCylindrical(2, 4.0, 6, 30.0, 0, 0.0, 0.0, 180.0);
            TEST_CHECK(approxEqual(def2.totalAngleDeg(), 180.0), "Test 32.2: totalAngleDeg == 180");
            TEST_CHECK(def2.angles().size() == 7, "Test 32.2: 6 divs = 7 angles (0 to 180)");
            TEST_CHECK(approxEqual(def2.angles().front(), 0.0) && approxEqual(def2.angles().back(), 180.0), "Test 32.2: bounds 0 and 180");
            CylindricalGrid cyl2(def2);
            TEST_CHECK(cyl2.circles().front().isFullCircle() == false, "Test 32.2: isFullCircle == false");
            TEST_CHECK(approxEqual(cyl2.circles().front().totalAngleDeg, 180.0), "Test 32.2: totalAngle == 180");
            std::cout << "  [PASS] Subtest 32.2: Test 2 (0° -> 180° demi-cercle) Validated" << std::endl;
        }

        // Subtest 32.3: Test 3 - Départ = 30°, Total = 120° -> secteur 30° -> 150°
        {
            GridDefinition def3("Cyl_30_150", GridType::Cylindrical);
            def3.generateCylindrical(2, 5.0, 4, 30.0, 0, 0.0, 30.0, 120.0);
            TEST_CHECK(approxEqual(def3.startAngleDeg(), 30.0), "Test 32.3: startAngleDeg == 30");
            TEST_CHECK(approxEqual(def3.totalAngleDeg(), 120.0), "Test 32.3: totalAngleDeg == 120");
            TEST_CHECK(def3.angles().size() == 5, "Test 32.3: 4 divs = 5 angles (30, 60, 90, 120, 150)");
            TEST_CHECK(approxEqual(def3.angles().front(), 30.0), "Test 32.3: first angle is 30");
            TEST_CHECK(approxEqual(def3.angles().back(), 150.0), "Test 32.3: last angle is 150");
            CylindricalGrid cyl3(def3);
            TEST_CHECK(approxEqual(cyl3.circles().front().startAngleDeg, 30.0), "Test 32.3: circle start == 30");
            TEST_CHECK(approxEqual(cyl3.circles().front().totalAngleDeg, 120.0), "Test 32.3: circle total == 120");
            std::cout << "  [PASS] Subtest 32.3: Test 3 (30° -> 150° sector, total 120°) Validated" << std::endl;
        }

        // Subtest 32.4: Test 4 - Départ = 0°, Total = 270° -> 270° seulement
        {
            GridDefinition def4("Cyl_270", GridType::Cylindrical);
            def4.generateCylindrical(2, 5.0, 3, 90.0, 0, 0.0, 0.0, 270.0);
            TEST_CHECK(approxEqual(def4.totalAngleDeg(), 270.0), "Test 32.4: totalAngleDeg == 270");
            TEST_CHECK(def4.angles().size() == 4, "Test 32.4: 3 divs = 4 angles (0, 90, 180, 270)");
            TEST_CHECK(approxEqual(def4.angles().back(), 270.0), "Test 32.4: last angle is 270");
            CylindricalGrid cyl4(def4);
            TEST_CHECK(cyl4.circles().front().isFullCircle() == false, "Test 32.4: isFullCircle == false");
            TEST_CHECK(approxEqual(cyl4.circles().front().totalAngleDeg, 270.0), "Test 32.4: totalAngle == 270");
            std::cout << "  [PASS] Subtest 32.4: Test 4 (0° -> 270° sector) Validated" << std::endl;
        }

        // Subtest 32.5: Test 5 - Départ = 0°, Total = 360° -> cercle complet
        {
            GridDefinition def5("Cyl_360", GridType::Cylindrical);
            def5.generateCylindrical(2, 5.0, 8, 45.0, 0, 0.0, 0.0, 360.0);
            TEST_CHECK(approxEqual(def5.totalAngleDeg(), 360.0), "Test 32.5: totalAngleDeg == 360");
            TEST_CHECK(def5.angles().size() == 8, "Test 32.5: 8 unique radial directions (no duplicate 360 == 0)");
            CylindricalGrid cyl5(def5);
            TEST_CHECK(cyl5.circles().front().isFullCircle() == true, "Test 32.5: isFullCircle == true for 360 deg");
            std::cout << "  [PASS] Subtest 32.5: Test 5 (0° -> 360° full circle) Validated" << std::endl;
        }

        // Subtest 32.6: JSON Serialization & Deserialization
        {
            GridDefinition defJson("Cyl_Json", GridType::Cylindrical);
            defJson.generateCylindrical(3, 2.5, 4, 30.0, 2, 3.0, 30.0, 120.0);
            std::string json = defJson.toJson();
            TEST_CHECK(json.find("\"startAngleDeg\": 30") != std::string::npos, "Test 32.6: json contains startAngleDeg");
            TEST_CHECK(json.find("\"totalAngleDeg\": 120") != std::string::npos, "Test 32.6: json contains totalAngleDeg");

            GridDefinition loaded = GridDefinition::fromJson(json);
            TEST_CHECK(loaded.type() == GridType::Cylindrical, "Test 32.6: type is Cylindrical");
            TEST_CHECK(approxEqual(loaded.startAngleDeg(), 30.0), "Test 32.6: loaded startAngleDeg == 30");
            TEST_CHECK(approxEqual(loaded.totalAngleDeg(), 120.0), "Test 32.6: loaded totalAngleDeg == 120");
            TEST_CHECK(loaded.angles().size() == 5, "Test 32.6: loaded angles size == 5");
            TEST_CHECK(approxEqual(loaded.angles().front(), 30.0), "Test 32.6: loaded first angle == 30");
            TEST_CHECK(approxEqual(loaded.angles().back(), 150.0), "Test 32.6: loaded last angle == 150");
            std::cout << "  [PASS] Subtest 32.6: JSON Serialization & Deserialization Validated" << std::endl;
        }

        // Subtest 32.7: Multi-Grid GridManager integration with Cylindrical Sector
        {
            GridManager gm;
            GridDefinition defSector("ActiveSectorGrid", GridType::Cylindrical);
            defSector.generateCylindrical(2, 4.0, 5, 18.0, 0, 0.0, 0.0, 90.0);
            gm.addGrid(defSector);
            gm.setActiveGridId(defSector.id());
            GridSystem* sys = gm.getGrid(defSector.id());
            TEST_CHECK(sys != nullptr, "Test 32.7: GridSystem exists in manager");
            TEST_CHECK(sys->type() == GridType::Cylindrical, "Test 32.7: System is cylindrical");
            TEST_CHECK(sys->cylindrical() != nullptr, "Test 32.7: cylindrical ptr not null");
            TEST_CHECK(sys->cylindrical()->circles().front().isFullCircle() == false, "Test 32.7: sector circle is not full");

            GridSnapManager snapMgr;
            snapMgr.setSnapEnabled(true);
            snapMgr.setSnapTolerance(0.1);
            gp_Pnt pNearOrigin(-0.02, -0.01, 0.0);
            GridSnapResult snapOrig = snapMgr.findSnap(pNearOrigin, &gm);
            TEST_CHECK(snapOrig.snapped, "Test 32.7: Snapped to origin");
            TEST_CHECK(snapOrig.type == GridSnapType::Origin, "Test 32.7: Snap type is origin");
            std::cout << "  [PASS] Subtest 32.7: GridManager & Snapping Integration Validated" << std::endl;
        }

        // Subtest 32.8: AngularPattern multi-group generation & Deduced sector (0°, 30°, 60°, 90° then 120°, 135°, 150°)
        {
            GridDefinition defPattern("Cyl_Patterns", GridType::Cylindrical);
            defPattern.setRadii({ 3.0, 6.0 });
            defPattern.setZLevels({ 0.0 });

            // Groupe 1: Position = 0°, Répéter = 3, Angle = 30° -> 0°, 30°, 60°, 90°
            AngularPattern group1{ 0.0, 3, 30.0 };
            defPattern.addAngularPattern(group1);

            TEST_CHECK(defPattern.angles().size() == 4, "Test 32.8: 4 angles generated for repeat=3");
            TEST_CHECK(approxEqual(defPattern.angles()[0], 0.0), "Test 32.8: angle 0 == 0");
            TEST_CHECK(approxEqual(defPattern.angles()[1], 30.0), "Test 32.8: angle 1 == 30");
            TEST_CHECK(approxEqual(defPattern.angles()[2], 60.0), "Test 32.8: angle 2 == 60");
            TEST_CHECK(approxEqual(defPattern.angles()[3], 90.0), "Test 32.8: angle 3 == 90");
            TEST_CHECK(approxEqual(defPattern.startAngleDeg(), 0.0), "Test 32.8: Deduced start == 0");
            TEST_CHECK(approxEqual(defPattern.totalAngleDeg(), 90.0), "Test 32.8: Deduced total == 90");

            // Groupe 2: Position = 120°, Répéter = 2, Angle = 15° -> 120°, 135°, 150°
            AngularPattern group2{ 120.0, 2, 15.0 };
            defPattern.addAngularPattern(group2);

            TEST_CHECK(defPattern.angles().size() == 7, "Test 32.8: 7 unique angles total");
            std::vector<double> expected = { 0.0, 30.0, 60.0, 90.0, 120.0, 135.0, 150.0 };
            for (size_t i = 0; i < expected.size(); ++i)
            {
                TEST_CHECK(approxEqual(defPattern.angles()[i], expected[i]), "Test 32.8: Angle matches expected value");
            }
            TEST_CHECK(approxEqual(defPattern.startAngleDeg(), 0.0), "Test 32.8: Deduced startAngle == 0");
            TEST_CHECK(approxEqual(defPattern.totalAngleDeg(), 150.0), "Test 32.8: Deduced totalAngle == 150");

            CylindricalGrid cyl(defPattern);
            TEST_CHECK(cyl.radialLines().size() == 7, "Test 32.8: 7 radial lines created");
            TEST_CHECK(cyl.circles().front().isFullCircle() == false, "Test 32.8: Not full circle");
            TEST_CHECK(approxEqual(cyl.circles().front().startAngleDeg, 0.0), "Test 32.8: Circle start == 0");
            TEST_CHECK(approxEqual(cyl.circles().front().totalAngleDeg, 150.0), "Test 32.8: Circle total == 150");
            std::cout << "  [PASS] Subtest 32.8: AngularPattern multi-group (0..90° then 120..150°) Validated" << std::endl;
        }

        // Subtest 32.9: Second user example (15°, repeat 4, step 20°) & Validation example (10°, repeat 5, step 15°)
        {
            // Position = 15°, Répéter = 4, Angle = 20° -> 15°, 35°, 55°, 75°, 95°
            GridDefinition defEx2("Cyl_Ex2", GridType::Cylindrical);
            defEx2.setRadii({ 5.0 });
            defEx2.addAngularPattern({ 15.0, 4, 20.0 });

            TEST_CHECK(defEx2.angles().size() == 5, "Test 32.9: 5 angles for repeat=4");
            std::vector<double> exp2 = { 15.0, 35.0, 55.0, 75.0, 95.0 };
            for (size_t i = 0; i < exp2.size(); ++i)
            {
                TEST_CHECK(approxEqual(defEx2.angles()[i], exp2[i]), "Test 32.9: Ex2 angle matches");
            }
            TEST_CHECK(approxEqual(defEx2.startAngleDeg(), 15.0), "Test 32.9: Ex2 startAngle == 15");
            TEST_CHECK(approxEqual(defEx2.totalAngleDeg(), 80.0), "Test 32.9: Ex2 totalAngle == 80 (15° -> 95°)");

            // Validation: Position = 10°, Répéter = 5, Angle = 15° -> 10°, 25°, 40°, 55°, 70°, 85°
            GridDefinition defVal("Cyl_Val", GridType::Cylindrical);
            defVal.setRadii({ 4.0 });
            defVal.setZLevels({ 0.0 });
            defVal.addAngularPattern({ 10.0, 5, 15.0 });

            TEST_CHECK(defVal.angles().size() == 6, "Test 32.9: 6 angles for repeat=5");
            std::vector<double> expVal = { 10.0, 25.0, 40.0, 55.0, 70.0, 85.0 };
            for (size_t i = 0; i < expVal.size(); ++i)
            {
                TEST_CHECK(approxEqual(defVal.angles()[i], expVal[i]), "Test 32.9: Val angle matches");
            }
            TEST_CHECK(approxEqual(defVal.startAngleDeg(), 10.0), "Test 32.9: Val startAngle == 10");
            TEST_CHECK(approxEqual(defVal.totalAngleDeg(), 75.0), "Test 32.9: Val totalAngle == 75 (10° -> 85°)");

            CylindricalGrid cylVal(defVal);
            TEST_CHECK(cylVal.radialLines().size() == 6, "Test 32.9: 6 radial lines");
            TEST_CHECK(approxEqual(cylVal.circles().front().startAngleDeg, 10.0), "Test 32.9: Start angle 10");
            TEST_CHECK(approxEqual(cylVal.circles().front().totalAngleDeg, 75.0), "Test 32.9: Total angle 75");
            std::cout << "  [PASS] Subtest 32.9: User examples (15°..95° and 10°..85°) Validated" << std::endl;
        }

        std::cout << "[PASS] Test 32: Advanced Cylindrical Grid Sectors (AngularPattern, startAngle, totalAngle, divisions, OCCT arcs, snapping, JSON) Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 33: Complete Structural Modeling & Snapping Pipeline on Cylindrical Grids
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 33: Structural Modeling & Snapping Pipeline on Cylindrical Grids ---" << std::endl;

        // Subtest 33.1: Polar Grid Geometry & Intersections Verification
        // R = {5, 10, 15, 20} m, theta = {0°, 30°, 60°, 90°}
        GridDefinition cylDef("Cyl_Structural", GridType::Cylindrical);
        cylDef.setRadii({ 5.0, 10.0, 15.0, 20.0 });
        cylDef.setAngles({ 0.0, 30.0, 60.0, 90.0 });
        cylDef.setZLevels({ 0.0 });

        CylindricalGrid cylGrid(cylDef);
        TEST_CHECK(cylGrid.intersections().size() == 16, "Test 33.1: 16 intersections (4 radii x 4 angles)");

        // Point (10 m, 30°): x = 10 * cos(30°) = 8.66025, y = 10 * sin(30°) = 5.0, z = 0.0
        gp_Pnt p10_30 = cylGrid.polarToWorld(10.0, 30.0, 0.0);
        TEST_CHECK(approxEqual(p10_30.X(), 10.0 * std::cos(30.0 * M_PI / 180.0)), "Test 33.1: (10, 30°) X coord");
        TEST_CHECK(approxEqual(p10_30.Y(), 5.0), "Test 33.1: (10, 30°) Y coord");
        TEST_CHECK(approxEqual(p10_30.Z(), 0.0), "Test 33.1: (10, 30°) Z coord");

        // Point (15 m, 60°): x = 15 * cos(60°) = 7.5, y = 15 * sin(60°) = 12.99038, z = 0.0
        gp_Pnt p15_60 = cylGrid.polarToWorld(15.0, 60.0, 0.0);
        TEST_CHECK(approxEqual(p15_60.X(), 7.5), "Test 33.1: (15, 60°) X coord");
        TEST_CHECK(approxEqual(p15_60.Y(), 15.0 * std::sin(60.0 * M_PI / 180.0)), "Test 33.1: (15, 60°) Y coord");
        TEST_CHECK(approxEqual(p15_60.Z(), 0.0), "Test 33.1: (15, 60°) Z coord");

        // Snapping directly on the cylindrical grid
        GridSnapResult snap10_30 = cylGrid.findClosestSnap(gp_Pnt(8.68, 5.02, 0.0), 0.10);
        TEST_CHECK(snap10_30.snapped, "Test 33.1: Snapped near (10, 30°)");
        TEST_CHECK(snap10_30.type == GridSnapType::Intersection, "Test 33.1: Snap type is Intersection");
        TEST_CHECK(approxEqual(snap10_30.point.X(), p10_30.X()) && approxEqual(snap10_30.point.Y(), p10_30.Y()),
                   "Test 33.1: Snapped exact point matches (10, 30°)");
        std::cout << "  [PASS] Subtest 33.1: Polar Grid Geometry & Intersections Validated" << std::endl;

        // Subtest 33.2: Modeling Structural Elements (Beams & Columns) on Cylindrical Grid Intersections
        Model structuralModel;
        // User clicks near (10 m, 30°) -> Snapped to p10_30, Node created
        int nBase1 = structuralModel.addNode(snap10_30.point.X(), snap10_30.point.Y(), snap10_30.point.Z());
        int nTop1 = structuralModel.addNode(snap10_30.point.X(), snap10_30.point.Y(), snap10_30.point.Z() + 3.0);
        int col1 = structuralModel.addColumn(nBase1, nTop1, 0.30, 0.30, "C1");
        TEST_CHECK(col1 > 0, "Test 33.2: Column 1 created at (10, 30°)");
        TEST_CHECK(approxEqual(structuralModel.getColumn(col1)->length(structuralModel), 3.0), "Test 33.2: Column 1 length is 3.0m");

        // User clicks near (15 m, 60°) -> Snapped to p15_60, Node created
        GridSnapResult snap15_60 = cylGrid.findClosestSnap(gp_Pnt(7.48, 12.97, 0.0), 0.10);
        TEST_CHECK(snap15_60.snapped, "Test 33.2: Snapped near (15, 60°)");
        int nBase2 = structuralModel.addNode(snap15_60.point.X(), snap15_60.point.Y(), snap15_60.point.Z());
        int nTop2 = structuralModel.addNode(snap15_60.point.X(), snap15_60.point.Y(), snap15_60.point.Z() + 3.0);
        int col2 = structuralModel.addColumn(nBase2, nTop2, 0.30, 0.30, "C2");
        TEST_CHECK(col2 > 0, "Test 33.2: Column 2 created at (15, 60°)");

        // User draws Beam connecting (10, 30°) top node to (15, 60°) top node
        int beam1 = structuralModel.addBeam(nTop1, nTop2, 0.25, 0.50);
        TEST_CHECK(beam1 > 0, "Test 33.2: Beam created between (10, 30°) and (15, 60°)");
        double expectedBeamLen = std::sqrt(std::pow(p15_60.X() - p10_30.X(), 2) + std::pow(p15_60.Y() - p10_30.Y(), 2));
        TEST_CHECK(approxEqual(structuralModel.getBeam(beam1)->length(structuralModel), expectedBeamLen),
                   "Test 33.2: Beam length matches distance between polar points");

        // Verify OpenCASCADE 3D shapes can be generated cleanly from these polar elements
        TopoDS_Shape colShape = BeamGeometry::createBeamShape(*structuralModel.getNode(nBase1), *structuralModel.getNode(nTop1), 0.30, 0.30, 0.0);
        TEST_CHECK(!colShape.IsNull(), "Test 33.2: Column 3D OCC shape is non-null");

        TopoDS_Shape beamShape = BeamGeometry::createBeamShape(*structuralModel.getNode(nTop1), *structuralModel.getNode(nTop2), 0.25, 0.50, 0.0);
        TEST_CHECK(!beamShape.IsNull(), "Test 33.2: Beam 3D OCC shape is non-null");
        std::cout << "  [PASS] Subtest 33.2: Modeling Beams & Columns on Cylindrical Grid Validated" << std::endl;

        // Subtest 33.3: Drawing Polygonal Slab Panel on 4 Cylindrical Grid Points
        // Panel on: (10, 30°), (15, 30°), (15, 60°), (10, 60°)
        gp_Pnt p10_60 = cylGrid.polarToWorld(10.0, 60.0, 0.0);
        gp_Pnt p15_30 = cylGrid.polarToWorld(15.0, 30.0, 0.0);
        int nSlab1 = structuralModel.addNode(p10_30.X(), p10_30.Y(), 3.0);
        int nSlab2 = structuralModel.addNode(p15_30.X(), p15_30.Y(), 3.0);
        int nSlab3 = structuralModel.addNode(p15_60.X(), p15_60.Y(), 3.0);
        int nSlab4 = structuralModel.addNode(p10_60.X(), p10_60.Y(), 3.0);

        int slabId = structuralModel.addSlab({ nSlab1, nSlab2, nSlab3, nSlab4 }, 0.20);
        TEST_CHECK(slabId > 0, "Test 33.3: Slab created on 4 polar grid points");
        const auto* slab = structuralModel.getSlab(slabId);
        TEST_CHECK(slab != nullptr, "Test 33.3: Slab exists");
        TEST_CHECK(slab->nodeIds().size() == 4, "Test 33.3: Slab has 4 vertices");
        TEST_CHECK(slab->area(structuralModel) > 0.0, "Test 33.3: Slab area is positive");
        std::cout << "  [PASS] Subtest 33.3: Polygonal Slab on Cylindrical Grid Intersections Validated" << std::endl;

        // Subtest 33.4: Snapping on Radial Lines & Concentric Arcs
        // 1. Ray 30° at R = 7.5 (between 5 and 10m): point is (7.5 * cos(30°), 7.5 * sin(30°)) = (6.49519, 3.75, 0)
        gp_Pnt ptOnRay(7.5 * std::cos(30.0 * M_PI / 180.0), 7.5 * 0.5, 0.0);
        gp_Pnt ptQueryNearRay(ptOnRay.X() + 0.03, ptOnRay.Y() - 0.02, 0.0);
        GridSnapResult snapRay = cylGrid.findClosestSnap(ptQueryNearRay, 0.20);
        TEST_CHECK(snapRay.snapped, "Test 33.4: Snapped near radial ray 30°");
        TEST_CHECK(snapRay.type == GridSnapType::RadialLine, "Test 33.4: Snap type is RadialLine");
        double angleOfSnapped = std::atan2(snapRay.point.Y(), snapRay.point.X()) * 180.0 / M_PI;
        TEST_CHECK(approxEqual(angleOfSnapped, 30.0), "Test 33.4: Snapped point lies exactly on 30° ray");
        TEST_CHECK(snapRay.point.Distance(ptQueryNearRay) < 0.05, "Test 33.4: Snapped distance to ray is under 0.05m");

        // 2. Arc R = 10.0 at theta = 45° (between 30° and 60°): point is (10 * cos(45°), 10 * sin(45°)) = (7.071, 7.071, 0)
        gp_Pnt ptOnArc(10.0 * std::cos(45.0 * M_PI / 180.0), 10.0 * std::sin(45.0 * M_PI / 180.0), 0.0);
        gp_Pnt ptQueryNearArc(ptOnArc.X() + 0.02, ptOnArc.Y() + 0.03, 0.0);
        GridSnapResult snapArc = cylGrid.findClosestSnap(ptQueryNearArc, 0.20);
        TEST_CHECK(snapArc.snapped, "Test 33.4: Snapped near concentric arc R=10m");
        TEST_CHECK(snapArc.type == GridSnapType::Circle, "Test 33.4: Snap type is Circle (Arc)");
        TEST_CHECK(approxEqual(snapArc.point.Distance(gp_Pnt(0, 0, 0)), 10.0), "Test 33.4: Snapped distance is exactly 10.0m radius");
        std::cout << "  [PASS] Subtest 33.4: Snapping to Radial Lines & Concentric Arcs Validated" << std::endl;

        // Subtest 33.5: Priority of Snapping Validation (Node > Intersection > Radial Line > Arc)
        GridSnapManager snapManager;
        snapManager.setSnapTolerance(0.50);

        GridSystem cylSystem(cylDef);
        cylSystem.setActive(true);
        cylSystem.setVisible(true);

        // a) Query near an existing structural node at (10, 30°):
        // Node NBase1 exists at exact intersection point p10_30
        GridSnapResult snapPrioNode = snapManager.findSnap(gp_Pnt(p10_30.X() + 0.04, p10_30.Y() + 0.03, 0.0), &cylSystem, &structuralModel);
        TEST_CHECK(snapPrioNode.snapped, "Test 33.5: Snapped near node");
        TEST_CHECK(snapPrioNode.type == GridSnapType::Node, "Test 33.5: Node has higher priority than Intersection");

        // b) Query near intersection (20 m, 90°) where NO node exists:
        gp_Pnt p20_90 = cylGrid.polarToWorld(20.0, 90.0, 0.0);
        GridSnapResult snapPrioInter = snapManager.findSnap(gp_Pnt(p20_90.X() + 0.03, p20_90.Y() - 0.02, 0.0), &cylSystem, &structuralModel);
        TEST_CHECK(snapPrioInter.snapped, "Test 33.5: Snapped near intersection");
        TEST_CHECK(snapPrioInter.type == GridSnapType::Intersection, "Test 33.5: Intersection has higher priority than Radial/Arc");

        // c) Query along radial line away from intersections:
        GridSnapResult snapPrioRad = snapManager.findSnap(ptQueryNearRay, &cylSystem, &structuralModel);
        TEST_CHECK(snapPrioRad.snapped, "Test 33.5: Snapped near ray");
        TEST_CHECK(snapPrioRad.type == GridSnapType::RadialLine, "Test 33.5: Radial Line prioritized before Arc");
        std::cout << "  [PASS] Subtest 33.5: Snapping Priority (Node > Intersection > Radial Line > Arc) Validated" << std::endl;

        // Subtest 33.6: Dynamic Modifications of Cylindrical Grid (Rotation, Origin, Angles, Radii)
        GridDefinition dynDef = cylDef;
        dynDef.setRotationDeg(45.0);
        dynDef.setOrigin(10.0, 20.0, 0.0);
        dynDef.setRadii({ 5.0, 10.0, 15.0, 20.0, 25.0 }); // ajout 25m

        CylindricalGrid dynGrid(dynDef);
        TEST_CHECK(dynGrid.intersections().size() == 20, "Test 33.6: 20 intersections (5 radii x 4 angles)");

        // Point (10 m, 30°) with rotation=45° and origin=(10, 20, 0):
        // effective angle = 30 + 45 = 75°
        // X = 10 + 10 * cos(75°), Y = 20 + 10 * sin(75°)
        double expDynX = 10.0 + 10.0 * std::cos(75.0 * M_PI / 180.0);
        double expDynY = 20.0 + 10.0 * std::sin(75.0 * M_PI / 180.0);
        gp_Pnt dynP = dynGrid.polarToWorld(10.0, 30.0, 0.0);
        TEST_CHECK(approxEqual(dynP.X(), expDynX), "Test 33.6: Dynamic origin + rotation X");
        TEST_CHECK(approxEqual(dynP.Y(), expDynY), "Test 33.6: Dynamic origin + rotation Y");

        GridSnapResult snapDyn = dynGrid.findClosestSnap(gp_Pnt(expDynX + 0.02, expDynY - 0.01, 0.0), 0.10);
        TEST_CHECK(snapDyn.snapped, "Test 33.6: Snapped to updated dynamic intersection");
        TEST_CHECK(snapDyn.type == GridSnapType::Intersection, "Test 33.6: Dynamic snap type is intersection");
        std::cout << "  [PASS] Subtest 33.6: Dynamic Grid Modification (Origin, Rotation, Radii) Validated" << std::endl;

        // Subtest 33.7: Multi-Grid Support, Active State Preservation & Visibility
        GridManager gmMulti;
        // gmMulti already has Cartesian "Main Grid" as default active grid
        TEST_CHECK(gmMulti.grids().size() == 1, "Test 33.7: Main Grid created initially");
        TEST_CHECK(gmMulti.activeGrid()->type() == GridType::Cartesian, "Test 33.7: Cartesian is active initially");

        // Add Cylindrical Grid
        GridSystem* addedCyl = gmMulti.addGrid(cylDef);
        TEST_CHECK(addedCyl != nullptr, "Test 33.7: Cylindrical grid added");
        std::string cylId = addedCyl->id();

        // Set Cylindrical Grid as ACTIVE
        gmMulti.setActiveGridId(cylId);
        TEST_CHECK(gmMulti.activeGridId() == cylId, "Test 33.7: Active grid switched to Cylindrical");
        TEST_CHECK(addedCyl->isActive() == true, "Test 33.7: Cylindrical grid isActive is true");

        // Update Cylindrical Grid definition -> isActive MUST be preserved!
        GridDefinition updatedDef = cylDef;
        updatedDef.setName("Cyl_Updated");
        gmMulti.updateGrid(cylId, updatedDef);
        TEST_CHECK(addedCyl->isActive() == true, "Test 33.7: Cylindrical grid isActive preserved after updateGrid!");
        TEST_CHECK(addedCyl->name() == "Cyl_Updated", "Test 33.7: Cylindrical name updated");

        // Snapping with GridManager when Cylindrical is active
        GridSnapResult snapFromMgr = snapManager.findSnap(gp_Pnt(p10_30.X() + 0.05, p10_30.Y() - 0.04, 0.0), &gmMulti);
        TEST_CHECK(snapFromMgr.snapped, "Test 33.7: Snapped through GridManager");
        TEST_CHECK(snapFromMgr.type == GridSnapType::Intersection, "Test 33.7: Intersection snap type through GridManager");

        // Switch active grid back to Cartesian Main Grid, keep Cylindrical visible
        std::string cartId = gmMulti.grids().front()->id();
        gmMulti.setActiveGridId(cartId);
        TEST_CHECK(gmMulti.activeGridId() == cartId, "Test 33.7: Active grid switched to Cartesian");

        // Snapping to Cylindrical Grid as Priority 3 (Secondary visible grid)
        GridSnapResult snapSecondary = snapManager.findSnap(gp_Pnt(p10_30.X() + 0.05, p10_30.Y() - 0.04, 0.0), &gmMulti);
        TEST_CHECK(snapSecondary.snapped, "Test 33.7: Snapped to visible non-active Cylindrical grid");
        TEST_CHECK(snapSecondary.type == GridSnapType::Intersection, "Test 33.7: Secondary grid intersection snapped");

        // Toggle visibility of Cylindrical grid to false
        gmMulti.setGridVisible(cylId, false);
        TEST_CHECK(addedCyl->isVisible() == false, "Test 33.7: Cylindrical grid visibility false");
        GridSnapResult snapInvisible = snapManager.findSnap(gp_Pnt(p10_30.X() + 0.05, p10_30.Y() - 0.04, 0.0), &gmMulti);
        // Should NOT snap to Cylindrical since it is hidden!
        TEST_CHECK(!snapInvisible.snapped || snapInvisible.point.Distance(p10_30) > 0.10,
                   "Test 33.7: Hidden Cylindrical grid is not snappable");
        std::cout << "  [PASS] Subtest 33.7: Multi-Grid Support, Active State Preservation & Visibility Validated" << std::endl;

        // Subtest 33.8: Undo/Redo Consistency for Elements Modeled on Cylindrical Grid
        structuralModel.pushUndoState("Draw Beam On Polar Grid");
        int testBeam = structuralModel.addBeam(nBase1, nBase2, 0.3, 0.4);
        TEST_CHECK(structuralModel.beams().size() >= 2, "Test 33.8: Beam added");

        // Undo
        bool undoSuccess = structuralModel.undo();
        TEST_CHECK(undoSuccess, "Test 33.8: Undo succeeded");
        TEST_CHECK(structuralModel.getBeam(testBeam) == nullptr, "Test 33.8: Beam removed by undo");

        // Redo
        bool redoSuccess = structuralModel.redo();
        TEST_CHECK(redoSuccess, "Test 33.8: Redo succeeded");
        TEST_CHECK(structuralModel.getBeam(testBeam) != nullptr, "Test 33.8: Beam restored by redo");
        TEST_CHECK(approxEqual(structuralModel.getNode(nBase1)->x(), p10_30.X()), "Test 33.8: Node 1 polar X maintained");
        TEST_CHECK(approxEqual(structuralModel.getNode(nBase2)->y(), p15_60.Y()), "Test 33.8: Node 2 polar Y maintained");
        std::cout << "  [PASS] Subtest 33.8: Undo/Redo Consistency on Cylindrical Grid Elements Validated" << std::endl;

        std::cout << "[PASS] Test 33: Complete Structural Modeling & Snapping Pipeline on Cylindrical Grids Passed Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 188 : niveaux et grilles dans l'historique Annuler / Rétablir (BUG-003)
    {
        TSA::Model::Model m;
        GridManager gm;
        gm.clearAllGrids();
        m.setGridManager(&gm);
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0.0, 5.0 });
        def.setYPositions({ 0.0, 5.0 });
        const std::string g1 = gm.addGrid(def)->id();
        gm.setActiveGridId(g1);

        auto* lm = m.levelManager();
        const std::string lvl = lm->addLevel("R+1", 3.0)->id;
        const int n = m.addNode(0, 0, 3.0);
        m.getNode(n)->setLevelId(lvl);

        // Élévation de niveau : le nœud rattaché suit ; Annuler rétablit niveau ET nœud.
        m.pushUndoState("Élévation R+1");
        lm->setLevelElevation(lvl, 4.0);
        TEST_CHECK(std::abs(m.getNode(n)->z() - 4.0) < 1e-9, "Test 188: nœud rattaché déplacé avec le niveau");
        TEST_CHECK(m.undo(), "Test 188: Annuler disponible");
        TEST_CHECK(std::abs(lm->getLevel(lvl)->elevation - 3.0) < 1e-9 && std::abs(m.getNode(n)->z() - 3.0) < 1e-9,
                   "Test 188: Annuler rétablit le niveau et le nœud");
        TEST_CHECK(m.redo() && std::abs(lm->getLevel(lvl)->elevation - 4.0) < 1e-9 && std::abs(m.getNode(n)->z() - 4.0) < 1e-9,
                   "Test 188: Rétablir réapplique les deux");

        // Grille ajoutée puis annulée ; l'affichage courant (visibilité) n'est pas remis en arrière.
        m.pushUndoState("Créer une grille");
        GridDefinition def2("G2", GridType::Cartesian);
        def2.setXPositions({ 0.0, 8.0 });
        def2.setYPositions({ 0.0, 8.0 });
        gm.addGrid(def2);
        gm.setGridVisible(g1, false);
        TEST_CHECK(gm.grids().size() == 2, "Test 188: deux grilles");
        TEST_CHECK(m.undo() && gm.grids().size() == 1 && gm.getGrid(g1), "Test 188: Annuler supprime la grille créée");
        TEST_CHECK(!gm.getGrid(g1)->isVisible() && gm.activeGridId() == g1, "Test 188: visibilité et grille active conservées");
        TEST_CHECK(m.redo() && gm.grids().size() == 2, "Test 188: Rétablir recrée la grille");

        // Transaction sans changement : aucune entrée, aucune notification (résultats préservés).
        const size_t before = m.undoManager()->undoCount();
        const auto revision = m.revision();
        {
            TSA::UndoRedo::EditTransaction tx(m, "Modifier les niveaux");
            tx.discardUnchanged();
        }
        TEST_CHECK(m.undoManager()->undoCount() == before && m.revision() == revision && m.canUndo(),
                   "Test 188: fenêtre des niveaux fermée sans changement → historique intact");
        m.setGridManager(nullptr);
        std::cout << "[PASS] Test 188: Niveaux et grilles annulables" << std::endl;
        passed++;
    }

    return true;
}
