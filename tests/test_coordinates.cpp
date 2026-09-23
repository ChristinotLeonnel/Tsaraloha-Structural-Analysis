#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>

#include "Coordinate/Point3D.h"
#include "Coordinate/LevelManager.h"
#include "Coordinate/CoordinateSystem.h"
#include "Model/Model.h"
#include "Grid/CartesianGrid.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridSystem.h"
#include "Grid/GridSnapManager.h"

using namespace TSA::Coordinate;
using namespace TSA::Model;
using namespace TSA::Grid;

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
    int total = 10;

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

    std::cout << "=================================================" << std::endl;
    std::cout << "RESULTS: " << passed << " / " << total << " tests passed successfully!" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
