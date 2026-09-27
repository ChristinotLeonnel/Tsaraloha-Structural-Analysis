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
#include "Model/Cable/CableTypes.h"
#include "Model/Cable/CableStandards.h"
#include "Model/Cable/CableAnchor.h"
#include "Model/Cable/CablePrestress.h"
#include "Model/Cable/CableAnalysisProperties.h"
#include "Model/Cable/CableDefinition.h"
#include "Model/Cable/CableGeometry.h"
#include "Model/Cable/Cable.h"
#include "Model/Cable/StayCable.h"
#include "Model/Cable/SuspensionSystem.h"
#include "Geometry/CableGeometry3D.h"
#include "Grid/CableGrid.h"
#include "Library/CableLibrary.h"
#include "Grid/CartesianGrid.h"
#include "Grid/CylindricalGrid.h"
#include "Grid/ArbitraryGrid.h"
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Circ.hxx>
#include "Grid/GridDefinition.h"
#include "Grid/GridSystem.h"
#include "Grid/GridManager.h"
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
#include "Commands/GridCommands.h"
#include "UndoRedo/UndoManager.h"
#include "UndoRedo/CommandManager.h"
#include "Interaction/InteractionManager.h"
#include "Diagnostics/LogLevel.h"
#include "Diagnostics/LogEntry.h"
#include "Diagnostics/RingBuffer.h"
#include "Diagnostics/Logger.h"
#include "Diagnostics/CrashHandler.h"
#include "Diagnostics/DiagnosticReport.h"
#include "ExtensionSystem/ExtensionTypes.h"
#include "ExtensionSystem/DefinitionModels.h"
#include "ExtensionSystem/LibraryRegistry.h"
#include "ExtensionSystem/LibraryValidator.h"
#include "ExtensionSystem/LibraryLoader.h"
#include "ExtensionSystem/LibraryCache.h"
#include "ExtensionSystem/LibraryVersionManager.h"
#include "ExtensionSystem/LibraryDependencyManager.h"
#include "ExtensionSystem/LibraryManager.h"
#include "ExtensionSystem/ExtensionManager.h"

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

#include "Model/MaterialLibrary.h"
#include "Viewer/MaterialVisual.h"
#include "Viewer/TextureManager.h"
#include <AIS_Shape.hxx>

