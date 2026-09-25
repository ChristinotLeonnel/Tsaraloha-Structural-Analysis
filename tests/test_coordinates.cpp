#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

#include "Coordinate/Point3D.h"
#include "Coordinate/LevelManager.h"
#include "Coordinate/CoordinateSystem.h"
#include "Model/Model.h"
#include "Model/Material.h"
#include "Model/Section.h"
#include "Model/Wall.h"
#include "Model/Foundation.h"
#include "Model/TrussMember.h"
#include "Grid/CartesianGrid.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridSystem.h"
#include "Grid/GridSnapManager.h"
#include "Geometry/BeamGeometry.h"

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>

using namespace TSA::Coordinate;
using namespace TSA::Model;
using namespace TSA::Grid;
using namespace TSA::Geometry;

static bool approxEqual(double a, double b, double eps = 1e-4)
{
    return std::abs(a - b) <= eps;
}

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

int main()
{
    int passed = 0;
    int total = 15;

    std::cout << "=================================================" << std::endl;
    std::cout << "TSA Unit Tests: 3D Coordinates, Grid & Levels" << std::endl;
    std::cout << "=================================================" << std::endl;

    CoordinateSystem coordSys;

    // -------------------------------------------------------------------------
    // TEST 1: Irregular Cartesian X coordinates (X = {0, 2, 5, 6})
    // -------------------------------------------------------------------------
    {
        coordSys.setXPositions({ 0.0, 2.0, 5.0, 6.0 });
        const auto& xPos = coordSys.xPositions();
        TEST_CHECK(xPos.size() == 4, "xPos size");
        TEST_CHECK(approxEqual(xPos[0], 0.0) && approxEqual(xPos[1], 2.0) &&
                   approxEqual(xPos[2], 5.0) && approxEqual(xPos[3], 6.0), "xPos values");

        auto xSpacings = coordSys.getXSpacings();
        TEST_CHECK(xSpacings.size() == 3, "xSpacings size");
        TEST_CHECK(approxEqual(xSpacings[0], 2.0) && approxEqual(xSpacings[1], 3.0) && approxEqual(xSpacings[2], 1.0), "xSpacings values");

        std::cout << "[PASS] Test 1: Irregular X coordinates {0, 2, 5, 6} m, spacings {2, 3, 1} m" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 2: Irregular Cartesian Y coordinates (Y = {0, 3, 7.5, 10})
    // -------------------------------------------------------------------------
    {
        coordSys.setYPositions({ 0.0, 3.0, 7.5, 10.0 });
        const auto& yPos = coordSys.yPositions();
        TEST_CHECK(yPos.size() == 4, "yPos size");
        TEST_CHECK(approxEqual(yPos[0], 0.0) && approxEqual(yPos[1], 3.0) &&
                   approxEqual(yPos[2], 7.5) && approxEqual(yPos[3], 10.0), "yPos values");

        auto ySpacings = coordSys.getYSpacings();
        TEST_CHECK(ySpacings.size() == 3, "ySpacings size");
        TEST_CHECK(approxEqual(ySpacings[0], 3.0) && approxEqual(ySpacings[1], 4.5) && approxEqual(ySpacings[2], 2.5), "ySpacings values");

        std::cout << "[PASS] Test 2: Irregular Y coordinates {0, 3, 7.5, 10} m, spacings {3, 4.5, 2.5} m" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 3: Story / Level system (Z = {0, 3, 6.5, 10})
    // -------------------------------------------------------------------------
    {
        auto* lm = coordSys.levelManager();
        lm->setFromElevations({ 0.0, 3.0, 6.5, 10.0 }, { "Niveau 0", "Niveau 1", "Niveau 2", "Niveau 3" });
        TEST_CHECK(lm->levelCount() == 4, "levelCount");
        TEST_CHECK(approxEqual(lm->getLevelByIndex(0)->elevation, 0.0), "L0 elevation");
        TEST_CHECK(approxEqual(lm->getLevelByIndex(1)->elevation, 3.0), "L1 elevation");
        TEST_CHECK(approxEqual(lm->getLevelByIndex(2)->elevation, 6.5), "L2 elevation");
        TEST_CHECK(approxEqual(lm->getLevelByIndex(3)->elevation, 10.0), "L3 elevation");

        auto zSpacings = lm->getSpacings();
        TEST_CHECK(zSpacings.size() == 3, "zSpacings size");
        TEST_CHECK(approxEqual(zSpacings[0], 3.0) && approxEqual(zSpacings[1], 3.5) && approxEqual(zSpacings[2], 3.5), "zSpacings values");

        std::cout << "[PASS] Test 3: Story levels {0, 3, 6.5, 10} m, spacings {3, 3.5, 3.5} m" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 4: Node creation at exact grid intersection (X=5, Y=3, Z=6.5)
    // -------------------------------------------------------------------------
    Model model;
    // Setup model's coordinate system matching our test
    model.coordinateSystem()->setXPositions({ 0.0, 2.0, 5.0, 6.0 });
    model.coordinateSystem()->setYPositions({ 0.0, 3.0, 7.5, 10.0 });
    model.levelManager()->setFromElevations({ 0.0, 3.0, 6.5, 10.0 },
                                           { "Niveau 0", "Niveau 1", "Niveau 2", "Niveau 3" });

    int nId_5_3_65 = -1;
    {
        // Grid indices: ix=2 (5.0m), iy=1 (3.0m), iz=2 (6.5m)
        nId_5_3_65 = model.addNodeAtGridIntersection(2, 1, 2);
        TEST_CHECK(nId_5_3_65 > 0, "nodeId valid");
        const auto* node = model.getNode(nId_5_3_65);
        TEST_CHECK(node != nullptr, "node not null");
        TEST_CHECK(approxEqual(node->x(), 5.0) && approxEqual(node->y(), 3.0) && approxEqual(node->z(), 6.5), "node coords");
        TEST_CHECK(node->levelId() == model.levelManager()->getLevelByIndex(2)->id, "node levelId");

        std::cout << "[PASS] Test 4: Node created at exact grid intersection (5.0, 3.0, 6.5) m associated with Level 2" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 5: Vertical column creation between Level 0 (Z=0) and Level 1 (Z=3)
    // -------------------------------------------------------------------------
    int col1Id = -1;
    {
        col1Id = model.addColumnBetweenLevels(0, 1, 5.0, 3.0, 0.30, 0.30);
        TEST_CHECK(col1Id > 0, "col1Id valid");
        const auto* col1 = model.getColumn(col1Id);
        TEST_CHECK(col1 != nullptr, "col1 not null");
        TEST_CHECK(col1->isVertical(model), "col1 isVertical");
        TEST_CHECK(approxEqual(col1->length(model), 3.0), "col1 length");
        TEST_CHECK(approxEqual(col1->bottomElevation(model), 0.0), "col1 bottomElevation");
        TEST_CHECK(approxEqual(col1->topElevation(model), 3.0), "col1 topElevation");

        std::cout << "[PASS] Test 5: Vertical column between Level 0 (0m) and Level 1 (3m), height = 3.0m" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 6: Vertical column creation between Level 1 (Z=3) and Level 2 (Z=6.5)
    // -------------------------------------------------------------------------
    int col2Id = -1;
    {
        col2Id = model.addColumnBetweenLevels(1, 2, 5.0, 3.0, 0.30, 0.30);
        TEST_CHECK(col2Id > 0, "col2Id valid");
        const auto* col2 = model.getColumn(col2Id);
        TEST_CHECK(col2 != nullptr, "col2 not null");
        TEST_CHECK(col2->isVertical(model), "col2 isVertical");
        TEST_CHECK(approxEqual(col2->length(model), 3.5), "col2 length");
        TEST_CHECK(approxEqual(col2->bottomElevation(model), 3.0), "col2 bottomElevation");
        TEST_CHECK(approxEqual(col2->topElevation(model), 6.5), "col2 topElevation");

        std::cout << "[PASS] Test 6: Vertical column between Level 1 (3m) and Level 2 (6.5m), height = 3.5m" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 7: Elevation change propagation (Level 2: 6.5m -> 7.0m)
    // -------------------------------------------------------------------------
    {
        auto* lvl2 = model.levelManager()->getLevelByIndex(2);
        std::string lvl2Id = lvl2->id;

        // Change elevation to 7.00 m
        model.levelManager()->setLevelElevation(lvl2Id, 7.0);

        // Verify Node coordinates were updated to Z = 7.00 m
        const auto* node = model.getNode(nId_5_3_65);
        TEST_CHECK(approxEqual(node->z(), 7.0), "node Z updated to 7.0m");

        // Verify Column 2 top elevation was updated to 7.00 m and height to 4.00 m
        const auto* col2 = model.getColumn(col2Id);
        TEST_CHECK(approxEqual(col2->topElevation(model), 7.0), "col2 topElevation updated to 7.0m");
        TEST_CHECK(approxEqual(col2->length(model), 4.0), "col2 length updated to 4.0m");

        std::cout << "[PASS] Test 7: Level 2 elevation change 6.5m -> 7.0m propagated to node (Z=7.0m) and column height (4.0m)" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 8: Snapping verification to (5.0, 3.0, 6.5)
    // -------------------------------------------------------------------------
    {
        GridDefinition gdef("TestGrid", GridType::Cartesian);
        gdef.setXPositions({ 0.0, 2.0, 5.0, 6.0 });
        gdef.setYPositions({ 0.0, 3.0, 7.5, 10.0 });
        gdef.setZLevels({ 0.0, 3.0, 6.5, 10.0 });

        CartesianGrid cartesian(gdef);

        // Query slightly off: (4.95, 3.05, 6.48), tolerance 0.15 m
        gp_Pnt query(4.95, 3.05, 6.48);
        GridSnapResult snap = cartesian.findClosestSnap(query, 0.15);

        TEST_CHECK(snap.snapped, "snap.snapped");
        TEST_CHECK(snap.type == GridSnapType::Intersection, "snap.type is Intersection");
        TEST_CHECK(approxEqual(snap.point.X(), 5.0), "snap X");
        TEST_CHECK(approxEqual(snap.point.Y(), 3.0), "snap Y");
        TEST_CHECK(approxEqual(snap.point.Z(), 6.5), "snap Z");

        std::cout << "[PASS] Test 8: Snapped near (4.95, 3.05, 6.48) to exact point (5.0, 3.0, 6.5)" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 9: 3D Grid Multi-Level Intersections & Priority Node Snap
    // -------------------------------------------------------------------------
    {
        GridDefinition gdef("3D_Building", GridType::Cartesian);
        gdef.setXPositions({ 0.0, 4.0, 8.0 });
        gdef.setYPositions({ 0.0, 6.0 });
        gdef.setZLevels({ 0.0, 3.0, 6.0 }); // 3 niveaux d'étages

        CartesianGrid cartesian(gdef);
        // Intersections = 3 (X) * 2 (Y) * 3 (Z) = 18 points 3D
        TEST_CHECK(cartesian.intersections().size() == 18, "Cartesian 3D intersections count == 18");

        GridSnapManager snapMgr;
        snapMgr.setSnapTolerance(0.50);

        // Nœud placé à (4.0, 6.0, 3.0)
        Model testModel;
        int nId = testModel.addNode(4.0, 6.0, 3.0);
        (void)nId;

        GridSystem gridSys(gdef);
        // Query proche du nœud (3.95, 6.02, 2.98)
        gp_Pnt query(3.95, 6.02, 2.98);
        GridSnapResult snapRes = snapMgr.findSnap(query, &gridSys, &testModel);

        TEST_CHECK(snapRes.snapped, "snapRes.snapped");
        TEST_CHECK(snapRes.type == GridSnapType::Node, "Snap priority to Model Node");
        TEST_CHECK(approxEqual(snapRes.point.X(), 4.0) && approxEqual(snapRes.point.Y(), 6.0) && approxEqual(snapRes.point.Z(), 3.0), "Snap point coords");

        std::cout << "[PASS] Test 9: 18 3D Grid intersections verified & Priority Node Snap confirmed" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 10: 3D Node & Element Rotation and Copy-and-Rotate
    // -------------------------------------------------------------------------
    {
        Model rotModel;
        int n1 = rotModel.addNode(1.0, 0.0, 0.0);
        int n2 = rotModel.addNode(1.0, 2.0, 0.0);
        int b1 = rotModel.addBeam(n1, n2, 0.3, 0.5);

        // Rotate 90° around Z-axis passing through (0, 0, 0)
        constexpr double kPi_2 = 3.14159265358979323846 / 2.0;
        bool ok = rotModel.rotateNodes({n1, n2}, gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), kPi_2);
        TEST_CHECK(ok, "rotateNodes succeeded");

        const auto* node1 = rotModel.getNode(n1);
        const auto* node2 = rotModel.getNode(n2);
        TEST_CHECK(node1 && approxEqual(node1->x(), 0.0) && approxEqual(node1->y(), 1.0) && approxEqual(node1->z(), 0.0), "Node 1 rotated to (0, 1, 0)");
        TEST_CHECK(node2 && approxEqual(node2->x(), -2.0) && approxEqual(node2->y(), 1.0) && approxEqual(node2->z(), 0.0), "Node 2 rotated to (-2, 1, 0)");

        // Copy and Rotate 90° further: (-1, 0, 0) and (-1, -2, 0)
        auto newIds = rotModel.copyAndRotateElements({n1, n2}, {b1}, {}, {}, gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), kPi_2, 1);
        TEST_CHECK(newIds.size() == 3, "Created 2 new nodes and 1 new beam");

        // Verify model now has 4 nodes and 2 beams
        TEST_CHECK(rotModel.nodes().size() == 4, "Total 4 nodes");
        TEST_CHECK(rotModel.beams().size() == 2, "Total 2 beams");

        std::cout << "[PASS] Test 10: 3D Rotation and Copy-and-Rotate of structural elements verified" << std::endl;
        passed++;
    }

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

    std::cout << "=================================================" << std::endl;
    std::cout << "RESULTS: " << passed << " / " << (total + 2) << " tests passed successfully!" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
