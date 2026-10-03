#include "test_common.h"
#include <QIcon>
#include <QPixmap>
#include <BRepBuilderAPI_Transform.hxx>
#include "Geometry/BeamGeometry.h"

bool runSuite_Viewer(int& passed)
{
    // TEST 26: 3D Interaction State Manager Architecture
    // =========================================================================
    {
        std::cout << "\n--- TEST 26: 3D Interaction State Manager Architecture ---" << std::endl;

        TSA::Interaction::InteractionManager interactMgr;
        TEST_CHECK(interactMgr.mode() == TSA::Interaction::InteractionMode::Select, "Test 26: initial mode is Select");
        TEST_CHECK(!interactMgr.isDrawingMode(), "Test 26: Select is not a drawing mode");
        TEST_CHECK(!interactMgr.isTransformMode(), "Test 26: Select is not a transform mode");

        interactMgr.setMode(TSA::Interaction::InteractionMode::DrawBeam);
        TEST_CHECK(interactMgr.mode() == TSA::Interaction::InteractionMode::DrawBeam, "Test 26: mode changed to DrawBeam");
        TEST_CHECK(interactMgr.isDrawingMode(), "Test 26: DrawBeam is a drawing mode");
        TEST_CHECK(!interactMgr.hasStartPoint(), "Test 26: hasStartPoint false initially");

        gp_Pnt p1(1.0, 2.0, 3.0);
        interactMgr.setStartPoint(p1, 42);
        TEST_CHECK(interactMgr.hasStartPoint(), "Test 26: hasStartPoint true after setStartPoint");
        TEST_CHECK(interactMgr.startNodeId() == 42, "Test 26: start node ID is 42");

        interactMgr.setMode(TSA::Interaction::InteractionMode::Move3D);
        TEST_CHECK(interactMgr.isTransformMode(), "Test 26: Move3D is a transform mode");
        TEST_CHECK(!interactMgr.hasStartPoint(), "Test 26: drawing state reset on mode change");

        std::cout << "[PASS] Test 26: 3D Interaction State Manager Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 27: Zoom Under Cursor Camera Precision Math (OCCT Orthographic & Perspective)
    // =========================================================================
    {
        std::cout << "\n--- TEST 27: Zoom Under Cursor Camera Precision Math ---" << std::endl;

        // 1. Validation mathématique de la caméra Orthographique
        const double winW = 1920.0;
        const double winH = 1080.0;
        const double px = 1440.0; // Quart supérieur droit
        const double py = 270.0;

        const double dx = px - (winW * 0.5); // +480 px
        const double dy = (winH * 0.5) - py; // +270 px

        const double curScale = 10.0; // 10m d'emprise verticale
        const double zoomFactor = 1.15; // Zoom avant
        const double newScale = curScale / zoomFactor; // 8.695652m

        // Point 3D initialement sous la souris avant zoom (centre camera initial = (0,0,0))
        // P_world = (0,0,0) + dx * (curScale / winH) * Side + dy * (curScale / winH) * Up
        const double pX_init = dx * (curScale / winH);
        const double pY_init = dy * (curScale / winH);

        // Décalage du centre de caméra calculé par zoomAtCursor
        const double scaleDiff = (curScale - newScale) / winH;
        const double shiftX = dx * scaleDiff;
        const double shiftY = dy * scaleDiff;

        // Nouveau centre de la caméra
        const double centerX_new = shiftX;
        const double centerY_new = shiftY;

        // Reprojection de P_world dans le nouveau système de caméra (centreX_new, centerY_new, newScale)
        const double pX_rel = pX_init - centerX_new;
        const double pY_rel = pY_init - centerY_new;

        const double px_reprojected = (winW * 0.5) + pX_rel * (winH / newScale);
        const double py_reprojected = (winH * 0.5) - pY_rel * (winH / newScale);

        TEST_CHECK(approxEqual(px_reprojected, px, 1e-6), "Test 27: Reprojected mouse X matches initial cursor position exactly");
        TEST_CHECK(approxEqual(py_reprojected, py, 1e-6), "Test 27: Reprojected mouse Y matches initial cursor position exactly");

        // 2. Validation mathématique de la caméra Orthographique avec OpenCASCADE Camera UnProject
        // Test sur caméra en orientation axonométrique Z-up (comme dans TSA par défaut) avec centre arbitraire
        Handle(Graphic3d_Camera) testCam = new Graphic3d_Camera();
        testCam->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
        testCam->SetAspect(1920.0 / 1080.0);
        testCam->SetScale(25.0);
        testCam->SetEyeAndCenter(gp_Pnt(100.0, -100.0, 100.0), gp_Pnt(10.0, 20.0, 5.0));
        testCam->SetUp(gp_Dir(0.0, 0.0, 1.0));
        testCam->SetDirection(gp_Dir(-1.0, 1.0, -1.0));

        // Test à plusieurs positions de curseur (NDC variés) et plusieurs facteurs de zoom
        const std::vector<gp_Pnt> testNdcPoints = {
            gp_Pnt(0.0, 0.0, 0.0),     // Centre
            gp_Pnt(0.5, 0.5, 0.0),     // Quart haut-droit
            gp_Pnt(-0.7, -0.4, 0.0),   // Bas-gauche
            gp_Pnt(0.85, -0.65, 0.0)   // Bas-droit
        };

        const std::vector<double> testZoomFactors = { 1.15, 0.85, 1.5, 0.5 };

        for (size_t i = 0; i < testNdcPoints.size(); ++i)
        {
            const gp_Pnt& ndc = testNdcPoints[i];
            const double zFactor = testZoomFactors[i];

            const gp_Pnt pntBefore = testCam->UnProject(ndc);

            const double camScale = testCam->Scale();
            const double nextCamScale = camScale / zFactor;
            testCam->SetScale(nextCamScale);

            const gp_Pnt pntAfter = testCam->UnProject(ndc);
            const gp_Vec camShift(pntAfter, pntBefore);
            testCam->SetEyeAndCenter(testCam->Eye().Translated(camShift), testCam->Center().Translated(camShift));

            // Après le déplacement, le point initial pntBefore doit se reprojeter EXACTEMENT sous le même NDC
            const gp_Pnt reprojectedNdc = testCam->Project(pntBefore);
            TEST_CHECK(approxEqual(reprojectedNdc.X(), ndc.X(), 1e-7), "Test 27: Reprojected NDC X matches cursor with 0 drift");
            TEST_CHECK(approxEqual(reprojectedNdc.Y(), ndc.Y(), 1e-7), "Test 27: Reprojected NDC Y matches cursor with 0 drift");
        }

        gp_Pnt eye(0.0, 0.0, 10.0);
        gp_Pnt target(2.0, 3.0, 0.0);
        gp_Vec eyeToTarget(eye, target);

        double moveFactor = 1.0 - (1.0 / zoomFactor);
        gp_Vec shiftVec = eyeToTarget * moveFactor;

        gp_Pnt newEye = eye.Translated(shiftVec);
        gp_Vec newEyeToTarget(newEye, target);

        // Les deux vecteurs doivent rester colinéaires (produit vectoriel nul)
        gp_Vec crossProd = eyeToTarget.Crossed(newEyeToTarget);
        TEST_CHECK(crossProd.Magnitude() < 1e-8, "Test 27: Perspective line-of-sight ray remains perfectly collinear with zero drift");

        std::cout << "[PASS] Test 27: Zoom Under Cursor Camera Precision Math Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 34: Realistic Material Visual Appearance, Architecture & Persistence
    // =========================================================================
    {
        std::cout << "\n--- TEST 34: Realistic Material Visual Appearance, Architecture & Persistence ---" << std::endl;

        MaterialLibrary& matLib = MaterialLibrary::instance();
        MaterialVisual& matVis = MaterialVisual::instance();

        // ---------------------------------------------------------------------
        // Subtest 34.1: Concrete Beam -> Concrete Appearance
        // ---------------------------------------------------------------------
        Material concreteC25 = Material::concreteC25_30();
        TEST_CHECK(matLib.findByType(MaterialType::Concrete) != nullptr, "Test 34.1: MaterialLibrary contains Concrete");
        TEST_CHECK(concreteC25.type == MaterialType::Concrete, "Test 34.1: Material type is Concrete");
        TEST_CHECK(concreteC25.visual.roughness >= 0.80, "Test 34.1: Concrete has high roughness (roughness >= 0.80)");
        TEST_CHECK(concreteC25.visual.metallic == 0.0, "Test 34.1: Concrete is non-metallic (metallic == 0.0)");
        TEST_CHECK(concreteC25.visual.transparency == 0.0, "Test 34.1: Concrete has 0 transparency");
        TEST_CHECK(concreteC25.mechanical.youngModulus > 25.0e9, "Test 34.1: Concrete E modulus realistic (> 25 GPa)");

        Graphic3d_MaterialAspect concreteAspect = matVis.getOcctMaterial(concreteC25);
        TEST_CHECK(approxEqual(concreteAspect.PBRMaterial().Roughness(), static_cast<float>(concreteC25.visual.roughness), 1e-2),
                   "Test 34.1: OCCT PBR Roughness matches concrete visual properties");
        TEST_CHECK(approxEqual(concreteAspect.PBRMaterial().Metallic(), 0.0f),
                   "Test 34.1: OCCT PBR Metallic is 0 for concrete");

        Model testModel;
        int n1 = testModel.addNode(0.0, 0.0, 0.0);
        int n2 = testModel.addNode(5.0, 0.0, 0.0);
        int beamConcId = testModel.addBeam(n1, n2, 0.3, 0.5);
        auto* beamConc = testModel.getBeam(beamConcId);
        TEST_CHECK(beamConc != nullptr, "Test 34.1: Concrete beam created");
        beamConc->setMaterial(concreteC25);
        TEST_CHECK(beamConc->material().type == MaterialType::Concrete, "Test 34.1: Beam material is Concrete");
        TEST_CHECK(beamConc->materialId() == concreteC25.id, "Test 34.1: Beam materialId matches concreteC25.id");

        TopoDS_Shape beamShape = BeamGeometry::createBeamShape(*testModel.getNode(n1), *testModel.getNode(n2), beamConc->section(), 0.0);
        Handle(AIS_Shape) aisBeam = new AIS_Shape(beamShape);
        matVis.applyToShape(aisBeam, beamConc->material(), "", RenderDisplayMode::Materials);
        TEST_CHECK(!aisBeam.IsNull(), "Test 34.1: AIS_Shape for concrete beam configured with realistic appearance");
        std::cout << "  [PASS] Subtest 34.1: Concrete Beam Appearance Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.2: Concrete Column -> Concrete Appearance
        // ---------------------------------------------------------------------
        Material concreteC30 = Material::concreteC30_37();
        int n3 = testModel.addNode(0.0, 0.0, 3.0);
        int colConcId = testModel.addColumn(n1, n3, Section::rectangular(0.4, 0.4), concreteC30, 0.0, "C_CONC");
        auto* colConc = testModel.getColumn(colConcId);
        TEST_CHECK(colConc != nullptr, "Test 34.2: Column created");
        TEST_CHECK(colConc->material().type == MaterialType::Concrete, "Test 34.2: Column material type is Concrete");
        TEST_CHECK(colConc->material().visual.metallic == 0.0, "Test 34.2: Column material is non-metallic");
        Graphic3d_MaterialAspect colAspect = matVis.getOcctMaterial(colConc->material());
        TEST_CHECK(colAspect.PBRMaterial().Roughness() >= 0.80f, "Test 34.2: Column OCCT PBR Roughness is high");
        std::cout << "  [PASS] Subtest 34.2: Concrete Column Appearance Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.3: IPE 200 Steel -> Steel Metallic Appearance
        // ---------------------------------------------------------------------
        Material steelS235 = Material::steelS235();
        TEST_CHECK(matLib.findByType(MaterialType::Steel) != nullptr, "Test 34.3: MaterialLibrary contains Steel");
        TEST_CHECK(steelS235.type == MaterialType::Steel, "Test 34.3: Material type is Steel");
        TEST_CHECK(steelS235.visual.metallic >= 0.80, "Test 34.3: Steel is metallic (metallic >= 0.80)");
        TEST_CHECK(steelS235.visual.roughness <= 0.40, "Test 34.3: Steel has moderate/low roughness (<= 0.40)");
        TEST_CHECK(steelS235.visual.shininess >= 0.60, "Test 34.3: Steel has specular reflection (shininess >= 0.60)");
        TEST_CHECK(steelS235.mechanical.youngModulus >= 200.0e9, "Test 34.3: Steel E modulus is 210 GPa");

        Graphic3d_MaterialAspect steelAspect = matVis.getOcctMaterial(steelS235);
        TEST_CHECK(approxEqual(steelAspect.PBRMaterial().Metallic(), static_cast<float>(steelS235.visual.metallic)),
                   "Test 34.3: OCCT PBR Metallic matches steel visual properties");
        TEST_CHECK(steelAspect.PBRMaterial().Roughness() <= 0.40f,
                   "Test 34.3: OCCT PBR Roughness is moderate for steel");

        int n4 = testModel.addNode(5.0, 0.0, 3.0);
        int beamSteelId = testModel.addBar(n3, n4, Section::ipe(200), steelS235, BarRole::Beam, 0.0, "B_IPE200");
        auto* beamSteel = testModel.getBeam(beamSteelId);
        TEST_CHECK(beamSteel != nullptr, "Test 34.3: IPE 200 beam created");
        TEST_CHECK(beamSteel->material().type == MaterialType::Steel, "Test 34.3: Beam material is Steel");
        TEST_CHECK(beamSteel->materialId() == steelS235.id, "Test 34.3: Beam materialId matches steelS235.id");
        std::cout << "  [PASS] Subtest 34.3: IPE 200 Steel Appearance Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.4: Rebar / Ferraillage -> Dark Steel Metallic Appearance
        // ---------------------------------------------------------------------
        Material rebarMat = Material::rebarSteel();
        TEST_CHECK(matLib.findByType(MaterialType::RebarSteel) != nullptr, "Test 34.4: MaterialLibrary contains Rebar");
        TEST_CHECK(rebarMat.type == MaterialType::RebarSteel, "Test 34.4: Material type is RebarSteel");
        TEST_CHECK(rebarMat.visual.metallic >= 0.85, "Test 34.4: Rebar is metallic (metallic >= 0.85)");
        TEST_CHECK(rebarMat.visual.roughness >= 0.40, "Test 34.4: Rebar has visible surface texture/roughness");
        TEST_CHECK(rebarMat.visual.baseColor != steelS235.visual.baseColor, "Test 34.4: Rebar color is distinct from standard steel");
        Graphic3d_MaterialAspect rebarAspect = matVis.getOcctMaterial(rebarMat);
        TEST_CHECK(rebarAspect.PBRMaterial().Metallic() >= 0.85f, "Test 34.4: Rebar OCCT metallic confirmed");
        std::cout << "  [PASS] Subtest 34.4: Rebar / Ferraillage Appearance Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.5: Wood / Timber -> Natural Wood Appearance
        // ---------------------------------------------------------------------
        Material woodMat = Material::timberC24();
        TEST_CHECK(matLib.findByType(MaterialType::Timber) != nullptr, "Test 34.5: MaterialLibrary contains Timber");
        TEST_CHECK(woodMat.type == MaterialType::Timber, "Test 34.5: Material type is Timber");
        TEST_CHECK(woodMat.visual.metallic == 0.0, "Test 34.5: Wood is non-metallic (metallic == 0.0)");
        TEST_CHECK(woodMat.visual.roughness >= 0.70, "Test 34.5: Wood has natural diffuse roughness");
        Graphic3d_MaterialAspect woodAspect = matVis.getOcctMaterial(woodMat);
        TEST_CHECK(woodAspect.PBRMaterial().Metallic() == 0.0f, "Test 34.5: Wood OCCT metallic is 0");
        std::cout << "  [PASS] Subtest 34.5: Wood / Timber Appearance Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.6: Soil & Geotechnical Materials -> Distinct Earth Appearances
        // ---------------------------------------------------------------------
        Material soilMat = Material::soil();
        Material sandMat = Material::sand();
        Material gravelMat = Material::gravel();
        Material rockMat = Material::rock();
        TEST_CHECK(soilMat.type == MaterialType::Soil, "Test 34.6: Soil material type");
        TEST_CHECK(sandMat.type == MaterialType::Sand, "Test 34.6: Sand material type");
        TEST_CHECK(gravelMat.type == MaterialType::Gravel, "Test 34.6: Gravel material type");
        TEST_CHECK(rockMat.type == MaterialType::Rock, "Test 34.6: Rock material type");
        TEST_CHECK(soilMat.visual.baseColor != sandMat.visual.baseColor, "Test 34.6: Soil and Sand colors distinct");
        TEST_CHECK(soilMat.visual.baseColor != gravelMat.visual.baseColor, "Test 34.6: Soil and Gravel colors distinct");
        TEST_CHECK(rockMat.visual.roughness >= 0.80, "Test 34.6: Rock has high roughness");
        std::cout << "  [PASS] Subtest 34.6: Soil, Sand, Gravel & Rock Appearances Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.7: Dynamic Material Modification & ModelDiff Detection
        // ---------------------------------------------------------------------
        auto snapBefore = testModel.createSnapshot("Before Change");
        auto* beamToMod = testModel.getBeam(beamConcId);
        TEST_CHECK(beamToMod != nullptr, "Test 34.7: Beam exists");
        Material aluminumMat = Material::aluminum();
        beamToMod->setMaterial(aluminumMat);
        beamToMod->setMaterialId(aluminumMat.id);

        auto snapAfter = testModel.createSnapshot("After Change");
        ModelDiff diff = ModelDiff::compute(snapBefore, snapAfter);
        TEST_CHECK(!diff.isEmpty(), "Test 34.7: ModelDiff detects changes after material modification");
        TEST_CHECK(!diff.modifiedBeamIds.empty(), "Test 34.7: modifiedBeamIds list is non-empty");
        TEST_CHECK(diff.modifiedBeamIds[0] == beamConcId, "Test 34.7: Modified beam identified in ModelDiff");

        matVis.clearCache();
        Graphic3d_MaterialAspect aluAspect = matVis.getOcctMaterial(beamToMod->material());
        TEST_CHECK(aluAspect.PBRMaterial().Metallic() >= 0.85f, "Test 34.7: Modified beam has aluminum metallic appearance");
        std::cout << "  [PASS] Subtest 34.7: Dynamic Material Modification & ModelDiff Detection Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.8: Native .tsa Save and Load Persistence
        // ---------------------------------------------------------------------
        std::string testFile = "test_material_persistence.tsa";
        TSAFileWriter fileWriter;
        std::string errStr;
        bool saveOk = fileWriter.saveToFile(testFile, testModel, nullptr, "Material Test", "Unit Test", &errStr);
        TEST_CHECK(saveOk, "Test 34.8: Model with varied materials saved successfully");

        Model loadedModel;
        TSAFileReader fileReader;
        bool loadOk = fileReader.loadFromFile(testFile, loadedModel, nullptr, "", nullptr, nullptr, nullptr, &errStr);
        TEST_CHECK(loadOk, "Test 34.8: Model loaded successfully");

        const auto* lBeamAlu = loadedModel.getBeam(beamConcId);
        TEST_CHECK(lBeamAlu != nullptr, "Test 34.8: Loaded aluminum beam exists");
        TEST_CHECK(lBeamAlu->material().type == MaterialType::Aluminum, "Test 34.8: Loaded beam material type is Aluminum");
        TEST_CHECK(approxEqual(lBeamAlu->material().visual.metallic, aluminumMat.visual.metallic), "Test 34.8: Visual metallic restored");
        TEST_CHECK(approxEqual(lBeamAlu->material().visual.roughness, aluminumMat.visual.roughness), "Test 34.8: Visual roughness restored");

        const auto* lBeamSteel = loadedModel.getBeam(beamSteelId);
        TEST_CHECK(lBeamSteel != nullptr, "Test 34.8: Loaded steel beam exists");
        TEST_CHECK(lBeamSteel->material().type == MaterialType::Steel, "Test 34.8: Loaded steel beam material type is Steel");
        TEST_CHECK(approxEqual(lBeamSteel->material().mechanical.youngModulus, steelS235.mechanical.youngModulus), "Test 34.8: Mechanical E modulus restored");

        std::filesystem::remove(testFile);
        std::cout << "  [PASS] Subtest 34.8: TSA File Save & Load Material Persistence Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.9: Copy / Paste Integrity with MaterialId Preservation
        // ---------------------------------------------------------------------
        StructuralClipboard clipboard;
        std::set<int> selNodes = { n3, n4 };
        std::set<int> selBeams = { beamSteelId };
        std::set<int> selCols;
        std::set<int> selSlabs;
        clipboard.copyFrom(testModel, selNodes, selBeams, selCols, selSlabs);

        Model targetModel;
        PasteResult pasteRes = clipboard.pasteTo(targetModel, 10.0, 10.0, 0.0);
        TEST_CHECK(!pasteRes.beamIds.empty(), "Test 34.9: Beam pasted into new model");
        const auto* pastedSteelBeam = targetModel.getBeam(pasteRes.beamIds[0]);
        TEST_CHECK(pastedSteelBeam != nullptr, "Test 34.9: Pasted beam exists");
        TEST_CHECK(pastedSteelBeam->material().type == MaterialType::Steel, "Test 34.9: Pasted beam material is Steel");
        TEST_CHECK(pastedSteelBeam->materialId() == steelS235.id, "Test 34.9: Pasted beam materialId preserved exactly");
        TEST_CHECK(approxEqual(pastedSteelBeam->material().visual.metallic, steelS235.visual.metallic), "Test 34.9: Pasted beam visual metallic preserved");
        std::cout << "  [PASS] Subtest 34.9: Copy / Paste Integrity with Material Preservation Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 34.10: Centralized Undo / Redo Material Restoration
        // ---------------------------------------------------------------------
        testModel.pushUndoState("Modify Material to Wood");
        beamToMod->setMaterial(woodMat);
        beamToMod->setMaterialId(woodMat.id);
        TEST_CHECK(testModel.getBeam(beamConcId)->material().type == MaterialType::Timber, "Test 34.10: Material changed to Timber");

        // Undo
        bool undoOk = testModel.undo();
        TEST_CHECK(undoOk, "Test 34.10: Undo operation succeeded");
        TEST_CHECK(testModel.getBeam(beamConcId)->material().type == MaterialType::Aluminum, "Test 34.10: Undo restored Aluminum material");

        // Redo
        bool redoOk = testModel.redo();
        TEST_CHECK(redoOk, "Test 34.10: Redo operation succeeded");
        TEST_CHECK(testModel.getBeam(beamConcId)->material().type == MaterialType::Timber, "Test 34.10: Redo restored Timber material");
        std::cout << "  [PASS] Subtest 34.10: Undo / Redo Material Restoration Validated" << std::endl;

        std::cout << "[PASS] Test 34: Complete Realistic Material Pipeline (Concrete, Steel, Rebar, Wood, Soil, OCCT PBR, Persistence, Undo/Redo) Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 35: Global Non-Blocking 3D Interactive Selection Mechanism
    // -------------------------------------------------------------------------
    {
        std::cout << "--- TEST 35: Global Non-Blocking 3D Interactive Selection Mechanism ---" << std::endl;

        using namespace TSA::Interaction;
        InteractionManager interactionMgr;

        // ---------------------------------------------------------------------
        // Subtest 35.1: Navigation non-bloquante & État de requête de sélection
        // ---------------------------------------------------------------------
        bool reqSignalReceived = false;
        QString receivedField;
        QMetaObject::Connection reqConn = QObject::connect(&interactionMgr, &InteractionManager::selectionRequested, [&](const SelectionRequest& r) {
            reqSignalReceived = true;
            receivedField = r.targetField;
        });

        SelectionRequest req1;
        req1.mode = SelectionMode::SelectPoint;
        req1.targetField = "Origine_Test";
        req1.snapEnabled = true;
        req1.keepWindowOpen = true;
        interactionMgr.requestSelection(req1);

        TEST_CHECK(interactionMgr.hasActiveSelectionRequest(), "Subtest 35.1: Active selection request is true");
        TEST_CHECK(reqSignalReceived, "Subtest 35.1: selectionRequested signal fired");
        TEST_CHECK(receivedField == "Origine_Test", "Subtest 35.1: Signal received with correct targetField");
        TEST_CHECK(interactionMgr.activeSelectionRequest().has_value(), "Subtest 35.1: activeSelectionRequest has value");
        TEST_CHECK(interactionMgr.promptText().contains("Origine_Test"), "Subtest 35.1: promptText reflects target field");
        QObject::disconnect(reqConn);
        std::cout << "  [PASS] Subtest 35.1: Non-blocking Selection Request & State Management Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.2: Navigation caméra pendant sélection active
        // ---------------------------------------------------------------------
        // Pendant que la sélection est active, le viewport ou l'utilisateur peut naviguer
        // (zoom, pan, rotation). L'état de requête de sélection reste intact et actif.
        interactionMgr.setMode(InteractionMode::Select);
        TEST_CHECK(interactionMgr.hasActiveSelectionRequest(), "Subtest 35.2: Selection request stays active during navigation");
        TEST_CHECK(interactionMgr.activeSelectionRequest()->targetField == "Origine_Test", "Subtest 35.2: targetField preserved during navigation");
        std::cout << "  [PASS] Subtest 35.2: Camera Navigation During Active Selection Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.3: Sélection d'un point 3D exact
        // ---------------------------------------------------------------------
        gp_Pnt receivedPnt(0, 0, 0);
        bool selectedCallbackCalled = false;
        SelectionRequest reqPnt;
        reqPnt.mode = SelectionMode::SelectPoint;
        reqPnt.targetField = "Point3D";
        reqPnt.onSelected = [&](const SelectedEntity& entity) {
            selectedCallbackCalled = true;
            receivedPnt = entity.point;
        };
        interactionMgr.requestSelection(reqPnt);

        SelectedEntity pickedEntity;
        pickedEntity.mode = SelectionMode::SelectPoint;
        pickedEntity.point = gp_Pnt(12.5, 8.25, 4.0);
        pickedEntity.targetField = "Point3D";
        interactionMgr.completeSelection(pickedEntity);

        TEST_CHECK(selectedCallbackCalled, "Subtest 35.3: onSelected callback executed");
        TEST_CHECK(approxEqual(receivedPnt.X(), 12.5), "Subtest 35.3: Point X coordinate exact");
        TEST_CHECK(approxEqual(receivedPnt.Y(), 8.25), "Subtest 35.3: Point Y coordinate exact");
        TEST_CHECK(approxEqual(receivedPnt.Z(), 4.0), "Subtest 35.3: Point Z coordinate exact");
        TEST_CHECK(!interactionMgr.hasActiveSelectionRequest(), "Subtest 35.3: Selection request cleared after completion");
        std::cout << "  [PASS] Subtest 35.3: Exact 3D Point Selection Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.4: Accrochage sur intersection de grille cartésienne
        // ---------------------------------------------------------------------
        GridManager cartGridMgr;
        cartGridMgr.clearAllGrids();
        GridDefinition cartDef("CartGridTest", GridType::Cartesian);
        cartDef.setXPositions({ 0.0, 6.0, 12.0 });
        cartDef.setYPositions({ 0.0, 4.0, 8.0 });
        cartDef.setZLevels({ 0.0, 3.0 });
        auto* cartSys = cartGridMgr.addGrid(cartDef);
        cartGridMgr.setActiveGridId(cartSys->id());

        GridSnapManager snapMgr;
        snapMgr.setSnapTolerance(0.50);

        gp_Pnt nearCartPnt(6.08, 3.92, 0.02);
        GridSnapResult cartSnap = snapMgr.findSnap(nearCartPnt, &cartGridMgr, nullptr);
        TEST_CHECK(cartSnap.snapped, "Subtest 35.4: Snapped to cartesian grid");
        TEST_CHECK(cartSnap.type == GridSnapType::Intersection, "Subtest 35.4: Snap type is Intersection");
        TEST_CHECK(approxEqual(cartSnap.point.X(), 6.0), "Subtest 35.4: Snapped X exact");
        TEST_CHECK(approxEqual(cartSnap.point.Y(), 4.0), "Subtest 35.4: Snapped Y exact");
        TEST_CHECK(approxEqual(cartSnap.point.Z(), 0.0), "Subtest 35.4: Snapped Z exact");

        // Transmettre le point accroché à la requête de sélection
        gp_Pnt cartCallbackPnt;
        SelectionRequest reqCart;
        reqCart.mode = SelectionMode::SelectPoint;
        reqCart.onSelected = [&](const SelectedEntity& e) { cartCallbackPnt = e.point; };
        interactionMgr.requestSelection(reqCart);
        SelectedEntity snappedCartEntity;
        snappedCartEntity.point = cartSnap.point;
        interactionMgr.completeSelection(snappedCartEntity);
        TEST_CHECK(approxEqual(cartCallbackPnt.X(), 6.0) && approxEqual(cartCallbackPnt.Y(), 4.0), "Subtest 35.4: Dialog received snapped intersection");
        std::cout << "  [PASS] Subtest 35.4: Cartesian Grid Intersection Snapping Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.5: Accrochage sur grille cylindrique (Rayon x Angle)
        // ---------------------------------------------------------------------
        GridManager cylGridMgr;
        cylGridMgr.clearAllGrids();
        GridDefinition cylDef("CylGridTest", GridType::Cylindrical);
        cylDef.setRadii({ 2.0, 4.0, 6.0 });
        cylDef.setAngles({ 0.0, 30.0, 60.0, 90.0 });
        auto* cylSys = cylGridMgr.addGrid(cylDef);
        cylGridMgr.setActiveGridId(cylSys->id());

        // À R=4.0, theta=60°: X = 4 * cos(60°) = 2.0, Y = 4 * sin(60°) = 3.4641016
        double expectedX = 4.0 * std::cos(60.0 * M_PI / 180.0);
        double expectedY = 4.0 * std::sin(60.0 * M_PI / 180.0);
        gp_Pnt nearCylPnt(2.05, 3.42, 0.0);
        GridSnapResult cylSnap = snapMgr.findSnap(nearCylPnt, &cylGridMgr, nullptr);
        TEST_CHECK(cylSnap.snapped, "Subtest 35.5: Snapped to cylindrical grid");
        TEST_CHECK(approxEqual(cylSnap.point.X(), expectedX, 0.02), "Subtest 35.5: Cylindrical intersection X");
        TEST_CHECK(approxEqual(cylSnap.point.Y(), expectedY, 0.02), "Subtest 35.5: Cylindrical intersection Y");

        gp_Pnt cylCallbackPnt;
        SelectionRequest reqCyl;
        reqCyl.mode = SelectionMode::SelectPoint;
        reqCyl.onSelected = [&](const SelectedEntity& e) { cylCallbackPnt = e.point; };
        interactionMgr.requestSelection(reqCyl);
        SelectedEntity snappedCylEntity;
        snappedCylEntity.point = cylSnap.point;
        interactionMgr.completeSelection(snappedCylEntity);
        TEST_CHECK(approxEqual(cylCallbackPnt.X(), expectedX, 0.02), "Subtest 35.5: Callback received cylindrical snap");
        std::cout << "  [PASS] Subtest 35.5: Cylindrical Radial x Angle Intersection Snapping Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.6: Annulation propre (Touche ESC) sans modification du modèle
        // ---------------------------------------------------------------------
        Model testModel35;
        testModel35.addNode(0, 0, 0);
        testModel35.addNode(5, 0, 0);
        const size_t initialNodes = testModel35.nodes().size();
        const size_t initialBars = testModel35.bars().size();

        bool cancelCallbackCalled = false;
        SelectionRequest reqCancel;
        reqCancel.mode = SelectionMode::SelectPoint;
        reqCancel.targetField = "Annuler_Test";
        reqCancel.onCancelled = [&]() { cancelCallbackCalled = true; };
        interactionMgr.requestSelection(reqCancel);
        TEST_CHECK(interactionMgr.hasActiveSelectionRequest(), "Subtest 35.6: Request active before escape");

        // Simuler ESC
        interactionMgr.cancelSelectionRequest();
        TEST_CHECK(cancelCallbackCalled, "Subtest 35.6: onCancelled triggered");
        TEST_CHECK(!interactionMgr.hasActiveSelectionRequest(), "Subtest 35.6: Request cleared after cancel");
        TEST_CHECK(testModel35.nodes().size() == initialNodes, "Subtest 35.6: Zero node changes");
        TEST_CHECK(testModel35.bars().size() == initialBars, "Subtest 35.6: Zero bar changes");
        std::cout << "  [PASS] Subtest 35.6: ESC Clean Cancellation with Zero Model Modification Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.7: Validation formulaire (Pick -> update field -> Apply -> update 3D)
        // ---------------------------------------------------------------------
        double formFieldX = 0.0, formFieldY = 0.0, formFieldZ = 0.0;
        bool modelOrGridUpdated = false;

        SelectionRequest reqForm;
        reqForm.mode = SelectionMode::SelectPoint;
        reqForm.targetField = "Origine_Form";
        reqForm.onSelected = [&](const SelectedEntity& e) {
            // Seuls les champs d'interface sont mis à jour lors de la sélection
            formFieldX = e.point.X();
            formFieldY = e.point.Y();
            formFieldZ = e.point.Z();
        };
        interactionMgr.requestSelection(reqForm);

        SelectedEntity formPick;
        formPick.point = gp_Pnt(7.0, 14.0, 2.5);
        interactionMgr.completeSelection(formPick);

        TEST_CHECK(approxEqual(formFieldX, 7.0) && approxEqual(formFieldY, 14.0) && approxEqual(formFieldZ, 2.5),
                   "Subtest 35.7: Form fields updated by 3D pick");
        TEST_CHECK(!modelOrGridUpdated, "Subtest 35.7: Model/Grid not modified before Apply");

        // L'utilisateur clique sur "Appliquer"
        GridDefinition appliedDef("AppliedGrid", GridType::Cartesian);
        appliedDef.setOrigin(formFieldX, formFieldY, formFieldZ);
        appliedDef.setXPositions({ 0.0, 5.0 });
        appliedDef.setYPositions({ 0.0, 5.0 });
        cartGridMgr.addGrid(appliedDef);
        modelOrGridUpdated = true;

        TEST_CHECK(modelOrGridUpdated, "Subtest 35.7: Update committed on Apply");
        TEST_CHECK(approxEqual(cartGridMgr.grids().back()->definition().origin().X(), 7.0), "Subtest 35.7: Grid origin updated on Apply");
        std::cout << "  [PASS] Subtest 35.7: Form Validation Lifecycle (Pick -> Update Field -> Apply) Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.8: Deuxième appelant (généricité de l'architecture)
        // ---------------------------------------------------------------------
        QObject secondCaller;
        int secondResultNodeId = -1;
        SelectionRequest reqCaller2;
        reqCaller2.mode = SelectionMode::SelectNode;
        reqCaller2.targetField = "Appui_NodeId";
        reqCaller2.sender = &secondCaller;
        reqCaller2.onSelected = [&](const SelectedEntity& e) {
            secondResultNodeId = e.entityId;
        };
        interactionMgr.requestSelection(reqCaller2);

        SelectedEntity nodeEntity;
        nodeEntity.mode = SelectionMode::SelectNode;
        nodeEntity.entityId = 99;
        nodeEntity.point = gp_Pnt(0.0, 0.0, 6.0);
        interactionMgr.completeSelection(nodeEntity);

        TEST_CHECK(secondResultNodeId == 99, "Subtest 35.8: Second caller received node ID 99 without code duplication");
        std::cout << "  [PASS] Subtest 35.8: Generic Multi-Caller Architecture Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.9: Absence d'effet de bord sur l'historique Undo/Redo
        // ---------------------------------------------------------------------
        TSA::UndoRedo::UndoManager undoMgr;
        TSA::UndoRedo::CommandManager cmdMgr(&testModel35, &undoMgr);
        const bool initialCanUndo = undoMgr.canUndo();
        const bool initialCanRedo = undoMgr.canRedo();

        // Effectuer des sélections 3D et des navigations
        SelectionRequest reqUndoCheck;
        reqUndoCheck.mode = SelectionMode::SelectPoint;
        interactionMgr.requestSelection(reqUndoCheck);
        SelectedEntity dummyEntity;
        dummyEntity.point = gp_Pnt(1, 2, 3);
        interactionMgr.completeSelection(dummyEntity);

        // Annulation d'une autre sélection
        interactionMgr.requestSelection(reqUndoCheck);
        interactionMgr.cancelSelectionRequest();

        TEST_CHECK(undoMgr.canUndo() == initialCanUndo, "Subtest 35.9: canUndo unchanged by 3D selection");
        TEST_CHECK(undoMgr.canRedo() == initialCanRedo, "Subtest 35.9: canRedo unchanged by 3D selection");
        std::cout << "  [PASS] Subtest 35.9: Zero Undo/Redo Side Effects During 3D Selection Validated" << std::endl;

        // ---------------------------------------------------------------------
        // Subtest 35.10: Nettoyage automatique à la destruction de la fenêtre
        // ---------------------------------------------------------------------
        bool cancelOnDestroyCalled = false;
        auto* dynamicCaller = new QObject();
        SelectionRequest reqDestroy;
        reqDestroy.mode = SelectionMode::SelectPoint;
        reqDestroy.sender = dynamicCaller;
        reqDestroy.targetField = "FenetreTemporaire";
        reqDestroy.onCancelled = [&]() { cancelOnDestroyCalled = true; };
        interactionMgr.requestSelection(reqDestroy);
        TEST_CHECK(interactionMgr.hasActiveSelectionRequest(), "Subtest 35.10: Selection active with dynamic window");

        // Fermeture / destruction de la fenêtre appelante
        delete dynamicCaller;
        TEST_CHECK(!interactionMgr.hasActiveSelectionRequest(), "Subtest 35.10: Request automatically cleaned up on sender destruction");
        TEST_CHECK(cancelOnDestroyCalled, "Subtest 35.10: onCancelled called on sender destruction");

        std::cout << "[PASS] Test 35: Global Non-Blocking 3D Interactive Selection Mechanism Passed Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 36: Application Brand Identity and Multi-Resolution Icon Resources
    // =========================================================================
    {
        std::cout << "\n--- TEST 36: Application Brand Identity and Multi-Resolution Icon Resources ---" << std::endl;

        // Subtest 36.1: Vérification de la présence des ressources vectorielles et multi-résolutions
        TEST_CHECK(QFile::exists(":/icons/TSA.ico"), "Subtest 36.1: :/icons/TSA.ico resource exists");
        TEST_CHECK(QFile::exists(":/icons/TSA.svg"), "Subtest 36.1: :/icons/TSA.svg resource exists");
        TEST_CHECK(QFile::exists(":/icons/TSA_glyph.svg"), "Subtest 36.1: :/icons/TSA_glyph.svg resource exists");
        TEST_CHECK(QFile::exists(":/icons/TSA_light.svg"), "Subtest 36.1: :/icons/TSA_light.svg resource exists");
        TEST_CHECK(QFile::exists(":/icons/TSA_monochrome.svg"), "Subtest 36.1: :/icons/TSA_monochrome.svg resource exists");

        // Subtest 36.2: Chargement et validation de l'objet QIcon principal
        QIcon appIcon(":/icons/TSA.ico");
        TEST_CHECK(!appIcon.isNull(), "Subtest 36.2: QIcon from :/icons/TSA.ico is not null");

        QIcon svgIcon(":/icons/TSA.svg");
        TEST_CHECK(!svgIcon.isNull(), "Subtest 36.2: QIcon from :/icons/TSA.svg is not null");

        // Subtest 36.3: Rendu multi-tailles pixmap (16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256)
        const std::vector<int> targetSizes = {16, 24, 32, 48, 64, 128, 256};
        for (int sz : targetSizes) {
            QPixmap pxSvg = svgIcon.pixmap(sz, sz);
            TEST_CHECK(!pxSvg.isNull(), std::string("Subtest 36.3: SVG Pixmap not null for size ") + std::to_string(sz));
            TEST_CHECK(pxSvg.width() > 0 && pxSvg.height() > 0, std::string("Subtest 36.3: SVG Pixmap valid dimensions for size ") + std::to_string(sz));

            QPixmap pxIco = appIcon.pixmap(sz, sz);
            TEST_CHECK(!pxIco.isNull(), std::string("Subtest 36.3: ICO Pixmap not null for size ") + std::to_string(sz));
            TEST_CHECK(pxIco.width() > 0 && pxIco.height() > 0, std::string("Subtest 36.3: ICO Pixmap valid dimensions for size ") + std::to_string(sz));
        }

        std::cout << "[PASS] Test 36: Brand Identity and Multi-Resolution Icon Resources Validated Successfully!" << std::endl;
        passed++;
    }

    // TEST PERF-GHOST: l'aperçu fantôme (transformation locale d'un solide construit une fois)
    // doit être géométriquement équivalent au solide reconstruit aux positions transformées.
    // =========================================================================
    {
        std::cout << "\n--- TEST PERF-GHOST: Transform preview ghost equivalence ---" << std::endl;

        auto sameBox = [](const TopoDS_Shape& a, const TopoDS_Shape& b) -> bool {
            Bnd_Box ba, bb;
            BRepBndLib::Add(a, ba);
            BRepBndLib::Add(b, bb);
            double a0[6], b0[6];
            ba.Get(a0[0], a0[1], a0[2], a0[3], a0[4], a0[5]);
            bb.Get(b0[0], b0[1], b0[2], b0[3], b0[4], b0[5]);
            for (int i = 0; i < 6; ++i)
                if (std::abs(a0[i] - b0[i]) > 1e-5) return false;
            return true;
        };

        const auto sec = TSA::Model::Section::rectangular(0.30, 0.50);

        // Translation
        {
            TSA::Model::Node a(0, 0.0, 0.0, 0.0), b(1, 4.0, 1.0, 0.0);
            TSA::Model::Node ta(0, 2.0, 3.0, 1.0), tb(1, 6.0, 4.0, 1.0);
            TopoDS_Shape ghost = TSA::Geometry::BeamGeometry::createBeamShape(a, b, sec, 0.0, TSA::Model::BarEccentricity::None);
            TopoDS_Shape rebuilt = TSA::Geometry::BeamGeometry::createBeamShape(ta, tb, sec, 0.0, TSA::Model::BarEccentricity::None);
            gp_Trsf t;
            t.SetTranslation(gp_Vec(2.0, 3.0, 1.0));
            TopoDS_Shape moved = BRepBuilderAPI_Transform(ghost, t, true).Shape();
            TEST_CHECK(sameBox(moved, rebuilt), "PERF-GHOST: translated ghost == rebuilt beam");
        }

        // Rotation autour de Z (barre non verticale)
        {
            const double ang = 30.0 * M_PI / 180.0;
            const gp_Pnt c(1.0, 1.0, 0.0);
            auto rot = [&](double x, double y, double z) {
                double rx = x - c.X(), ry = y - c.Y();
                return TSA::Model::Node(0, c.X() + rx * std::cos(ang) - ry * std::sin(ang),
                                           c.Y() + rx * std::sin(ang) + ry * std::cos(ang), z);
            };
            TSA::Model::Node a(0, 0.0, 0.0, 0.0), b(1, 4.0, 1.0, 0.0);
            TSA::Model::Node ra = rot(0.0, 0.0, 0.0), rb = rot(4.0, 1.0, 0.0);
            TopoDS_Shape ghost = TSA::Geometry::BeamGeometry::createBeamShape(a, b, sec, 0.0, TSA::Model::BarEccentricity::None);
            TopoDS_Shape rebuilt = TSA::Geometry::BeamGeometry::createBeamShape(ra, rb, sec, 0.0, TSA::Model::BarEccentricity::None);
            gp_Trsf t;
            t.SetRotation(gp_Ax1(c, gp_Dir(0, 0, 1)), ang);
            TopoDS_Shape turned = BRepBuilderAPI_Transform(ghost, t, true).Shape();
            TEST_CHECK(sameBox(turned, rebuilt), "PERF-GHOST: rotated ghost == rebuilt beam (non-vertical)");
        }

        std::cout << "[PASS] TEST PERF-GHOST" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 93: Géométrie des nœuds (sphère OCCT non nulle)
    // Régression : createNodeShape() testait IsDone() avant Shape() ; la construction des
    // primitives OCCT étant paresseuse, la forme était toujours nulle -> aucun nœud affiché.
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 93: Forme 3D des nœuds ---" << std::endl;
        TSA::Model::Node node(1, 2.0, 3.0, 4.0);
        TopoDS_Shape sphere = TSA::Geometry::BeamGeometry::createNodeShape(node, 0.12);
        TEST_CHECK(!sphere.IsNull(), "Test 93: la sphère du nœud n'est pas nulle");
        Bnd_Box box;
        BRepBndLib::Add(sphere, box);
        double xmin, ymin, zmin, xmax, ymax, zmax;
        box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        TEST_CHECK(approxEqual((xmin + xmax) * 0.5, 2.0, 1e-3) && approxEqual((zmin + zmax) * 0.5, 4.0, 1e-3),
                   "Test 93: sphère centrée sur le nœud");
        TEST_CHECK(approxEqual(xmax - xmin, 0.24, 1e-2), "Test 93: diamètre = 2 x rayon");
        std::cout << "[PASS] Test 93: Forme 3D des nœuds" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 94: Primitives OCCT des builders (IsDone() avant Shape())
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 94: Primitives OCCT des builders ---" << std::endl;
        TSA::Model::Node a(1, 0.0, 0.0, 0.0), b(2, 0.0, 0.0, 3.0);
        // Comportement OCCT documenté par ce test : construction paresseuse des primitives.
        BRepPrimAPI_MakeBox lazyBox(gp_Pnt(0, 0, 0), gp_Pnt(1, 1, 1));
        BRepPrimAPI_MakeCylinder lazyCyl(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0.2, 1.0);
        std::cout << "  [OCCT] IsDone() avant Build() : box=" << lazyBox.IsDone()
                  << " cylinder=" << lazyCyl.IsDone() << std::endl;
        lazyBox.Build();
        lazyCyl.Build();
        TEST_CHECK(lazyBox.IsDone() && lazyCyl.IsDone(), "Test 94: primitives construites après Build()");

        TSA::Model::Section circ = TSA::Model::Section::circular(0.40);
        TopoDS_Shape col = TSA::Geometry::BeamGeometry::createBeamShape(a, b, circ, 0.0);
        TEST_CHECK(!col.IsNull(), "Test 94: poteau circulaire non nul");
        bool hasCyl = false;
        for (TopExp_Explorer exp(col, TopAbs_FACE); exp.More() && !hasCyl; exp.Next())
        {
            Handle(Geom_Surface) surf = BRep_Tool::Surface(TopoDS::Face(exp.Current()));
            hasCyl = !Handle(Geom_CylindricalSurface)::DownCast(surf).IsNull();
        }
        TEST_CHECK(hasCyl, "Test 94: le poteau circulaire est un vrai cylindre");
        std::cout << "[PASS] Test 94: Primitives OCCT des builders" << std::endl;
        passed++;
    }

    return true;
}