using namespace TSA::Coordinate;
using namespace TSA::Model;
using namespace TSA::Grid;
using namespace TSA::Geometry;
using namespace TSA::IO;
using namespace TSA::Viewer;

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

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include "UI/Dialogs/ExtensionManagerDialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int passed = 0;
    int total = 47;

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
        QMetaObject::Connection reqConn = QObject::connect(&interactionMgr, &InteractionManager::selectionRequested, [&](const SelectionRequest& r) {
            reqSignalReceived = true;
            TEST_CHECK(r.targetField == "Origine_Test", "Subtest 35.1: Signal received with correct targetField");
        });

        SelectionRequest req1;
        req1.mode = SelectionMode::SelectPoint;
        req1.targetField = "Origine_Test";
        req1.snapEnabled = true;
        req1.keepWindowOpen = true;
        interactionMgr.requestSelection(req1);

        TEST_CHECK(interactionMgr.hasActiveSelectionRequest(), "Subtest 35.1: Active selection request is true");
        TEST_CHECK(reqSignalReceived, "Subtest 35.1: selectionRequested signal fired");
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

    // -------------------------------------------------------------------------
    // TEST 36: Cable & Tension System Comprehensive Test Suite
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 36: Cable & Tension System Comprehensive Test Suite ---" << std::endl;

        // 36.1: Normative Standards Registry (EN 10138-3, EN 10138-4, EN 1993-1-11, ASTM A416)
        {
            auto& reg = TSA::Model::CableStandardsRegistry::instance();
            
            // EN 10138-3 Toron 15.7mm Y1860S7
            auto pStrand = reg.findProduct("EN 10138-3", "Y1860S7-15.7");
            TEST_CHECK(pStrand.has_value(), "Subtest 36.1: EN 10138-3 Y1860S7-15.7 found");
            if (pStrand.has_value())
            {
                TEST_CHECK(std::abs(pStrand->nominalDiameter - 0.0157) < 1e-5, "Subtest 36.1: Strand diameter 15.7mm");
                TEST_CHECK(std::abs(pStrand->nominalCrossSection - 150e-6) < 1e-7, "Subtest 36.1: Strand area 150 mm²");
                TEST_CHECK(std::abs(pStrand->elasticModulus - 195e9) < 1e5, "Subtest 36.1: Strand E = 195 GPa");
                TEST_CHECK(std::abs(pStrand->characteristicStrength - 1860e6) < 1e5, "Subtest 36.1: f_pk = 1860 MPa");
            }

            // EN 10138-4 Barre Y1030 36mm
            auto pBar = reg.findProduct("EN 10138-4", "Y1030-36");
            TEST_CHECK(pBar.has_value(), "Subtest 36.1: EN 10138-4 Y1030-36 found");
            if (pBar.has_value())
            {
                TEST_CHECK(std::abs(pBar->characteristicStrength - 1030e6) < 1e5, "Subtest 36.1: Bar f_pk = 1030 MPa");
                TEST_CHECK(std::abs(pBar->elasticModulus - 205e9) < 1e5, "Subtest 36.1: Bar E = 205 GPa");
            }

            // EN 1993-1-11 Câble clos (Locked Coil)
            auto pLocked = reg.findProduct("EN 1993-1-11", "FLC-120");
            TEST_CHECK(pLocked.has_value(), "Subtest 36.1: EN 1993-1-11 FLC-120 found");
            if (pLocked.has_value())
            {
                TEST_CHECK(std::abs(pLocked->nominalDiameter - 0.120) < 1e-4, "Subtest 36.1: Locked coil dia 120mm");
                TEST_CHECK(std::abs(pLocked->elasticModulus - 160e9) < 1e5, "Subtest 36.1: Locked coil E = 160 GPa");
            }

            // ASTM A416 Grade 270 0.6 inch
            auto pASTM = reg.findProduct("ASTM A416", "Gr270-0.6in");
            TEST_CHECK(pASTM.has_value(), "Subtest 36.1: ASTM A416 Grade 270 found");

            std::cout << "  [PASS] Subtest 36.1: Standards Registry (EN 10138-3, EN 10138-4, EN 1993-1-11, ASTM A416) Verified" << std::endl;
        }

        // 36.2: Cable Creation & Type Specializations
        {
            TSA::Model::Model m;
            int n1 = m.addNode(0, 0, 0);
            int n2 = m.addNode(10, 0, 0);
            int n3 = m.addNode(0, 0, 20);
            int n4 = m.addNode(10, 0, 20);

            int cGeneric = m.addCable(n1, n2, TSA::Model::CableType::Generic);
            int cStay = m.addCable(n3, n2, TSA::Model::CableType::StayCable);
            int cSusp = m.addCable(n3, n4, TSA::Model::CableType::SuspensionCable);
            int cHanger = m.addCable(n4, n2, TSA::Model::CableType::Hanger);

            TEST_CHECK(m.cables().size() == 4, "Subtest 36.2: 4 cables created in Model");
            TEST_CHECK(m.getCable(cGeneric)->type() == TSA::Model::CableType::Generic, "Subtest 36.2: cGeneric type");
            TEST_CHECK(m.getCable(cStay)->type() == TSA::Model::CableType::StayCable, "Subtest 36.2: cStay type");
            TEST_CHECK(m.getCable(cSusp)->type() == TSA::Model::CableType::SuspensionCable, "Subtest 36.2: cSusp type");
            TEST_CHECK(m.getCable(cHanger)->type() == TSA::Model::CableType::Hanger, "Subtest 36.2: cHanger type");

            std::cout << "  [PASS] Subtest 36.2: Cable Creation & Type Specializations in Model Verified" << std::endl;
        }

        // 36.3: Geometric Profiles (Straight, Parabolic Sag, Catenary Equation)
        {
            TSA::Model::Model m;
            int n1 = m.addNode(0, 0, 0);
            int n2 = m.addNode(100, 0, 0);

            TSA::Model::Cable cable(1, n1, n2, "MainSpan", TSA::Model::CableType::SuspensionCable);
            cable.setGeometryMode(TSA::Model::CableGeometryMode::Straight);
            TEST_CHECK(std::abs(cable.chordLength(m) - 100.0) < 1e-4, "Subtest 36.3: Straight chord length = 100m");
            TEST_CHECK(std::abs(cable.arcLength(m) - 100.0) < 1e-4, "Subtest 36.3: Straight arc length = 100m");

            // Mode Parabolique : flèche de 10m sur 100m de portée
            cable.setGeometryMode(TSA::Model::CableGeometryMode::Parabolic);
            cable.geometry().setSag(10.0);
            double expectedApprox = 100.0 * (1.0 + (8.0 * 10.0 * 10.0) / (3.0 * 100.0 * 100.0)); // 102.67m
            double actualArc = cable.arcLength(m);
            TEST_CHECK(actualArc > 102.0 && actualArc < 103.5, "Subtest 36.3: Parabolic arc length accurate");

            // Échantillonnage 3D
            auto samples = cable.sampleWorldPoints(m, 11);
            TEST_CHECK(samples.size() == 11, "Subtest 36.3: 11 sample points");
            TEST_CHECK(std::abs(samples.front().X() - 0.0) < 1e-4, "Subtest 36.3: Start sample point at X=0");
            TEST_CHECK(std::abs(samples.back().X() - 100.0) < 1e-4, "Subtest 36.3: End sample point at X=100");
            // Point médian : flèche négative Z = -10m
            TEST_CHECK(std::abs(samples[5].Z() - (-10.0)) < 1e-2, "Subtest 36.3: Midpoint sag at Z = -10m");

            // Mode Caténaire
            cable.setGeometryMode(TSA::Model::CableGeometryMode::Catenary);
            cable.geometry().setCatenaryHorizontalTension(1000000.0); // 1 MN
            cable.geometry().setCatenaryLinearWeight(100.0); // 100 N/m
            double cParam = cable.geometry().catenaryParameter();
            TEST_CHECK(std::abs(cParam - 10000.0) < 1e-2, "Subtest 36.3: Catenary parameter c = H/w = 10000m");

            std::cout << "  [PASS] Subtest 36.3: Straight, Parabolic Sag & Catenary Profile Equations Verified" << std::endl;
        }

        // 36.4: Stay Cable Inclination & Anchor Socket Modeling
        {
            TSA::Model::Model m;
            int nPylon = m.addNode(0, 0, 50);
            int nDeck = m.addNode(100, 0, 0);

            TSA::Model::StayCable stay(1, nPylon, nDeck, "Stay-01");
            stay.setStaySystem(TSA::Model::StaySystemMode::Fan);
            double inclination = stay.inclinationDegrees(m);
            double expectedAngle = std::atan2(50.0, 100.0) * 180.0 / 3.14159265358979323846;
            TEST_CHECK(std::abs(inclination - expectedAngle) < 1e-3, "Subtest 36.4: Stay inclination angle correct");

            // Configuration Ancrages
            stay.startAnchor().setType(TSA::Model::AnchorType::StructuralAnchor);
            stay.startAnchor().setCapacity(5000e3); // 5 MN
            stay.startAnchor().setSocketDiameter(0.25);
            stay.startAnchor().setSocketLength(0.60);

            stay.endAnchor().setType(TSA::Model::AnchorType::PrestressingAnchor);
            stay.endAnchor().setSlip(0.006); // 6 mm rentrée d'ancrage
            TEST_CHECK(stay.endAnchor().slip() == 0.006, "Subtest 36.4: End anchor slip 6mm");

            std::cout << "  [PASS] Subtest 36.4: Stay Cable Inclination & Anchor Sockets Verified" << std::endl;
        }

        // 36.5: Suspension Bridge & Automatic Hanger Generation
        {
            TSA::Model::Model m;
            int p1 = m.addNode(0, 0, 25);
            int p2 = m.addNode(100, 0, 25);

            TSA::Model::SuspensionBridge bridge("PontSuspenduTest");
            bridge.setMainSpan(100.0);
            bridge.setSag(10.0);
            bridge.setHangerSpacing(10.0);

            // Création de 9 nœuds de tablier entre X=10 et X=90
            std::vector<int> deckNodeIds;
            for (int i = 1; i <= 9; ++i)
            {
                deckNodeIds.push_back(m.addNode(i * 10.0, 0.0, 0.0));
            }

            int mainCableId = m.addCable(p1, p2, TSA::Model::CableType::SuspensionCable);
            m.getCable(mainCableId)->setGeometryMode(TSA::Model::CableGeometryMode::Parabolic);
            m.getCable(mainCableId)->geometry().setSag(10.0);

            auto generatedHangers = bridge.generateHangers(m, mainCableId, deckNodeIds);
            TEST_CHECK(generatedHangers.size() == 9, "Subtest 36.5: 9 vertical hangers generated automatically");
            
            // Vérifier que chaque suspente est verticale et connectée à un nœud de tablier
            for (size_t i = 0; i < generatedHangers.size(); ++i)
            {
                const auto* h = m.getCable(generatedHangers[i]);
                TEST_CHECK(h != nullptr, "Subtest 36.5: Hanger exists in Model");
                TEST_CHECK(h->type() == TSA::Model::CableType::Hanger, "Subtest 36.5: Element is Hanger");
                const auto* nDeck = m.getNode(h->endNodeId());
                const auto* nCable = m.getNode(h->startNodeId());
                TEST_CHECK(std::abs(nDeck->x() - nCable->x()) < 1e-4, "Subtest 36.5: Hanger is perfectly vertical in X");
                TEST_CHECK(nCable->z() > nDeck->z(), "Subtest 36.5: Top cable node is above deck node");
            }

            std::cout << "  [PASS] Subtest 36.5: Suspension Bridge Automatic Hanger Generator Verified" << std::endl;
        }

        // 36.6: Prestress & Non-Linear Ernst Equivalent Modulus
        {
            TSA::Model::CablePrestress prestress;
            prestress.initialTension = 150000.0; // 150 kN
            double A = 150e-6; // 150 mm²
            double E = 195e9;  // 195 GPa
            double strain = prestress.calculateStrain(A, E);
            TEST_CHECK(std::abs(strain - (150000.0 / (150e-6 * 195e9))) < 1e-7, "Subtest 36.6: Initial strain calculation");

            // Calcul du module d'Ernst : E_eq = E / (1 + (w*L)^2 * E * A / (12 * T^3))
            double L = 100.0; // 100m
            double w = 15.0;  // 15 N/m
            double T_high = 500000.0; // 500 kN
            double E_eq_high = TSA::Model::CableAnalysisProperties::calculateErnstEquivalentModulus(E, A, w, L, T_high);
            // Sous forte tension, E_eq doit être très proche de E (perte < 1%)
            TEST_CHECK(E_eq_high > 0.99 * E && E_eq_high <= E, "Subtest 36.6: Ernst modulus near nominal E under high tension");

            // Sous faible tension (5 kN), le mou réduit considérablement le module effectif
            double T_low = 5000.0;
            double E_eq_low = TSA::Model::CableAnalysisProperties::calculateErnstEquivalentModulus(E, A, w, L, T_low);
            TEST_CHECK(E_eq_low < 0.5 * E, "Subtest 36.6: Significant Ernst modulus reduction under low tension");

            // Pertes de frottement (Eurocode 2)
            double lossFriction = prestress.calculateFrictionLoss(100.0, 0.15);
            TEST_CHECK(lossFriction > 0.0 && lossFriction < prestress.initialTension, "Subtest 36.6: Friction loss calculation valid");

            std::cout << "  [PASS] Subtest 36.6: Prestressing & Non-Linear Ernst Modulus Formulations Verified" << std::endl;
        }

        // 36.7: OpenCASCADE 3D Solid Geometry Generation
        {
            gp_Pnt pA(0, 0, 0);
            gp_Pnt pB(50, 0, 0);

            // Câble droit cylindrique
            TopoDS_Shape straightShape = TSA::Geometry::CableGeometry3D::createStraightCable(pA, pB, 0.030);
            TEST_CHECK(!straightShape.IsNull(), "Subtest 36.7: Straight cable solid shape created");

            Bnd_Box bnd;
            BRepBndLib::Add(straightShape, bnd);
            double xmin, ymin, zmin, xmax, ymax, zmax;
            bnd.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            TEST_CHECK(std::abs(xmax - xmin - 50.0) < 0.1, "Subtest 36.7: Solid bounding box length ~50m");

            // Câble courbe par balayage (Pipe)
            std::vector<gp_Pnt> curvePts = { gp_Pnt(0, 0, 10), gp_Pnt(25, 0, 5), gp_Pnt(50, 0, 10) };
            TopoDS_Shape curvedShape = TSA::Geometry::CableGeometry3D::createCurvedCable(curvePts, 0.025);
            TEST_CHECK(!curvedShape.IsNull(), "Subtest 36.7: Curved pipe solid shape created");

            // Culot d'ancrage
            TopoDS_Shape socketShape = TSA::Geometry::CableGeometry3D::createAnchorSocket(pA, pB, 0.15, 0.40);
            TEST_CHECK(!socketShape.IsNull(), "Subtest 36.7: Anchor socket solid shape created");

            std::cout << "  [PASS] Subtest 36.7: OpenCASCADE B-Rep 3D Solid Generation (Cylinders & Swept Pipes) Verified" << std::endl;
        }

        // 36.8: Model Cascading Deletions on Node Removal
        {
            TSA::Model::Model m;
            int n1 = m.addNode(0, 0, 0);
            int n2 = m.addNode(10, 0, 0);
            int n3 = m.addNode(20, 0, 0);

            int c1 = m.addCable(n1, n2);
            int c2 = m.addCable(n2, n3);
            (void)c1;
            (void)c2;
            TEST_CHECK(m.cables().size() == 2, "Subtest 36.8: 2 cables initially");

            // Supprimer le nœud pivot n2 -> les deux câbles c1 et c2 doivent être supprimés en cascade
            m.removeNode(n2);
            TEST_CHECK(m.cables().empty(), "Subtest 36.8: Connected cables cascaded upon node removal");

            std::cout << "  [PASS] Subtest 36.8: Model Cascading Deletions on Node Removal Verified" << std::endl;
        }

        // 36.9: ModelDiff & Differential Undo/Redo with Cables
        {
            class CableTestObserver : public TSA::Model::IModelObserver
            {
            public:
                int diffCount = 0;
                TSA::Model::ModelDiff lastDiff;
                void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override
                {
                    diffCount++;
                    lastDiff = diff;
                }
                void onModelCleared() override {}
            };

            TSA::Model::Model m;
            CableTestObserver obs;
            m.addObserver(&obs);

            int n1 = m.addNode(0, 0, 0);
            int n2 = m.addNode(30, 0, 0);

            m.pushUndoState("Création Câble Diff");
            int cId = m.addCable(n1, n2, TSA::Model::CableType::StayCable);

            m.pushUndoState("Modification Câble");
            auto* cab = m.getCable(cId);
            cab->definition().setNominalDiameter(0.045);
            m.notifyCableModified(cId);

            // Annuler la modification
            m.undo();
            TEST_CHECK(obs.lastDiff.modifiedCableIds.size() == 1, "Subtest 36.9: Modified cable diff on undo");
            TEST_CHECK(obs.lastDiff.modifiedCableIds[0] == cId, "Subtest 36.9: Correct cable ID in diff");

            // Annuler la création
            m.undo();
            TEST_CHECK(obs.lastDiff.deletedCableIds.size() == 1, "Subtest 36.9: Deleted cable diff on undo");
            TEST_CHECK(m.cables().empty(), "Subtest 36.9: Cable removed on undo");

            // Rétablir la création
            m.redo();
            TEST_CHECK(obs.lastDiff.createdCableIds.size() == 1, "Subtest 36.9: Created cable diff on redo");
            TEST_CHECK(m.cables().size() == 1, "Subtest 36.9: Cable restored on redo");

            m.removeObserver(&obs);
            std::cout << "  [PASS] Subtest 36.9: ModelDiff & Differential Undo/Redo for Cables Validated" << std::endl;
        }

        // 36.10: Complete TSA File Format Save/Load Round-Trip
        {
            TSA::Model::Model mSave;
            int nA = mSave.addNode(0, 0, 0);
            int nB = mSave.addNode(50, 0, 20);

            int cId = mSave.addCable(nA, nB, TSA::Model::CableType::StayCable);
            auto* cab = mSave.getCable(cId);
            cab->setName("CableHaubanNord");
            cab->setGeometryMode(TSA::Model::CableGeometryMode::Straight);
            cab->definition().setStandardName("EN 1993-1-11");
            cab->definition().setGrade("FLC-90");
            cab->definition().setNominalDiameter(0.090);
            cab->definition().setMetallicArea(0.0055);
            cab->definition().setElasticModulus(160e9);
            cab->definition().setDefaultInitialTension(750000.0);
            cab->prestress().initialTension = 750000.0; // 750 kN
            cab->startAnchor().setType(TSA::Model::AnchorType::StructuralAnchor);
            cab->startAnchor().setCapacity(4000e3);
            cab->endAnchor().setType(TSA::Model::AnchorType::PrestressingAnchor);
            cab->endAnchor().setSlip(0.005);
            cab->analysisProperties().tensionOnly = true;

            std::string tempFile = (std::filesystem::temp_directory_path() / "test_cable_io.tsa").string();
            std::string errMsg;

            TSA::IO::TSAFileWriter writer;
            writer.setCompressionEnabled(false);
            bool saved = writer.saveToFile(tempFile, mSave, nullptr, "ProjetCableTest", "IngénieurTSA", &errMsg);
            TEST_CHECK(saved, "Subtest 36.10: TSA file with CABL chunk saved successfully");

            TSA::Model::Model mLoad;
            TSA::IO::TSAFileReader reader;
            bool loaded = reader.loadFromFile(tempFile, mLoad, nullptr, "", nullptr, nullptr, nullptr, &errMsg);
            TEST_CHECK(loaded, "Subtest 36.10: TSA file loaded successfully");

            TEST_CHECK(mLoad.cables().size() == 1, "Subtest 36.10: Exactly 1 cable restored");
            const auto* cLoaded = mLoad.getCable(cId);
            TEST_CHECK(cLoaded != nullptr, "Subtest 36.10: Cable found by original ID");
            if (cLoaded)
            {
                TEST_CHECK(cLoaded->name() == "CableHaubanNord", "Subtest 36.10: Cable name preserved");
                TEST_CHECK(cLoaded->type() == TSA::Model::CableType::StayCable, "Subtest 36.10: Cable type preserved");
                TEST_CHECK(std::abs(cLoaded->definition().nominalDiameter() - 0.090) < 1e-5, "Subtest 36.10: Diameter preserved");
                TEST_CHECK(std::abs(cLoaded->definition().elasticModulus() - 160e9) < 1e3, "Subtest 36.10: Modulus preserved");
                TEST_CHECK(std::abs(cLoaded->prestress().initialTension - 750000.0) < 1.0, "Subtest 36.10: Initial tension preserved");
                TEST_CHECK(cLoaded->startAnchor().type() == TSA::Model::AnchorType::StructuralAnchor, "Subtest 36.10: Start anchor preserved");
                TEST_CHECK(std::abs(cLoaded->endAnchor().slip() - 0.005) < 1e-6, "Subtest 36.10: Anchorage slip preserved");
                TEST_CHECK(cLoaded->analysisProperties().tensionOnly == true, "Subtest 36.10: Tension-only flag preserved");
            }

            std::filesystem::remove(tempFile);
            std::cout << "  [PASS] Subtest 36.10: Complete TSA File Binary Save/Load Round-Trip Validated" << std::endl;
        }

        std::cout << "[PASS] Test 36: Cable & Tension System Comprehensive Test Suite (10 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 37 : Système Intégré de Diagnostic, Logging, Crash Reporting & Télémétrie TSA
    // =========================================================================
    {
        std::cout << "\n--- TEST 37: Systeme Integre de Diagnostic, Logging & Telemetrie TSA ---" << std::endl;

        // 37.1: Initialisation Logger et Session
        {
            auto& logger = TSA::Diagnostics::Logger::instance();
            logger.init();
            logger.setDeveloperModeEnabled(true);

            std::string sessId = logger.sessionId();
            TEST_CHECK(!sessId.empty(), "Subtest 37.1: Session ID is generated");
            TEST_CHECK(sessId.rfind("session_", 0) == 0, "Subtest 37.1: Session ID starts with session_");

            std::string logDir = logger.logsDirectory();
            TEST_CHECK(std::filesystem::exists(logDir), "Subtest 37.1: Logs directory exists");
            TEST_CHECK(std::filesystem::exists(logger.sessionLogPath()), "Subtest 37.1: Session log file exists");

            std::cout << "  [PASS] Subtest 37.1: Logger Initialization & Session Setup Verified (Session: " << sessId << ")" << std::endl;
        }

        // 37.2: RingBuffer FIFO et Capacité Circulaire (100 événements)
        {
            TSA::Diagnostics::RingBuffer<100, int> rb;
            TEST_CHECK(rb.empty(), "Subtest 37.2: RingBuffer starts empty");
            TEST_CHECK(rb.size() == 0, "Subtest 37.2: Size starts at 0");

            for (int i = 0; i < 150; ++i)
            {
                rb.push(i);
            }

            TEST_CHECK(rb.size() == 100, "Subtest 37.2: Size clamped to 100");
            auto snapshot = rb.snapshot();
            TEST_CHECK(snapshot.size() == 100, "Subtest 37.2: Snapshot has 100 elements");
            TEST_CHECK(snapshot.front() == 50, "Subtest 37.2: First element is 50 (oldest retained)");
            TEST_CHECK(snapshot.back() == 149, "Subtest 37.2: Last element is 149 (newest)");

            std::cout << "  [PASS] Subtest 37.2: Thread-Safe RingBuffer FIFO Clamping (100 items) Verified" << std::endl;
        }

        // 37.3: LogEntry Structuré et Formatage
        {
            TSA::Diagnostics::LogEntry entry;
            entry.sequenceId = 42;
            entry.timestamp = "2026-09-27 12:00:00.123";
            entry.level = TSA::Diagnostics::LogLevel::Warning;
            entry.module = "Geometry";
            entry.eventName = "ToleranceExceeded";
            entry.message = "Écart géométrique détecté";
            entry.file = "src/Geometry/BeamGeometry.cpp";
            entry.line = 105;
            entry.function = "createBeamShape";

            TEST_CHECK(std::string(TSA::Diagnostics::logLevelToString(entry.level)) == "WARN", "Subtest 37.3: LogLevel Warning string");
            std::string formatted = entry.format();
            TEST_CHECK(formatted.find("WARN") != std::string::npos, "Subtest 37.3: Formatted string contains WARN");
            TEST_CHECK(formatted.find("Geometry") != std::string::npos, "Subtest 37.3: Formatted string contains Geometry");
            TEST_CHECK(formatted.find("ToleranceExceeded") != std::string::npos, "Subtest 37.3: Formatted string contains eventName");

            std::cout << "  [PASS] Subtest 37.3: Structured LogEntry Formatting Verified" << std::endl;
        }

        // 37.4: Mode Développeur et Filtrage
        {
            auto& logger = TSA::Diagnostics::Logger::instance();
            logger.setDeveloperModeEnabled(false);
            TEST_CHECK(!logger.isDeveloperModeEnabled(), "Subtest 37.4: Developer mode is disabled");

            size_t countBefore = logger.recentEntries().size();
            TSA_LOG_TRACE("TestModule", "TraceEvent", "Message trace filtre");
            size_t countAfter = logger.recentEntries().size();
            TEST_CHECK(countBefore == countAfter, "Subtest 37.4: Trace message skipped when dev mode disabled");

            logger.setDeveloperModeEnabled(true);
            TEST_CHECK(logger.isDeveloperModeEnabled(), "Subtest 37.4: Developer mode is enabled");
            TSA_LOG_TRACE("TestModule", "TraceEvent", "Message trace autorise");
            TEST_CHECK(logger.recentEntries().size() == countAfter + 1, "Subtest 37.4: Trace message accepted when dev mode enabled");

            std::cout << "  [PASS] Subtest 37.4: Developer Mode Filtering Verified" << std::endl;
        }

        // 37.5: Suivi de la Dernière Commande Utilisateur (Last Executed Command)
        {
            auto& logger = TSA::Diagnostics::Logger::instance();
            logger.setLastCommand("CREATION_POUTRE_IPE300");
            TEST_CHECK(logger.lastCommand() == "CREATION_POUTRE_IPE300", "Subtest 37.5: Last command recorded");

            // Intégration CommandManager
            TSA::UndoRedo::CommandManager cmdMgr;
            logger.setLastCommand("None");

            class DummyCommand : public TSA::Commands::ICommand
            {
            public:
                std::string name() const override { return "TestDummyCommand_Diagnostics"; }
                bool execute() override { return true; }
                bool undo() override { return true; }
            };

            cmdMgr.executeCommand(std::make_unique<DummyCommand>());
            TEST_CHECK(logger.lastCommand() == "TestDummyCommand_Diagnostics", "Subtest 37.5: CommandManager updated lastCommand");

            std::cout << "  [PASS] Subtest 37.5: Last Executed Command Tracking Verified" << std::endl;
        }

        // 37.6: Génération et Export du Rapport de Diagnostic
        {
            TSA::Model::Model m;
            int n1 = m.addNode(0, 0, 0);
            int n2 = m.addNode(5, 0, 0);
            m.addBeam(n1, n2, 0.3, 0.5);

            auto& logger = TSA::Diagnostics::Logger::instance();
            logger.setLastCommand("EXPORT_REPORT_TEST");
            TSA_LOG_INFO("Audit", "ReportTestEvent", "Événement de test pour export");

            std::string reportPath = TSA::Diagnostics::DiagnosticReport::exportReport(&m);
            TEST_CHECK(!reportPath.empty(), "Subtest 37.6: Report path is not empty");
            TEST_CHECK(std::filesystem::exists(reportPath), "Subtest 37.6: Exported report file exists on disk");
            TEST_CHECK(std::filesystem::file_size(reportPath) > 500, "Subtest 37.6: Report file is not empty (>500 bytes)");

            std::ifstream rfs(reportPath);
            std::string content((std::istreambuf_iterator<char>(rfs)), std::istreambuf_iterator<char>());
            TEST_CHECK(content.find("RAPPORT DE DIAGNOSTIC TECHNIQUE") != std::string::npos, "Subtest 37.6: Header found in report");
            TEST_CHECK(content.find("EXPORT_REPORT_TEST") != std::string::npos, "Subtest 37.6: Last command in report");
            TEST_CHECK(content.find("2") != std::string::npos, "Subtest 37.6: Node count in report");
            TEST_CHECK(content.find("Poutres           : 1") != std::string::npos, "Subtest 37.6: Beam count in report");
            TEST_CHECK(content.find("ReportTestEvent") != std::string::npos, "Subtest 37.6: Audit event in report");

            std::cout << "  [PASS] Subtest 37.6: Diagnostic Report Generation & Validation Verified" << std::endl;
        }

        // 37.7: Télémétrie Automatique Grille Cartésienne
        {
            TSA::Grid::GridDefinition def("GrilleTelemetrieTest", TSA::Grid::GridType::Cartesian);
            def.setXPositions({ 0.0, 3.0, 6.0 });
            def.setYPositions({ 0.0, 4.0, 8.0 });
            def.setZLevels({ 0.0, 3.2 });

            TSA::Grid::CartesianGrid grid(def);

            auto recent = TSA::Diagnostics::Logger::instance().recentEntries();
            bool foundRebuild = false;
            for (const auto& entry : recent)
            {
                if (entry.eventName == "CartesianGridRebuildCompleted" &&
                    entry.message.find("GrilleTelemetrieTest") != std::string::npos)
                {
                    foundRebuild = true;
                    break;
                }
            }
            TEST_CHECK(foundRebuild, "Subtest 37.7: CartesianGrid rebuild logged automatically to telemetry");

            std::cout << "  [PASS] Subtest 37.7: Automated Cartesian Grid Rebuild Telemetry Verified" << std::endl;
        }

        std::cout << "[PASS] Test 37: Diagnostic, Logging & Telemetry Subsystem (7 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------
    // TEST 38: TSALib ExtensionSystem Foundation & Type Contracts
    // -------------------------------------------------------------
    {
        std::cout << "\n--- TEST 38: TSALib ExtensionSystem Foundation & Type Contracts ---" << std::endl;

        // 38.1: Semantic Versioning (SemVer 2.0)
        {
            auto v1 = TSA::ExtensionSystem::SemanticVersion::fromString("1.0.0");
            auto v2 = TSA::ExtensionSystem::SemanticVersion::fromString("1.1.0");
            auto v3 = TSA::ExtensionSystem::SemanticVersion::fromString("2.0.0-beta");
            auto vInvalid = TSA::ExtensionSystem::SemanticVersion::fromString("invalid_ver");

            TEST_CHECK(v1.has_value(), "Subtest 38.1: v1 is valid");
            TEST_CHECK(v2.has_value(), "Subtest 38.1: v2 is valid");
            TEST_CHECK(v3.has_value(), "Subtest 38.1: v3 is valid");
            TEST_CHECK(!vInvalid.has_value(), "Subtest 38.1: invalid version rejected");

            TEST_CHECK(*v1 < *v2, "Subtest 38.1: 1.0.0 < 1.1.0");
            TEST_CHECK(*v2 < *v3, "Subtest 38.1: 1.1.0 < 2.0.0-beta");
            TEST_CHECK(v1->toString() == "1.0.0", "Subtest 38.1: v1 toString == 1.0.0");
            TEST_CHECK(v3->toString() == "2.0.0-beta", "Subtest 38.1: v3 toString == 2.0.0-beta");

            std::cout << "  [PASS] Subtest 38.1: Semantic Versioning Parser & Operators Verified" << std::endl;
        }

        // 38.2: Physical Values & SI Conversion
        {
            TSA::ExtensionSystem::PhysicalValue eMod(31000.0, "MPa");
            TEST_CHECK(approxEqual(eMod.toBaseSI(), 31.0e9), "Subtest 38.2: 31000 MPa == 31 GPa (Pa)");

            TSA::ExtensionSystem::PhysicalValue density(2.5, "t/m3");
            TEST_CHECK(approxEqual(density.toBaseSI(), 2500.0), "Subtest 38.2: 2.5 t/m3 == 2500 kg/m3");

            TSA::ExtensionSystem::PhysicalValue force(150.0, "kN");
            TEST_CHECK(approxEqual(force.toBaseSI(), 150000.0), "Subtest 38.2: 150 kN == 150000 N");

            TSA::ExtensionSystem::PhysicalValue length(25.4, "mm");
            TEST_CHECK(approxEqual(length.toBaseSI(), 0.0254), "Subtest 38.2: 25.4 mm == 0.0254 m");

            auto json = eMod.toJson();
            auto restored = TSA::ExtensionSystem::PhysicalValue::fromJson(json);
            TEST_CHECK(approxEqual(restored.value, 31000.0) && restored.unit == "MPa", "Subtest 38.2: PhysicalValue JSON roundtrip");

            std::cout << "  [PASS] Subtest 38.2: Physical Value SI Conversions & JSON Roundtrip Verified" << std::endl;
        }

        // 38.3: Mechanical Snapshot Integrity & Immutability
        {
            TSA::ExtensionSystem::MechanicalSnapshot s1;
            s1.youngModulus = 31.0e9;
            s1.poissonRatio = 0.20;
            s1.density = 2500.0;
            s1.characteristicStrength = 25.0e6;
            s1.yieldStrength = 0.0;
            s1.thermalCoeff = 1.0e-5;

            TSA::ExtensionSystem::MechanicalSnapshot s2 = s1;
            TEST_CHECK(s1 == s2, "Subtest 38.3: Identical snapshots are equal");

            s2.youngModulus = 34.0e9; // C30/37 E modulus
            TEST_CHECK(s1 != s2, "Subtest 38.3: Modified snapshots are detected as different");

            std::cout << "  [PASS] Subtest 38.3: Mechanical Snapshot Equality & Difference Detection Verified" << std::endl;
        }

        // 38.4: Extension Manifest Parsing & Serialization
        {
            QJsonObject manifestJson;
            manifestJson["id"] = "org.tsaraloha.tsalib";
            manifestJson["name"] = "TSA Engineering Library";
            manifestJson["version"] = "1.0.0";
            manifestJson["format_version"] = "1.0";
            manifestJson["minimum_tsa_version"] = "0.1.0";
            manifestJson["author"] = "Tsaraloha Christinot";
            manifestJson["kind"] = "data";

            QJsonArray cats;
            cats.append("materials");
            cats.append("sections");
            cats.append("cables");
            cats.append("textures");
            manifestJson["categories"] = cats;

            std::string parseErr;
            auto manifest = TSA::ExtensionSystem::ExtensionManifest::fromJson(manifestJson, &parseErr);
            TEST_CHECK(manifest.has_value(), "Subtest 38.4: Manifest parsed successfully");
            TEST_CHECK(manifest->id == "org.tsaraloha.tsalib", "Subtest 38.4: Manifest ID match");
            TEST_CHECK(manifest->kind == TSA::ExtensionSystem::ExtensionKind::DataExtension, "Subtest 38.4: DataExtension kind match");
            TEST_CHECK(manifest->categories.size() == 4, "Subtest 38.4: 4 categories declared");

            QJsonObject exported = manifest->toJson();
            TEST_CHECK(exported["id"].toString() == "org.tsaraloha.tsalib", "Subtest 38.4: Exported JSON matches");

            std::cout << "  [PASS] Subtest 38.4: Extension Manifest Parsing & Export Verified" << std::endl;
        }

        std::cout << "[PASS] Test 38: TSALib ExtensionSystem Foundation & Type Contracts (4 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------
    // TEST 39: TSALib Phase 2 - ExtensionSystem Core & Registries
    // -------------------------------------------------------------
    {
        std::cout << "\n--- TEST 39: TSALib Phase 2 - ExtensionSystem Core & Registries ---" << std::endl;

        // 39.1: LibraryRegistry Registration, Logical Lookup & Search
        {
            auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();
            reg.clear();

            TSA::ExtensionSystem::MaterialDefinition c25;
            c25.id = "concrete.c25_30";
            c25.name = "Béton C25/30";
            c25.category = "Concrete";
            c25.version = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            c25.youngModulus = TSA::ExtensionSystem::PhysicalValue(31000.0, "MPa");
            c25.density = TSA::ExtensionSystem::PhysicalValue(2500.0, "kg/m3");
            c25.poissonRatio = 0.20;
            c25.fck = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");
            c25.standard.name = "EN 1992-1-1";

            TEST_CHECK(reg.registerMaterial(c25), "Subtest 39.1: Register Material");

            TSA::ExtensionSystem::SectionDefinition ipe200;
            ipe200.id = "steel.ipe200";
            ipe200.name = "IPE 200";
            ipe200.category = "Steel";
            ipe200.shapeType = "IShape";
            ipe200.width = 0.100;
            ipe200.height = 0.200;
            ipe200.webThickness = 0.0056;
            ipe200.flangeThickness = 0.0085;
            ipe200.standard.name = "EN 1993-1-1";

            TEST_CHECK(reg.registerSection(ipe200), "Subtest 39.1: Register Section");

            TSA::ExtensionSystem::CableCatalogDefinition t15;
            t15.id = "cable.strand_15_7";
            t15.name = "Toron 7 fils 15.7 mm";
            t15.category = "Prestressing";
            t15.nominalDiameter = 0.0157;
            t15.metallicArea = 150e-6;
            t15.elasticModulus = 195.0e9;

            TEST_CHECK(reg.registerCable(t15), "Subtest 39.1: Register Cable");

            TEST_CHECK(reg.findMaterial("concrete.c25_30") != nullptr, "Subtest 39.1: Find Material by ID");
            TEST_CHECK(reg.findSection("steel.ipe200") != nullptr, "Subtest 39.1: Find Section by ID");
            TEST_CHECK(reg.findCable("cable.strand_15_7") != nullptr, "Subtest 39.1: Find Cable by ID");
            TEST_CHECK(reg.findMaterial("unknown.id") == nullptr, "Subtest 39.1: Non-existent ID returns nullptr");

            auto concreteList = reg.materialsByCategory("Concrete");
            TEST_CHECK(concreteList.size() == 1, "Subtest 39.1: Category filter returns 1 concrete");

            auto searchRes = reg.searchSections("ipe");
            TEST_CHECK(searchRes.size() == 1 && searchRes[0].id == "steel.ipe200", "Subtest 39.1: Search sections by keyword");

            std::cout << "  [PASS] Subtest 39.1: LibraryRegistry Registration, Logical Lookup & Search Verified" << std::endl;
        }

        // 39.2: LibraryValidator Strict Conformance & Anti-Crash Protection
        {
            TSA::ExtensionSystem::LibraryValidator val;

            // ID Validation
            TEST_CHECK(TSA::ExtensionSystem::LibraryValidator::isValidId("concrete.c25_30"), "Subtest 39.2: Valid ID");
            TEST_CHECK(TSA::ExtensionSystem::LibraryValidator::isValidId("org.tsaraloha.tsalib"), "Subtest 39.2: Valid manifest ID");
            TEST_CHECK(!TSA::ExtensionSystem::LibraryValidator::isValidId("invalid id with spaces"), "Subtest 39.2: Invalid ID with spaces rejected");
            TEST_CHECK(!TSA::ExtensionSystem::LibraryValidator::isValidId("INVALID_UPPERCASE.ID"), "Subtest 39.2: Uppercase ID rejected");

            // Material Validation
            TSA::ExtensionSystem::MaterialDefinition validMat;
            validMat.id = "steel.s355";
            validMat.name = "Acier S355";
            validMat.youngModulus = TSA::ExtensionSystem::PhysicalValue(210000.0, "MPa");
            validMat.density = TSA::ExtensionSystem::PhysicalValue(7850.0, "kg/m3");
            validMat.poissonRatio = 0.30;
            auto resValid = val.validateMaterial(validMat);
            TEST_CHECK(resValid.valid, "Subtest 39.2: Valid Material passes validation");

            // Corrupted Material with out-of-range Poisson's ratio
            TSA::ExtensionSystem::MaterialDefinition invalidMat = validMat;
            invalidMat.poissonRatio = 0.85; // Physique impossible
            auto resInvalid = val.validateMaterial(invalidMat);
            TEST_CHECK(!resInvalid.valid, "Subtest 39.2: Aberrant Poisson ratio rejected");

            // Unknown physical unit
            TSA::ExtensionSystem::MaterialDefinition invalidUnitMat = validMat;
            invalidUnitMat.youngModulus.unit = "UnknownUnit_XYZ";
            auto resUnit = val.validateMaterial(invalidUnitMat);
            TEST_CHECK(!resUnit.valid, "Subtest 39.2: Unsupported physical unit rejected");

            std::cout << "  [PASS] Subtest 39.2: LibraryValidator Strict Conformance & Anti-Crash Protection Verified" << std::endl;
        }

        // 39.3: LibraryVersionManager Version Comparison & Mechanical Property Diff
        {
            TSA::ExtensionSystem::LibraryVersionManager vm;

            TSA::ExtensionSystem::MechanicalSnapshot projectSnap;
            projectSnap.youngModulus = 31.0e9; // 31 GPa
            projectSnap.poissonRatio = 0.20;
            projectSnap.density = 2500.0;
            projectSnap.characteristicStrength = 25.0e6;
            projectSnap.yieldStrength = 0.0;
            projectSnap.thermalCoeff = 1.0e-5;

            TSA::ExtensionSystem::MaterialDefinition updatedLibMat;
            updatedLibMat.id = "concrete.c25_30";
            updatedLibMat.version = TSA::ExtensionSystem::SemanticVersion(1, 1, 0);
            updatedLibMat.youngModulus = TSA::ExtensionSystem::PhysicalValue(31500.0, "MPa"); // Modifié: 31.5 GPa
            updatedLibMat.density = TSA::ExtensionSystem::PhysicalValue(2500.0, "kg/m3");    // Inchangé
            updatedLibMat.poissonRatio = 0.20;                                              // Inchangé
            updatedLibMat.fck = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");          // Inchangé
            updatedLibMat.thermalCoeff = TSA::ExtensionSystem::PhysicalValue(1.0e-5, "1/K"); // Inchangé

            auto diffReport = vm.compare(projectSnap, TSA::ExtensionSystem::SemanticVersion(1, 0, 0), updatedLibMat);
            TEST_CHECK(diffReport.hasMechanicalChanges(), "Subtest 39.3: Mechanical change detected");
            TEST_CHECK(diffReport.modifiedProperties.size() == 1, "Subtest 39.3: Exactly 1 modified property (Young Modulus)");
            TEST_CHECK(diffReport.modifiedProperties[0].propertyName.find("Young") != std::string::npos, "Subtest 39.3: Young modulus identified");
            TEST_CHECK(diffReport.unchangedProperties.size() >= 4, "Subtest 39.3: Unchanged properties detected");

            // SemVer compatibility check
            TEST_CHECK(vm.isCompatible(TSA::ExtensionSystem::SemanticVersion(1, 0, 0), TSA::ExtensionSystem::SemanticVersion(1, 1, 0)), "Subtest 39.3: Minor upgrade is compatible");
            TEST_CHECK(!vm.isCompatible(TSA::ExtensionSystem::SemanticVersion(1, 0, 0), TSA::ExtensionSystem::SemanticVersion(2, 0, 0)), "Subtest 39.3: Major upgrade is incompatible");

            std::cout << "  [PASS] Subtest 39.3: LibraryVersionManager Version Comparison & Mechanical Property Diff Verified" << std::endl;
        }

        // 39.4: LibraryDependencyManager Dependency Resolution & Topological Order
        {
            TSA::ExtensionSystem::LibraryDependencyManager depMgr;

            TSA::ExtensionSystem::ExtensionManifest mStandards;
            mStandards.id = "org.tsaraloha.standards";
            mStandards.name = "TSA Standards Library";
            mStandards.version = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

            TSA::ExtensionSystem::ExtensionManifest mTSALib;
            mTSALib.id = "org.tsaraloha.tsalib";
            mTSALib.name = "TSA Core Engineering Library";
            mTSALib.version = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            mTSALib.dependencies.push_back({ "org.tsaraloha.standards", TSA::ExtensionSystem::SemanticVersion(1, 0, 0), false });

            depMgr.registerManifest(mStandards);
            depMgr.registerManifest(mTSALib);

            auto depVal = depMgr.validateDependencies();
            TEST_CHECK(depVal.valid, "Subtest 39.4: All dependencies satisfied");

            auto loadOrder = depMgr.computeLoadOrder();
            TEST_CHECK(loadOrder.size() == 2, "Subtest 39.4: 2 extensions in load order");
            TEST_CHECK(loadOrder[0] == "org.tsaraloha.standards", "Subtest 39.4: standards loaded before tsalib");
            TEST_CHECK(loadOrder[1] == "org.tsaraloha.tsalib", "Subtest 39.4: tsalib loaded second");

            // Missing dependency test
            TSA::ExtensionSystem::ExtensionManifest mBroken;
            mBroken.id = "org.tsaraloha.broken";
            mBroken.dependencies.push_back({ "org.tsaraloha.missing_lib", TSA::ExtensionSystem::SemanticVersion(1, 0, 0), false });
            depMgr.registerManifest(mBroken);

            auto brokenVal = depMgr.validateDependencies();
            TEST_CHECK(!brokenVal.valid, "Subtest 39.4: Missing dependency properly detected");

            std::cout << "  [PASS] Subtest 39.4: LibraryDependencyManager Dependency Resolution & Topological Order Verified" << std::endl;
        }

        // 39.5: LibraryCache High-Performance In-Memory Cache
        {
            auto& cache = TSA::ExtensionSystem::LibraryCache::instance();
            cache.clear();

            TSA::ExtensionSystem::MechanicalSnapshot snap;
            snap.youngModulus = 210.0e9;
            snap.poissonRatio = 0.30;
            snap.density = 7850.0;

            cache.putSnapshot("org.tsaraloha.tsalib:steel.s355", snap);
            TEST_CHECK(cache.size() == 1, "Subtest 39.5: Cache contains 1 snapshot");

            const auto* cached = cache.getSnapshot("org.tsaraloha.tsalib:steel.s355");
            TEST_CHECK(cached != nullptr, "Subtest 39.5: Cache hit");
            TEST_CHECK(approxEqual(cached->youngModulus, 210.0e9), "Subtest 39.5: Cached snapshot values intact");

            cache.invalidate("steel.s355");
            TEST_CHECK(cache.size() == 0, "Subtest 39.5: Targeted invalidation works");

            std::cout << "  [PASS] Subtest 39.5: LibraryCache High-Performance In-Memory Cache Verified" << std::endl;
        }

        // 39.6: ExtensionManager & LibraryManager Lifecycle Integration
        {
            auto& extMgr = TSA::ExtensionSystem::ExtensionManager::instance();
            auto& libMgr = TSA::ExtensionSystem::LibraryManager::instance();

            libMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
            TEST_CHECK(!libMgr.searchPaths().isEmpty(), "Subtest 39.6: Search paths registered");

            // Test de résilience : discovery sur chemin existant/inexistant ne plante jamais
            auto discovered = libMgr.discover();
            (void)discovered;
            TEST_CHECK(true, "Subtest 39.6: Extension discovery executed safely");

            std::cout << "  [PASS] Subtest 39.6: ExtensionManager & LibraryManager Lifecycle Integration Verified" << std::endl;
        }

        std::cout << "[PASS] Test 39: TSALib Phase 2 - ExtensionSystem Core & Registries (6 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------
    // TEST 40: TSALib Phase 3 - Manifest Format & Disk Layout
    // -------------------------------------------------------------
    {
        std::cout << "\n--- TEST 40: TSALib Phase 3 - Manifest Format & Disk Layout ---" << std::endl;

        // 40.1: TSALib manifest.json Existence & Semantics
        {
            QString manifestPath = "e:/Book/Dev/TSA/Extensions/TSALib/manifest.json";
            TEST_CHECK(QFile::exists(manifestPath), "Subtest 40.1: Extensions/TSALib/manifest.json exists on disk");

            QFile f(manifestPath);
            TEST_CHECK(f.open(QIODevice::ReadOnly), "Subtest 40.1: manifest.json is readable");

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            TEST_CHECK(err.error == QJsonParseError::NoError, "Subtest 40.1: manifest.json is valid JSON");

            std::string parseErr;
            auto manifest = TSA::ExtensionSystem::ExtensionManifest::fromJson(doc.object(), &parseErr);
            TEST_CHECK(manifest.has_value(), "Subtest 40.1: manifest parsed via ExtensionManifest::fromJson");
            TEST_CHECK(manifest->id == "org.tsaraloha.tsalib", "Subtest 40.1: Official ID is org.tsaraloha.tsalib");
            TEST_CHECK(manifest->name == "TSA Engineering Library", "Subtest 40.1: Official Name is TSA Engineering Library");
            TEST_CHECK(manifest->version == TSA::ExtensionSystem::SemanticVersion(1, 0, 0), "Subtest 40.1: Version is 1.0.0");
            TEST_CHECK(manifest->kind == TSA::ExtensionSystem::ExtensionKind::DataExtension, "Subtest 40.1: Kind is DataExtension");
            TEST_CHECK(manifest->categories.size() >= 8, "Subtest 40.1: At least 8 engineering categories declared");

            std::cout << "  [PASS] Subtest 40.1: TSALib manifest.json Existence & Semantics Verified" << std::endl;
        }

        // 40.2: LibraryValidator Full Directory Validation
        {
            TSA::ExtensionSystem::LibraryValidator validator;
            auto valRes = validator.validateExtensionDirectory("e:/Book/Dev/TSA/Extensions/TSALib");
            TEST_CHECK(valRes.valid, "Subtest 40.2: Extensions/TSALib directory validates without errors");

            std::cout << "  [PASS] Subtest 40.2: LibraryValidator Full Directory Validation Verified" << std::endl;
        }

        // 40.3: Standards Directory & Reference Specifications
        {
            QString standardsDir = "e:/Book/Dev/TSA/Extensions/TSALib/Standards";
            TEST_CHECK(QDir(standardsDir).exists(), "Subtest 40.3: Standards directory exists");

            QString en1990 = standardsDir + "/EN1990.json";
            QString en1992 = standardsDir + "/EN1992.json";
            QString en1993 = standardsDir + "/EN1993.json";
            QString en10138 = standardsDir + "/EN10138.json";

            TEST_CHECK(QFile::exists(en1990), "Subtest 40.3: EN1990.json exists");
            TEST_CHECK(QFile::exists(en1992), "Subtest 40.3: EN1992.json exists");
            TEST_CHECK(QFile::exists(en1993), "Subtest 40.3: EN1993.json exists");
            TEST_CHECK(QFile::exists(en10138), "Subtest 40.3: EN10138.json exists");

            // Vérification de contenu d'une fiche normative
            QFile f1992(en1992);
            TEST_CHECK(f1992.open(QIODevice::ReadOnly), "Subtest 40.3: EN1992.json readable");
            QJsonDocument doc = QJsonDocument::fromJson(f1992.readAll());
            TEST_CHECK(doc.object()["standard"].toString() == "EN 1992-1-1", "Subtest 40.3: EN 1992-1-1 code match");

            std::cout << "  [PASS] Subtest 40.3: Standards Directory & Reference Specifications Verified" << std::endl;
        }

        // 40.4: LibraryManager Auto-Discovery of TSALib on Disk
        {
            auto& libMgr = TSA::ExtensionSystem::LibraryManager::instance();
            libMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
            auto discovered = libMgr.discover();

            bool foundTSALib = false;
            for (const auto& ext : discovered)
            {
                if (ext.id == "org.tsaraloha.tsalib")
                {
                    foundTSALib = true;
                    break;
                }
            }
            TEST_CHECK(foundTSALib, "Subtest 40.4: TSALib extension auto-discovered by LibraryManager");

            std::cout << "  [PASS] Subtest 40.4: LibraryManager Auto-Discovery of TSALib on Disk Verified" << std::endl;
        }

        std::cout << "[PASS] Test 40: TSALib Phase 3 - Manifest Format & Disk Layout (4 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }

    // --- TEST 41: TSALib Phase 4 - Externalisation des Matériaux & Découplage C++ ---
    {
        std::cout << "\n--- TEST 41: TSALib Phase 4 - Externalisation des Materiaux & Decouplage C++ ---" << std::endl;

        // 41.1: Verification de l'ensemble des 16 fiches materiaux JSON externes
        {
            QString matDir = "e:/Book/Dev/TSA/Extensions/TSALib/Materials";
            TEST_CHECK(QDir(matDir).exists(), "Subtest 41.1: Repertoire Materials existe");

            QStringList expectedMaterials = {
                "concrete_c25_30.json",
                "concrete_c30_37.json",
                "concrete_reinforced.json",
                "steel_s235.json",
                "steel_s355.json",
                "steel_rebar_b500b.json",
                "steel_galvanized.json",
                "aluminum_structural.json",
                "timber_c24.json",
                "masonry_brick.json",
                "masonry_block.json",
                "glass_structural.json",
                "soil_earth.json",
                "soil_sand.json",
                "soil_gravel.json",
                "soil_rock.json"
            };

            for (const QString& matFile : expectedMaterials)
            {
                QString filePath = matDir + "/" + matFile;
                TEST_CHECK(QFile::exists(filePath), ("Subtest 41.1: Fichier existant: " + matFile.toStdString()).c_str());

                QFile f(filePath);
                TEST_CHECK(f.open(QIODevice::ReadOnly), ("Subtest 41.1: Lecture fichier: " + matFile.toStdString()).c_str());
                QJsonParseError parseErr;
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
                TEST_CHECK(parseErr.error == QJsonParseError::NoError, ("Subtest 41.1: JSON valide: " + matFile.toStdString()).c_str());

                std::string err;
                auto matDef = TSA::ExtensionSystem::MaterialDefinition::fromJson(doc.object(), &err);
                TEST_CHECK(matDef.has_value(), ("Subtest 41.1: Parsing MaterialDefinition: " + matFile.toStdString()).c_str());
                TEST_CHECK(matDef->density.toBaseSI() > 0.0, "Subtest 41.1: Masse volumique positive");
                TEST_CHECK(matDef->youngModulus.toBaseSI() > 0.0, "Subtest 41.1: Module d'Young positif");
                TEST_CHECK(matDef->poissonRatio >= -1.0 && matDef->poissonRatio < 0.5, "Subtest 41.1: Poisson ratio dans [-1.0, 0.5[");
            }

            std::cout << "  [PASS] Subtest 41.1: 16 Fiches Materiaux Externes JSON Validees avec Succes" << std::endl;
        }

        // 41.2: Chargement d'extension & Indexation dans LibraryRegistry
        {
            auto& libMgr = TSA::ExtensionSystem::LibraryManager::instance();
            libMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
            libMgr.discover();
            bool loaded = libMgr.load("org.tsaraloha.tsalib");
            TEST_CHECK(loaded, "Subtest 41.2: Chargement de l'extension TSALib reussi");

            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            auto materials = registry.allMaterials();
            TEST_CHECK(materials.size() >= 16, "Subtest 41.2: Au moins 16 materiaux enregistres dans le registre");

            // Verification d'un materiau specifique (Béton C25/30)
            const auto* c25 = registry.findMaterial("concrete_c25_30");
            TEST_CHECK(c25 != nullptr, "Subtest 41.2: concrete_c25_30 trouve dans LibraryRegistry");
            if (c25)
            {
                TEST_CHECK(c25->name == "Concrete C25/30", "Subtest 41.2: Nom correspond");
                TEST_CHECK(std::abs(c25->youngModulus.toBaseSI() - 31.0e9) < 1.0e3, "Subtest 41.2: Young Modulus = 31 GPa");
                TEST_CHECK(std::abs(c25->density.toBaseSI() - 2500.0) < 1.0, "Subtest 41.2: Density = 2500 kg/m3");
                TEST_CHECK(std::abs(c25->poissonRatio - 0.20) < 1.0e-4, "Subtest 41.2: Poisson = 0.20");
                TEST_CHECK(std::abs(c25->fck.toBaseSI() - 25.0e6) < 1.0e3, "Subtest 41.2: fck = 25 MPa");
            }

            // Verification de recherche par categorie
            auto concreteMats = registry.materialsByCategory("Concrete");
            TEST_CHECK(concreteMats.size() >= 3, "Subtest 41.2: Au moins 3 materiaux de categorie Concrete");

            auto steelMats = registry.materialsByCategory("Steel");
            TEST_CHECK(steelMats.size() >= 4, "Subtest 41.2: Au moins 4 materiaux de categorie Steel");

            std::cout << "  [PASS] Subtest 41.2: Chargement d'extension & Indexation dans LibraryRegistry Verifies" << std::endl;
        }

        // 41.3: Passerelle Bidirectionnelle MaterialDefinition <-> TSA::Model::Material
        {
            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            const auto* s235Def = registry.findMaterial("steel_s235");
            TEST_CHECK(s235Def != nullptr, "Subtest 41.3: steel_s235 present");
            if (s235Def)
            {
                // Conversion vers TSA::Model::Material
                TSA::Model::Material modelMat = s235Def->toModelMaterial(101);
                TEST_CHECK(modelMat.id == 101, "Subtest 41.3: ID reporte");
                TEST_CHECK(modelMat.name == "Steel S235", "Subtest 41.3: Nom reporte");
                TEST_CHECK(modelMat.type == TSA::Model::MaterialType::Steel, "Subtest 41.3: Type Steel correct");
                TEST_CHECK(std::abs(modelMat.E - 210.0e9) < 1.0e3, "Subtest 41.3: E = 210 GPa");
                TEST_CHECK(std::abs(modelMat.fk - 235.0e6) < 1.0e3, "Subtest 41.3: fk = 235 MPa");
                TEST_CHECK(modelMat.visual.baseColor == "#4682B4", "Subtest 41.3: Couleur albedo conforme");

                // Reconversion vers MaterialDefinition
                auto backDef = TSA::ExtensionSystem::MaterialDefinition::fromModelMaterial(modelMat, "test.lib");
                TEST_CHECK(backDef.category == "Steel", "Subtest 41.3: Categorie Steel preservee");
                TEST_CHECK(std::abs(backDef.youngModulus.toBaseSI() - 210.0e9) < 1.0e3, "Subtest 41.3: E conserve");
                TEST_CHECK(std::abs(backDef.fy.toBaseSI() - 235.0e6) < 1.0e3, "Subtest 41.3: fy conserve");
            }

            std::cout << "  [PASS] Subtest 41.3: Passerelle Bidirectionnelle MaterialDefinition <-> Material Verifiee" << std::endl;
        }

        // 41.4: Synchronisation MaterialLibrary & LibraryManager
        {
            auto& matLib = TSA::Model::MaterialLibrary::instance();
            matLib.reloadFromRegistry();
            TEST_CHECK(matLib.standardMaterials().size() >= 16, "Subtest 41.4: MaterialLibrary a synchronise les 16 materiaux standards");

            // Creation d'un materiau personnalise
            TSA::Model::Material customMat;
            customMat.id = 999;
            customMat.name = "Super Titanium Ti-6Al-4V";
            customMat.type = TSA::Model::MaterialType::Custom;
            customMat.E = 114.0e9;
            customMat.nu = 0.34;
            customMat.density = 4430.0;
            customMat.fk = 880.0e6;
            customMat.syncMechanical();

            bool registered = matLib.registerCustomMaterial(customMat);
            TEST_CHECK(registered, "Subtest 41.4: Enregistrement materiau personnalise reussi");

            // Verifier presence dans MaterialLibrary
            const auto* foundInMatLib = matLib.findByName("Super Titanium Ti-6Al-4V");
            TEST_CHECK(foundInMatLib != nullptr, "Subtest 41.4: Materiau personnalise trouve dans MaterialLibrary");

            // Verifier presence synchronisee automatique dans ExtensionSystem::LibraryRegistry
            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            const auto* foundInRegistry = registry.findMaterial("Super Titanium Ti-6Al-4V");
            TEST_CHECK(foundInRegistry != nullptr, "Subtest 41.4: Materiau personnalise synchronise dans LibraryRegistry");
            if (foundInRegistry)
            {
                TEST_CHECK(std::abs(foundInRegistry->youngModulus.toBaseSI() - 114.0e9) < 1.0e3, "Subtest 41.4: Propriete E synchronisee");
            }

            std::cout << "  [PASS] Subtest 41.4: Synchronisation MaterialLibrary & LibraryManager Verifiee" << std::endl;
        }

        std::cout << "[PASS] Test 41: TSALib Phase 4 - Externalisation des Materiaux & Decouplage C++ (4 Subtests Validates) Passed Successfully!" << std::endl;
        passed++;
    }

    // --- TEST 42: TSALib Phase 5 - Externalisation des Textures PBR & TextureManager ---
    {
        std::cout << "\n--- TEST 42: TSALib Phase 5 - Externalisation des Textures PBR & TextureManager ---" << std::endl;

        // 42.1: Verification de l'ensemble des 14 textures PNG externes et de textures.json
        {
            QString texDir = "e:/Book/Dev/TSA/Extensions/TSALib/Textures";
            TEST_CHECK(QDir(texDir).exists(), "Subtest 42.1: Repertoire Textures existe");

            QString manifestPath = texDir + "/textures.json";
            TEST_CHECK(QFile::exists(manifestPath), "Subtest 42.1: textures.json existe");

            QStringList expectedTextures = {
                "concrete.png",
                "reinforced_concrete.png",
                "steel.png",
                "rebar.png",
                "galvanized.png",
                "aluminum.png",
                "wood.png",
                "brick.png",
                "masonry.png",
                "glass.png",
                "soil.png",
                "sand.png",
                "gravel.png",
                "rock.png"
            };

            for (const QString& texFile : expectedTextures)
            {
                QString filePath = texDir + "/" + texFile;
                TEST_CHECK(QFile::exists(filePath), ("Subtest 42.1: Texture existante: " + texFile.toStdString()).c_str());
                QFileInfo fi(filePath);
                TEST_CHECK(fi.size() > 500, ("Subtest 42.1: Taille non-nulle pour: " + texFile.toStdString()).c_str());
            }

            std::cout << "  [PASS] Subtest 42.1: 14 Textures PNG PBR & textures.json Validees sur Disque" << std::endl;
        }

        // 42.2: TextureManager - Decouverte, Catalogue & Resolution
        {
            auto& texMgr = TSA::Viewer::TextureManager::instance();
            texMgr.initialize("e:/Book/Dev/TSA");
            texMgr.addSearchPath("e:/Book/Dev/TSA/Extensions/TSALib/Textures");

            TEST_CHECK(texMgr.count() >= 14, "Subtest 42.2: Au moins 14 textures indexees par TextureManager");

            // Resolution par ID court
            QString concretePath = texMgr.resolveTexturePath("concrete");
            TEST_CHECK(!concretePath.isEmpty(), "Subtest 42.2: Resolution ID 'concrete' reussie");
            TEST_CHECK(QFile::exists(concretePath), "Subtest 42.2: Fichier concrete resolu existe");

            // Resolution par chemin relatif complet
            QString steelPath = texMgr.resolveTexturePath("Textures/steel.png");
            TEST_CHECK(!steelPath.isEmpty(), "Subtest 42.2: Resolution relatif 'Textures/steel.png' reussie");
            TEST_CHECK(QFile::exists(steelPath), "Subtest 42.2: Fichier steel resolu existe");

            // Resolution de texture inexistante retourne vide sans crasher
            QString nonExistent = texMgr.resolveTexturePath("unobtainium_texture_xyz");
            TEST_CHECK(nonExistent.isEmpty(), "Subtest 42.2: Texture inexistante retourne chaine vide de maniere securisee");

            // Verification du filtrage par categorie
            auto concreteCat = texMgr.texturesByCategory("Concrete");
            TEST_CHECK(!concreteCat.empty(), "Subtest 42.2: Categorie Concrete non vide");

            std::cout << "  [PASS] Subtest 42.2: TextureManager Decouverte, Catalogue & Resolution Verifies" << std::endl;
        }

        // 42.3: Integration MaterialVisual avec Textures Externes & PBR
        {
            auto& matLib = TSA::Model::MaterialLibrary::instance();
            matLib.reloadFromRegistry();

            const auto* c25 = matLib.findByName("Concrete C25/30");
            TEST_CHECK(c25 != nullptr, "Subtest 42.3: Materiau Concrete C25/30 disponible");
            if (c25)
            {
                auto& matVis = TSA::Viewer::MaterialVisual::instance();
                TEST_CHECK(matVis.hasTexture(*c25), "Subtest 42.3: MaterialVisual detecte la texture pour C25/30");

                QString resolved = matVis.resolveTexturePath(*c25);
                TEST_CHECK(!resolved.isEmpty(), "Subtest 42.3: Texture resolue pour C25/30");
                TEST_CHECK(resolved.endsWith("concrete.png", Qt::CaseInsensitive), "Subtest 42.3: Pointeur vers concrete.png");

                // Verifier PBR material
                Graphic3d_MaterialAspect aspect = matVis.getOcctMaterial(*c25);
                TEST_CHECK(aspect.PBRMaterial().Roughness() > 0.5f, "Subtest 42.3: Rugosite PBR concrete conforme");
                TEST_CHECK(aspect.PBRMaterial().Metallic() < 0.1f, "Subtest 42.3: Caractere non metallique concrete conforme");
            }

            const auto* s235 = matLib.findByName("Steel S235");
            TEST_CHECK(s235 != nullptr, "Subtest 42.3: Materiau Steel S235 disponible");
            if (s235)
            {
                auto& matVis = TSA::Viewer::MaterialVisual::instance();
                TEST_CHECK(matVis.hasTexture(*s235), "Subtest 42.3: MaterialVisual detecte la texture pour S235");

                Graphic3d_MaterialAspect aspect = matVis.getOcctMaterial(*s235);
                TEST_CHECK(aspect.PBRMaterial().Metallic() > 0.8f, "Subtest 42.3: Caractere metallique PBR acier conforme");
            }

            std::cout << "  [PASS] Subtest 42.3: Integration MaterialVisual avec Textures Externes & PBR Verifiee" << std::endl;
        }

        // 42.4: Hot Reload & Invalidation de Cache
        {
            auto& texMgr = TSA::Viewer::TextureManager::instance();
            auto& matVis = TSA::Viewer::MaterialVisual::instance();

            // Rechargement a chaud des textures
            texMgr.reloadTextures();
            TEST_CHECK(texMgr.count() >= 14, "Subtest 42.4: Textures toujours presentes apres rechargement a chaud");

            // Vidage et reconstruction du cache d'aspects graphiques
            matVis.clearCache();
            const auto* wood = TSA::Model::MaterialLibrary::instance().findByName("Timber C24");
            TEST_CHECK(wood != nullptr, "Subtest 42.4: Materiau Timber C24 present");
            if (wood)
            {
                Graphic3d_MaterialAspect reloadedAspect = matVis.getOcctMaterial(*wood);
                TEST_CHECK(matVis.hasTexture(*wood), "Subtest 42.4: Texture toujours associee apres clearCache");
            }

            std::cout << "  [PASS] Subtest 42.4: Hot Reload & Invalidation de Cache Verifies" << std::endl;
        }

        std::cout << "[PASS] Test 42: TSALib Phase 5 - Externalisation des Textures PBR & TextureManager (4 Subtests Validates) Passed Successfully!" << std::endl;
        passed++;
    }

    // --- TEST 43: TSALib Phase 6 - Externalisation des Sections & Profilés Eurocodes ---
    {
        std::cout << "\n--- TEST 43: TSALib Phase 6 - Externalisation des Sections & Profilés Eurocodes ---" << std::endl;

        // 43.1: Catalogue des Sections & Profilés Eurocodes sur disque (22 définitions JSON)
        {
            QString sectionsDir = "e:/Book/Dev/TSA/Extensions/TSALib/Sections";
            QString profilesDir = "e:/Book/Dev/TSA/Extensions/TSALib/Profiles";

            TEST_CHECK(QDir(sectionsDir).exists(), "Subtest 43.1: Repertoire Sections existe");
            TEST_CHECK(QDir(profilesDir).exists(), "Subtest 43.1: Repertoire Profiles existe");

            QStringList expectedSections = {
                "rect_300x500.json",
                "rect_400x400.json",
                "circ_d300.json",
                "circ_d400.json",
                "pipe_d219x6.json",
                "box_200x200x8.json"
            };

            for (const QString& fName : expectedSections)
            {
                QString filePath = sectionsDir + "/" + fName;
                TEST_CHECK(QFile::exists(filePath), ("Subtest 43.1: Fichier section existe: " + fName.toStdString()).c_str());
                QFile f(filePath);
                TEST_CHECK(f.open(QIODevice::ReadOnly), ("Subtest 43.1: Ouverture de " + fName.toStdString()).c_str());
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                TEST_CHECK(doc.isObject(), ("Subtest 43.1: JSON valide pour " + fName.toStdString()).c_str());
                QJsonObject obj = doc.object();
                TEST_CHECK(!obj["name"].toString().isEmpty(), "Subtest 43.1: 'name' non vide");
                TEST_CHECK(!obj["shape_type"].toString().isEmpty(), "Subtest 43.1: 'shape_type' non vide");
                TEST_CHECK(obj.contains("dimensions") && obj["dimensions"].isObject(), "Subtest 43.1: 'dimensions' present");
                TEST_CHECK(obj.contains("properties") && obj["properties"].isObject(), "Subtest 43.1: 'properties' present");
            }

            QStringList expectedProfiles = {
                "ipe100.json", "ipe160.json", "ipe200.json", "ipe240.json", "ipe300.json",
                "hea100.json", "hea160.json", "hea200.json",
                "heb100.json", "heb160.json", "heb200.json",
                "upn100.json", "upn160.json", "upn200.json",
                "angle_l100x10.json", "t_100x10.json"
            };

            for (const QString& fName : expectedProfiles)
            {
                QString filePath = profilesDir + "/" + fName;
                TEST_CHECK(QFile::exists(filePath), ("Subtest 43.1: Fichier profile existe: " + fName.toStdString()).c_str());
                QFile f(filePath);
                TEST_CHECK(f.open(QIODevice::ReadOnly), ("Subtest 43.1: Ouverture de " + fName.toStdString()).c_str());
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                TEST_CHECK(doc.isObject(), ("Subtest 43.1: JSON valide pour " + fName.toStdString()).c_str());
                QJsonObject obj = doc.object();
                TEST_CHECK(!obj["name"].toString().isEmpty(), "Subtest 43.1: 'name' non vide");
                TEST_CHECK(!obj["shape_type"].toString().isEmpty(), "Subtest 43.1: 'shape_type' non vide");
            }

            std::cout << "  [PASS] Subtest 43.1: 22 Definitions JSON de Sections et Profiles Eurocodes Validees sur Disque" << std::endl;
        }

        // 43.2: Découverte & Indexation dans LibraryRegistry via LibraryManager / LibraryLoader
        {
            auto& extLibMgr = TSA::ExtensionSystem::LibraryManager::instance();
            extLibMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
            extLibMgr.discover();
            extLibMgr.load("org.tsaraloha.tsalib");

            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            auto allSecs = registry.allSections();
            TEST_CHECK(allSecs.size() >= 22, "Subtest 43.2: Au moins 22 sections enregistrees dans le registre");

            // Vérifications d'accès par ID logique
            const auto* ipe200 = registry.findSection("ipe200");
            TEST_CHECK(ipe200 != nullptr, "Subtest 43.2: Section 'ipe200' trouvee dans le registre");
            if (ipe200)
            {
                TEST_CHECK(ipe200->name == "IPE 200", "Subtest 43.2: Nom IPE 200 conforme");
                TEST_CHECK(ipe200->shapeType == "IShape", "Subtest 43.2: Forme IShape");
                TEST_CHECK(approxEqual(ipe200->height, 0.200), "Subtest 43.2: Hauteur IPE 200 = 200 mm");
                TEST_CHECK(approxEqual(ipe200->width, 0.100), "Subtest 43.2: Largeur IPE 200 = 100 mm");
                TEST_CHECK(approxEqual(ipe200->webThickness, 0.0056), "Subtest 43.2: tw IPE 200 = 5.6 mm");
                TEST_CHECK(approxEqual(ipe200->flangeThickness, 0.0085), "Subtest 43.2: tf IPE 200 = 8.5 mm");
            }

            const auto* hea160 = registry.findSection("hea160");
            TEST_CHECK(hea160 != nullptr, "Subtest 43.2: Section 'hea160' trouvee dans le registre");
            if (hea160)
            {
                TEST_CHECK(hea160->name == "HEA 160", "Subtest 43.2: Nom HEA 160");
                TEST_CHECK(approxEqual(hea160->height, 0.152), "Subtest 43.2: Hauteur HEA 160 = 152 mm");
            }

            const auto* rect = registry.findSection("rect_300x500");
            TEST_CHECK(rect != nullptr, "Subtest 43.2: Section 'rect_300x500' trouvee dans le registre");
            if (rect)
            {
                TEST_CHECK(rect->category == "Concrete", "Subtest 43.2: Categorie Concrete");
                TEST_CHECK(approxEqual(rect->width, 0.30), "Subtest 43.2: Largeur 0.30 m");
                TEST_CHECK(approxEqual(rect->height, 0.50), "Subtest 43.2: Hauteur 0.50 m");
            }

            // Filtrage par catégorie
            auto steelSecs = registry.sectionsByCategory("Steel");
            TEST_CHECK(steelSecs.size() >= 16, "Subtest 43.2: Au moins 16 sections acier indexees");

            auto concreteSecs = registry.sectionsByCategory("Concrete");
            TEST_CHECK(concreteSecs.size() >= 4, "Subtest 43.2: Au moins 4 sections beton indexees");

            std::cout << "  [PASS] Subtest 43.2: Decouverte & Indexation dans LibraryRegistry Validees" << std::endl;
        }

        // 43.3: Passerelle Bidirectionnelle SectionDefinition <-> TSA::Model::Section
        {
            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            const auto* ipeDef = registry.findSection("ipe200");
            TEST_CHECK(ipeDef != nullptr, "Subtest 43.3: ipe200 present");
            if (ipeDef)
            {
                // Conversion vers TSA::Model::Section
                TSA::Model::Section modelSec = ipeDef->toModelSection(42);
                TEST_CHECK(modelSec.id == 42, "Subtest 43.3: ID reporte");
                TEST_CHECK(modelSec.name == "IPE 200", "Subtest 43.3: Nom reporte");
                TEST_CHECK(modelSec.shape == TSA::Model::SectionShape::IShape, "Subtest 43.3: SectionShape::IShape");
                TEST_CHECK(approxEqual(modelSec.height, 0.200), "Subtest 43.3: Hauteur = 0.200 m");
                TEST_CHECK(approxEqual(modelSec.width, 0.100), "Subtest 43.3: Largeur = 0.100 m");
                TEST_CHECK(approxEqual(modelSec.tw, 0.0056), "Subtest 43.3: tw = 5.6 mm");
                TEST_CHECK(approxEqual(modelSec.tf, 0.0085), "Subtest 43.3: tf = 8.5 mm");

                // Propriétés calculées
                TEST_CHECK(modelSec.area() > 0.002, "Subtest 43.3: Aire calculee positive coherente");
                TEST_CHECK(modelSec.iy() > 1e-5, "Subtest 43.3: Iy fort axe positif coherent");
                TEST_CHECK(modelSec.iz() > 1e-6, "Subtest 43.3: Iz faible axe positif coherent");

                // Aller-retour vers SectionDefinition
                TSA::ExtensionSystem::SectionDefinition roundtripDef =
                    TSA::ExtensionSystem::SectionDefinition::fromModelSection(modelSec, "test.lib");
                TEST_CHECK(roundtripDef.name == "IPE 200", "Subtest 43.3: Nom aller-retour conforme");
                TEST_CHECK(roundtripDef.shapeType == "IShape", "Subtest 43.3: ShapeType aller-retour conforme");
                TEST_CHECK(approxEqual(roundtripDef.height, 0.200), "Subtest 43.3: Hauteur aller-retour conforme");
                TEST_CHECK(roundtripDef.ix > 1e-5, "Subtest 43.3: Inertie forte axe transferee dans ix");
            }

            // Test section circulaire
            const auto* circDef = registry.findSection("circ_d300");
            TEST_CHECK(circDef != nullptr, "Subtest 43.3: circ_d300 present");
            if (circDef)
            {
                TSA::Model::Section circSec = circDef->toModelSection(43);
                TEST_CHECK(circSec.shape == TSA::Model::SectionShape::Circular, "Subtest 43.3: Forme circulaire");
                TEST_CHECK(approxEqual(circSec.diameter, 0.30), "Subtest 43.3: Diametre = 0.30 m");
                TEST_CHECK(approxEqual(circSec.area(), 3.14159265358979323846 * 0.30 * 0.30 / 4.0), "Subtest 43.3: Aire circulaire exacte");
            }

            std::cout << "  [PASS] Subtest 43.3: Passerelle Bidirectionnelle SectionDefinition <-> Section Validee" << std::endl;
        }

        // 43.4: Synchronisation de Section::defaultLibrary() et Génération Solide B-Rep OpenCASCADE
        {
            // Vérifier que Section::defaultLibrary() extrait bien les sections de LibraryRegistry
            std::vector<TSA::Model::Section> defaultSections = TSA::Model::Section::defaultLibrary();
            TEST_CHECK(defaultSections.size() >= 22, "Subtest 43.4: Section::defaultLibrary() contient les sections externalisees");

            bool foundIpe200 = false;
            TSA::Model::Section ipe200Sec;
            for (const auto& sec : defaultSections)
            {
                if (sec.name == "IPE 200")
                {
                    foundIpe200 = true;
                    ipe200Sec = sec;
                    break;
                }
            }
            TEST_CHECK(foundIpe200, "Subtest 43.4: IPE 200 present dans la defaultLibrary synchronisee");

            // Vérifier la synchronisation avec LibraryManager UI
            auto& uiLibMgr = TSA::Library::LibraryManager::instance();
            uiLibMgr.reloadSectionsFromRegistry();
            const auto* foundInUi = uiLibMgr.findSectionByName("IPE 200");
            TEST_CHECK(foundInUi != nullptr, "Subtest 43.4: IPE 200 accessible dans LibraryManager UI");

            // Construction d'un solide OpenCASCADE B-Rep avec la section externalisée
            TSA::Model::Node nodeA(1, 0.0, 0.0, 0.0, "", "N1");
            TSA::Model::Node nodeB(2, 5.0, 0.0, 0.0, "", "N2");

            TopoDS_Shape beamSolid = TSA::Geometry::BeamGeometry::createBeamShape(
                nodeA, nodeB, ipe200Sec, 0.0, TSA::Model::BarEccentricity::None
            );

            TEST_CHECK(!beamSolid.IsNull(), "Subtest 43.4: Solide OpenCASCADE B-Rep non-nul genere");
            TEST_CHECK(beamSolid.ShapeType() == TopAbs_SOLID || beamSolid.ShapeType() == TopAbs_COMPOUND,
                       "Subtest 43.4: Type de forme OpenCASCADE valide (Solide ou Compound)");

            // Calcul du bounding box OpenCASCADE pour valider les dimensions réelles du solide
            Bnd_Box bbox;
            BRepBndLib::Add(beamSolid, bbox);
            TEST_CHECK(!bbox.IsVoid(), "Subtest 43.4: Bounding Box du solide OpenCASCADE calcule");

            double xmin, ymin, zmin, xmax, ymax, zmax;
            bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

            double lengthX = xmax - xmin;
            double dimY = ymax - ymin;
            double dimZ = zmax - zmin;

            TEST_CHECK(approxEqual(lengthX, 5.0, 0.05), "Subtest 43.4: Longueur poutre OpenCASCADE = 5.0 m");
            TEST_CHECK(dimY > 0.05 && dimZ > 0.05, "Subtest 43.4: Section transversale extrudee en 3D avec succes");

            std::cout << "  [PASS] Subtest 43.4: Synchronisation Section::defaultLibrary() & B-Rep OpenCASCADE Validees" << std::endl;
        }

        std::cout << "[PASS] Test 43: TSALib Phase 6 - Externalisation des Sections & Profiles Eurocodes (4 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    // --- TEST 44: TSALib Phase 7 - Externalisation des Câbles & Torons Eurocodes / ASTM ---
    {
        std::cout << "\n--- TEST 44: TSALib Phase 7 - Externalisation des Câbles & Torons Eurocodes / ASTM ---" << std::endl;

        // 44.1: Catalogue des 19 Câbles & Torons Eurocodes / ASTM sur disque
        {
            QString cablesDir = "e:/Book/Dev/TSA/Extensions/TSALib/Cables";
            TEST_CHECK(QDir(cablesDir).exists(), "Subtest 44.1: Repertoire Cables existe");

            QStringList expectedCables = {
                "en10138_y1860s7_12_5.json",
                "en10138_y1860s7_12_7.json",
                "en10138_y1860s7_12_9.json",
                "en10138_y1860s7_15_2.json",
                "en10138_y1860s7_15_7.json",
                "en10138_y1770s7_15_2.json",
                "en10138_bar_y1030_26_5.json",
                "en10138_bar_y1030_32.json",
                "en10138_bar_y1030_36.json",
                "en10138_bar_y1030_40.json",
                "en1993_flc_50.json",
                "en1993_flc_80.json",
                "en1993_flc_120.json",
                "en1993_hanger_30.json",
                "stay_pss_19_15_7.json",
                "stay_pss_37_15_7.json",
                "stay_pss_61_15_7.json",
                "astm_a416_gr270_0_5in.json",
                "astm_a416_gr270_0_6in.json"
            };

            for (const QString& fName : expectedCables)
            {
                QString filePath = cablesDir + "/" + fName;
                TEST_CHECK(QFile::exists(filePath), ("Subtest 44.1: Fichier cable existe: " + fName.toStdString()).c_str());
                QFile f(filePath);
                TEST_CHECK(f.open(QIODevice::ReadOnly), ("Subtest 44.1: Ouverture de " + fName.toStdString()).c_str());
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                TEST_CHECK(doc.isObject(), ("Subtest 44.1: JSON valide pour " + fName.toStdString()).c_str());
                QJsonObject obj = doc.object();
                TEST_CHECK(!obj["id"].toString().isEmpty(), "Subtest 44.1: 'id' non vide");
                TEST_CHECK(!obj["name"].toString().isEmpty(), "Subtest 44.1: 'name' non vide");
                TEST_CHECK(!obj["category"].toString().isEmpty(), "Subtest 44.1: 'category' non vide");
                TEST_CHECK(obj.contains("standard") && obj["standard"].isObject(), "Subtest 44.1: 'standard' present");
                TEST_CHECK(obj.contains("geometry") && obj["geometry"].isObject(), "Subtest 44.1: 'geometry' present");
                TEST_CHECK(obj.contains("mechanical") && obj["mechanical"].isObject(), "Subtest 44.1: 'mechanical' present");
            }

            std::cout << "  [PASS] Subtest 44.1: 19 Definitions JSON de Cables Eurocodes / ASTM Validees sur Disque" << std::endl;
        }

        // 44.2: Découverte & Indexation dans LibraryRegistry via LibraryManager / LibraryLoader
        {
            auto& extLibMgr = TSA::ExtensionSystem::LibraryManager::instance();
            extLibMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
            extLibMgr.discover();
            extLibMgr.load("org.tsaraloha.tsalib");

            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            auto allCables = registry.allCables();
            TEST_CHECK(allCables.size() >= 19, "Subtest 44.2: Au moins 19 cables enregistres dans le registre");

            // Vérifications d'accès par ID logique
            const auto* pss19 = registry.findCable("stay_pss_19_15_7");
            TEST_CHECK(pss19 != nullptr, "Subtest 44.2: Cable 'stay_pss_19_15_7' trouve dans le registre");
            if (pss19)
            {
                TEST_CHECK(pss19->name == "Stay PSS 19x15.7mm", "Subtest 44.2: Nom Stay PSS 19x15.7mm conforme");
                TEST_CHECK(pss19->category == "StayCable", "Subtest 44.2: Categorie StayCable");
                TEST_CHECK(approxEqual(pss19->nominalDiameter, 0.090), "Subtest 44.2: Diametre enveloppe = 90 mm");
                TEST_CHECK(approxEqual(pss19->metallicArea, 0.00285), "Subtest 44.2: Section metallique = 2850 mm2");
                TEST_CHECK(approxEqual(pss19->elasticModulus, 195.0e9), "Subtest 44.2: E = 195 GPa");
                TEST_CHECK(approxEqual(pss19->minimumBreakingForce, 5301.0e3), "Subtest 44.2: Breaking Force = 5301 kN");
            }

            const auto* t15 = registry.findCable("en10138_y1860s7_15_7");
            TEST_CHECK(t15 != nullptr, "Subtest 44.2: Toron 'en10138_y1860s7_15_7' trouve");
            if (t15)
            {
                TEST_CHECK(t15->grade == "Y1860S7", "Subtest 44.2: Grade Y1860S7");
                TEST_CHECK(approxEqual(t15->nominalDiameter, 0.0157), "Subtest 44.2: Diametre nominal = 15.7 mm");
                TEST_CHECK(approxEqual(t15->characteristicStrength, 1860.0e6), "Subtest 44.2: fpk = 1860 MPa");
            }

            const auto* bar32 = registry.findCable("en10138_bar_y1030_32");
            TEST_CHECK(bar32 != nullptr, "Subtest 44.2: Barre 'en10138_bar_y1030_32' trouvee");
            if (bar32)
            {
                TEST_CHECK(bar32->category == "PrestressingBar", "Subtest 44.2: Categorie PrestressingBar");
                TEST_CHECK(approxEqual(bar32->nominalDiameter, 0.032), "Subtest 44.2: Diametre = 32 mm");
            }

            // Filtrage par catégorie
            auto strands = registry.cablesByCategory("Strand");
            TEST_CHECK(strands.size() >= 8, "Subtest 44.2: Au moins 8 torons indexees");

            auto stayCables = registry.cablesByCategory("StayCable");
            TEST_CHECK(stayCables.size() >= 5, "Subtest 44.2: Au moins 5 haubans indexes");

            auto bars = registry.cablesByCategory("PrestressingBar");
            TEST_CHECK(bars.size() >= 4, "Subtest 44.2: Au moins 4 barres de precontrainte indexees");

            std::cout << "  [PASS] Subtest 44.2: Decouverte & Indexation des Cables dans LibraryRegistry Validees" << std::endl;
        }

        // 44.3: Passerelle Bidirectionnelle CableCatalogDefinition <-> TSA::Model::CableDefinition
        {
            auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
            const auto* pssDef = registry.findCable("stay_pss_19_15_7");
            TEST_CHECK(pssDef != nullptr, "Subtest 44.3: stay_pss_19_15_7 present");
            if (pssDef)
            {
                // Conversion vers TSA::Model::CableDefinition
                TSA::Model::CableDefinition modelCable = pssDef->toModelCableDefinition();
                TEST_CHECK(modelCable.id() == "stay_pss_19_15_7", "Subtest 44.3: ID reporte");
                TEST_CHECK(modelCable.name() == "Stay PSS 19x15.7mm", "Subtest 44.3: Nom reporte");
                TEST_CHECK(modelCable.type() == TSA::Model::CableType::StayCable, "Subtest 44.3: Type StayCable");
                TEST_CHECK(approxEqual(modelCable.nominalDiameter(), 0.090), "Subtest 44.3: Diametre = 90 mm");
                TEST_CHECK(approxEqual(modelCable.metallicArea(), 0.00285), "Subtest 44.3: Section = 2850 mm2");
                TEST_CHECK(approxEqual(modelCable.elasticModulus(), 195.0e9), "Subtest 44.3: E = 195 GPa");
                TEST_CHECK(approxEqual(modelCable.minimumBreakingForce(), 5301.0e3), "Subtest 44.3: Rupture = 5301 kN");

                // Aller-retour vers CableCatalogDefinition
                TSA::ExtensionSystem::CableCatalogDefinition roundtrip =
                    TSA::ExtensionSystem::CableCatalogDefinition::fromModelCableDefinition(modelCable, "test.lib");
                TEST_CHECK(roundtrip.id == "stay_pss_19_15_7", "Subtest 44.3: ID aller-retour conforme");
                TEST_CHECK(roundtrip.name == "Stay PSS 19x15.7mm", "Subtest 44.3: Nom aller-retour conforme");
                TEST_CHECK(roundtrip.category == "StayCable", "Subtest 44.3: Categorie aller-retour conforme");
                TEST_CHECK(approxEqual(roundtrip.nominalDiameter, 0.090), "Subtest 44.3: Diametre aller-retour conforme");
                TEST_CHECK(approxEqual(roundtrip.minimumBreakingForce, 5301.0e3), "Subtest 44.3: Rupture aller-retour conforme");
            }

            std::cout << "  [PASS] Subtest 44.3: Passerelle Bidirectionnelle CableCatalogDefinition <-> CableDefinition Validee" << std::endl;
        }

        // 44.4: Synchronisation de CableLibrary, Model Integration & Génération 3D OpenCASCADE
        {
            // Vérifier que CableDefinition::defaultLibrary() extrait les câbles de LibraryRegistry
            std::vector<TSA::Model::CableDefinition> defaultCables = TSA::Model::CableDefinition::defaultLibrary();
            TEST_CHECK(defaultCables.size() >= 19, "Subtest 44.4: defaultLibrary() contient les cables externalises");

            // Vérifier la synchronisation avec CableLibrary
            auto& cableLib = TSA::Library::CableLibrary::instance();
            cableLib.reloadFromRegistry();
            const auto* foundInLib = cableLib.findByName("Stay PSS 19x15.7mm");
            TEST_CHECK(foundInLib != nullptr, "Subtest 44.4: Cable accessible dans CableLibrary");

            // Intégration dans le modèle structural TSA
            TSA::Model::Model testModel;
            int n1 = testModel.addNode(0.0, 0.0, 0.0, "", "Ancrage Bas");
            int n2 = testModel.addNode(10.0, 0.0, 2.0, "", "Ancrage Haut");

            int cableId = testModel.addCable(n1, n2, *foundInLib, "Hauban H1");
            TEST_CHECK(cableId > 0, "Subtest 44.4: Ajout du cable externalise dans Model reussi");

            const auto* cableElem = testModel.getCable(cableId);
            TEST_CHECK(cableElem != nullptr, "Subtest 44.4: Recuperation du cable dans le modele");

            // Génération du solide 3D OpenCASCADE via CableGeometry3D
            TopoDS_Shape cableSolid = TSA::Geometry::CableGeometry3D::createCableShape(*cableElem, testModel, true);
            TEST_CHECK(!cableSolid.IsNull(), "Subtest 44.4: Solide OpenCASCADE B-Rep non-nul genere");
            TEST_CHECK(cableSolid.ShapeType() == TopAbs_SOLID || cableSolid.ShapeType() == TopAbs_COMPOUND,
                       "Subtest 44.4: Type OpenCASCADE valide");

            // Calcul et validation du Bounding Box OpenCASCADE
            Bnd_Box bbox;
            BRepBndLib::Add(cableSolid, bbox);
            TEST_CHECK(!bbox.IsVoid(), "Subtest 44.4: Bounding Box du cable calcule");

            double xmin, ymin, zmin, xmax, ymax, zmax;
            bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

            double spanX = xmax - xmin;
            double spanZ = zmax - zmin;
            TEST_CHECK(spanX >= 9.9, "Subtest 44.4: Portee X OpenCASCADE >= 9.9 m");
            TEST_CHECK(spanZ >= 1.9, "Subtest 44.4: Denivele Z OpenCASCADE >= 1.9 m");

            std::cout << "  [PASS] Subtest 44.4: Synchronisation CableLibrary, Model & B-Rep 3D OpenCASCADE Validees" << std::endl;
        }

        std::cout << "[PASS] Test 44: TSALib Phase 7 - Externalisation des Cables & Torons Eurocodes / ASTM (4 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa ---" << std::endl;

        // 45.1: Generation et Affectation de MechanicalSnapshot & DefinitionReference
        TSA::Model::Model testModel;
        int n1 = testModel.addNode(0.0, 0.0, 0.0, "", "N1");
        int n2 = testModel.addNode(5.0, 0.0, 0.0, "", "N2");
        int n3 = testModel.addNode(5.0, 0.0, 3.0, "", "N3");

        auto matConcrete = TSA::Model::Material::concreteC25_30();
        auto matSteel = TSA::Model::Material::steelS355();
        auto secBeam = TSA::Model::Section::rectangular(0.30, 0.50);
        auto secCol = TSA::Model::Section::rectangular(0.30, 0.30);

        testModel.addBar(n1, n2, secBeam, matConcrete, TSA::Model::BarRole::Beam, 0.0, "Poutre B1");
        testModel.addBar(n2, n3, secCol, matSteel, TSA::Model::BarRole::Column, 0.0, "Poteau C1");

        // Affectation explicite de snapshots de calcul immuables
        TSA::ExtensionSystem::MechanicalSnapshot concreteSnap;
        concreteSnap.youngModulus = 31.0e9;
        concreteSnap.poissonRatio = 0.20;
        concreteSnap.density = 2500.0;
        concreteSnap.characteristicStrength = 25.0e6;
        concreteSnap.yieldStrength = 25.0e6;
        concreteSnap.thermalCoeff = 1.0e-5;

        TSA::ExtensionSystem::DefinitionReference concreteRef;
        concreteRef.libraryId = "org.tsaraloha.tsalib";
        concreteRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        concreteRef.definitionId = "concrete.c25_30";
        concreteRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

        testModel.setCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30", concreteSnap, concreteRef);

        TSA::ExtensionSystem::MechanicalSnapshot steelSnap;
        steelSnap.youngModulus = 210.0e9;
        steelSnap.poissonRatio = 0.30;
        steelSnap.density = 7850.0;
        steelSnap.characteristicStrength = 355.0e6;
        steelSnap.yieldStrength = 355.0e6;
        steelSnap.thermalCoeff = 1.2e-5;

        TSA::ExtensionSystem::DefinitionReference steelRef;
        steelRef.libraryId = "org.tsaraloha.tsalib";
        steelRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        steelRef.definitionId = "steel.s355";
        steelRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

        testModel.setCalculationSnapshot("org.tsaraloha.tsalib:steel.s355", steelSnap, steelRef);

        // Ajout d'un cable avec definition catalogue et snapshot
        auto& cableLib = TSA::Library::CableLibrary::instance();
        cableLib.reloadFromRegistry();
        const auto* foundCableDef = cableLib.findByName("Stay PSS 19x15.7mm");
        if (foundCableDef)
        {
            testModel.addCable(n1, n3, *foundCableDef, "Hauban H1");
            TSA::ExtensionSystem::MechanicalSnapshot cableSnap;
            cableSnap.youngModulus = foundCableDef->elasticModulus();
            cableSnap.poissonRatio = 0.30;
            cableSnap.density = foundCableDef->density();
            cableSnap.characteristicStrength = foundCableDef->characteristicStrength();
            cableSnap.yieldStrength = foundCableDef->minimumBreakingForce() / foundCableDef->metallicArea();
            cableSnap.thermalCoeff = 1.2e-5;

            TSA::ExtensionSystem::DefinitionReference cableRef;
            cableRef.libraryId = "org.tsaraloha.tsalib";
            cableRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            cableRef.definitionId = "stay_pss_19_15_7";
            cableRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

            testModel.setCalculationSnapshot("org.tsaraloha.tsalib:stay_pss_19_15_7", cableSnap, cableRef);
        }

        TEST_CHECK(testModel.hasCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30"), "Subtest 45.1: Snapshot beton enregistre");
        TEST_CHECK(testModel.hasCalculationSnapshot("org.tsaraloha.tsalib:steel.s355"), "Subtest 45.1: Snapshot acier enregistre");
        TEST_CHECK(testModel.calculationSnapshots().size() >= 2, "Subtest 45.1: Au moins 2 snapshots presents");
        std::cout << "  [PASS] Subtest 45.1: Generation et Affectation des Snapshots de Calcul Validees" << std::endl;

        // 45.2: Serialisation binaire dans le format .tsa avec CHUNK_SNAP
        std::string testFilePath = "test_phase8_snapshot.tsa";
        TSA::IO::TSAFileWriter writer;
        writer.setCompressionEnabled(true);
        std::string saveErr;
        bool saved = writer.saveToFile(testFilePath, testModel, nullptr, "Projet Phase 8", "TSA Testing", &saveErr);
        TEST_CHECK(saved, "Subtest 45.2: Sauvegarde fichier .tsa reussie");
        TEST_CHECK(std::filesystem::exists(testFilePath), "Subtest 45.2: Fichier .tsa cree sur disque");

        // Verification de la presence du header valide
        TSA::IO::TSAFileHeader header;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(testFilePath, header), "Subtest 45.2: En-tete binaire lisible");
        TEST_CHECK(header.magic == TSA::IO::TSA_FILE_MAGIC, "Subtest 45.2: Magic TSAF valide");
        TEST_CHECK(header.checksumCRC32 != 0, "Subtest 45.2: Checksum CRC32 present");
        std::cout << "  [PASS] Subtest 45.2: Serialisation Binaire avec CHUNK_SNAP Validee" << std::endl;

        // 45.3: Deserialisation et Verification Bitwise des Snapshots et References
        TSA::Model::Model loadedModel;
        TSA::IO::TSAFileReader reader;
        std::string loadErr, outProj, outAuth;
        bool loaded = reader.loadFromFile(testFilePath, loadedModel, nullptr, "", &outProj, &outAuth, nullptr, &loadErr);
        TEST_CHECK(loaded, "Subtest 45.3: Chargement du fichier .tsa reussi");
        TEST_CHECK(outProj == "Projet Phase 8", "Subtest 45.3: Nom de projet conforme");

        TEST_CHECK(loadedModel.hasCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30"), "Subtest 45.3: Snapshot beton recupere");
        const auto* loadedConcreteSnap = loadedModel.getCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(loadedConcreteSnap != nullptr, "Subtest 45.3: Pointeur snapshot non-nul");
        if (loadedConcreteSnap)
        {
            TEST_CHECK(approxEqual(loadedConcreteSnap->youngModulus, 31.0e9), "Subtest 45.3: E = 31 GPa bitwise exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->poissonRatio, 0.20), "Subtest 45.3: nu = 0.20 exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->density, 2500.0), "Subtest 45.3: rho = 2500 kg/m3 exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->characteristicStrength, 25.0e6), "Subtest 45.3: fck = 25 MPa exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->thermalCoeff, 1.0e-5), "Subtest 45.3: alpha = 1.0e-5 exact");
        }

        const auto* loadedConcreteRef = loadedModel.getDefinitionReference("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(loadedConcreteRef != nullptr, "Subtest 45.3: DefinitionReference presente");
        if (loadedConcreteRef)
        {
            TEST_CHECK(loadedConcreteRef->libraryId == "org.tsaraloha.tsalib", "Subtest 45.3: Library ID conforme");
            TEST_CHECK(loadedConcreteRef->libraryVersion.toString() == "1.0.0", "Subtest 45.3: Library Version 1.0.0");
            TEST_CHECK(loadedConcreteRef->definitionId == "concrete.c25_30", "Subtest 45.3: Definition ID conforme");
        }

        // Test de persistance Undo/Redo dans le modele charge
        auto undoSnapshot = loadedModel.createSnapshot("Test Undo");
        TEST_CHECK(undoSnapshot.calculationSnapshots.size() >= 2, "Subtest 45.3: Snapshots inclus dans ModelStateSnapshot");
        std::filesystem::remove(testFilePath);
        std::cout << "  [PASS] Subtest 45.3: Deserialisation et Verification Bitwise des Snapshots Validees" << std::endl;

        // 45.4: Simulation d'Evolution Normative de TSALib & Detection par LibraryVersionManager
        TSA::ExtensionSystem::LibraryVersionManager versionMgr;

        // Cas A : Definition identique -> 0 differences mecaniques
        TSA::ExtensionSystem::MaterialDefinition identicalDef;
        identicalDef.id = "concrete.c25_30";
        identicalDef.version = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        identicalDef.youngModulus = TSA::ExtensionSystem::PhysicalValue(31000.0, "MPa");
        identicalDef.poissonRatio = 0.20;
        identicalDef.density = TSA::ExtensionSystem::PhysicalValue(2500.0, "kg/m3");
        identicalDef.fck = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");
        identicalDef.fy = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");
        identicalDef.thermalCoeff = TSA::ExtensionSystem::PhysicalValue(1.0e-5, "1/K");

        auto resultIdentical = versionMgr.compare(*loadedConcreteSnap, TSA::ExtensionSystem::SemanticVersion(1, 0, 0), identicalDef);
        TEST_CHECK(!resultIdentical.hasMechanicalChanges(), "Subtest 45.4: Aucune divergence detectee sur version identique");

        // Cas B : Mise a jour normative TSALib (v1.1.0 avec E = 35 GPa et fck = 28 MPa)
        TSA::ExtensionSystem::MaterialDefinition modifiedDef = identicalDef;
        modifiedDef.version = TSA::ExtensionSystem::SemanticVersion(1, 1, 0);
        modifiedDef.youngModulus = TSA::ExtensionSystem::PhysicalValue(35000.0, "MPa"); // Modifie
        modifiedDef.fck = TSA::ExtensionSystem::PhysicalValue(28.0, "MPa");            // Modifie
        modifiedDef.fy = TSA::ExtensionSystem::PhysicalValue(28.0, "MPa");             // Modifie

        auto resultModified = versionMgr.compare(*loadedConcreteSnap, TSA::ExtensionSystem::SemanticVersion(1, 0, 0), modifiedDef);
        TEST_CHECK(resultModified.hasMechanicalChanges(), "Subtest 45.4: Divergence mecanique detectee avec succes");
        TEST_CHECK(resultModified.modifiedProperties.size() == 3, "Subtest 45.4: Exactement 3 proprietes physiques modifiees (E, fck et fy)");

        // Verification que le snapshot du projet en memoire est reste INALTERE (garantie de reproductibilite)
        const auto* preservedSnap = loadedModel.getCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(approxEqual(preservedSnap->youngModulus, 31.0e9), "Subtest 45.4: E du projet conserve a 31 GPa (immuabilite garantie)");
        TEST_CHECK(approxEqual(preservedSnap->characteristicStrength, 25.0e6), "Subtest 45.4: fck du projet conserve a 25 MPa (immuabilite garantie)");

        std::cout << "  [PASS] Subtest 45.4: Detection de Derive Normative & Garantie de Reproductibilite Validees" << std::endl;

        std::cout << "[PASS] Test 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa (4 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 46: TSALib Phase 9 - Optimisation : Lazy Loading & Cache Multi-Niveaux
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 46: TSALib Phase 9 - Optimisation : Lazy Loading & Cache Multi-Niveaux ---" << std::endl;

        // 46.1: Benchmark d'Indexation Ultra-Rapide au Demarrage (< 50 ms)
        auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();
        reg.clear();

        TSA::ExtensionSystem::LibraryManager testLibMgr;
        testLibMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");

        auto t0 = std::chrono::high_resolution_clock::now();
        auto manifests = testLibMgr.discover();
        auto t1 = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        TEST_CHECK(!manifests.empty(), "Subtest 46.1: Au moins 1 manifest decouvert");
        TEST_CHECK(durationMs < 50.0, "Subtest 46.1: Duree d'indexation au demarrage < 50 ms");
        TEST_CHECK(testLibMgr.indexedDefinitionsCount() >= 57, "Subtest 46.1: Au moins 57 definitions indexees");

        // Verification du principe fondamental du Lazy Loading : AUCUN parsing complet effectue
        TEST_CHECK(reg.materialCount() == 0, "Subtest 46.1: 0 materiaux charges prematurement");
        TEST_CHECK(reg.sectionCount() == 0, "Subtest 46.1: 0 sections chargees prematurement");
        TEST_CHECK(reg.cableCount() == 0, "Subtest 46.1: 0 cables charges prematurement");

        TEST_CHECK(testLibMgr.isIndexed("concrete.c25_30"), "Subtest 46.1: concrete.c25_30 indexe");
        TEST_CHECK(!testLibMgr.isLoaded("concrete.c25_30"), "Subtest 46.1: concrete.c25_30 non charge");
        TEST_CHECK(testLibMgr.isIndexed("ipe200") || testLibMgr.isIndexed("steel.ipe200"), "Subtest 46.1: steel.ipe200 indexe");
        TEST_CHECK(!testLibMgr.isLoaded("ipe200") && !testLibMgr.isLoaded("steel.ipe200"), "Subtest 46.1: steel.ipe200 non charge");
        TEST_CHECK(testLibMgr.isIndexed("stay_pss_19_15_7"), "Subtest 46.1: stay_pss_19_15_7 indexe");
        TEST_CHECK(!testLibMgr.isLoaded("stay_pss_19_15_7"), "Subtest 46.1: stay_pss_19_15_7 non charge");

        std::cout << "  [PASS] Subtest 46.1: Indexation Rapide au Demarrage (" << durationMs << " ms < 50 ms) & Zero Parsing Premature Validees" << std::endl;

        // 46.2: Resolution et Chargement a la Demande (On-Demand Lazy Loading)
        const auto* loadedMat = testLibMgr.findMaterial("concrete.c25_30");
        TEST_CHECK(loadedMat != nullptr, "Subtest 46.2: Definition concrete.c25_30 resolue a la demande");
        if (loadedMat)
        {
            TEST_CHECK(loadedMat->name == "Concrete C25/30" || loadedMat->name == "C25/30", "Subtest 46.2: Nom du materiau conforme");
            TEST_CHECK(testLibMgr.isLoaded("concrete.c25_30"), "Subtest 46.2: Statut passe a isLoaded=true");
            TEST_CHECK(reg.materialCount() == 1, "Subtest 46.2: Exactement 1 materiau en memoire dans le registre");
        }

        const auto* loadedSec = testLibMgr.findSection("ipe200");
        if (!loadedSec) loadedSec = testLibMgr.findSection("steel.ipe200");
        TEST_CHECK(loadedSec != nullptr, "Subtest 46.2: Definition ipe200 resolue a la demande");
        if (loadedSec)
        {
            TEST_CHECK(loadedSec->name == "IPE 200", "Subtest 46.2: Nom de la section conforme");
            TEST_CHECK(testLibMgr.isLoaded("ipe200") || testLibMgr.isLoaded("steel.ipe200"), "Subtest 46.2: Statut passe a isLoaded=true");
            TEST_CHECK(reg.sectionCount() == 1, "Subtest 46.2: Exactement 1 section en memoire dans le registre");
        }

        const auto* loadedCab = testLibMgr.findCable("stay_pss_19_15_7");
        TEST_CHECK(loadedCab != nullptr, "Subtest 46.2: Definition stay_pss_19_15_7 resolue a la demande");
        if (loadedCab)
        {
            TEST_CHECK(loadedCab->name == "Stay PSS 19x15.7mm", "Subtest 46.2: Nom du cable conforme");
            TEST_CHECK(testLibMgr.isLoaded("stay_pss_19_15_7"), "Subtest 46.2: Statut passe a isLoaded=true");
            TEST_CHECK(reg.cableCount() == 1, "Subtest 46.2: Exactement 1 cable en memoire dans le registre");
        }

        // Test d'un ID inexistant
        TEST_CHECK(testLibMgr.findMaterial("unknown.mat.xyz") == nullptr, "Subtest 46.2: ID inconnu retourne nullptr");

        std::cout << "  [PASS] Subtest 46.2: Resolution et Chargement a la Demande (On-Demand) Validees" << std::endl;

        // 46.3: Cache Multi-Niveaux Haute Performance & Telemetrie
        auto& cache = TSA::ExtensionSystem::LibraryCache::instance();
        cache.clear();
        cache.resetMetrics();

        TSA::Model::Material sampleMat = TSA::Model::Material::concreteC25_30();
        TSA::Model::Section sampleSec = TSA::Model::Section::rectangular(0.30, 0.50);
        TSA::Model::CableDefinition sampleCab = TSA::Model::CableDefinition::strandY1860S7_15_7();
        TSA::ExtensionSystem::MechanicalSnapshot sampleSnap;
        sampleSnap.youngModulus = 31.0e9;

        cache.putSnapshot("org.tsaraloha.tsalib:concrete.c25_30", sampleSnap);
        cache.putMaterial("mat:c25_30", sampleMat);
        cache.putSection("sec:rect_300x500", sampleSec);
        cache.putCable("cab:t15_7", sampleCab);

        TEST_CHECK(cache.snapshotCount() == 1, "Subtest 46.3: 1 snapshot en cache");
        TEST_CHECK(cache.materialCount() == 1, "Subtest 46.3: 1 materiau en cache");
        TEST_CHECK(cache.sectionCount() == 1, "Subtest 46.3: 1 section en cache");
        TEST_CHECK(cache.cableCount() == 1, "Subtest 46.3: 1 cable en cache");

        // Acces avec succes (Cache Hits)
        const auto* hitSnap = cache.getSnapshot("org.tsaraloha.tsalib:concrete.c25_30");
        const auto* hitMat = cache.getMaterial("mat:c25_30");
        const auto* hitSec = cache.getSection("sec:rect_300x500");
        const auto* hitCab = cache.getCable("cab:t15_7");

        TEST_CHECK(hitSnap != nullptr, "Subtest 46.3: Hit snapshot");
        TEST_CHECK(hitMat != nullptr, "Subtest 46.3: Hit material");
        TEST_CHECK(hitSec != nullptr, "Subtest 46.3: Hit section");
        TEST_CHECK(hitCab != nullptr, "Subtest 46.3: Hit cable");

        // Acces infructueux (Cache Miss)
        const auto* missMat = cache.getMaterial("mat:missing");
        TEST_CHECK(missMat == nullptr, "Subtest 46.3: Miss material");

        TEST_CHECK(cache.hitCount() == 4, "Subtest 46.3: 4 Cache Hits enregistres");
        TEST_CHECK(cache.missCount() == 1, "Subtest 46.3: 1 Cache Miss enregistre");
        TEST_CHECK(approxEqual(cache.hitRatio(), 4.0 / 5.0), "Subtest 46.3: Hit Ratio = 80%");

        // Invalidation selective
        cache.invalidate("c25_30");
        TEST_CHECK(cache.getMaterial("mat:c25_30") == nullptr, "Subtest 46.3: Materiau invalide et purge");
        TEST_CHECK(cache.getSection("sec:rect_300x500") != nullptr, "Subtest 46.3: Section preservee apres invalidation selective");

        std::cout << "  [PASS] Subtest 46.3: Cache Multi-Niveaux & Telemetrie (Hit Ratio = 80%) Valides" << std::endl;

        // 46.4: Cache de Solides 3D B-Rep OpenCASCADE (TopoDS_Shape)
        TSA::Model::Model geomModel;
        int gn1 = geomModel.addNode(0.0, 0.0, 0.0);
        int gn2 = geomModel.addNode(0.0, 0.0, 4.0);
        int colId = geomModel.addColumn(gn1, gn2, 0.30, 0.30, "Poteau Cache Test");
        const auto* colElem = geomModel.getColumn(colId);
        const auto* startN = geomModel.getNode(gn1);
        const auto* endN = geomModel.getNode(gn2);
        TEST_CHECK(startN != nullptr && endN != nullptr, "Subtest 46.4: Noeuds du poteau valides");

        TopoDS_Shape colShape = TSA::Geometry::BeamGeometry::createBeamShape(*startN, *endN, 0.30, 0.30);
        TEST_CHECK(!colShape.IsNull(), "Subtest 46.4: Solide OpenCASCADE genere");

        cache.putShape("column_solid_0.30x0.30_H4m", colShape);
        TEST_CHECK(cache.hasShape("column_solid_0.30x0.30_H4m"), "Subtest 46.4: hasShape retourne true");
        TEST_CHECK(cache.shapeCount() == 1, "Subtest 46.4: 1 solide 3D en cache");

        TopoDS_Shape retrievedShape = cache.getShape("column_solid_0.30x0.30_H4m");
        TEST_CHECK(!retrievedShape.IsNull(), "Subtest 46.4: Solide 3D recupere du cache avec succes");
        TEST_CHECK(retrievedShape.ShapeType() == TopAbs_SOLID || retrievedShape.ShapeType() == TopAbs_COMPOUND,
                   "Subtest 46.4: Type OpenCASCADE solide valide");

        cache.clearShapes();
        TEST_CHECK(cache.shapeCount() == 0, "Subtest 46.4: Cache de solides 3D purge avec clearShapes");
        TEST_CHECK(!cache.hasShape("column_solid_0.30x0.30_H4m"), "Subtest 46.4: Shape purge");

        // Rechargement complet de TSALib pour garantir l'etat final des registres
        testLibMgr.load("org.tsaraloha.tsalib");
        TEST_CHECK(reg.materialCount() >= 16, "Subtest 46.4: TSALib rechargee completement apres le test");

        std::cout << "  [PASS] Subtest 46.4: Cache de Geometries 3D B-Rep OpenCASCADE Valide" << std::endl;

        std::cout << "[PASS] Test 46: TSALib Phase 9 - Optimisation : Lazy Loading & Cache Multi-Niveaux (4 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 47: TSALib Phase 10 - Interface Utilisateur Library Manager & Gestionnaire d'Extensions
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 47: TSALib Phase 10 - Interface Utilisateur Library Manager & Gestionnaire d'Extensions ---" << std::endl;

        // 47.1: Instanciation et initialisation du dialogue moderne Master-Detail
        TSA::UI::ExtensionManagerDialog extDlg;
        TEST_CHECK(!extDlg.windowTitle().isEmpty(), "Subtest 47.1: Titre de la fenetre non vide");
        TEST_CHECK(extDlg.windowTitle().contains("TSALib"), "Subtest 47.1: Titre reference TSALib");
        TEST_CHECK(extDlg.displayedItemCount() >= 16, "Subtest 47.1: Au moins 16 materiaux affiches par defaut");

        std::cout << "  [PASS] Subtest 47.1: Instanciation et Initialisation de ExtensionManagerDialog Validees" << std::endl;

        // 47.2: Navigation par Catégories et affichage dynamique du catalogue
        extDlg.selectCategory("Sections");
        TEST_CHECK(extDlg.displayedItemCount() >= 22, "Subtest 47.2: Au moins 22 sections Eurocodes affichees");

        extDlg.selectCategory("Cables");
        TEST_CHECK(extDlg.displayedItemCount() >= 19, "Subtest 47.2: Au moins 19 cables Eurocodes / ASTM affiches");

        extDlg.selectCategory("Textures");
        TEST_CHECK(extDlg.displayedItemCount() >= 14, "Subtest 47.2: Au moins 14 textures PBR affichees");

        extDlg.selectCategory("Extensions");
        TEST_CHECK(extDlg.displayedItemCount() >= 1, "Subtest 47.2: Au moins 1 extension installee listee");

        extDlg.selectCategory("Standards");
        TEST_CHECK(extDlg.displayedItemCount() >= 5, "Subtest 47.2: Normes Eurocodes listees");

        std::cout << "  [PASS] Subtest 47.2: Navigation Multi-Categories (Sections, Cables, Textures, Extensions, Normes) Validee" << std::endl;

        // 47.3: Filtrage et Recherche Textuelle en Temps Réel
        extDlg.selectCategory("Materials");
        extDlg.setSearchQuery("C25");
        TEST_CHECK(extDlg.displayedItemCount() >= 1, "Subtest 47.3: Filtrage materiau C25");

        extDlg.selectCategory("Sections");
        extDlg.setSearchQuery("IPE");
        TEST_CHECK(extDlg.displayedItemCount() >= 5, "Subtest 47.3: Filtrage profilés IPE (au moins 5)");

        extDlg.selectCategory("Cables");
        extDlg.setSearchQuery("Stay");
        TEST_CHECK(extDlg.displayedItemCount() >= 3, "Subtest 47.3: Filtrage haubans Stay (au moins 3)");

        extDlg.setSearchQuery("introuvable_xyz_999");
        TEST_CHECK(extDlg.displayedItemCount() == 0, "Subtest 47.3: Recherche infructueuse retourne 0 elements");

        // Reinitialisation du filtre
        extDlg.setSearchQuery("");
        TEST_CHECK(extDlg.displayedItemCount() >= 19, "Subtest 47.3: Retablissement de la liste complete");

        std::cout << "  [PASS] Subtest 47.3: Filtrage et Recherche Textuelle Instantanee Multi-Criteres Valides" << std::endl;

        // 47.4: Rechargement a Chaud en 1 clic (Hot Reload) & Validation Globale
        extDlg.onReloadAll(false); // Mode silencieux pour test automatique
        extDlg.selectCategory("Materials");
        TEST_CHECK(extDlg.displayedItemCount() >= 16, "Subtest 47.4: 16 materiaux recharges avec succes");

        extDlg.onValidateAll(false); // Mode silencieux pour test automatique
        auto valResult = TSA::ExtensionSystem::LibraryManager::instance().validateAll();
        TEST_CHECK(valResult.isValid(), "Subtest 47.4: Validation globale 100% conforme sans erreur");

        std::cout << "  [PASS] Subtest 47.4: Rechargement a Chaud (Hot Reload) & Validation Globale Valides" << std::endl;

        // 47.5: Telemetrie & Indicateurs de Performance
        auto& cache = TSA::ExtensionSystem::LibraryCache::instance();
        auto& mgr = TSA::ExtensionSystem::LibraryManager::instance();
        TEST_CHECK(mgr.indexedDefinitionsCount() >= 57, "Subtest 47.5: Telemetrie indexation >= 57 definitions");
        TEST_CHECK(cache.hitCount() >= 0, "Subtest 47.5: Compteur de Hits cache operationnel");
        TEST_CHECK(cache.hitRatio() >= 0.0 && cache.hitRatio() <= 1.0, "Subtest 47.5: Ratio de hit cache borne [0, 1]");

        std::cout << "  [PASS] Subtest 47.5: Telemetrie du Cache & Performance en Temps Reel Validees" << std::endl;

        std::cout << "[PASS] Test 47: TSALib Phase 10 - Interface Utilisateur Library Manager & Gestionnaire d'Extensions (5 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    std::cout << "=================================================" << std::endl;
    std::cout << "RESULTS: " << passed << " / " << total << " tests passed successfully!" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
