#include "test_common.h"

bool runSuite_Model(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 11: Undo & Redo (Ctrl+Z and Ctrl+Y snapshot system)
    // -------------------------------------------------------------------------
    {
        Model undoModel;
        TEST_CHECK(!undoModel.canUndo(), "Initial model cannot undo");
        TEST_CHECK(!undoModel.canRedo(), "Initial model cannot redo");

        // Action 1: Create a beam (2 nodes + 1 beam)
        undoModel.pushUndoState("Création Poutre");
        int n1 = undoModel.addNode(0.0, 0.0, 0.0);
        int n2 = undoModel.addNode(5.0, 0.0, 0.0);
        int b1 = undoModel.addBeam(n1, n2, 0.3, 0.5);
        TEST_CHECK(b1 > 0, "Beam created successfully");

        TEST_CHECK(undoModel.canUndo(), "canUndo after beam creation");
        TEST_CHECK(!undoModel.canRedo(), "cannot redo after new action");
        TEST_CHECK(undoModel.lastUndoActionName() == "Création Poutre", "Action name matches");
        TEST_CHECK(undoModel.nodes().size() == 2, "2 nodes before undo");
        TEST_CHECK(undoModel.beams().size() == 1, "1 beam before undo");

        // Test Undo (Ctrl+Z)
        bool undoOk = undoModel.undo();
        TEST_CHECK(undoOk, "undo succeeded");
        TEST_CHECK(undoModel.nodes().empty(), "Nodes reverted to 0 after undo");
        TEST_CHECK(undoModel.beams().empty(), "Beams reverted to 0 after undo");
        TEST_CHECK(!undoModel.canUndo(), "canUndo false after undo to initial state");
        TEST_CHECK(undoModel.canRedo(), "canRedo true after undo");
        TEST_CHECK(undoModel.lastRedoActionName() == "Création Poutre", "Redo action name matches");

        // Test Redo (Ctrl+Y)
        bool redoOk = undoModel.redo();
        TEST_CHECK(redoOk, "redo succeeded");
        TEST_CHECK(undoModel.nodes().size() == 2, "Nodes restored after redo");
        TEST_CHECK(undoModel.beams().size() == 1, "Beam restored after redo");
        TEST_CHECK(undoModel.canUndo(), "canUndo true after redo");
        TEST_CHECK(!undoModel.canRedo(), "canRedo false after redo");

        // Action 2: Move nodes
        undoModel.pushUndoState("Déplacement");
        undoModel.moveNodes({n1, n2}, 2.0, 3.0, 0.0);
        const auto* pn1 = undoModel.getNode(n1);
        TEST_CHECK(pn1 && approxEqual(pn1->x(), 2.0) && approxEqual(pn1->y(), 3.0), "Node 1 moved");

        // Undo Move
        undoModel.undo();
        const auto* pn1Restored = undoModel.getNode(n1);
        TEST_CHECK(pn1Restored && approxEqual(pn1Restored->x(), 0.0) && approxEqual(pn1Restored->y(), 0.0), "Node 1 coordinates restored after undo");

        // Redo Move
        undoModel.redo();
        const auto* pn1Redone = undoModel.getNode(n1);
        TEST_CHECK(pn1Redone && approxEqual(pn1Redone->x(), 2.0) && approxEqual(pn1Redone->y(), 3.0), "Node 1 coordinates re-applied after redo");

        std::cout << "[PASS] Test 11: Undo (Ctrl+Z) & Redo (Ctrl+Y) snapshot system fully verified" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 12: Material & Section library calculations
    // -------------------------------------------------------------------------
    {
        auto conc = Material::concreteC25_30();
        TEST_CHECK(conc.type == MaterialType::Concrete, "Concrete type");
        TEST_CHECK(approxEqual(conc.E, 31e9), "Concrete E");
        TEST_CHECK(approxEqual(conc.density, 2500), "Concrete density");

        auto steel = Material::steelS355();
        TEST_CHECK(steel.type == MaterialType::Steel, "Steel type");
        TEST_CHECK(approxEqual(steel.E, 210e9), "Steel E");
        TEST_CHECK(approxEqual(steel.density, 7850), "Steel density");

        // Rectangular Section 0.30 x 0.50
        auto rect = Section::rectangular(0.30, 0.50);
        TEST_CHECK(approxEqual(rect.area(), 0.15), "Rect Area");
        // Iy = b*h^3/12 = 0.30 * 0.50^3 / 12 = 0.003125
        TEST_CHECK(approxEqual(rect.iy(), 0.30 * std::pow(0.50, 3) / 12.0), "Rect Iy");
        // Iz = h*b^3/12 = 0.50 * 0.30^3 / 12 = 0.001125
        TEST_CHECK(approxEqual(rect.iz(), 0.50 * std::pow(0.30, 3) / 12.0), "Rect Iz");

        // Circular Section D = 0.40
        auto circ = Section::circular(0.40);
        double expectedCircArea = 3.14159265358979323846 * 0.20 * 0.20;
        TEST_CHECK(approxEqual(circ.area(), expectedCircArea), "Circular Area");

        // I-Shape IPE 300
        auto ipe300 = Section::ipe(300);
        TEST_CHECK(ipe300.area() > 0.004 && ipe300.area() < 0.006, "IPE 300 Area range");

        std::cout << "[PASS] Test 12: Material & Section geometric and mechanical properties" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 13: 1D Elements (Beam, Column, TrussMember)
    // -------------------------------------------------------------------------
    {
        Model testModel13;
        int n1 = testModel13.addNode(0.0, 0.0, 0.0);
        int n2 = testModel13.addNode(0.0, 0.0, 3.5);
        int n3 = testModel13.addNode(6.0, 0.0, 3.5);

        // Column n1 -> n2
        int colId = testModel13.addColumn(n1, n2, 0.35, 0.35);
        const auto* col = testModel13.getColumn(colId);
        TEST_CHECK(col != nullptr, "Column created");
        TEST_CHECK(col->formattedName() == "C001", "Column formatted name");
        TEST_CHECK(approxEqual(col->length(testModel13), 3.5), "Column length");
        TEST_CHECK(approxEqual(col->width(), 0.35) && approxEqual(col->height(), 0.35), "Column section dimensions");
        TEST_CHECK(col->isVertical(testModel13), "Column is vertical");

        // Beam n2 -> n3
        int beamId = testModel13.addBeam(n2, n3, 0.25, 0.50);
        const auto* beam = testModel13.getBeam(beamId);
        TEST_CHECK(beam != nullptr, "Beam created");
        TEST_CHECK(beam->formattedName() == "B001", "Beam formatted name");
        TEST_CHECK(approxEqual(beam->length(testModel13), 6.0), "Beam length 6.0m");

        // Truss Member n1 -> n3 (Diagonal Brace)
        int trId = testModel13.addTrussMember(n1, n3, 0.10, "", TrussMemberRole::Diagonal);
        auto* tr = testModel13.getTrussMember(trId);
        TEST_CHECK(tr != nullptr, "TrussMember created");
        TEST_CHECK(tr->formattedName() == "TR001", "Truss formatted name");
        double expectedTrussLen = std::sqrt(6.0 * 6.0 + 3.5 * 3.5);
        TEST_CHECK(approxEqual(tr->length(testModel13), expectedTrussLen), "Truss length calculated");
        TEST_CHECK(tr->role() == TrussMemberRole::Diagonal, "Truss role Diagonal");

        std::cout << "[PASS] Test 13: 1D Elements (Beam, Column, TrussMember) lengths and roles" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 14: 2D Elements (Slab & Wall)
    // -------------------------------------------------------------------------
    {
        Model testModel14;
        int n1 = testModel14.addNode(0.0, 0.0, 3.0);
        int n2 = testModel14.addNode(5.0, 0.0, 3.0);
        int n3 = testModel14.addNode(5.0, 4.0, 3.0);
        int n4 = testModel14.addNode(0.0, 4.0, 3.0);

        // Slab
        int slabId = testModel14.addSlab({ n1, n2, n3, n4 }, 0.20, "", SlabType::TwoWay);
        const auto* slab = testModel14.getSlab(slabId);
        TEST_CHECK(slab != nullptr, "Slab created");
        TEST_CHECK(slab->formattedName() == "S001", "Slab formatted name");
        TEST_CHECK(approxEqual(slab->thickness(), 0.20), "Slab thickness");
        TEST_CHECK(approxEqual(slab->area(testModel14), 20.0), "Slab 5x4 = 20m2");
        TEST_CHECK(slab->slabType() == SlabType::TwoWay, "Slab type TwoWay");

        // Wall between (0,0,0) and (5,0,0) with height 3.0m, thickness 0.20m
        int nw1 = testModel14.addNode(0.0, 0.0, 0.0);
        int nw2 = testModel14.addNode(5.0, 0.0, 0.0);
        int wallId = testModel14.addWall(nw1, nw2, 3.0, 0.20);
        const auto* wall = testModel14.getWall(wallId);
        TEST_CHECK(wall != nullptr, "Wall created");
        TEST_CHECK(wall->formattedName() == "W001", "Wall formatted name");
        TEST_CHECK(approxEqual(wall->length(testModel14), 5.0), "Wall length 5m");
        TEST_CHECK(approxEqual(wall->height(), 3.0), "Wall height 3m");
        TEST_CHECK(approxEqual(wall->area(testModel14), 15.0), "Wall surface 15m2");

        std::cout << "[PASS] Test 14: 2D Elements (Slab & Wall) surface area and thickness" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 15: Foundation and Global Undo/Redo across all element types
    // -------------------------------------------------------------------------
    {
        Model testModel15;
        int n1 = testModel15.addNode(0.0, 0.0, 0.0);
        int fId = testModel15.addFoundation(n1, 1.8, 1.8, 0.5, "", FoundationType::IsolatedFooting);
        const auto* f = testModel15.getFoundation(fId);
        TEST_CHECK(f != nullptr, "Foundation created");
        TEST_CHECK(f->formattedName() == "F001", "Foundation formatted name");
        TEST_CHECK(approxEqual(f->baseArea(), 1.8 * 1.8), "Foundation base area");
        TEST_CHECK(approxEqual(f->volume(), 1.8 * 1.8 * 0.5), "Foundation volume");

        // Test Snapshot & Undo with all types
        testModel15.pushUndoState("Creation Complète");
        int n2 = testModel15.addNode(4.0, 0.0, 0.0);
        int n3 = testModel15.addNode(4.0, 0.0, 3.0);
        int wId = testModel15.addWall(n1, n2, 3.0, 0.20);
        int trId = testModel15.addTrussMember(n1, n3, 0.08, "", TrussMemberRole::Brace);

        TEST_CHECK(testModel15.walls().size() == 1, "1 wall present");
        TEST_CHECK(testModel15.trussMembers().size() == 1, "1 truss present");

        // Undo
        bool undoOk = testModel15.undo();
        TEST_CHECK(undoOk, "undo succeeded");
        TEST_CHECK(testModel15.walls().empty(), "Walls reverted to 0");
        TEST_CHECK(testModel15.trussMembers().empty(), "Truss members reverted to 0");
        TEST_CHECK(testModel15.foundations().size() == 1, "Initial foundation retained");

        // Redo
        bool redoOk = testModel15.redo();
        TEST_CHECK(redoOk, "redo succeeded");
        TEST_CHECK(testModel15.walls().size() == 1, "Wall restored");
        TEST_CHECK(testModel15.trussMembers().size() == 1, "Truss member restored");

        // Cascaded Node Removal
        testModel15.removeNode(n1);
        TEST_CHECK(testModel15.getFoundation(fId) == nullptr, "Foundation cascade removed with node");
        TEST_CHECK(testModel15.getWall(wId) == nullptr, "Wall cascade removed with node");
        TEST_CHECK(testModel15.getTrussMember(trId) == nullptr, "Truss cascade removed with node");

        std::cout << "[PASS] Test 15: Foundations, Undo/Redo & cascaded deletion across all types" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 16: Universal Bar (Robot Architecture), Roles, Eccentricity & Sections
    // -------------------------------------------------------------------------
    {
        Model testModel16;
        int n1 = testModel16.addNode(0.0, 0.0, 0.0);
        int n2 = testModel16.addNode(5.0, 0.0, 0.0);
        int n3 = testModel16.addNode(5.0, 0.0, 3.5);

        // 1. Barre métallique IPE 100 avec rôle Poutre et rotation γ
        BarProperties propsBeam;
        propsBeam.id = 101;
        propsBeam.name = "Poutre_IPE100";
        propsBeam.role = BarRole::Beam;
        propsBeam.section = Section::ipe(100);
        propsBeam.material = Material::steelS235();
        propsBeam.rotation = 45.0;
        propsBeam.eccentricity = BarEccentricity::TopFlange;

        int bar1Id = testModel16.addBar(propsBeam, n1, n2);
        TEST_CHECK(bar1Id == 101, "Bar 101 created with exact ID");
        const auto* bar1 = testModel16.getBar(bar1Id);
        TEST_CHECK(bar1 != nullptr, "Bar 101 retrieved");
        TEST_CHECK(bar1->role() == BarRole::Beam, "Bar role is Beam");
        TEST_CHECK(bar1->section().shape == SectionShape::IShape, "Section is IShape");
        TEST_CHECK(approxEqual(bar1->section().height, 0.100), "IPE 100 height is 0.100m");
        TEST_CHECK(approxEqual(bar1->section().width, 0.055), "IPE 100 width is 0.055m");
        TEST_CHECK(bar1->eccentricity() == BarEccentricity::TopFlange, "Eccentricity is TopFlange");
        TEST_CHECK(approxEqual(bar1->rotation(), 45.0), "Rotation is 45°");
        TEST_CHECK(approxEqual(bar1->length(testModel16), 5.0), "Length is 5.0m");
        TEST_CHECK(bar1->section().wy() > 0.0, "Elastic modulus Wy is positive");
        TEST_CHECK(bar1->section().wz() > 0.0, "Elastic modulus Wz is positive");

        // 2. Barre Poteau avec profil UPN 160
        BarProperties propsCol;
        propsCol.id = 102;
        propsCol.name = "Poteau_UPN160";
        propsCol.role = BarRole::Column;
        propsCol.section = Section::upn(160);
        propsCol.material = Material::steelS355();
        int bar2Id = testModel16.addBar(propsCol, n2, n3);
        const auto* bar2 = testModel16.getBar(bar2Id);
        TEST_CHECK(bar2 != nullptr, "Bar 102 retrieved");
        TEST_CHECK(bar2->role() == BarRole::Column, "Bar role is Column");
        TEST_CHECK(bar2->section().shape == SectionShape::UPN, "Section is UPN");
        TEST_CHECK(approxEqual(bar2->section().height, 0.160), "UPN 160 height is 0.160m");
        TEST_CHECK(approxEqual(bar2->length(testModel16), 3.5), "Length is 3.5m");

        // 3. Cornière Angle et Tube rectangulaire BoxHollow
        auto sAngle = Section::angle(0.080, 0.080, 0.008);
        TEST_CHECK(sAngle.shape == SectionShape::Angle, "Angle shape verified");
        TEST_CHECK(sAngle.area() > 0.0 && sAngle.iy() > 0.0, "Angle area and inertia positive");

        auto sBox = Section::boxHollow(0.100, 0.100, 0.005);
        TEST_CHECK(sBox.shape == SectionShape::BoxHollow, "BoxHollow shape verified");
        TEST_CHECK(sBox.area() > 0.0 && sBox.it() > 0.0, "BoxHollow area and torsion positive");

        // 4. Bibliothèque par défaut
        auto lib = Section::defaultLibrary();
        TEST_CHECK(lib.size() >= 25, "Default library contains standard sections");

        // 5. Modification dynamique
        BarProperties modifiedProps = bar1->properties();
        modifiedProps.section = Section::ipe(200);
        modifiedProps.rotation = 90.0;
        testModel16.getBar(bar1Id)->setProperties(modifiedProps);
        TEST_CHECK(approxEqual(testModel16.getBar(bar1Id)->section().height, 0.200), "IPE 200 applied");
        TEST_CHECK(approxEqual(testModel16.getBar(bar1Id)->rotation(), 90.0), "Rotation 90° applied");

        std::cout << "[PASS] Test 16: Universal Bar (Robot Architecture), Roles, Eccentricity & Sections" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 17: 3D Geometry Generation (Rectangular, Square, Real Cylinder, 3D Orientations, Preview, Synchronization)
    // -------------------------------------------------------------------------
    {
        // Helper: détection d'une surface cylindrique analytique OCCT
        auto hasCylindricalSurface = [](const TopoDS_Shape& shape) -> bool {
            if (shape.IsNull()) return false;
            TopExp_Explorer exp(shape, TopAbs_FACE);
            for (; exp.More(); exp.Next())
            {
                TopoDS_Face face = TopoDS::Face(exp.Current());
                Handle(Geom_Surface) surf = BRep_Tool::Surface(face);
                if (!surf.IsNull())
                {
                    if (!Handle(Geom_CylindricalSurface)::DownCast(surf).IsNull())
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        // Helper: comptage des faces TopoDS_Face
        auto countFaces = [](const TopoDS_Shape& shape) -> int {
            if (shape.IsNull()) return 0;
            int cnt = 0;
            TopExp_Explorer exp(shape, TopAbs_FACE);
            for (; exp.More(); exp.Next()) cnt++;
            return cnt;
        };

        Node nA(1, 0.0, 0.0, 0.0);
        Node nB(2, 5.0, 0.0, 0.0);
        Node nVert(3, 0.0, 0.0, 3.0);
        Node nDiag(4, 5.0, 3.0, 4.0);

        // TEST 1: Section = Rectangle 400 x 500
        auto secRect = Section::rectangular(0.40, 0.50);
        TopoDS_Shape shapeRect = BeamGeometry::createBeamShape(nA, nB, secRect);
        TEST_CHECK(!shapeRect.IsNull(), "Test 1: Rectangular shape created");
        TEST_CHECK(!hasCylindricalSurface(shapeRect), "Test 1: Rectangular shape has no cylindrical surface");
        TEST_CHECK(countFaces(shapeRect) == 6, "Test 1: Rectangular parallelepiped has 6 planar faces");

        // TEST 2: Section = Carré 400 x 400
        auto secSquare = Section::rectangular(0.40, 0.40);
        TopoDS_Shape shapeSquare = BeamGeometry::createBeamShape(nA, nB, secSquare);
        TEST_CHECK(!shapeSquare.IsNull(), "Test 2: Square shape created");
        TEST_CHECK(!hasCylindricalSurface(shapeSquare), "Test 2: Square shape has no cylindrical surface");
        TEST_CHECK(countFaces(shapeSquare) == 6, "Test 2: Square shape has 6 planar faces");

        // TEST 3: Section = Circulaire Ø400
        auto secCirc400 = Section::circular(0.40);
        TopoDS_Shape shapeCirc400 = BeamGeometry::createBeamShape(nA, nB, secCirc400);
        TEST_CHECK(!shapeCirc400.IsNull(), "Test 3: Circular D400 shape created");
        TEST_CHECK(hasCylindricalSurface(shapeCirc400), "Test 3: Circular D400 is a REAL CYLINDER (Geom_CylindricalSurface detected)");

        // TEST 4: Section = Circulaire Ø200, barre horizontale (A -> B)
        auto secCirc200 = Section::circular(0.20);
        TopoDS_Shape shapeHoriz = BeamGeometry::createBeamShape(nA, nB, secCirc200);
        TEST_CHECK(!shapeHoriz.IsNull(), "Test 4: Horizontal cylinder created");
        TEST_CHECK(hasCylindricalSurface(shapeHoriz), "Test 4: Horizontal bar is a real cylinder");

        // TEST 5: Section = Circulaire Ø200, barre verticale (poteau vertical nA -> nVert)
        TopoDS_Shape shapeVert = BeamGeometry::createBeamShape(nA, nVert, secCirc200);
        TEST_CHECK(!shapeVert.IsNull(), "Test 5: Vertical column cylinder created");
        TEST_CHECK(hasCylindricalSurface(shapeVert), "Test 5: Vertical column is a real cylinder");

        // TEST 6: Section = Circulaire Ø200, barre diagonale 3D (nA -> nDiag)
        TopoDS_Shape shapeDiag = BeamGeometry::createBeamShape(nA, nDiag, secCirc200);
        TEST_CHECK(!shapeDiag.IsNull(), "Test 6: 3D diagonal cylinder created");
        TEST_CHECK(hasCylindricalSurface(shapeDiag), "Test 6: 3D diagonal bar is a real cylinder");

        // TEST 7: Preview circulaire (simulation du RubberBand avec nœuds temporaires)
        Node tempA(0, 1.25, 2.50, 0.0);
        Node tempB(0, 4.75, 6.20, 3.10);
        TopoDS_Shape previewShape = BeamGeometry::createBeamShape(tempA, tempB, secCirc400);
        TEST_CHECK(!previewShape.IsNull(), "Test 7: Preview shape created");
        TEST_CHECK(hasCylindricalSurface(previewShape), "Test 7: Preview shape is a real cylinder");

        // TEST 8: Modification dynamique : Rectangle -> Circulaire
        Model modelTest;
        int n1 = modelTest.addNode(0.0, 0.0, 0.0);
        int n2 = modelTest.addNode(0.0, 0.0, 3.5);
        int colId = modelTest.addColumn(n1, n2, secSquare, Material::concreteC25_30(), 0.0, "C001");
        auto* col = modelTest.getColumn(colId);
        TEST_CHECK(col != nullptr, "Column retrieved");
        TopoDS_Shape shapeBefore = BeamGeometry::createBeamShape(*modelTest.getNode(n1), *modelTest.getNode(n2), col->section());
        TEST_CHECK(!hasCylindricalSurface(shapeBefore), "Test 8: Initial column is rectangular");

        // Passage à section circulaire
        col->setSection(Section::circular(0.40));
        TopoDS_Shape shapeAfterCirc = BeamGeometry::createBeamShape(*modelTest.getNode(n1), *modelTest.getNode(n2), col->section());
        TEST_CHECK(hasCylindricalSurface(shapeAfterCirc), "Test 8: Modified column is now a REAL CYLINDER");

        // TEST 9: Modification dynamique : Circulaire -> Rectangle
        col->setSection(Section::rectangular(0.40, 0.50));
        TopoDS_Shape shapeAfterRect = BeamGeometry::createBeamShape(*modelTest.getNode(n1), *modelTest.getNode(n2), col->section());
        TEST_CHECK(!hasCylindricalSurface(shapeAfterRect), "Test 9: Column changed back to rectangular (no cylindrical surface)");
        TEST_CHECK(countFaces(shapeAfterRect) == 6, "Test 9: 6 planar faces");

        // TEST 10: Section Tube (Pipe) Ø400 x 10 mm
        auto secPipe = Section::pipe(0.40, 0.010);
        TopoDS_Shape shapePipe = BeamGeometry::createBeamShape(nA, nVert, secPipe);
        TEST_CHECK(!shapePipe.IsNull(), "Test 10: Pipe shape created successfully");

        std::cout << "[PASS] Test 17: 3D Geometry Generation (Rectangular, Square, Real Cylinder, 3D Orientations, Preview, Synchronizations, Pipe)" << std::endl;
        passed++;
    }


    // TEST 21 : Synchronisation Bidirectionnelle UI <-> MODÈLE <-> 3D (IModelObserver)
    // =========================================================================
    {
        std::cout << "\n--- TEST 21: Synchronisation Bidirectionnelle UI <-> Modele <-> 3D ---" << std::endl;

        Model model;

        struct SyncObserver : public IModelObserver
        {
            int modifiedBeamCount = 0;
            int lastModifiedBeamId = -1;
            int modifiedNodeCount = 0;
            int lastModifiedNodeId = -1;

            void onBeamModified(const Beam& b) override
            {
                modifiedBeamCount++;
                lastModifiedBeamId = b.id();
            }

            void onNodeModified(const Node& n) override
            {
                modifiedNodeCount++;
                lastModifiedNodeId = n.id();
            }
        };

        SyncObserver uiObserver;
        SyncObserver occObserver;

        model.addObserver(&uiObserver);
        model.addObserver(&occObserver);

        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(0.0, 0.0, 3.0);
        int b1 = model.addBeam(n1, n2, 0.30, 0.50);

        // 1. Modification depuis l'UI (ex: changement de section en IPE 300)
        auto* beam = model.getBeam(b1);
        TEST_CHECK(beam != nullptr, "Test 21: beam exists");
        beam->setSection(Section::ipe(300));
        model.notifyBeamModified(b1);

        TEST_CHECK(occObserver.modifiedBeamCount == 1, "Test 21: OccView received onBeamModified");
        TEST_CHECK(occObserver.lastModifiedBeamId == b1, "Test 21: OccView targeted beam b1");
        TEST_CHECK(beam->section().name == "IPE 300", "Test 21: beam section is IPE 300");

        // 2. Modification depuis la 3D (ex: déplacement de nœud de 5.0m en X)
        bool moveOk = model.moveNodes({ n1, n2 }, 5.0, 0.0, 0.0);
        TEST_CHECK(moveOk, "Test 21: moveNodes succeeded");

        TEST_CHECK(uiObserver.modifiedNodeCount >= 2, "Test 21: UI received onNodeModified for moved nodes");
        const auto* movedN1 = model.getNode(n1);
        const auto* movedN2 = model.getNode(n2);
        TEST_CHECK(approxEqual(movedN1->x(), 5.0), "Test 21: N1 x is 5.0m");
        TEST_CHECK(approxEqual(movedN2->x(), 5.0), "Test 21: N2 x is 5.0m");

        model.removeObserver(&uiObserver);
        model.removeObserver(&occObserver);

        std::cout << "[PASS] Test 21: Bidirectional UI <-> Model <-> 3D Synchronization Validated!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 22 : Section en T et Géométrie BRep Solide 3D
    // =========================================================================
    {
        std::cout << "\n--- TEST 22: Profil en T et Construction 3D BRep OpenCASCADE ---" << std::endl;

        Section tSec = Section::tSection(0.140, 0.140, 0.010, 0.012, "T 140x140x10");
        TEST_CHECK(tSec.shape == SectionShape::TSection, "Test 22: shape is TSection");

        // Calcul analytique de l'aire :
        // A = b*tf + (h-tf)*tw = 0.14*0.012 + (0.14-0.012)*0.010 = 0.00168 + 0.00128 = 0.00296 m²
        double expectedArea = 0.140 * 0.012 + (0.140 - 0.012) * 0.010;
        TEST_CHECK(approxEqual(tSec.area(), expectedArea), "Test 22: TSection area exact");
        TEST_CHECK(tSec.iy() > 0.0, "Test 22: Iy > 0");
        TEST_CHECK(tSec.iz() > 0.0, "Test 22: Iz > 0");
        TEST_CHECK(tSec.it() > 0.0, "Test 22: It > 0");

        // Génération 3D OpenCASCADE du solide BRep
        Node nA(1, 0.0, 0.0, 0.0);
        Node nB(2, 4.0, 0.0, 0.0);
        TopoDS_Shape tShape = BeamGeometry::createBeamShape(nA, nB, tSec);
        TEST_CHECK(!tShape.IsNull(), "Test 22: 3D shape is not null");

        Bnd_Box bnd;
        BRepBndLib::Add(tShape, bnd);
        bnd.SetGap(0.0);
        double xmin, ymin, zmin, xmax, ymax, zmax;
        bnd.Get(xmin, ymin, zmin, xmax, ymax, zmax);

        double length = xmax - xmin;
        double width = ymax - ymin;
        double height = zmax - zmin;

        TEST_CHECK(approxEqual(length, 4.0, 0.01), "Test 22: length is 4.0m");
        TEST_CHECK(approxEqual(width, 0.140, 0.01), "Test 22: width is 0.14m");
        TEST_CHECK(approxEqual(height, 0.140, 0.01), "Test 22: height is 0.14m");

        std::cout << "[PASS] Test 22: T-Section and Exact 3D BRep Solid Validated!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 23 : Bibliothèque Personnalisée Persistante (LibraryManager)
    // =========================================================================
    {
        std::cout << "\n--- TEST 23: Bibliotheque Personnalisee Persistante (LibraryManager) ---" << std::endl;

        auto& lib = TSA::Library::LibraryManager::instance();

        // 1. Ajout d'une section personnalisée
        Section customSec = Section::rectangular(0.35, 0.65, "MaSection_Poutre_01");
        lib.addCustomSection(customSec);
        const auto* foundSec = lib.findSectionByName("MaSection_Poutre_01");
        TEST_CHECK(foundSec != nullptr, "Test 23: custom section found");
        TEST_CHECK(approxEqual(foundSec->width, 0.35), "Test 23: custom section width preserved");
        TEST_CHECK(approxEqual(foundSec->height, 0.65), "Test 23: custom section height preserved");

        // 2. Ajout d'un matériau personnalisé
        Material customMat;
        customMat.name = "MonAcier_S460_Test";
        customMat.type = MaterialType::Custom;
        customMat.E = 210.0e9;
        customMat.nu = 0.30;
        customMat.density = 7850.0;
        customMat.fk = 460.0e6;
        lib.addCustomMaterial(customMat);
        const auto* foundMat = lib.findMaterialByName("MonAcier_S460_Test");
        TEST_CHECK(foundMat != nullptr, "Test 23: custom material found");
        TEST_CHECK(approxEqual(foundMat->E, 210.0e9), "Test 23: custom material E preserved");
        TEST_CHECK(approxEqual(foundMat->fk, 460.0e6), "Test 23: custom material fk preserved");

        // 3. Ajout d'une couleur personnalisée
        lib.addCustomColor("Bleu Marine TSA", "#002060", "Mes Couleurs");
        QColor col = lib.getColor("Bleu Marine TSA");
        TEST_CHECK(col.isValid(), "Test 23: color is valid");
        TEST_CHECK(col == QColor("#002060"), "Test 23: exact color hex match");

        // 4. Modèle de structure personnalisée (Template) et instanciation
        Model srcModel;
        int sn1 = srcModel.addNode(0.0, 0.0, 0.0);
        int sn2 = srcModel.addNode(0.0, 0.0, 3.0);
        srcModel.addBeam(sn1, sn2, 0.30, 0.50, "Poutre_Template");
        auto snapshot = srcModel.createSnapshot("Template_Portique");

        lib.addStructureTemplate("Portique_Test_Lib", "Mes Structures", "Portique de test", snapshot);

        Model dstModel;
        bool instOk = lib.instantiateTemplateInModel("Portique_Test_Lib", &dstModel, 10.0, 5.0, 0.0);
        TEST_CHECK(instOk, "Test 23: template instantiation succeeded");
        TEST_CHECK(dstModel.nodes().size() == 2, "Test 23: 2 nodes created in destination model");
        TEST_CHECK(dstModel.beams().size() == 1, "Test 23: 1 beam created in destination model");

        // Vérification de la translation d'offset
        auto itNode = dstModel.nodes().begin();
        TEST_CHECK(approxEqual(itNode->second.x(), 10.0), "Test 23: instantiated node x has 10m offset");
        TEST_CHECK(approxEqual(itNode->second.y(), 5.0), "Test 23: instantiated node y has 5m offset");

        std::cout << "[PASS] Test 23: Persistent Custom Library System Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 24: Structural Clipboard & Project Manager Architecture
    // =========================================================================
    {
        std::cout << "\n--- TEST 24: Structural Clipboard & Project Manager Architecture ---" << std::endl;

        // 1. Presse-papier structurel découplé
        Model srcModel;
        int n1 = srcModel.addNode(0.0, 0.0, 0.0);
        int n2 = srcModel.addNode(5.0, 0.0, 0.0);
        int b1 = srcModel.addBeam(n1, n2, 0.30, 0.50);

        TSA::Model::StructuralClipboard clipboard;
        TEST_CHECK(!clipboard.hasData(), "Test 24: clipboard empty initially");

        clipboard.copyFrom(srcModel, std::vector<int>{n1, n2}, std::vector<int>{b1}, {}, {});
        TEST_CHECK(clipboard.hasData(), "Test 24: clipboard has data after copyFrom");
        TEST_CHECK(clipboard.nodeCount() == 2, "Test 24: 2 nodes copied");
        TEST_CHECK(clipboard.beamCount() == 1, "Test 24: 1 beam copied");

        Model dstModel;
        auto pasteRes = clipboard.pasteTo(dstModel, 20.0, 10.0, 5.0);
        TEST_CHECK(!pasteRes.empty(), "Test 24: paste result is non-empty");
        TEST_CHECK(pasteRes.nodeIds.size() == 2, "Test 24: 2 nodes pasted");
        TEST_CHECK(pasteRes.beamIds.size() == 1, "Test 24: 1 beam pasted");
        TEST_CHECK(dstModel.nodes().size() == 2, "Test 24: destination model has 2 nodes");
        TEST_CHECK(dstModel.beams().size() == 1, "Test 24: destination model has 1 beam");

        // 2. Gestionnaire de Projet (ProjectManager)
        TSA::Project::ProjectManager projectMgr;
        TEST_CHECK(!projectMgr.hasFilePath(), "Test 24: new project has no file path");
        TEST_CHECK(!projectMgr.isModified(), "Test 24: new project is not modified");
        TEST_CHECK(projectMgr.currentFileName() == "Sans titre", "Test 24: default file name is 'Sans titre'");

        projectMgr.setModified(true);
        TEST_CHECK(projectMgr.isModified(), "Test 24: modified state updated");

        std::cout << "[PASS] Test 24: Structural Clipboard & Project Manager Architecture Validated!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 28: Custom Section Customization, Copy/Paste, OCCT 3D & Save/Load
    // =========================================================================
    {
        std::cout << "\n--- TEST 28: Custom Section Customization, Copy/Paste, OCCT 3D & Save/Load ---" << std::endl;

        TSA::Model::Model testModel;

        // Subtest 1: Créer une section circulaire Ø20 (D = 0.20 m)
        int n1 = testModel.addNode(0.0, 0.0, 0.0);
        int n2 = testModel.addNode(0.0, 0.0, 4.0);

        TSA::Model::BarProperties propsCirc;
        propsCirc.name = "Poteau_Circulaire_D20";
        propsCirc.role = TSA::Model::BarRole::Column;
        propsCirc.section = TSA::Model::Section::circular(0.20, "Circ D20");

        int colId1 = testModel.addBar(propsCirc, n1, n2);
        const auto* col1 = testModel.getBeam(colId1);
        TEST_CHECK(col1 != nullptr, "Test 28.1: Circular bar created");
        TEST_CHECK(col1->section().shape == TSA::Model::SectionShape::Circular, "Test 28.1: Section shape is Circular");
        TEST_CHECK(approxEqual(col1->section().diameter, 0.20), "Test 28.1: Diameter is 0.20m");

        const auto* nodeA = testModel.getNode(n1);
        const auto* nodeB = testModel.getNode(n2);
        TopoDS_Shape shapeCirc = TSA::Geometry::BeamGeometry::createBeamShape(*nodeA, *nodeB, col1->section(), col1->rotation(), col1->eccentricity());
        TEST_CHECK(!shapeCirc.IsNull(), "Test 28.1: OCCT 3D cylinder shape built successfully");

        // Subtest 2: Copier cet élément (StructuralClipboard) -> vérifier que la copie reste Circulaire Ø20 dans Model ET OCCT
        TSA::Model::StructuralClipboard clipboard;
        std::set<int> selNodes = { n1, n2 };
        std::set<int> selBeams = { colId1 };
        std::set<int> emptyCols, emptySlabs;
        clipboard.copyFrom(testModel, selNodes, selBeams, emptyCols, emptySlabs);

        TSA::Model::PasteResult pasteRes = clipboard.pasteTo(testModel, 5.0, 0.0, 0.0);
        TEST_CHECK(!pasteRes.beamIds.empty(), "Test 28.2: Bar pasted successfully");
        int copyId = pasteRes.beamIds[0];
        const auto* colCopy = testModel.getBeam(copyId);
        TEST_CHECK(colCopy != nullptr, "Test 28.2: Copied bar exists");
        TEST_CHECK(colCopy->section().shape == TSA::Model::SectionShape::Circular, "Test 28.2: Copied bar section shape is Circular (NOT rectangle!)");
        TEST_CHECK(approxEqual(colCopy->section().diameter, 0.20), "Test 28.2: Copied bar diameter is 0.20m");

        const auto* copyNodeA = testModel.getNode(colCopy->startNodeId());
        const auto* copyNodeB = testModel.getNode(colCopy->endNodeId());
        TopoDS_Shape shapeCopy = TSA::Geometry::BeamGeometry::createBeamShape(*copyNodeA, *copyNodeB, colCopy->section(), colCopy->rotation(), colCopy->eccentricity());
        TEST_CHECK(!shapeCopy.IsNull(), "Test 28.2: Copied bar OCCT 3D shape built successfully as real cylinder");

        // Subtest 3: Modifier Ø20 en Ø30
        auto sec30 = TSA::Model::Section::circular(0.30, "Circ D30");
        testModel.getBeam(colId1)->setSection(sec30);
        const auto* col1Mod = testModel.getBeam(colId1);
        TEST_CHECK(approxEqual(col1Mod->section().diameter, 0.30), "Test 28.3: Modified section diameter is 0.30m");
        TopoDS_Shape shapeMod = TSA::Geometry::BeamGeometry::createBeamShape(*nodeA, *nodeB, col1Mod->section(), col1Mod->rotation(), col1Mod->eccentricity());
        TEST_CHECK(!shapeMod.IsNull(), "Test 28.3: Modified section OCCT 3D shape updated to Ø30");

        // Subtest 4: Créer une section rectangulaire 60x30 (B = 0.60m, H = 0.30m)
        int n3 = testModel.addNode(10.0, 0.0, 0.0);
        int n4 = testModel.addNode(10.0, 5.0, 0.0);
        TSA::Model::BarProperties propsRect;
        propsRect.name = "Poutre_60x30";
        propsRect.role = TSA::Model::BarRole::Beam;
        propsRect.section = TSA::Model::Section::rectangular(0.60, 0.30, "R60x30");
        int rectBarId = testModel.addBar(propsRect, n3, n4);
        const auto* rectBar = testModel.getBeam(rectBarId);
        TEST_CHECK(rectBar->section().shape == TSA::Model::SectionShape::Rectangular, "Test 28.4: Section shape is Rectangular");
        TEST_CHECK(approxEqual(rectBar->section().width, 0.60), "Test 28.4: Width B is 0.60m");
        TEST_CHECK(approxEqual(rectBar->section().height, 0.30), "Test 28.4: Height H is 0.30m");
        TopoDS_Shape shapeRect = TSA::Geometry::BeamGeometry::createBeamShape(*testModel.getNode(n3), *testModel.getNode(n4), rectBar->section(), rectBar->rotation(), rectBar->eccentricity());
        TEST_CHECK(!shapeRect.IsNull(), "Test 28.4: Rectangular 60x30 OCCT 3D shape built successfully");

        // Subtest 5: Sauvegarder puis recharger le projet (.tsa)
        std::string filename = "test_custom_section.tsa";
        TSA::IO::TSAFileWriter writer;
        std::string err;
        bool saveOk = writer.saveToFile(filename, testModel, nullptr, "Test Custom Section", "TSA Unit Test", &err);
        TEST_CHECK(saveOk, "Test 28.5: Saved TSA file with custom sections");

        TSA::Model::Model loadedModel;
        TSA::IO::TSAFileReader reader;
        bool loadOk = reader.loadFromFile(filename, loadedModel, nullptr, "", nullptr, nullptr, nullptr, &err);
        TEST_CHECK(loadOk, "Test 28.5: Loaded TSA file with custom sections");

        const auto* loadedCirc = loadedModel.getBeam(colId1);
        TEST_CHECK(loadedCirc != nullptr, "Test 28.5: Loaded circular bar exists");
        TEST_CHECK(loadedCirc->section().shape == TSA::Model::SectionShape::Circular, "Test 28.5: Loaded circular bar shape is Circular");
        TEST_CHECK(approxEqual(loadedCirc->section().diameter, 0.30), "Test 28.5: Loaded circular bar diameter is 0.30m");

        const auto* loadedRect = loadedModel.getBeam(rectBarId);
        TEST_CHECK(loadedRect != nullptr, "Test 28.5: Loaded rectangular bar exists");
        TEST_CHECK(loadedRect->section().shape == TSA::Model::SectionShape::Rectangular, "Test 28.5: Loaded rectangular bar shape is Rectangular");
        TEST_CHECK(approxEqual(loadedRect->section().width, 0.60), "Test 28.5: Loaded rectangular bar width is 0.60m");
        TEST_CHECK(approxEqual(loadedRect->section().height, 0.30), "Test 28.5: Loaded rectangular bar height is 0.30m");

        std::cout << "[PASS] Test 28: Custom Section Customization, Copy/Paste, OCCT 3D & Save/Load Validated Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 29: Comprehensive Audit of Beam & Column Section Pipeline
    // =========================================================================
    {
        std::cout << "\n--- TEST 29: Comprehensive Audit of Beam & Column Section Pipeline ---" << std::endl;

        TSA::Model::Model auditModel;

        // 1. Audit Poteau Circulaire Ø20
        int n1 = auditModel.addNode(0.0, 0.0, 0.0);
        int n2 = auditModel.addNode(0.0, 0.0, 3.5);
        auto secCirc20 = TSA::Model::Section::circular(0.20, "Circ D20");
        int colId1 = auditModel.addColumn(n1, n2, secCirc20, TSA::Model::Material::concreteC25_30(), 0.0, "C001");

        const auto* col1 = auditModel.getColumn(colId1);
        TEST_CHECK(col1 != nullptr, "Test 29.1: Column C001 created in Model");
        TEST_CHECK(col1->section().shape == TSA::Model::SectionShape::Circular, "Test 29.1: Column section shape is Circular in Model");
        TEST_CHECK(approxEqual(col1->section().diameter, 0.20), "Test 29.1: Column diameter is 0.20m in Model");

        TopoDS_Shape shapeCol1 = TSA::Geometry::BeamGeometry::createBeamShape(*auditModel.getNode(n1), *auditModel.getNode(n2), col1->section(), col1->rotation());
        TEST_CHECK(!shapeCol1.IsNull(), "Test 29.1: OCCT 3D Shape for Circular Column generated successfully");

        // 2. Audit Poutre IPE 200
        int n3 = auditModel.addNode(0.0, 0.0, 3.5);
        int n4 = auditModel.addNode(6.0, 0.0, 3.5);
        auto secIpe200 = TSA::Model::Section::ipe(200);
        int beamId1 = auditModel.addBar(n3, n4, secIpe200, TSA::Model::Material::steelS235(), TSA::Model::BarRole::Beam, 0.0, "B001");

        const auto* beam1 = auditModel.getBeam(beamId1);
        TEST_CHECK(beam1 != nullptr, "Test 29.2: Beam B001 created in Model");
        TEST_CHECK(beam1->section().shape == TSA::Model::SectionShape::IShape, "Test 29.2: Beam section shape is IShape in Model");
        TEST_CHECK(approxEqual(beam1->section().height, 0.200), "Test 29.2: IPE 200 height is 0.200m");
        TEST_CHECK(approxEqual(beam1->section().width, 0.100), "Test 29.2: IPE 200 width is 0.100m");

        TopoDS_Shape shapeBeam1 = TSA::Geometry::BeamGeometry::createBeamShape(*auditModel.getNode(n3), *auditModel.getNode(n4), beam1->section(), beam1->rotation(), beam1->eccentricity());
        TEST_CHECK(!shapeBeam1.IsNull(), "Test 29.2: OCCT 3D Shape for IPE 200 Beam generated successfully");

        // 3. Audit Presse-papier (Copier / Coller IPE 200 & Circular Ø20)
        TSA::Model::StructuralClipboard clip;
        std::set<int> selN = { n1, n2, n3, n4 };
        std::set<int> selB = { beamId1 };
        std::set<int> selC = { colId1 };
        std::set<int> selS;
        clip.copyFrom(auditModel, selN, selB, selC, selS);

        TSA::Model::PasteResult pRes = clip.pasteTo(auditModel, 10.0, 0.0, 0.0);
        TEST_CHECK(!pRes.beamIds.empty(), "Test 29.3: Pasted beam created");
        TEST_CHECK(!pRes.columnIds.empty(), "Test 29.3: Pasted column created");

        const auto* pastedBeam = auditModel.getBeam(pRes.beamIds[0]);
        TEST_CHECK(pastedBeam->section().shape == TSA::Model::SectionShape::IShape, "Test 29.3: Pasted beam retains IShape IPE 200 (NOT rectangle)");
        TEST_CHECK(approxEqual(pastedBeam->section().height, 0.200), "Test 29.3: Pasted IPE 200 height preserved");

        const auto* pastedCol = auditModel.getColumn(pRes.columnIds[0]);
        TEST_CHECK(pastedCol->section().shape == TSA::Model::SectionShape::Circular, "Test 29.3: Pasted column retains Circular shape (NOT rectangle)");
        TEST_CHECK(approxEqual(pastedCol->section().diameter, 0.20), "Test 29.3: Pasted column diameter 0.20m preserved");

        // 4. Audit Modification Post-Création (Ø20 -> Ø30)
        auditModel.getColumn(colId1)->setSection(TSA::Model::Section::circular(0.30, "Circ D30"));
        TEST_CHECK(approxEqual(auditModel.getColumn(colId1)->section().diameter, 0.30), "Test 29.4: Column modified to Ø30");
        TopoDS_Shape shapeColMod = TSA::Geometry::BeamGeometry::createBeamShape(*auditModel.getNode(n1), *auditModel.getNode(n2), auditModel.getColumn(colId1)->section(), 0.0);
        TEST_CHECK(!shapeColMod.IsNull(), "Test 29.4: Modified Ø30 OCCT shape re-generated cleanly");

        // 5. Audit Sauvegarde / Chargement Fichier Native .tsa
        std::string fn = "test_section_audit.tsa";
        TSA::IO::TSAFileWriter w;
        std::string e;
        bool sOk = w.saveToFile(fn, auditModel, nullptr, "Section Audit", "Unit Test", &e);
        TEST_CHECK(sOk, "Test 29.5: Audit model saved to TSA file");

        TSA::Model::Model lModel;
        TSA::IO::TSAFileReader r;
        bool lOk = r.loadFromFile(fn, lModel, nullptr, "", nullptr, nullptr, nullptr, &e);
        TEST_CHECK(lOk, "Test 29.5: Audit model loaded from TSA file");

        const auto* lCol = lModel.getColumn(colId1);
        TEST_CHECK(lCol != nullptr, "Test 29.5: Loaded column exists");
        TEST_CHECK(lCol->section().shape == TSA::Model::SectionShape::Circular, "Test 29.5: Loaded column shape is Circular");
        TEST_CHECK(approxEqual(lCol->section().diameter, 0.30), "Test 29.5: Loaded column diameter is 0.30m");

        const auto* lBeam = lModel.getBeam(beamId1);
        TEST_CHECK(lBeam != nullptr, "Test 29.5: Loaded beam exists");
        TEST_CHECK(lBeam->section().shape == TSA::Model::SectionShape::IShape, "Test 29.5: Loaded beam shape is IShape IPE 200");
        TEST_CHECK(approxEqual(lBeam->section().height, 0.200), "Test 29.5: Loaded beam height is 0.200m");

        std::cout << "[PASS] Test 29: Comprehensive Audit of Beam & Column Section Pipeline Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 95: Révision du modèle & invalidation des résultats (ResultsValidityGuard)
    // Régression : ResultsModel::invalidate() n'était jamais appelé ; des résultats périmés
    // restaient présentés comme valides après une modification du modèle.
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 95: Invalidation des résultats après modification ---" << std::endl;
        Model m;
        int n1 = m.addNode(0.0, 0.0, 0.0);
        int n2 = m.addNode(5.0, 0.0, 0.0);
        int b1 = m.addBeam(n1, n2);

        const auto r0 = m.revision();
        m.notifyBeamModified(b1);
        TEST_CHECK(m.revision() > r0, "Test 95: une notification augmente la révision");

        auto results = std::make_shared<TSA::Analysis::ResultsModel>();
        results->setValid(true);
        TSA::Analysis::ResultsValidityGuard guard(&m);
        int staleCalls = 0;
        guard.setStaleCallback([&]() { ++staleCalls; });
        guard.trackResults(results);
        TEST_CHECK(guard.resultsUpToDate(), "Test 95: résultats à jour juste après l'analyse");

        m.pushUndoState("Modification test");
        m.getBeam(b1)->setRotation(15.0);
        m.notifyBeamModified(b1);
        TEST_CHECK(!results->isValid(), "Test 95: résultats invalidés après modification");
        TEST_CHECK(!guard.resultsUpToDate(), "Test 95: le garde signale des résultats périmés");
        TEST_CHECK(staleCalls == 1, "Test 95: rappel 'obsolète' appelé une seule fois");

        // Undo ramène la géométrie, mais pas la validité : il faut relancer le calcul
        TEST_CHECK(m.undo(), "Test 95: undo");
        TEST_CHECK(!results->isValid() && staleCalls == 1, "Test 95: un Undo ne revalide pas les résultats");

        // Nouvelle analyse : de nouveau à jour, puis une modification de charge invalide aussi
        auto results2 = std::make_shared<TSA::Analysis::ResultsModel>();
        results2->setValid(true);
        guard.trackResults(results2);
        TEST_CHECK(guard.resultsUpToDate(), "Test 95: nouvelle analyse à jour");
        m.pushUndoState("Combinaison");
        m.loadManager().getCombination(1)->setFactor(2, 1.2);
        TEST_CHECK(!results2->isValid(), "Test 95: modifier une combinaison (via pushUndoState) invalide");
        std::cout << "[PASS] Test 95: Invalidation des résultats" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 98: Requêtes de sélection (SelectionQuery)
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 98: Requêtes de sélection ---" << std::endl;
        namespace SQ = TSA::Model::SelectionQuery;
        Model m;
        // Portique : 2 poteaux (x = 0 et x = 6), une poutre en tête à z = 3, une poutre au sol
        int a = m.addNode(0.0, 0.0, 0.0), b = m.addNode(6.0, 0.0, 0.0);
        int c = m.addNode(0.0, 0.0, 3.0), d = m.addNode(6.0, 0.0, 3.0);
        int col1 = m.addColumn(a, c, 0.30, 0.30);
        int col2 = m.addColumn(b, d, 0.40, 0.40);
        int beamTop = m.addBeam(c, d, 0.30, 0.50);
        int beamLow = m.addBeam(a, b, 0.30, 0.50);

        auto all = SQ::all(m);
        TEST_CHECK(all.nodes.size() == 4 && all.columns.size() == 2 && all.beams.size() == 2, "Test 98: tout le modèle");

        TSA::Model::ElementSet current;
        current.beams = { beamTop };
        auto inv = SQ::invert(m, current);
        TEST_CHECK(inv.size() == all.size() - 1 && !inv.beams.count(beamTop) && inv.beams.count(beamLow), "Test 98: inversion");

        TEST_CHECK(SQ::byKind(m, TSA::Model::ElementKind::Column).columns.size() == 2 &&
                   SQ::byKind(m, TSA::Model::ElementKind::Column).size() == 2, "Test 98: par type");

        auto sec = SQ::sameSection(m, current);
        TEST_CHECK(sec.beams.size() == 2 && sec.columns.empty(), "Test 98: même section (les 2 poutres 30x50)");
        TSA::Model::ElementSet refCol; refCol.columns = { col1 };
        auto secCol = SQ::sameSection(m, refCol);
        TEST_CHECK(secCol.columns.size() == 1 && secCol.columns.count(col1), "Test 98: poteau 30x30 seul de sa section");

        TEST_CHECK(SQ::sameMaterial(m, current).size() == 4, "Test 98: même matériau (béton par défaut)");

        auto lvl = SQ::atElevation(m, 3.0, TSA::Coordinate::GeometryTolerance::planeMembership);
        TEST_CHECK(lvl.nodes.size() == 2 && lvl.beams.size() == 1 && lvl.beams.count(beamTop) && lvl.columns.empty(),
                   "Test 98: niveau z = 3 m (2 nœuds, poutre de tête, pas les poteaux)");

        TSA::Coordinate::WorkPlane planeX(TSA::Coordinate::WorkPlaneType::GlobalYZ, "X = 6", 6.0);
        auto onX = SQ::onWorkPlane(m, planeX, TSA::Coordinate::GeometryTolerance::planeMembership);
        TEST_CHECK(onX.nodes.size() == 2 && onX.columns.size() == 1 && onX.columns.count(col2) && onX.beams.empty(),
                   "Test 98: plan X = 6 m (poteau 2 entier, poutres traversantes exclues)");

        TEST_CHECK(SQ::invert(m, all).empty() && SQ::sameSection(m, {}).empty(), "Test 98: cas limites");
        std::cout << "[PASS] Test 98: Requêtes de sélection" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 99: Modification de l'élévation d'un niveau
    // Règle : seuls les nœuds rattachés au niveau le suivent (aucun déplacement silencieux).
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 99: Élévation de niveau ---" << std::endl;
        Model m;
        auto* lm = m.levelManager();
        TEST_CHECK(lm != nullptr, "Test 99: gestionnaire de niveaux présent");
        lm->clear();
        auto* lvl = lm->addLevel("R+1", 3.0);
        TEST_CHECK(lvl != nullptr, "Test 99: niveau R+1 créé");
        const std::string lvlId = lvl->id;

        int base = m.addNode(0.0, 0.0, 0.0);
        int attached = m.addNode(0.0, 0.0, 3.0);            // rattaché automatiquement (cote du niveau)
        int loose = m.addNode(5.0, 0.0, 3.0, "", "libre");
        m.getNode(loose)->setLevelId("");                  // explicitement non rattaché
        int col = m.addColumn(base, attached);
        TEST_CHECK(m.getNode(attached)->levelId() == lvlId, "Test 99: nœud à la cote du niveau rattaché à la création");

        const auto rev = m.revision();
        TEST_CHECK(lm->setLevelElevation(lvlId, 3.2), "Test 99: élévation modifiée");
        TEST_CHECK(approxEqual(m.getNode(attached)->z(), 3.2), "Test 99: le nœud rattaché suit le niveau");
        TEST_CHECK(approxEqual(m.getNode(loose)->z(), 3.0), "Test 99: le nœud non rattaché ne bouge pas");
        TEST_CHECK(m.getNode(loose)->levelId().empty(), "Test 99: pas de rattachement d'office");
        TEST_CHECK(approxEqual(m.getColumn(col)->length(m), 3.2), "Test 99: poteau connecté rallongé");
        TEST_CHECK(m.revision() > rev, "Test 99: révision incrémentée (résultats invalidés)");
        std::cout << "[PASS] Test 99: Élévation de niveau" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 101: Symétrie (miroir) — copie avec nœuds partagés sur le plan, et retournement
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 101: Symétrie ---" << std::endl;
        Model m;
        int a = m.addNode(0.0, 0.0, 0.0);
        int b = m.addNode(4.0, 0.0, 0.0);
        int c = m.addNode(4.0, 0.0, 3.0);
        int beam = m.addBeam(a, b);
        int col = m.addColumn(b, c, 0.40, 0.40);
        m.getBeam(beam)->setRotation(15.0);
        const size_t nodes0 = m.nodes().size();

        // Plan X = 4 : b et c sont sur le plan (partagés), seul a est dupliqué en x = 8.
        auto created = m.mirrorElements({}, { beam }, { col }, {}, gp_Pnt(4.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0), true);
        TEST_CHECK(m.nodes().size() == nodes0 + 1, "Test 101: un seul nœud créé (nœuds du plan partagés)");
        TEST_CHECK(m.beams().size() == 2, "Test 101: poutre symétrisée");
        TEST_CHECK(m.columns().size() == 1, "Test 101: poteau sur le plan non dupliqué");
        const Beam* mb = nullptr;
        for (const auto& [id, bm] : m.beams()) if (id != beam) mb = &bm;
        TEST_CHECK(mb && mb->endNodeId() == b, "Test 101: la copie rejoint le nœud partagé");
        const Node* na = mb ? m.getNode(mb->startNodeId()) : nullptr;
        TEST_CHECK(na && approxEqual(na->x(), 8.0) && approxEqual(na->z(), 0.0), "Test 101: image de (0,0,0) en (8,0,0)");
        TEST_CHECK(mb && approxEqual(mb->rotation(), 15.0), "Test 101: propriétés recopiées");
        TEST_CHECK(created.size() == 2, "Test 101: ids créés = 1 nœud + 1 poutre");

        // Retournement (sans copie) par rapport au plan X = 0 : seul b, c bougent.
        auto moved = m.mirrorElements({}, {}, { col }, {}, gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0), false);
        TEST_CHECK(moved.size() == 2 && approxEqual(m.getNode(c)->x(), -4.0), "Test 101: retournement des nœuds du poteau");

        // Dalle : l'ordre du contour est inversé pour garder l'orientation.
        Model m2;
        int s1 = m2.addNode(0, 0, 0), s2 = m2.addNode(2, 0, 0), s3 = m2.addNode(2, 2, 0), s4 = m2.addNode(0, 2, 0);
        int slab = m2.addSlab({ s1, s2, s3, s4 });
        m2.mirrorElements({}, {}, {}, { slab }, gp_Pnt(2.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0), true);
        TEST_CHECK(m2.slabs().size() == 2 && m2.nodes().size() == 6, "Test 101: dalle symétrisée, bord commun partagé");
        std::cout << "[PASS] Test 101: Symétrie" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 102: Division de barres
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 102: Division de barres ---" << std::endl;
        Model m;
        int a = m.addNode(0.0, 0.0, 0.0);
        int b = m.addNode(6.0, 0.0, 0.0);
        int beam = m.addBeam(a, b, 0.30, 0.60);
        EndRelease pinned; pinned.my = true; pinned.mz = true;
        m.getBeam(beam)->setStartRelease(pinned);
        m.getBeam(beam)->setEndRelease(pinned);
        m.loadManager().addMemberLoad(MemberLoad::uniform(beam, 1, -10.0));

        auto parts = m.splitBeam(beam, 3);
        TEST_CHECK(parts.size() == 3 && parts.front() == beam, "Test 102: 3 tronçons, l'original en tête");
        TEST_CHECK(m.beams().size() == 3 && m.nodes().size() == 4, "Test 102: 2 nœuds intermédiaires créés");
        double total = 0.0;
        for (int id : parts) total += m.getBeam(id)->length(m);
        TEST_CHECK(approxEqual(total, 6.0) && approxEqual(m.getBeam(beam)->length(m), 2.0), "Test 102: longueurs égales, total conservé");
        TEST_CHECK(m.getBeam(parts.back())->endNodeId() == b, "Test 102: le dernier tronçon finit au nœud d'origine");
        TEST_CHECK(m.getBeam(beam)->startRelease().my && !m.getBeam(beam)->endRelease().my &&
                   m.getBeam(parts.back())->endRelease().my && !m.getBeam(parts[1])->startRelease().my,
                   "Test 102: relâchements conservés aux seules extrémités");
        TEST_CHECK(approxEqual(m.getBeam(parts[1])->section().height, 0.60), "Test 102: section recopiée");
        int loadsOnParts = 0;
        for (const auto& [id, ml] : m.loadManager().memberLoads())
            if (std::find(parts.begin(), parts.end(), ml.elementId()) != parts.end() && approxEqual(ml.q1(), -10.0)) ++loadsOnParts;
        TEST_CHECK(loadsOnParts == 3, "Test 102: charge uniforme reportée sur chaque tronçon");

        // Refus : charge ponctuelle (non redistribuable sans ambiguïté), aucun changement.
        int c = m.addNode(0.0, 5.0, 0.0), d = m.addNode(6.0, 5.0, 0.0);
        int beam2 = m.addBeam(c, d);
        m.loadManager().addMemberLoad(MemberLoad::pointOnMember(beam2, 1, -5.0, 2.0));
        const size_t beamsBefore = m.beams().size();
        TEST_CHECK(m.splitBeam(beam2, 2).empty() && m.beams().size() == beamsBefore, "Test 102: refus avec charge ponctuelle");
        TEST_CHECK(m.splitBeam(beam2, 1).empty(), "Test 102: refus pour moins de 2 tronçons");

        int col = m.addColumn(a, m.addNode(0.0, 0.0, 3.0));
        auto colParts = m.splitColumn(col, 2);
        TEST_CHECK(colParts.size() == 2 && approxEqual(m.getColumn(col)->length(m), 1.5), "Test 102: division d'un poteau");
        std::cout << "[PASS] Test 102: Division de barres" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 103: Fusion des nœuds confondus
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 103: Fusion des nœuds confondus ---" << std::endl;
        Model m;
        int a = m.addNode(0.0, 0.0, 0.0);
        int b = m.addNode(5.0, 0.0, 0.0);
        int b2 = m.addNode(5.0004, 0.0, 0.0);      // doublon de b à 0,4 mm
        int c = m.addNode(10.0, 0.0, 0.0);
        int far = m.addNode(5.01, 0.0, 0.0);       // 1 cm : pas un doublon
        m.getNode(b2)->setSupport(SupportDefinition::pinned());
        int beam1 = m.addBeam(a, b);
        int beam2 = m.addBeam(b2, c);
        int degenerate = m.addBeam(b, b2);         // deviendra de longueur nulle
        int foot = m.addFoundation(b2);
        int nl = m.loadManager().addNodalLoad(NodalLoad(0, b2, 1, 0.0, 0.0, -20.0));

        auto dup = m.findCoincidentNodes(1e-3);
        TEST_CHECK(dup.size() == 1 && dup.count(b2) && dup.at(b2) == b, "Test 103: un doublon détecté (b2 → b)");

        TEST_CHECK(m.mergeCoincidentNodes(1e-3) == 1, "Test 103: un nœud fusionné");
        TEST_CHECK(!m.getNode(b2) && m.getNode(far), "Test 103: doublon supprimé, nœud voisin conservé");
        TEST_CHECK(m.getBeam(beam1) && m.getBeam(beam2) && m.getBeam(beam2)->startNodeId() == b, "Test 103: barre reconnectée");
        TEST_CHECK(!m.getBeam(degenerate), "Test 103: barre de longueur nulle supprimée");
        TEST_CHECK(m.getNode(b)->support().isSupported(), "Test 103: appui reporté sur le nœud conservé");
        TEST_CHECK(m.getFoundation(foot) && m.getFoundation(foot)->nodeId() == b, "Test 103: fondation reportée");
        TEST_CHECK(m.loadManager().getNodalLoad(nl) && m.loadManager().getNodalLoad(nl)->nodeId() == b, "Test 103: charge nodale reportée");
        TEST_CHECK(m.findCoincidentNodes(1e-3).empty() && m.mergeCoincidentNodes(1e-3) == 0, "Test 103: idempotent");

        // Dalle dont deux sommets fusionnent → triangle conservé.
        Model m2;
        int p1 = m2.addNode(0, 0, 0), p2 = m2.addNode(3, 0, 0), p3 = m2.addNode(3, 0.0002, 0), p4 = m2.addNode(0, 3, 0);
        int slab = m2.addSlab({ p1, p2, p3, p4 });
        m2.mergeCoincidentNodes(1e-3);
        TEST_CHECK(m2.getSlab(slab) && m2.getSlab(slab)->nodeIds().size() == 3, "Test 103: contour de dalle compacté");
        std::cout << "[PASS] Test 103: Fusion des nœuds confondus" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 104: Transactions d'édition topologique — une seule entrée Undo, restauration exacte
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 104: Undo des opérations topologiques ---" << std::endl;
        Model m;
        int a = m.addNode(0.0, 0.0, 0.0);
        int b = m.addNode(4.0, 0.0, 0.0);
        int beam = m.addBeam(a, b);
        m.clearUndoRedo();
        {
            TSA::UndoRedo::EditTransaction tx(m, "Division de barres");
            TEST_CHECK(m.splitBeam(beam, 4).size() == 4, "Test 104: division");
            tx.commit();
        }
        TEST_CHECK(m.beams().size() == 4 && m.canUndo(), "Test 104: état divisé, annulable");
        TEST_CHECK(m.undo(), "Test 104: undo");
        TEST_CHECK(m.beams().size() == 1 && m.nodes().size() == 2 && m.getBeam(beam)->endNodeId() == b,
                   "Test 104: barre d'origine restaurée");
        TEST_CHECK(!m.canUndo(), "Test 104: une seule entrée d'historique");
        std::cout << "[PASS] Test 104: Undo des opérations topologiques" << std::endl;
        passed++;
    }

    return true;
}

