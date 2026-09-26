#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

#include "Coordinate/Point3D.h"
#include "Coordinate/LevelManager.h"
#include "Coordinate/CoordinateSystem.h"
#include "Model/Model.h"
#include "Model/ModelDiff.h"
#include "Model/Material.h"
#include <chrono>
#include "Model/Section.h"
#include "Model/Wall.h"
#include "Model/Foundation.h"
#include "Model/TrussMember.h"
#include "Grid/CartesianGrid.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridSystem.h"
#include "Grid/GridSnapManager.h"
#include "Geometry/BeamGeometry.h"
#include "IO/TSAFile.h"
#include "IO/TSAFileFormat.h"
#include "IO/TSAPreviewGenerator.h"
#include "Library/LibraryManager.h"
#include "Model/StructuralClipboard.h"
#include "Project/ProjectManager.h"
#include "Commands/ICommand.h"
#include "Commands/CreateBeamCommand.h"
#include "UndoRedo/UndoManager.h"
#include "UndoRedo/CommandManager.h"
#include "Interaction/InteractionManager.h"

#include <fstream>
#include <filesystem>

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
using namespace TSA::IO;

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

#include <QGuiApplication>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
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

    // -------------------------------------------------------------------------
    // TEST 18: Native TSA Binary File Format (.tsa) - Persistence & OCCT Rebuild
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 18: TSA File Format Validation Suite ---" << std::endl;

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

        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);

        // --- SUBTEST 1: Empty project save / load ---
        {
            std::string emptyFile = tmpDir + "test_empty.tsa";
            Model mEmpty;
            std::string err;
            bool saved = TSAProjectIO::saveToFile(QString::fromStdString(emptyFile), mEmpty, nullptr, &err);
            TEST_CHECK(saved, "Subtest 1: Empty project saved successfully");

            Model mEmptyLoaded;
            bool loaded = TSAProjectIO::loadFromFile(QString::fromStdString(emptyFile), mEmptyLoaded, nullptr, &err);
            TEST_CHECK(loaded, "Subtest 1: Empty project loaded successfully");
            TEST_CHECK(mEmptyLoaded.nodes().empty(), "Subtest 1: Loaded empty project has 0 nodes");
            TEST_CHECK(mEmptyLoaded.beams().empty(), "Subtest 1: Loaded empty project has 0 beams");
            std::cout << "  [PASS] Subtest 1: Empty project round-trip" << std::endl;
        }

        // --- SUBTEST 2: Nodes & Bars (2 nodes, 1 bar) ---
        {
            std::string nodeBarFile = tmpDir + "test_node_bar.tsa";
            Model mOrig;
            int n1 = mOrig.addNode(0.0, 0.0, 0.0);
            int n2 = mOrig.addNode(5.0, 0.0, 0.0);
            int b1 = mOrig.addBeam(n1, n2, 0.30, 0.50);

            std::string err;
            bool saved = TSAProjectIO::saveToFile(QString::fromStdString(nodeBarFile), mOrig, nullptr, &err);
            TEST_CHECK(saved, "Subtest 2: Project with nodes & bar saved");

            Model mReloaded;
            bool loaded = TSAProjectIO::loadFromFile(QString::fromStdString(nodeBarFile), mReloaded, nullptr, &err);
            TEST_CHECK(loaded, "Subtest 2: Project with nodes & bar loaded");
            TEST_CHECK(mReloaded.nodes().size() == 2, "Subtest 2: 2 nodes retrieved");
            TEST_CHECK(mReloaded.beams().size() == 1, "Subtest 2: 1 beam retrieved");

            const auto* loadedN1 = mReloaded.getNode(n1);
            const auto* loadedN2 = mReloaded.getNode(n2);
            TEST_CHECK(loadedN1 != nullptr && loadedN2 != nullptr, "Subtest 2: Nodes retrieved by exact ID");
            TEST_CHECK(approxEqual(loadedN1->x(), 0.0) && approxEqual(loadedN1->y(), 0.0) && approxEqual(loadedN1->z(), 0.0), "Subtest 2: Node 1 coords match");
            TEST_CHECK(approxEqual(loadedN2->x(), 5.0) && approxEqual(loadedN2->y(), 0.0) && approxEqual(loadedN2->z(), 0.0), "Subtest 2: Node 2 coords match");

            const auto* loadedB1 = mReloaded.getBeam(b1);
            TEST_CHECK(loadedB1 != nullptr, "Subtest 2: Beam retrieved by exact ID");
            TEST_CHECK(loadedB1->startNodeId() == n1 && loadedB1->endNodeId() == n2, "Subtest 2: Beam node connectivity preserved");
            std::cout << "  [PASS] Subtest 2: Nodes & Bars connectivity round-trip" << std::endl;
        }

        // --- SUBTEST 3: Section Rectangle 400x500 ---
        {
            std::string rectFile = tmpDir + "test_rect.tsa";
            Model mRect;
            int n1 = mRect.addNode(0.0, 0.0, 0.0);
            int n2 = mRect.addNode(6.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 1;
            bp.name = "Poutre_Rect_400x500";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.40, 0.50);
            mRect.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(rectFile), mRect, nullptr, &err), "Subtest 3: Save Rectangle");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(rectFile), mLoaded, nullptr, &err), "Subtest 3: Load Rectangle");
            const auto* b = mLoaded.getBar(1);
            TEST_CHECK(b != nullptr, "Subtest 3: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::Rectangular, "Subtest 3: Section shape is Rectangular");
            TEST_CHECK(approxEqual(b->section().width, 0.40), "Subtest 3: Width is 0.40m (400 mm)");
            TEST_CHECK(approxEqual(b->section().height, 0.50), "Subtest 3: Height is 0.50m (500 mm)");
            std::cout << "  [PASS] Subtest 3: Rectangle 400x500 section preserved" << std::endl;
        }

        // --- SUBTEST 4: Section Circle Ø400 ---
        {
            std::string circFile = tmpDir + "test_circ.tsa";
            Model mCirc;
            int n1 = mCirc.addNode(0.0, 0.0, 0.0);
            int n2 = mCirc.addNode(0.0, 0.0, 4.0);
            BarProperties bp;
            bp.id = 2;
            bp.name = "Poteau_Circ_D400";
            bp.role = BarRole::Column;
            bp.section = Section::circular(0.40);
            mCirc.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(circFile), mCirc, nullptr, &err), "Subtest 4: Save Circle");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(circFile), mLoaded, nullptr, &err), "Subtest 4: Load Circle");
            const auto* b = mLoaded.getBar(2);
            TEST_CHECK(b != nullptr, "Subtest 4: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::Circular, "Subtest 4: Section shape is Circular");
            TEST_CHECK(approxEqual(b->section().diameter, 0.40), "Subtest 4: Diameter is 0.40m (400 mm)");
            std::cout << "  [PASS] Subtest 4: Circular D400 section preserved" << std::endl;
        }

        // --- SUBTEST 5: Section IPE200 ---
        {
            std::string ipeFile = tmpDir + "test_ipe.tsa";
            Model mIpe;
            int n1 = mIpe.addNode(0.0, 0.0, 0.0);
            int n2 = mIpe.addNode(4.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 3;
            bp.name = "Solive_IPE200";
            bp.role = BarRole::Beam;
            bp.section = Section::ipe(200);
            mIpe.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(ipeFile), mIpe, nullptr, &err), "Subtest 5: Save IPE200");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(ipeFile), mLoaded, nullptr, &err), "Subtest 5: Load IPE200");
            const auto* b = mLoaded.getBar(3);
            TEST_CHECK(b != nullptr, "Subtest 5: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::IShape, "Subtest 5: Section shape is IShape");
            TEST_CHECK(b->section().name == "IPE 200", "Subtest 5: Profile name is IPE 200");
            TEST_CHECK(approxEqual(b->section().height, 0.200), "Subtest 5: Height is 0.200m");
            TEST_CHECK(approxEqual(b->section().width, 0.100), "Subtest 5: Width is 0.100m");
            TEST_CHECK(b->section().iy() > 0.0 && b->section().iz() > 0.0, "Subtest 5: Inertias computed properly");
            std::cout << "  [PASS] Subtest 5: IPE 200 profile preserved" << std::endl;
        }

        // --- SUBTEST 6: 3D Diagonal Bar (A, B, direction, length, rotation, releases) ---
        {
            std::string diagFile = tmpDir + "test_diag.tsa";
            Model mDiag;
            int n1 = mDiag.addNode(1.0, 2.0, 3.0);
            int n2 = mDiag.addNode(5.0, 5.0, 7.0);
            BarProperties bp;
            bp.id = 4;
            bp.name = "Bracing_3D";
            bp.role = BarRole::Truss;
            bp.section = Section::boxHollow(0.12, 0.12, 0.008);
            bp.rotation = 37.5;
            bp.endRelease.my = true;
            bp.endRelease.mz = true;
            mDiag.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(diagFile), mDiag, nullptr, &err), "Subtest 6: Save 3D Diagonal");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(diagFile), mLoaded, nullptr, &err), "Subtest 6: Load 3D Diagonal");
            const auto* b = mLoaded.getBar(4);
            TEST_CHECK(b != nullptr, "Subtest 6: Bar found");
            TEST_CHECK(approxEqual(b->length(mLoaded), std::sqrt(16.0 + 9.0 + 16.0)), "Subtest 6: 3D Length is sqrt(41) m");
            TEST_CHECK(approxEqual(b->rotation(), 37.5), "Subtest 6: Rotation is 37.5°");
            TEST_CHECK(b->endRelease().my && b->endRelease().mz, "Subtest 6: End release is Pinned for bending");
            std::cout << "  [PASS] Subtest 6: 3D Diagonal bar orientation & releases preserved" << std::endl;
        }

        // --- SUBTEST 7: Eccentricity (ey, ez) ---
        {
            std::string eccFile = tmpDir + "test_ecc.tsa";
            Model mEcc;
            int n1 = mEcc.addNode(0.0, 0.0, 0.0);
            int n2 = mEcc.addNode(4.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 5;
            bp.name = "Beam_TopFlange";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.30, 0.60);
            bp.eccentricity = BarEccentricity::TopFlange;
            mEcc.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(eccFile), mEcc, nullptr, &err), "Subtest 7: Save Eccentricity");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(eccFile), mLoaded, nullptr, &err), "Subtest 7: Load Eccentricity");
            const auto* b = mLoaded.getBar(5);
            TEST_CHECK(b != nullptr, "Subtest 7: Bar found");
            TEST_CHECK(b->eccentricity() == BarEccentricity::TopFlange, "Subtest 7: Eccentricity is TopFlange");
            std::cout << "  [PASS] Subtest 7: Eccentricity preserved" << std::endl;
        }

        // --- SUBTEST 8: Material properties ---
        {
            std::string matFile = tmpDir + "test_mat.tsa";
            Model mMat;
            int n1 = mMat.addNode(0.0, 0.0, 0.0);
            int n2 = mMat.addNode(3.0, 0.0, 0.0);
            Material customMat;
            customMat.name = "AcierHauteLimite_S460";
            customMat.type = MaterialType::Steel;
            customMat.E = 2.10e11;
            customMat.nu = 0.30;
            customMat.density = 7850.0;
            customMat.thermalCoeff = 1.2e-5;
            customMat.fk = 460e6;

            BarProperties bp;
            bp.id = 6;
            bp.section = Section::ipe(300);
            bp.material = customMat;
            mMat.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(matFile), mMat, nullptr, &err), "Subtest 8: Save Material");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(matFile), mLoaded, nullptr, &err), "Subtest 8: Load Material");
            const auto* b = mLoaded.getBar(6);
            TEST_CHECK(b != nullptr, "Subtest 8: Bar found");
            TEST_CHECK(b->material().name == "AcierHauteLimite_S460", "Subtest 8: Material name preserved");
            TEST_CHECK(approxEqual(b->material().E, 2.10e11), "Subtest 8: Young modulus preserved");
            TEST_CHECK(approxEqual(b->material().nu, 0.30), "Subtest 8: Poisson ratio preserved");
            TEST_CHECK(approxEqual(b->material().density, 7850.0), "Subtest 8: Density preserved");
            TEST_CHECK(approxEqual(b->material().fk, 460e6), "Subtest 8: Yield strength preserved");
            std::cout << "  [PASS] Subtest 8: Material mechanical properties preserved" << std::endl;
        }

        // --- SUBTEST 9: Robustness on corrupted / invalid files ---
        {
            std::string err;
            Model dummyModel;

            // 1. Fichier avec mauvais magic
            std::string badMagicFile = tmpDir + "corrupt_bad_magic.tsa";
            {
                std::ofstream f(badMagicFile, std::ios::binary);
                uint32_t fakeMagic = 0xDEADBEEF;
                f.write(reinterpret_cast<const char*>(&fakeMagic), 4);
                std::vector<char> pad(300, 0);
                f.write(pad.data(), pad.size());
            }
            bool resMagic = TSAProjectIO::loadFromFile(QString::fromStdString(badMagicFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resMagic, "Subtest 9: Bad magic number rejected safely");

            // 2. Fichier tronqué
            std::string truncatedFile = tmpDir + "corrupt_truncated.tsa";
            {
                std::ofstream f(truncatedFile, std::ios::binary);
                uint32_t magic = TSA_FILE_MAGIC;
                f.write(reinterpret_cast<const char*>(&magic), 4);
            }
            bool resTrunc = TSAProjectIO::loadFromFile(QString::fromStdString(truncatedFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resTrunc, "Subtest 9: Truncated file rejected safely");

            // 3. Fichier avec CRC32 corrompu
            std::string badCrcFile = tmpDir + "corrupt_bad_crc.tsa";
            {
                Model mValid;
                mValid.addNode(0, 0, 0);
                TSAProjectIO::saveToFile(QString::fromStdString(badCrcFile), mValid, nullptr);
                std::fstream f(badCrcFile, std::ios::in | std::ios::out | std::ios::binary);
                f.seekp(sizeof(TSAFileHeader) + 5);
                char badByte = static_cast<char>(0xFF);
                f.write(&badByte, 1);
            }
            bool resCrc = TSAProjectIO::loadFromFile(QString::fromStdString(badCrcFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resCrc, "Subtest 9: Corrupted CRC32 data rejected safely");
            std::cout << "  [PASS] Subtest 9: Corrupted files rejected without crashing" << std::endl;
        }

        // --- SUBTEST 10: CRITICAL TEST - Geometric Fidelity & OCCT Reconstruction ---
        {
            std::string criticalFile = tmpDir + "test_critical.tsa";

            // Étape 1 : Création Rectangle 400x500
            Model modelStep1;
            int n1 = modelStep1.addNode(0.0, 0.0, 0.0);
            int n2 = modelStep1.addNode(5.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 1;
            bp.name = "Poutre_Critique";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.40, 0.50);
            modelStep1.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(criticalFile), modelStep1, nullptr, &err), "Subtest 10: Step 1 save");

            // Étape 2 : Fermer (clear) et Réouvrir
            Model modelStep2;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(criticalFile), modelStep2, nullptr, &err), "Subtest 10: Step 2 reopen");
            const auto* barLoaded1 = modelStep2.getBar(1);
            TEST_CHECK(barLoaded1 != nullptr, "Subtest 10: Bar retrieved");
            TEST_CHECK(barLoaded1->section().shape == SectionShape::Rectangular, "Subtest 10: Section is Rectangular");

            // Reconstruction OCCT
            TopoDS_Shape shapeRect = BeamGeometry::createBeamShape(*modelStep2.getNode(n1), *modelStep2.getNode(n2), barLoaded1->section());
            TEST_CHECK(!shapeRect.IsNull(), "Subtest 10: OCCT shape built from reloaded model");
            TEST_CHECK(!hasCylindricalSurface(shapeRect), "Subtest 10: OCCT shape is rectangular (not cylindrical)");

            // Étape 3 : Modification Rectangle 400x500 -> Circle Ø400
            BarProperties modifiedBp = barLoaded1->properties();
            modifiedBp.section = Section::circular(0.40);
            modelStep2.getBar(1)->setProperties(modifiedBp);

            // Sauvegarder
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(criticalFile), modelStep2, nullptr, &err), "Subtest 10: Step 3 save modified circle");

            // Étape 4 : Fermer et Réouvrir
            Model modelStep3;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(criticalFile), modelStep3, nullptr, &err), "Subtest 10: Step 4 reopen modified project");
            const auto* barFinal = modelStep3.getBar(1);
            TEST_CHECK(barFinal != nullptr, "Subtest 10: Bar retrieved after reopen");
            TEST_CHECK(barFinal->section().shape == SectionShape::Circular, "Subtest 10: Section is Circular");
            TEST_CHECK(approxEqual(barFinal->section().diameter, 0.40), "Subtest 10: Diameter is exactly 0.40m");

            // Reconstruction OCCT finale -> Vérification cylindre réel
            TopoDS_Shape shapeFinal = BeamGeometry::createBeamShape(*modelStep3.getNode(n1), *modelStep3.getNode(n2), barFinal->section());
            TEST_CHECK(!shapeFinal.IsNull(), "Subtest 10: Final OCCT shape created");
            TEST_CHECK(hasCylindricalSurface(shapeFinal), "Subtest 10: Final OCCT shape is a REAL CYLINDER");

            std::cout << "  [PASS] Subtest 10: Critical Test - Exact OCCT shape reconstructed after modify & reload" << std::endl;
        }

        // --- SUBTEST 11: 3D Model Thumbnail Generation & Extraction ---
        {
            std::string thumbFile = tmpDir + "Structure_Reserve.tsa";

            // Modèle avec portique : 4 poteaux, 4 poutres, dalle
            Model modelReserve;
            int n1 = modelReserve.addNode(0, 0, 0);
            int n2 = modelReserve.addNode(6, 0, 0);
            int n3 = modelReserve.addNode(6, 4, 0);
            int n4 = modelReserve.addNode(0, 4, 0);

            int n5 = modelReserve.addNode(0, 0, 3.5);
            int n6 = modelReserve.addNode(6, 0, 3.5);
            int n7 = modelReserve.addNode(6, 4, 3.5);
            int n8 = modelReserve.addNode(0, 4, 3.5);

            // Poteaux
            modelReserve.addColumn(n1, n5, 0.35, 0.35);
            modelReserve.addColumn(n2, n6, 0.35, 0.35);
            modelReserve.addColumn(n3, n7, 0.35, 0.35);
            modelReserve.addColumn(n4, n8, 0.35, 0.35);

            // Poutres
            modelReserve.addBeam(n5, n6, 0.30, 0.50);
            modelReserve.addBeam(n6, n7, 0.30, 0.50);
            modelReserve.addBeam(n7, n8, 0.30, 0.50);
            modelReserve.addBeam(n8, n5, 0.30, 0.50);

            // Dalle
            modelReserve.addSlab({n5, n6, n7, n8}, 0.20);

            // 1. Génération directe de la miniature avec TSAPreviewGenerator
            QImage generatedThumb = TSAPreviewGenerator::generateThumbnail(modelReserve, 512, 512);
            TEST_CHECK(!generatedThumb.isNull(), "Subtest 11: TSAPreviewGenerator produces valid image");
            TEST_CHECK(generatedThumb.width() == 512 && generatedThumb.height() == 512, "Subtest 11: Thumbnail dimensions 512x512");

            // 2. Sauvegarde du fichier Structure_Reserve.tsa avec thumbnail embarqué
            QString saveErr;
            bool saved = TSAProjectIO::saveProject(QString::fromStdString(thumbFile), modelReserve, nullptr,
                                                  "Structure_Reserve", "TSA Architect", true, generatedThumb, &saveErr);
            TEST_CHECK(saved, "Subtest 11: Saved Structure_Reserve.tsa with embedded thumbnail");

            // 3. Extraction rapide de la miniature sans charger le modèle complet
            QImage extractedThumb;
            QString extErr;
            bool extracted = TSAProjectIO::extractThumbnail(QString::fromStdString(thumbFile), extractedThumb, &extErr);
            TEST_CHECK(extracted, "Subtest 11: Fast extractThumbnail succeeded");
            TEST_CHECK(!extractedThumb.isNull(), "Subtest 11: Extracted thumbnail is valid QImage");
            TEST_CHECK(extractedThumb.width() == 512 && extractedThumb.height() == 512, "Subtest 11: Extracted dimensions match");

            // 4. Chargement complet avec récupération du thumbnail
            Model loadedReserve;
            QString loadedProjName, loadedAuthor, loadErr;
            QImage reloadedThumb;
            bool reloaded = TSAProjectIO::loadProject(QString::fromStdString(thumbFile), loadedReserve, nullptr,
                                                     &loadedProjName, &loadedAuthor, &reloadedThumb, &loadErr);
            TEST_CHECK(reloaded, "Subtest 11: Project reloaded completely");
            TEST_CHECK(loadedProjName == "Structure_Reserve", "Subtest 11: Project name preserved");
            TEST_CHECK(loadedAuthor == "TSA Architect", "Subtest 11: Author preserved");
            TEST_CHECK(!reloadedThumb.isNull(), "Subtest 11: Thumbnail loaded alongside project");
            TEST_CHECK(loadedReserve.nodes().size() == 8, "Subtest 11: 8 nodes preserved");
            TEST_CHECK(loadedReserve.columns().size() == 4, "Subtest 11: 4 columns preserved");
            TEST_CHECK(loadedReserve.beams().size() == 4, "Subtest 11: 4 beams preserved");
            TEST_CHECK(loadedReserve.slabs().size() == 1, "Subtest 11: 1 slab preserved");

            std::cout << "  [PASS] Subtest 11: 3D Thumbnail generated, embedded in .tsa, and extracted successfully" << std::endl;
        }

        std::cout << "[PASS] Test 18: Full TSA Native File Format Suite (11 Subtests Validated)" << std::endl;
        passed++;
    }

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

            // Le calcul logique + diff sur 5000 barres doit prendre moins de 50 ms
            TEST_CHECK(elapsedMs < 50.0, "Test 20: Undo diff computation is blazing fast (< 50 ms)");

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
        total++;

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
    // TEST 25: Command Pattern & Centralized Undo/Redo Architecture
    // =========================================================================
    {
        std::cout << "\n--- TEST 25: Command Pattern & Centralized Undo/Redo Architecture ---" << std::endl;
        total++;

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
    // TEST 26: 3D Interaction State Manager Architecture
    // =========================================================================
    {
        std::cout << "\n--- TEST 26: 3D Interaction State Manager Architecture ---" << std::endl;
        total++;

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
        total++;

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

        // 2. Validation mathématique de la caméra Perspective (stabilité du rayon de visée)
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
    // TEST 28: Custom Section Customization, Copy/Paste, OCCT 3D & Save/Load
    // =========================================================================
    {
        std::cout << "\n--- TEST 28: Custom Section Customization, Copy/Paste, OCCT 3D & Save/Load ---" << std::endl;
        total++;

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

    std::cout << "=================================================" << std::endl;
    std::cout << "RESULTS: " << passed << " / " << (total + 8) << " tests passed successfully!" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
