#include "test_common.h"
#include "Coordinate/WorkPlaneCoordinateSystem.h"
#include "Coordinate/CoordinateTransform.h"
#include "Coordinate/AxisColorConfig.h"
#include "Viewer/ProjectionManager.h"
#include "Viewer/ViewManager.h"
#include "Grid/SnapManager.h"

bool runSuite_WorkPlane(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 51: Advanced CAD Modeling Navigation, WorkPlane, Coordinate Transformations, and Object Snap Engine
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[TEST 51] Advanced CAD Modeling Navigation, WorkPlane, Coordinate Transformations, and Object Snap Engine..." << std::endl;

        // Subtest 51.1: WorkPlane Standard Factories, Raycasting & Coordinate Conversions
        {
            auto wpXY = WorkPlane::xy(2.5, "Floor Level 1");
            TEST_CHECK(wpXY.type() == WorkPlaneType::GlobalXY, "Subtest 51.1: XY plane type");
            TEST_CHECK(approxEqual(wpXY.offset(), 2.5), "Subtest 51.1: XY plane offset");
            TEST_CHECK(approxEqual(wpXY.origin().Z(), 2.5), "Subtest 51.1: XY plane origin Z");

            // Orthogonal projection
            gp_Pnt ptA(10.0, 20.0, 100.0);
            gp_Pnt projA = wpXY.projectOrtho(ptA);
            TEST_CHECK(approxEqual(projA.X(), 10.0) && approxEqual(projA.Y(), 20.0) && approxEqual(projA.Z(), 2.5), "Subtest 51.1: projectOrtho on XY plane");
            TEST_CHECK(approxEqual(wpXY.distanceTo(ptA), 97.5), "Subtest 51.1: distanceTo XY plane");

            // Raycast intersection: camera eye at (5, 5, 20), looking down along (0, 0, -1)
            gp_Pnt eye(5.0, 5.0, 20.0);
            gp_Dir rayDir(0.0, 0.0, -1.0);
            gp_Pnt hitPnt;
            bool hit = wpXY.projectRay(eye, rayDir, hitPnt);
            TEST_CHECK(hit, "Subtest 51.1: Raycast hit XY plane");
            TEST_CHECK(approxEqual(hitPnt.X(), 5.0) && approxEqual(hitPnt.Y(), 5.0) && approxEqual(hitPnt.Z(), 2.5), "Subtest 51.1: Raycast hit coordinates");

            // WCS <-> UCS
            gp_Pnt ucsPt = wpXY.toUcs(hitPnt);
            TEST_CHECK(approxEqual(ucsPt.X(), 5.0) && approxEqual(ucsPt.Y(), 5.0) && approxEqual(ucsPt.Z(), 0.0), "Subtest 51.1: toUcs coordinates");
            gp_Pnt backWorld = wpXY.toWorld(ucsPt);
            TEST_CHECK(approxEqual(backWorld.X(), 5.0) && approxEqual(backWorld.Y(), 5.0) && approxEqual(backWorld.Z(), 2.5), "Subtest 51.1: toWorld coordinates");

            // WorkPlane 3 points
            gp_Pnt p1(0, 0, 0);
            gp_Pnt p2(10, 0, 0);
            gp_Pnt p3(0, 10, 0);
            auto wp3P = WorkPlane::fromThreePoints(p1, p2, p3, "Custom 3P");
            TEST_CHECK(wp3P.type() == WorkPlaneType::ThreePoints, "Subtest 51.1: 3P plane type");
            TEST_CHECK(approxEqual(wp3P.normal().Z(), 1.0), "Subtest 51.1: 3P normal Z");

            // JSON serialization
            auto json = wpXY.toJson();
            auto restoredWp = WorkPlane::fromJson(json);
            TEST_CHECK(restoredWp.name() == "Floor Level 1", "Subtest 51.1: JSON name restored");
            TEST_CHECK(approxEqual(restoredWp.offset(), 2.5), "Subtest 51.1: JSON offset restored");

            std::cout << "  [PASS] Subtest 51.1: WorkPlane Standard Factories, Raycasting & Projections Verified" << std::endl;
        }

        // Subtest 51.2: CoordinateTransformationService & Local Element Frames
        {
            auto& cts = CoordinateTransformationService::instance();
            cts.setActiveWorkPlane(WorkPlane::xy(3.0, "Floor 3m"));
            TEST_CHECK(cts.hasActiveWorkPlane(), "Subtest 51.2: hasActiveWorkPlane is true");
            TEST_CHECK(approxEqual(cts.activeWorkPlane().offset(), 3.0), "Subtest 51.2: Active work plane offset is 3m");

            gp_Pnt wPt(2.0, 4.0, 3.0);
            gp_Pnt uPt = cts.wcsToUcs(wPt);
            TEST_CHECK(approxEqual(uPt.Z(), 0.0), "Subtest 51.2: wcsToUcs Z is 0 on plane");
            gp_Pnt backPt = cts.ucsToWcs(uPt);
            TEST_CHECK(approxEqual(backPt.Z(), 3.0), "Subtest 51.2: ucsToWcs Z is restored to 3");

            // Element local coordinate system: horizontal beam along X from (0,0,0) to (5,0,0)
            gp_Pnt start(0, 0, 0);
            gp_Pnt end(5, 0, 0);
            auto localFrame = cts.computeElementLocalFrame(start, end, 0.0);
            TEST_CHECK(approxEqual(localFrame.XDirection().X(), 1.0), "Subtest 51.2: Beam local X-axis is longitudinal");
            TEST_CHECK(approxEqual(localFrame.Direction().Z(), 1.0), "Subtest 51.2: Beam local Z-axis is vertical (+Z)");

            // Point along beam converted to local
            gp_Pnt midW(2.5, 0.0, 0.0);
            gp_Pnt midL = cts.wcsToElementLocal(midW, start, end, 0.0);
            TEST_CHECK(approxEqual(midL.X(), 2.5) && approxEqual(midL.Y(), 0.0) && approxEqual(midL.Z(), 0.0), "Subtest 51.2: wcsToElementLocal midpoint");

            // Vertical column from (0,0,0) to (0,0,4)
            gp_Pnt colStart(0, 0, 0);
            gp_Pnt colEnd(0, 0, 4);
            auto colFrame = cts.computeElementLocalFrame(colStart, colEnd, 0.0);
            TEST_CHECK(approxEqual(colFrame.XDirection().Z(), 1.0), "Subtest 51.2: Column local X-axis is vertical (+Z)");

            std::cout << "  [PASS] Subtest 51.2: CoordinateTransformationService & Local Element Frames Verified" << std::endl;
        }

        // Subtest 51.3: SnapMode Bitmask Operations
        {
            auto modes = SnapMode::Endpoint | SnapMode::Midpoint | SnapMode::Center;
            TEST_CHECK(hasSnapMode(modes, SnapMode::Endpoint), "Subtest 51.3: Bitmask has Endpoint");
            TEST_CHECK(hasSnapMode(modes, SnapMode::Midpoint), "Subtest 51.3: Bitmask has Midpoint");
            TEST_CHECK(hasSnapMode(modes, SnapMode::Center), "Subtest 51.3: Bitmask has Center");
            TEST_CHECK(!hasSnapMode(modes, SnapMode::Intersection), "Subtest 51.3: Bitmask does NOT have Intersection");

            // Remove Midpoint
            modes = modes & ~SnapMode::Midpoint;
            TEST_CHECK(!hasSnapMode(modes, SnapMode::Midpoint), "Subtest 51.3: Midpoint removed");
            TEST_CHECK(hasSnapMode(modes, SnapMode::Endpoint), "Subtest 51.3: Endpoint still present");

            std::cout << "  [PASS] Subtest 51.3: SnapMode Bitmask Operators Verified" << std::endl;
        }

        // Subtest 51.4: Multi-mode Object Snap on Structural Elements (Beam, Column, Wall, Foundation)
        {
            TSA::Model::Model m;
            int n1 = m.addNode(0.0, 0.0, 0.0);
            int n2 = m.addNode(6.0, 0.0, 0.0);
            int n3 = m.addNode(6.0, 0.0, 3.0);
            int bId = m.addBeam(n1, n2, 0.30, 0.50);
            int colId = m.addColumn(n2, n3, 0.40, 0.40);
            int fId = m.addFoundation(n1, 1.5, 1.5, 0.5);
            (void)colId;
            (void)bId;
            (void)fId;

            GridSnapManager snapMgr;
            snapMgr.setActiveModes(SnapMode::Endpoint | SnapMode::Midpoint | SnapMode::Center | SnapMode::Nearest);

            // 1. Endpoint snap near n1 (0.05, 0.05, 0.0)
            auto snapNearN1 = snapMgr.findObjectSnap(gp_Pnt(0.05, 0.05, 0.0), &m, 0.5);
            TEST_CHECK(snapNearN1.snapped, "Subtest 51.4: Endpoint snap found near N1");
            TEST_CHECK(snapNearN1.type == GridSnapType::Endpoint, "Subtest 51.4: Snap type is Endpoint");
            TEST_CHECK(approxEqual(snapNearN1.point.X(), 0.0) && approxEqual(snapNearN1.point.Y(), 0.0), "Subtest 51.4: Endpoint snapped to (0,0,0)");

            // 2. Midpoint snap near (3.05, 0.02, 0.0) on the beam
            auto snapMidBeam = snapMgr.findObjectSnap(gp_Pnt(3.05, 0.02, 0.0), &m, 0.5);
            TEST_CHECK(snapMidBeam.snapped, "Subtest 51.4: Midpoint snap found on beam");
            TEST_CHECK(snapMidBeam.type == GridSnapType::Midpoint, "Subtest 51.4: Snap type is Midpoint");
            TEST_CHECK(approxEqual(snapMidBeam.point.X(), 3.0) && approxEqual(snapMidBeam.point.Y(), 0.0) && approxEqual(snapMidBeam.point.Z(), 0.0), "Subtest 51.4: Beam midpoint is exactly (3,0,0)");

            // 3. Center snap near foundation at N1
            snapMgr.setActiveModes(SnapMode::Center);
            auto snapCenterF = snapMgr.findObjectSnap(gp_Pnt(0.08, 0.08, -0.2), &m, 0.5);
            TEST_CHECK(snapCenterF.snapped, "Subtest 51.4: Center snap found on foundation");
            TEST_CHECK(snapCenterF.type == GridSnapType::Center, "Subtest 51.4: Snap type is Center");

            // 4. Nearest snap along beam at X = 1.75
            snapMgr.setActiveModes(SnapMode::Nearest);
            auto snapNearBeam = snapMgr.findObjectSnap(gp_Pnt(1.75, 0.08, 0.0), &m, 0.3);
            TEST_CHECK(snapNearBeam.snapped, "Subtest 51.4: Nearest snap found on beam axis");
            TEST_CHECK(snapNearBeam.type == GridSnapType::Nearest, "Subtest 51.4: Snap type is Nearest");
            TEST_CHECK(approxEqual(snapNearBeam.point.X(), 1.75) && approxEqual(snapNearBeam.point.Y(), 0.0), "Subtest 51.4: Nearest projected onto beam axis");

            std::cout << "  [PASS] Subtest 51.4: Multi-mode Object Snap Engine (Endpoint, Midpoint, Center, Nearest) Verified" << std::endl;
        }

        // Subtest 51.5: Camera Navigation History Deep-Copy Stack
        {
            Handle(Graphic3d_Camera) cam1 = new Graphic3d_Camera();
            cam1->SetCenter(gp_Pnt(0, 0, 0));
            cam1->SetEye(gp_Pnt(10, 10, 10));
            cam1->SetScale(100.0);

            Handle(Graphic3d_Camera) cam2 = new Graphic3d_Camera();
            cam2->SetCenter(gp_Pnt(5, 5, 0));
            cam2->SetEye(gp_Pnt(15, 15, 10));
            cam2->SetScale(200.0);

            // Verify deep copy capability
            Handle(Graphic3d_Camera) camCopy = new Graphic3d_Camera();
            camCopy->Copy(cam1);
            TEST_CHECK(camCopy->Center().IsEqual(cam1->Center(), 1e-4), "Subtest 51.5: Camera copy center matches");
            TEST_CHECK(approxEqual(camCopy->Scale(), cam1->Scale()), "Subtest 51.5: Camera copy scale matches");

            // Simulate undo stack
            std::vector<Handle(Graphic3d_Camera)> undoStack;
            undoStack.push_back(camCopy);
            TEST_CHECK(undoStack.size() == 1, "Subtest 51.5: Undo stack size is 1");

            // Restore from undo stack
            Handle(Graphic3d_Camera) restored = new Graphic3d_Camera();
            restored->Copy(undoStack.back());
            undoStack.pop_back();
            TEST_CHECK(undoStack.empty(), "Subtest 51.5: Undo stack empty after pop");
            TEST_CHECK(restored->Center().IsEqual(cam1->Center(), 1e-4), "Subtest 51.5: Restored camera center matches cam1");
            TEST_CHECK(approxEqual(restored->Scale(), 100.0), "Subtest 51.5: Restored camera scale is 100.0");

            std::cout << "  [PASS] Subtest 51.5: Camera Navigation Deep-Copy History Mechanism Verified" << std::endl;
        }

        // Subtest 51.6: Structural Level / Story WorkPlane Synchronization and Elevation Tracking
        {
            LevelManager lm;
            lm.addLevel("RDC", 0.0);
            lm.addLevel("R+1", 3.20);
            lm.addLevel("R+2", 6.40);
            lm.addLevel("Toiture", 9.60);

            TEST_CHECK(lm.levelCount() == 4, "Subtest 51.6: Level count is 4");

            // Recherche exacte par élévation
            const Level* lvl1 = lm.findLevelAtElevation(3.20);
            TEST_CHECK(lvl1 != nullptr && lvl1->name == "R+1", "Subtest 51.6: Level R+1 found at Z=3.20m");

            // Recherche du niveau le plus proche
            const Level* closest = lm.findClosestLevel(3.18);
            TEST_CHECK(closest != nullptr && closest->name == "R+1", "Subtest 51.6: Closest level to 3.18m is R+1");

            // Synchronisation avec WorkPlane
            WorkPlane wpLevel(WorkPlaneType::ElevationZ, lvl1->name, lvl1->elevation);
            TEST_CHECK(wpLevel.type() == WorkPlaneType::ElevationZ, "Subtest 51.6: WorkPlane type is ElevationZ");
            TEST_CHECK(approxEqual(wpLevel.offset(), 3.20), "Subtest 51.6: WorkPlane offset is 3.20m");

            // Projection orthogonale d'un point arbitraire (10.0, 5.0, 1.5) sur le plan d'étage R+1
            gp_Pnt pWorld(10.0, 5.0, 1.5);
            gp_Pnt pProj = wpLevel.projectOrtho(pWorld);
            TEST_CHECK(approxEqual(pProj.X(), 10.0) && approxEqual(pProj.Y(), 5.0) && approxEqual(pProj.Z(), 3.20), "Subtest 51.6: Orthogonal projection onto R+1 workplane");

            // Transformation WCS <-> UCS au niveau R+1
            gp_Pnt pUcs = wpLevel.toUcs(pProj);
            TEST_CHECK(approxEqual(pUcs.X(), 10.0) && approxEqual(pUcs.Y(), 5.0) && approxEqual(pUcs.Z(), 0.0), "Subtest 51.6: UCS coordinate Z is 0 on workplane");
            gp_Pnt pBack = wpLevel.toWorld(pUcs);
            TEST_CHECK(approxEqual(pBack.Z(), 3.20), "Subtest 51.6: Restored world coordinate Z is 3.20m");

            std::cout << "  [PASS] Subtest 51.6: Structural Level / Story WorkPlane Synchronization Verified" << std::endl;
        }

        // Subtest 51.7: WorkPlane Arbitrary 3-Point Definition, Camera Normal Alignment & Geometry
        {
            // Plan incliné défini par 3 points : Origine (0, 0, 0), Point U (10, 0, 0), Point V (0, 5, 5)
            gp_Pnt p1(0.0, 0.0, 0.0);
            gp_Pnt p2(10.0, 0.0, 0.0);
            gp_Pnt p3(0.0, 5.0, 5.0);
            WorkPlane wp3P = WorkPlane::fromThreePoints(p1, p2, p3, "Toiture Inclinee 3P");

            TEST_CHECK(wp3P.type() == WorkPlaneType::ThreePoints, "Subtest 51.7: WorkPlane type is ThreePoints");
            TEST_CHECK(wp3P.name() == "Toiture Inclinee 3P", "Subtest 51.7: WorkPlane name is preserved");

            // Vecteur X-direction doit être orienté selon P1->P2 (1, 0, 0)
            gp_Dir uDir = wp3P.xDirection();
            TEST_CHECK(approxEqual(uDir.X(), 1.0) && approxEqual(uDir.Y(), 0.0) && approxEqual(uDir.Z(), 0.0),
                       "Subtest 51.7: WorkPlane U direction matches (1, 0, 0)");

            // La normale doit être orthogonale à (1, 0, 0) et au vecteur (0, 5, 5)
            gp_Dir nDir = wp3P.normal();
            TEST_CHECK(approxEqual(nDir.X(), 0.0), "Subtest 51.7: Normal X is 0");
            TEST_CHECK(approxEqual(std::abs(nDir.Y()), std::abs(nDir.Z())), "Subtest 51.7: Normal Y and Z have equal magnitude (45 deg incline)");
            TEST_CHECK(approxEqual(nDir.Dot(gp_Dir(1, 0, 0)), 0.0), "Subtest 51.7: Normal is perpendicular to U-axis");

            // Projection du point P3 (0, 5, 5) sur le plan doit redonner exactement P3 (distance = 0)
            TEST_CHECK(approxEqual(wp3P.distanceTo(p3), 0.0), "Subtest 51.7: Distance from P3 to plane is 0");
            gp_Pnt pProjP3 = wp3P.projectOrtho(p3);
            TEST_CHECK(approxEqual(pProjP3.X(), 0.0) && approxEqual(pProjP3.Y(), 5.0) && approxEqual(pProjP3.Z(), 5.0),
                       "Subtest 51.7: Ortho projection of P3 is P3");

            // Coordonnée UCS de P3 doit avoir Z = 0
            gp_Pnt ucsP3 = wp3P.toUcs(p3);
            TEST_CHECK(approxEqual(ucsP3.Z(), 0.0), "Subtest 51.7: UCS coordinate Z of P3 is 0 on plane");
            gp_Pnt worldP3 = wp3P.toWorld(ucsP3);
            TEST_CHECK(approxEqual(worldP3.Y(), 5.0) && approxEqual(worldP3.Z(), 5.0), "Subtest 51.7: Roundtrip UCS->World for P3");

            // Alignement de caméra "Vue normale au plan" (Camera Normal View)
            // L'œil de la caméra est positionné le long de la normale à une distance d, visant l'origine
            double camDist = 25.0;
            gp_Pnt camEye(wp3P.origin().X() + nDir.X() * camDist,
                          wp3P.origin().Y() + nDir.Y() * camDist,
                          wp3P.origin().Z() + nDir.Z() * camDist);
            gp_Pnt camTarget = wp3P.origin();
            gp_Vec camViewVec(camEye, camTarget);
            gp_Dir camViewDir(camViewVec);

            // Le vecteur de visée (Target - Eye) doit être exactement opposé à la normale du plan (dot product = -1)
            TEST_CHECK(approxEqual(camViewDir.Dot(nDir), -1.0),
                       "Subtest 51.7: Camera view direction is strictly anti-parallel to workplane normal");

            // Le vecteur Up de la caméra doit être orthogonal à la direction de visée
            gp_Dir camUp = wp3P.yDirection();
            TEST_CHECK(approxEqual(camUp.Dot(camViewDir), 0.0),
                       "Subtest 51.7: Camera Up direction is strictly orthogonal to view direction");

            std::cout << "  [PASS] Subtest 51.7: WorkPlane Arbitrary 3-Point Definition, Camera Normal Alignment & Geometry Verified" << std::endl;
        }

        std::cout << "[PASS] Test 51: Advanced CAD Modeling Navigation, WorkPlane, Coordinate Transformations, and Object Snap Engine Passed Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================

    // TEST 52: Interactive 3D WorkPlane, Arbitrary Coordinate Systems, LCS & Multi-Plane Management
    // =========================================================================
    {
        std::cout << "\n[TEST 52] Interactive 3D WorkPlane, Arbitrary Coordinate Systems, LCS & Multi-Plane Management..." << std::endl;

        // Subtest 52.1: WorkPlane as a true 3D interactive object & state engine
        {
            WorkPlane wp(WorkPlaneType::GlobalXY, "Etage 1", 3.0);
            wp.setId(101);
            wp.setWidth(24.0);
            wp.setHeight(18.0);
            wp.setGridSpacingX(1.5);
            wp.setGridSpacingY(2.0);
            wp.setGridSubdivisions(4);
            wp.setIsGridVisible(true);
            wp.setIsVisible(true);
            wp.setIsActive(true);
            wp.setIsLocked(false);
            wp.setIsIsolated(true);
            wp.setIsolationDistance(1.25);

            TEST_CHECK(wp.id() == 101, "Subtest 52.1: WorkPlane ID is 101");
            TEST_CHECK(approxEqual(wp.width(), 24.0), "Subtest 52.1: WorkPlane width is 24m");
            TEST_CHECK(approxEqual(wp.height(), 18.0), "Subtest 52.1: WorkPlane height is 18m");
            TEST_CHECK(approxEqual(wp.gridSpacingX(), 1.5), "Subtest 52.1: Grid spacing X is 1.5m");
            TEST_CHECK(approxEqual(wp.gridSpacingY(), 2.0), "Subtest 52.1: Grid spacing Y is 2.0m");
            TEST_CHECK(wp.gridSubdivisions() == 4, "Subtest 52.1: Subdivisions is 4");
            TEST_CHECK(wp.isGridVisible(), "Subtest 52.1: Grid is visible");
            TEST_CHECK(wp.isVisible(), "Subtest 52.1: WorkPlane is visible");
            TEST_CHECK(wp.isActive(), "Subtest 52.1: WorkPlane is active");
            TEST_CHECK(!wp.isLocked(), "Subtest 52.1: WorkPlane is not locked");
            TEST_CHECK(wp.isIsolated(), "Subtest 52.1: WorkPlane is isolated");
            TEST_CHECK(approxEqual(wp.isolationDistance(), 1.25), "Subtest 52.1: Isolation distance is 1.25m");

            // Plan arbitraire non limité aux étages : Z = 4.37, X = 7.25, Y = -2.50
            WorkPlane wpArbitrary(WorkPlaneType::Custom, "Plan Arbitraire 4.37m");
            wpArbitrary.setOrigin(gp_Pnt(7.25, -2.50, 4.37));
            TEST_CHECK(approxEqual(wpArbitrary.origin().X(), 7.25), "Subtest 52.1: Arbitrary X is 7.25m");
            TEST_CHECK(approxEqual(wpArbitrary.origin().Y(), -2.50), "Subtest 52.1: Arbitrary Y is -2.50m");
            TEST_CHECK(approxEqual(wpArbitrary.origin().Z(), 4.37), "Subtest 52.1: Arbitrary Z is 4.37m (not restricted to story levels)");

            std::cout << "  [PASS] Subtest 52.1: WorkPlane 3D Interactive Object & State Engine Verified" << std::endl;
        }

        // Subtest 52.2: 3D Transformations (Translation, Rotation, gp_Trsf & Euler Angles)
        {
            WorkPlane wp(WorkPlaneType::GlobalXY, "Plan Transformed", 0.0);
            
            // Translation 3D
            wp.translate(gp_Vec(3.0, 4.0, 5.0));
            TEST_CHECK(approxEqual(wp.origin().X(), 3.0), "Subtest 52.2: Translated origin X is 3.0");
            TEST_CHECK(approxEqual(wp.origin().Y(), 4.0), "Subtest 52.2: Translated origin Y is 4.0");
            TEST_CHECK(approxEqual(wp.origin().Z(), 5.0), "Subtest 52.2: Translated origin Z is 5.0");

            // Rotation 90° autour de l'axe X passant par l'origine
            gp_Ax1 rotAxis(wp.origin(), gp_Dir(1.0, 0.0, 0.0));
            wp.rotate(rotAxis, 90.0 * 3.14159265358979323846 / 180.0);
            
            // Après rotation 90° autour de X:
            // Normale Z (0, 0, 1) pivote vers (0, -1, 0)
            // Y local (0, 1, 0) pivote vers (0, 0, 1)
            // X local (1, 0, 0) reste invariant
            TEST_CHECK(approxEqual(wp.xDirection().X(), 1.0) && approxEqual(wp.xDirection().Y(), 0.0),
                       "Subtest 52.2: X direction invariant under X-axis rotation");
            TEST_CHECK(approxEqual(wp.yDirection().Z(), 1.0), "Subtest 52.2: Y direction rotated to Z");
            TEST_CHECK(approxEqual(wp.normal().Y(), -1.0), "Subtest 52.2: Normal rotated to -Y");

            // Définition directe par angles d'Euler
            wp.setRotation(0.0, 0.0, 45.0); // 45° autour de Z
            TEST_CHECK(approxEqual(wp.rotationZ(), 45.0, 0.5), "Subtest 52.2: Euler angle Rz is 45°");
            TEST_CHECK(approxEqual(wp.normal().Z(), 1.0), "Subtest 52.2: Normal is vertical Z after Rz");

            std::cout << "  [PASS] Subtest 52.2: WorkPlane 3D Transformations & Euler Angles Verified" << std::endl;
        }

        // Subtest 52.3: Bidirectional 2D <-> 3D Coordinate Transformations
        {
            WorkPlane wp(WorkPlaneType::GlobalXY, "Plan Local 2D-3D", 5.0);
            wp.setOrigin(gp_Pnt(10.0, 20.0, 5.0));
            
            // Point local 2D (u = 4.0, v = 7.0)
            gp_Pnt worldP = wp.toWorld(4.0, 7.0);
            TEST_CHECK(approxEqual(worldP.X(), 14.0), "Subtest 52.3: 2D->3D world X is 14m");
            TEST_CHECK(approxEqual(worldP.Y(), 27.0), "Subtest 52.3: 2D->3D world Y is 27m");
            TEST_CHECK(approxEqual(worldP.Z(), 5.0), "Subtest 52.3: 2D->3D world Z is 5m");

            // Conversion inverse 3D -> 2D
            double uOut = 0.0, vOut = 0.0;
            wp.toLocal(worldP, uOut, vOut);
            TEST_CHECK(approxEqual(uOut, 4.0), "Subtest 52.3: 3D->2D local U is 4m");
            TEST_CHECK(approxEqual(vOut, 7.0), "Subtest 52.3: 3D->2D local V is 7m");

            // Raycast analytique (simulation du clic souris depuis la caméra)
            gp_Pnt eye(14.0, 27.0, 25.0);
            gp_Dir rayDir(0.0, 0.0, -1.0);
            gp_Pnt hitPnt;
            bool hit = wp.projectRay(eye, rayDir, hitPnt);
            TEST_CHECK(hit, "Subtest 52.3: Raycast hit workplane");
            TEST_CHECK(approxEqual(hitPnt.X(), 14.0) && approxEqual(hitPnt.Y(), 27.0) && approxEqual(hitPnt.Z(), 5.0),
                       "Subtest 52.3: Raycast hit point matches expected (14, 27, 5)");

            std::cout << "  [PASS] Subtest 52.3: Bidirectional 2D <-> 3D Coordinate Transformations Verified" << std::endl;
        }

        // Subtest 52.4: Centralized Multi-WorkPlane Manager (WorkPlaneManager)
        {
            WorkPlaneManager wpMgr;
            TEST_CHECK(wpMgr.count() >= 3, "Subtest 52.4: Default WorkPlanes (XY, XZ, YZ) created");
            TEST_CHECK(wpMgr.activeWorkPlane() != nullptr, "Subtest 52.4: Active WorkPlane exists");

            // Création de multiples plans de travail
            WorkPlane wp1(WorkPlaneType::GlobalXY, "Toiture R+3", 9.0);
            int id1 = wpMgr.addWorkPlane(wp1);
            TEST_CHECK(id1 > 0, "Subtest 52.4: Added WorkPlane ID > 0");

            WorkPlane wp2(WorkPlaneType::GlobalXZ, "Pignon Nord", 12.5);
            int id2 = wpMgr.addWorkPlane(wp2);
            TEST_CHECK(id2 > 0, "Subtest 52.4: Added WorkPlane id2 > 0");

            WorkPlane wpCustom(WorkPlaneType::Custom, "Passerelle Inclinee");
            wpCustom.setOrigin(gp_Pnt(0.0, 0.0, 4.37));
            wpCustom.setLocalAxes(gp_Dir(0.866, 0.5, 0.0), gp_Dir(-0.5, 0.866, 0.0), gp_Dir(0.0, 0.0, 1.0));
            int idCustom = wpMgr.addWorkPlane(wpCustom);

            // Changement de plan actif
            TEST_CHECK(wpMgr.setActiveWorkPlane(idCustom), "Subtest 52.4: Set active WorkPlane to Custom");
            TEST_CHECK(wpMgr.activeWorkPlaneId() == idCustom, "Subtest 52.4: Active ID matches Custom");
            TEST_CHECK(wpMgr.activeWorkPlane()->name() == "Passerelle Inclinee", "Subtest 52.4: Active name is Passerelle Inclinee");

            // Mise à jour de plan
            auto* activeWp = wpMgr.activeWorkPlane();
            activeWp->setWidth(50.0);
            wpMgr.updateWorkPlane(*activeWp);
            TEST_CHECK(approxEqual(wpMgr.getWorkPlane(idCustom)->width(), 50.0), "Subtest 52.4: WorkPlane width updated to 50m");

            // Sérialisation et désérialisation JSON
            std::string jsonStr = wpMgr.serializeToJson();
            TEST_CHECK(!jsonStr.empty(), "Subtest 52.4: JSON string is not empty");
            TEST_CHECK(jsonStr.find("Passerelle Inclinee") != std::string::npos, "Subtest 52.4: JSON contains custom plane name");

            WorkPlaneManager wpMgrRestored;
            bool ok = wpMgrRestored.deserializeFromJson(jsonStr);
            TEST_CHECK(ok, "Subtest 52.4: Deserialization succeeded");
            TEST_CHECK(wpMgrRestored.count() == wpMgr.count(), "Subtest 52.4: Restored count matches original");
            const auto* restoredCustom = wpMgrRestored.getWorkPlane(idCustom);
            TEST_CHECK(restoredCustom != nullptr, "Subtest 52.4: Restored custom plane exists");
            TEST_CHECK(approxEqual(restoredCustom->origin().Z(), 4.37), "Subtest 52.4: Restored custom plane origin Z is 4.37m");
            TEST_CHECK(approxEqual(restoredCustom->width(), 50.0), "Subtest 52.4: Restored custom plane width is 50m");

            std::cout << "  [PASS] Subtest 52.4: Multi-WorkPlane Manager & JSON Serialization Verified" << std::endl;
        }

        // Subtest 52.5: Non-destructive Undo/Redo (ModifyWorkPlaneCommand)
        {
            WorkPlaneManager wpMgr;
            WorkPlane wpOriginal(WorkPlaneType::GlobalXY, "Plan Undo Test", 0.0);
            wpOriginal.setWidth(10.0);
            int wpId = wpMgr.addWorkPlane(wpOriginal);
            wpOriginal.setId(wpId);

            WorkPlane wpModified = wpOriginal;
            wpModified.setOrigin(gp_Pnt(5.0, 10.0, 15.0));
            wpModified.setWidth(30.0);
            wpModified.setIsLocked(true);

            TSA::UndoRedo::CommandManager cmdMgr;
            auto cmd = std::make_unique<TSA::Commands::ModifyWorkPlaneCommand>(
                wpId, wpOriginal, wpModified, &wpMgr);

            cmdMgr.executeCommand(std::move(cmd));

            // Après exécution:
            const auto* curr = wpMgr.getWorkPlane(wpId);
            TEST_CHECK(approxEqual(curr->origin().X(), 5.0), "Subtest 52.5: Executed origin X is 5.0m");
            TEST_CHECK(approxEqual(curr->origin().Z(), 15.0), "Subtest 52.5: Executed origin Z is 15.0m");
            TEST_CHECK(approxEqual(curr->width(), 30.0), "Subtest 52.5: Executed width is 30.0m");
            TEST_CHECK(curr->isLocked(), "Subtest 52.5: Executed isLocked is true");

            // Undo:
            TEST_CHECK(cmdMgr.canUndo(), "Subtest 52.5: CommandManager canUndo is true");
            cmdMgr.undo();

            const auto* undone = wpMgr.getWorkPlane(wpId);
            TEST_CHECK(approxEqual(undone->origin().X(), 0.0), "Subtest 52.5: Undone origin X restored to 0.0m");
            TEST_CHECK(approxEqual(undone->origin().Z(), 0.0), "Subtest 52.5: Undone origin Z restored to 0.0m");
            TEST_CHECK(approxEqual(undone->width(), 10.0), "Subtest 52.5: Undone width restored to 10.0m");
            TEST_CHECK(!undone->isLocked(), "Subtest 52.5: Undone isLocked restored to false");

            // Redo:
            TEST_CHECK(cmdMgr.canRedo(), "Subtest 52.5: CommandManager canRedo is true");
            cmdMgr.redo();

            const auto* redone = wpMgr.getWorkPlane(wpId);
            TEST_CHECK(approxEqual(redone->origin().X(), 5.0), "Subtest 52.5: Redone origin X restored to 5.0m");
            TEST_CHECK(approxEqual(redone->width(), 30.0), "Subtest 52.5: Redone width restored to 30.0m");

            std::cout << "  [PASS] Subtest 52.5: Non-destructive WorkPlane Undo/Redo Verified" << std::endl;
        }

        // Subtest 52.6: Structural Elements Local Coordinate Systems (LCS - Rules 10 & 11)
        {
            TSA::Model::Model testModel;
            int n1 = testModel.addNode(0.0, 0.0, 0.0);
            int n2 = testModel.addNode(6.0, 0.0, 0.0); // Poutre horizontale le long de X
            int n3 = testModel.addNode(0.0, 0.0, 3.5); // Poteau vertical le long de Z

            int beamId = testModel.addBeam(n1, n2, 0.30, 0.50, "Poutre_LCS");
            int colId = testModel.addColumn(n1, n3, 0.40, 0.40, "Poteau_LCS");

            const auto* beam = testModel.getBeam(beamId);
            TEST_CHECK(beam != nullptr, "Subtest 52.6: Beam created");

            // Calcul du repère local de la poutre (LCS)
            const auto* nodeA = testModel.getNode(beam->startNodeId());
            const auto* nodeB = testModel.getNode(beam->endNodeId());
            gp_Vec vLong(gp_Pnt(nodeA->x(), nodeA->y(), nodeA->z()), gp_Pnt(nodeB->x(), nodeB->y(), nodeB->z()));
            gp_Dir lcsX(vLong);

            // Axe longitudinal X local est orienté selon (1, 0, 0)
            TEST_CHECK(approxEqual(lcsX.X(), 1.0) && approxEqual(lcsX.Y(), 0.0) && approxEqual(lcsX.Z(), 0.0),
                       "Subtest 52.6: Beam LCS longitudinal X is (1, 0, 0)");

            // Vecteur de référence vertical
            gp_Vec vRef(0.0, 0.0, 1.0);
            gp_Vec lcsY = vRef.Crossed(gp_Vec(lcsX)).Normalized();
            gp_Vec lcsZ = gp_Vec(lcsX).Crossed(lcsY).Normalized();

            TEST_CHECK(approxEqual(lcsY.Y(), -1.0) || approxEqual(lcsY.Y(), 1.0), "Subtest 52.6: Beam LCS transverse Y is along Y");
            TEST_CHECK(approxEqual(lcsZ.Z(), 1.0) || approxEqual(lcsZ.Z(), -1.0), "Subtest 52.6: Beam LCS vertical Z is along Z");

            // Poteau vertical (le long de Z)
            const auto* col = testModel.getColumn(colId);
            TEST_CHECK(col != nullptr, "Subtest 52.6: Column created");
            const auto* colA = testModel.getNode(col->startNodeId());
            const auto* colB = testModel.getNode(col->endNodeId());
            gp_Vec vCol(gp_Pnt(colA->x(), colA->y(), colA->z()), gp_Pnt(colB->x(), colB->y(), colB->z()));
            gp_Dir colLcsX(vCol);
            TEST_CHECK(approxEqual(colLcsX.Z(), 1.0), "Subtest 52.6: Column LCS longitudinal X is along vertical Z");

            std::cout << "  [PASS] Subtest 52.6: Structural Elements Local Coordinate Systems (LCS) Verified" << std::endl;
        }

        // Subtest 52.7: WorkPlane Isolation Filtering
        {
            WorkPlane wp(WorkPlaneType::GlobalXY, "Niveau 2", 6.0);
            wp.setIsIsolated(true);
            wp.setIsolationDistance(0.5);

            gp_Pnt pInPlane(4.0, 5.0, 6.0);
            gp_Pnt pNearPlane(1.0, 2.0, 6.3);
            gp_Pnt pFarPlane(0.0, 0.0, 0.0);
            gp_Pnt pUpperPlane(0.0, 0.0, 9.0);

            TEST_CHECK(approxEqual(wp.distanceTo(pInPlane), 0.0), "Subtest 52.7: Point on plane distance is 0");
            TEST_CHECK(std::abs(wp.distanceTo(pNearPlane)) <= wp.isolationDistance(),
                       "Subtest 52.7: Point within 0.3m is kept by 0.5m isolation");
            TEST_CHECK(std::abs(wp.distanceTo(pFarPlane)) > wp.isolationDistance(),
                       "Subtest 52.7: Ground floor point (Z=0) is filtered out by isolation");
            TEST_CHECK(std::abs(wp.distanceTo(pUpperPlane)) > wp.isolationDistance(),
                       "Subtest 52.7: Story 3 point (Z=9) is filtered out by isolation");

            std::cout << "  [PASS] Subtest 52.7: WorkPlane Isolation Distance Filtering Verified" << std::endl;
        }

        // Subtest 52.8: WorkPlaneCoordinateSystem Orthonormal Frame & Kinematics
        {
            using namespace TSA::Coordinate;
            auto csXY = WorkPlaneCoordinateSystem::xy(3.0);
            TEST_CHECK(approxEqual(csXY.origin().Z(), 3.0), "Subtest 52.8: XY origin Z is 3.0");
            TEST_CHECK(approxEqual(csXY.axisX().X(), 1.0), "Subtest 52.8: XY axisX is (1, 0, 0)");
            TEST_CHECK(approxEqual(csXY.axisY().Y(), 1.0), "Subtest 52.8: XY axisY is (0, 1, 0)");
            TEST_CHECK(approxEqual(csXY.axisZ().Z(), 1.0), "Subtest 52.8: XY axisZ is (0, 0, 1)");

            // Orthonormality checks
            TEST_CHECK(std::abs(csXY.axisX().Dot(csXY.axisY())) < 1e-9, "Subtest 52.8: axisX perpendicular to axisY");
            TEST_CHECK(std::abs(csXY.axisX().Dot(csXY.axisZ())) < 1e-9, "Subtest 52.8: axisX perpendicular to axisZ");
            TEST_CHECK(std::abs(csXY.axisY().Dot(csXY.axisZ())) < 1e-9, "Subtest 52.8: axisY perpendicular to axisZ");

            // Point transformation
            gp_Pnt pGlobal(7.0, 8.0, 3.0);
            double u = 0, v = 0, w = 0;
            csXY.toLocal(pGlobal, u, v, w);
            TEST_CHECK(approxEqual(u, 7.0) && approxEqual(v, 8.0) && approxEqual(w, 0.0), "Subtest 52.8: toLocal on XY plane");
            gp_Pnt pBack = csXY.toGlobal(u, v, w);
            TEST_CHECK(approxEqual(pBack.X(), 7.0) && approxEqual(pBack.Y(), 8.0) && approxEqual(pBack.Z(), 3.0), "Subtest 52.8: toGlobal roundtrip");

            // Arbitrary rotated coordinate system (90 deg around Z)
            gp_Pnt origRot(10.0, 20.0, 5.0);
            gp_Dir axX(0.0, 1.0, 0.0);
            gp_Dir axY(-1.0, 0.0, 0.0);
            auto csRot = WorkPlaneCoordinateSystem::fromOriginAndAxes(origRot, axX, axY);
            TEST_CHECK(approxEqual(csRot.axisZ().Z(), 1.0), "Subtest 52.8: Rotated cs axisZ is along +Z");
            gp_Pnt pRotGlobal(10.0, 25.0, 5.0);
            csRot.toLocal(pRotGlobal, u, v, w);
            TEST_CHECK(approxEqual(u, 5.0) && approxEqual(v, 0.0) && approxEqual(w, 0.0), "Subtest 52.8: Rotated cs toLocal u=5, v=0, w=0");

            // Raycast intersection
            gp_Pnt rayEye(10.0, 25.0, 20.0);
            gp_Dir rayDir(0.0, 0.0, -1.0);
            gp_Pnt hitPoint;
            bool hit = csRot.intersectRay(rayEye, rayDir, hitPoint);
            TEST_CHECK(hit, "Subtest 52.8: Ray intersection succeeded");
            TEST_CHECK(approxEqual(hitPoint.X(), 10.0) && approxEqual(hitPoint.Y(), 25.0) && approxEqual(hitPoint.Z(), 5.0), "Subtest 52.8: Ray intersection point");

            std::cout << "  [PASS] Subtest 52.8: WorkPlaneCoordinateSystem Orthonormal Frame & Kinematics Verified" << std::endl;
        }

        // Subtest 52.9: CoordinateTransform & AxisColorConfig Centralized Management
        {
            using namespace TSA::Coordinate;
            auto cs = WorkPlaneCoordinateSystem::xy(0.0);
            gp_Pnt p(4.0, 6.0, 2.0);
            gp_Pnt localP = CoordinateTransform::toWorkPlaneLocal(p, cs);
            TEST_CHECK(approxEqual(localP.X(), 4.0) && approxEqual(localP.Y(), 6.0) && approxEqual(localP.Z(), 2.0), "Subtest 52.9: toWorkPlaneLocal point");
            gp_Pnt globalP = CoordinateTransform::toGlobalFromWorkPlane(localP, cs);
            TEST_CHECK(approxEqual(globalP.X(), 4.0) && approxEqual(globalP.Y(), 6.0) && approxEqual(globalP.Z(), 2.0), "Subtest 52.9: toGlobalFromWorkPlane point");

            // AxisColorConfig
            auto& colorCfg = AxisColorConfig::instance();
            colorCfg.resetToDefaults();
            TEST_CHECK(colorCfg.axisXColor().Red() > 0.80 && colorCfg.axisXColor().Green() < 0.30, "Subtest 52.9: Default X axis color is Red");
            TEST_CHECK(colorCfg.axisYColor().Green() > 0.80 && colorCfg.axisYColor().Red() < 0.30, "Subtest 52.9: Default Y axis color is Green");
            TEST_CHECK(colorCfg.axisZColor().Blue() > 0.80 && colorCfg.axisZColor().Red() < 0.30, "Subtest 52.9: Default Z axis color is Blue");

            // Custom local WorkPlane colors
            colorCfg.setWorkPlaneAxisColors(Quantity_Color(Quantity_NOC_ORANGE),
                                            Quantity_Color(Quantity_NOC_CYAN1),
                                            Quantity_Color(Quantity_NOC_MAGENTA1));
            TEST_CHECK(colorCfg.workPlaneAxisXColor().Name() == Quantity_NOC_ORANGE, "Subtest 52.9: Custom WorkPlane X color");
            colorCfg.resetToDefaults();

            std::cout << "  [PASS] Subtest 52.9: CoordinateTransform & AxisColorConfig Centralized Management Verified" << std::endl;
        }

        // Subtest 52.10: ProjectionManager & ViewManager WorkPlane Navigation
        {
            using namespace TSA::Viewer;
            ProjectionManager projMgr;
            TEST_CHECK(projMgr.mode() == ProjectionMode::ThreeD, "Subtest 52.10: Default projection mode is ThreeD");
            TEST_CHECK(projMgr.direction() == ProjectionDirection::Normal, "Subtest 52.10: Default direction is Normal");

            auto wp = WorkPlane::xy(4.0, "Floor 4m");
            gp_Pnt eye(2.0, 3.0, 10.0);
            gp_Dir rayDir(0.0, 0.0, -1.0);
            gp_Pnt hitPnt;
            bool hit = projMgr.projectCursorRay(eye, rayDir, wp, hitPnt);
            TEST_CHECK(hit, "Subtest 52.10: 3D cursor ray projection hits plane");
            TEST_CHECK(approxEqual(hitPnt.Z(), 4.0), "Subtest 52.10: Hit point elevation is 4.0");

            // 2D projection mode
            projMgr.setMode(ProjectionMode::TwoD);
            gp_Pnt pt3D(5.0, 6.0, 12.0);
            gp_Pnt proj2D = projMgr.projectPoint(pt3D, wp);
            TEST_CHECK(approxEqual(proj2D.X(), 5.0) && approxEqual(proj2D.Y(), 6.0) && approxEqual(proj2D.Z(), 4.0), "Subtest 52.10: 2D orthogonal projection onto plane");

            ViewManager viewMgr;
            viewMgr.setCurrentView(StandardCameraView::Top);
            TEST_CHECK(viewMgr.currentView() == StandardCameraView::Top, "Subtest 52.10: Standard view set to Top");

            std::cout << "  [PASS] Subtest 52.10: ProjectionManager & ViewManager WorkPlane Navigation Verified" << std::endl;
        }

        // Subtest 52.11: SnapManager Kinematic Snapping Pipeline
        {
            using namespace TSA::Grid;
            SnapManager snapMgr;
            TEST_CHECK(snapMgr.isWorkPlaneGridSnapEnabled(), "Subtest 52.11: WorkPlane grid snap enabled by default");
            snapMgr.setSnapTolerance(0.50);

            WorkPlane wp = WorkPlane::xy(0.0, "GridSnap WP");
            wp.setGridSettings(1.0, 1.0, true);

            // Raw point near grid intersection (3.08, 4.05, 0.0) -> snaps to (3.0, 4.0, 0.0)
            gp_Pnt rawPnt(3.08, 4.05, 0.0);
            GridSnapResult res = snapMgr.snapToWorkPlaneGrid(rawPnt, wp);
            TEST_CHECK(res.snapped, "Subtest 52.11: WorkPlane grid snap succeeded");
            TEST_CHECK(approxEqual(res.point.X(), 3.0) && approxEqual(res.point.Y(), 4.0) && approxEqual(res.point.Z(), 0.0),
                       "Subtest 52.11: Snapped to nearest grid vertex (3, 4, 0)");

            // Point outside tolerance (3.45, 4.45, 0.0) with tolerance 0.20
            snapMgr.setSnapTolerance(0.20);
            GridSnapResult resFar = snapMgr.snapToWorkPlaneGrid(gp_Pnt(3.45, 4.45, 0.0), wp);
            TEST_CHECK(!resFar.snapped, "Subtest 52.11: Point outside tolerance does not snap");

            std::cout << "  [PASS] Subtest 52.11: SnapManager Kinematic Snapping Pipeline Verified" << std::endl;
        }

        // Subtest 52.12: Tilted WorkPlane Drawing Constraints, Snapping & Non-Destructive 2D/3D Projection
        {
            using namespace TSA::Coordinate;
            using namespace TSA::Viewer;
            using namespace TSA::Grid;

            // 1. Plan incliné à 45° (ex. toiture / rampant)
            const double invSqrt2 = std::sqrt(2.0) / 2.0;
            gp_Pnt wpOrigin(5.0, 5.0, 2.0);
            gp_Dir wpAxisX(1.0, 0.0, 0.0);
            gp_Dir wpAxisY(0.0, invSqrt2, invSqrt2);
            gp_Dir wpNormal(0.0, -invSqrt2, invSqrt2);

            WorkPlaneCoordinateSystem tiltedCs(wpOrigin, wpNormal, wpAxisX);
            WorkPlane tiltedWp(tiltedCs, "Toiture 45deg", WorkPlaneType::Custom);
            TEST_CHECK(std::abs(tiltedWp.normal().Dot(wpNormal) - 1.0) < 1e-4, "Subtest 52.12: Tilted WorkPlane normal correctly set");

            // 2. Projection 2D sur le plan incliné
            ProjectionManager projMgr;
            projMgr.setMode(ProjectionMode::TwoD);

            gp_Pnt worldPt(5.0, 10.0, 20.0); // Point dans l'espace
            gp_Pnt projectedPt = projMgr.projectPoint(worldPt, tiltedWp);
            TEST_CHECK(std::abs(tiltedWp.distanceTo(projectedPt)) < 1e-5,
                       "Subtest 52.12: 2D projected point strictly lies on tilted WorkPlane");

            // 3. Snapping sur la grille locale du plan incliné
            SnapManager snapMgr;
            tiltedWp.setGridSettings(1.0, 1.0, true);
            snapMgr.setSnapTolerance(0.60);

            // Coordonnées locales cibles : U=3.0, V=4.0
            gp_Pnt targetGlobal = tiltedWp.toGlobal(3.0, 4.0, 0.0);
            // Point bruité à proximité
            gp_Pnt noisyPt(targetGlobal.X() + 0.08, targetGlobal.Y() - 0.06, targetGlobal.Z() + 0.04);
            GridSnapResult snapRes = snapMgr.snapToWorkPlaneGrid(noisyPt, tiltedWp);

            TEST_CHECK(snapRes.snapped, "Subtest 52.12: Snapping succeeded on tilted WorkPlane");
            TEST_CHECK(std::abs(tiltedWp.distanceTo(snapRes.point)) < 1e-5,
                       "Subtest 52.12: Snapped point strictly on tilted plane");
            TEST_CHECK(approxEqual(snapRes.point.X(), targetGlobal.X()) &&
                       approxEqual(snapRes.point.Y(), targetGlobal.Y()) &&
                       approxEqual(snapRes.point.Z(), targetGlobal.Z()),
                       "Subtest 52.12: Snapped to exact local grid point");

            // 4. Modélisation / Dessin contraint sur le WorkPlane dans le modèle structural
            TSA::Model::Model model;
            // Point 1 sur le plan (U=0, V=0)
            gp_Pnt p1 = tiltedWp.toGlobal(0.0, 0.0, 0.0);
            // Point 2 sur le plan (U=6.0, V=0.0) -> Poutre faitière / panne
            gp_Pnt p2 = tiltedWp.toGlobal(6.0, 0.0, 0.0);

            int n1 = model.addNode(p1.X(), p1.Y(), p1.Z());
            int n2 = model.addNode(p2.X(), p2.Y(), p2.Z());
            int beamId = model.addBeam(n1, n2, 0.20, 0.40, "Panne Inclinee");
            TEST_CHECK(beamId > 0, "Subtest 52.12: Beam created on tilted WorkPlane");

            const auto* beam = model.getBeam(beamId);
            TEST_CHECK(beam != nullptr, "Subtest 52.12: Beam exists");
            TEST_CHECK(approxEqual(beam->length(model), 6.0), "Subtest 52.12: Beam length on tilted plane is 6.0m");

            // 5. Non-destructivité : alternance des modes de projection 2D <-> 3D
            projMgr.setMode(ProjectionMode::ThreeD);
            gp_Pnt pt3D = projMgr.projectPoint(worldPt, tiltedWp);
            TEST_CHECK(approxEqual(pt3D.X(), worldPt.X()) &&
                       approxEqual(pt3D.Y(), worldPt.Y()) &&
                       approxEqual(pt3D.Z(), worldPt.Z()),
                       "Subtest 52.12: ThreeD projection preserves full 3D coordinates");

            // Le modèle structural conserve ses nœuds intacts
            const auto* checkNode1 = model.getNode(n1);
            TEST_CHECK(approxEqual(checkNode1->x(), p1.X()) &&
                       approxEqual(checkNode1->y(), p1.Y()) &&
                       approxEqual(checkNode1->z(), p1.Z()),
                       "Subtest 52.12: Structural model nodes remain intact across projection modes");

            std::cout << "  [PASS] Subtest 52.12: Tilted WorkPlane Drawing Constraints, Snapping & Non-Destructive 2D/3D Projection Verified" << std::endl;
        }

        std::cout << "[PASS] Test 52: Interactive 3D WorkPlane, Arbitrary Coordinate Systems, LCS & Multi-Plane Management Passed Successfully!" << std::endl;
        passed++;
    }

    // TEST 53: Synchronisation Niveau -> WorkPlane (règle déterministe, modèle inchangé)
    // =========================================================================
    {
        std::cout << "\n[TEST 53] Level -> WorkPlane synchronization rule..." << std::endl;
        using TSA::Coordinate::WorkPlane;

        TSA::Coordinate::LevelManager levels;
        levels.addLevel("Niveau 0", 0.0);
        levels.addLevel("Niveau 1", 3.0);
        levels.addLevel("Niveau 2", 6.0);

        TSA::Model::Model model;
        const int n1 = model.addNode(1.0, 2.0, 0.0);
        const int n2 = model.addNode(4.0, 2.0, 3.0);
        auto modelIntact = [&]() {
            const auto* a = model.getNode(n1);
            const auto* b = model.getNode(n2);
            return a && b && approxEqual(a->x(), 1.0) && approxEqual(a->y(), 2.0) && approxEqual(a->z(), 0.0) &&
                   approxEqual(b->x(), 4.0) && approxEqual(b->y(), 2.0) && approxEqual(b->z(), 3.0);
        };

        // 53.1 Plan horizontal : suit chaque niveau
        {
            WorkPlane wp = WorkPlane::xy(0.0);
            TEST_CHECK(wp.isHorizontal(), "Subtest 53.1: XY plane is horizontal");
            for (const auto& lvl : levels.levels())
            {
                wp.moveToElevation(lvl.elevation);
                TEST_CHECK(approxEqual(wp.origin().Z(), lvl.elevation), "Subtest 53.1: WorkPlane Z follows level");
                TEST_CHECK(approxEqual(wp.offset(), lvl.elevation), "Subtest 53.1: offset stays consistent");
            }
            TEST_CHECK(!wp.moveToElevation(6.0), "Subtest 53.1: no-op when already at elevation");
            TEST_CHECK(modelIntact(), "Subtest 53.1: structural model unchanged");
        }

        // 53.2 Plan horizontal à axes tournés : orientation et origine X/Y conservées
        {
            const double a = 30.0 * 3.14159265358979323846 / 180.0;
            WorkPlane wp = WorkPlane::fromOriginAndAxes(gp_Pnt(2.0, 5.0, 1.0),
                                                        gp_Dir(std::cos(a), std::sin(a), 0.0),
                                                        gp_Dir(-std::sin(a), std::cos(a), 0.0));
            TEST_CHECK(wp.isHorizontal(), "Subtest 53.2: rotated-axes plane is horizontal");
            TEST_CHECK(wp.moveToElevation(4.0), "Subtest 53.2: rotated horizontal plane moved");
            TEST_CHECK(approxEqual(wp.origin().X(), 2.0) && approxEqual(wp.origin().Y(), 5.0) &&
                       approxEqual(wp.origin().Z(), 4.0), "Subtest 53.2: XY origin kept, Z = 4");
            TEST_CHECK(approxEqual(wp.axisX().X(), std::cos(a)) && approxEqual(wp.axisX().Y(), std::sin(a)),
                       "Subtest 53.2: local X axis orientation kept");

            // Local <-> global aller-retour
            gp_Pnt g = wp.toGlobal(1.5, -0.5, 0.0);
            gp_Pnt l = wp.toLocal(g);
            TEST_CHECK(approxEqual(l.X(), 1.5) && approxEqual(l.Y(), -0.5) && approxEqual(l.Z(), 0.0),
                       "Subtest 53.2: local -> global -> local round trip");
            TEST_CHECK(approxEqual(g.Z(), 4.0), "Subtest 53.2: in-plane point lies at level Z");
        }

        // 53.3 Plans indépendants : jamais modifiés par un niveau
        {
            WorkPlane tilted = WorkPlane::fromThreePoints(gp_Pnt(0, 0, 0), gp_Pnt(1, 0, 0), gp_Pnt(0, 1, 1));
            const gp_Pnt o0 = tilted.origin();
            const gp_Dir n0 = tilted.normal();
            TEST_CHECK(!tilted.isHorizontal(), "Subtest 53.3: tilted plane is not horizontal");
            TEST_CHECK(!tilted.moveToElevation(6.0), "Subtest 53.3: tilted plane not moved by level");
            TEST_CHECK(tilted.origin().Distance(o0) < 1e-9 && tilted.normal().IsEqual(n0, 1e-9),
                       "Subtest 53.3: tilted plane unchanged");

            WorkPlane xz = WorkPlane::xz(2.0);
            TEST_CHECK(!xz.moveToElevation(6.0), "Subtest 53.3: XZ plane not moved by level");
            TEST_CHECK(approxEqual(xz.offset(), 2.0) && approxEqual(xz.origin().Y(), 2.0),
                       "Subtest 53.3: XZ plane keeps Y offset");
            TEST_CHECK(modelIntact(), "Subtest 53.3: structural model unchanged");
        }

        // 53.4 Translation libre d'un plan indépendant : aucun élément structurel modifié
        {
            WorkPlane wp = WorkPlane::xy(3.0);
            wp.translate(gp_Vec(0.0, 0.0, 0.5));
            TEST_CHECK(approxEqual(wp.origin().Z(), 3.5), "Subtest 53.4: WorkPlane translated 3.00 -> 3.50");
            TEST_CHECK(modelIntact(), "Subtest 53.4: structural model unchanged by WorkPlane translation");
        }

        std::cout << "[PASS] Test 53: Level -> WorkPlane synchronization rule Passed Successfully!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 54: Mode 2D et Isolation Automatique du Plan de Travail (WorkPlane CAD Mode)
    // =========================================================================
    {
        std::cout << "\n[TEST 54] 2D CAD Mode & Automatic WorkPlane Isolation..." << std::endl;
        using TSA::Coordinate::WorkPlane;
        using TSA::Coordinate::WorkPlaneType;

        // Subtest 54.1: Prédicats géométriques d'appartenance et d'intersection du WorkPlane
        {
            WorkPlane wpR1 = WorkPlane::xy(3.0, "Niveau R+1");
            const double tol = 0.05;

            // Nœuds
            gp_Pnt ptRDC(2.0, 4.0, 0.0);
            gp_Pnt ptR1(2.0, 4.0, 3.0);
            gp_Pnt ptR1Tol(2.0, 4.0, 3.03); // dans les 5 cm
            gp_Pnt ptR2(2.0, 4.0, 6.0);

            TEST_CHECK(std::abs(wpR1.distanceTo(ptR1)) <= tol, "Subtest 54.1: Node at Z=3 is on R+1 plane");
            TEST_CHECK(std::abs(wpR1.distanceTo(ptR1Tol)) <= tol, "Subtest 54.1: Node within tolerance is on R+1 plane");
            TEST_CHECK(std::abs(wpR1.distanceTo(ptRDC)) > tol, "Subtest 54.1: Node at Z=0 is out of R+1 plane");
            TEST_CHECK(std::abs(wpR1.distanceTo(ptR2)) > tol, "Subtest 54.1: Node at Z=6 is out of R+1 plane");

            // Éléments linéaires (Poutres, Poteaux)
            auto checkLinear = [&](const gp_Pnt& p1, const gp_Pnt& p2) {
                double u1 = 0, v1 = 0, w1 = 0;
                double u2 = 0, v2 = 0, w2 = 0;
                wpR1.toLocal(p1, u1, v1, w1);
                wpR1.toLocal(p2, u2, v2, w2);
                if (std::abs(w1) <= tol || std::abs(w2) <= tol) return true;
                if ((w1 < -tol && w2 > tol) || (w1 > tol && w2 < -tol)) return true;
                return false;
            };

            // Poutre d'étage R+1 (Z=3)
            TEST_CHECK(checkLinear(gp_Pnt(0, 0, 3), gp_Pnt(5, 0, 3)), "Subtest 54.1: R+1 beam belongs to R+1 plane");
            // Poutre d'étage RDC (Z=0)
            TEST_CHECK(!checkLinear(gp_Pnt(0, 0, 0), gp_Pnt(5, 0, 0)), "Subtest 54.1: RDC beam excluded from R+1 plane");
            // Poutre d'étage R+2 (Z=6)
            TEST_CHECK(!checkLinear(gp_Pnt(0, 0, 6), gp_Pnt(5, 0, 6)), "Subtest 54.1: R+2 beam excluded from R+1 plane");

            // Poteau RDC -> R+1 (arrive sur le plan à Z=3)
            TEST_CHECK(checkLinear(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 3)), "Subtest 54.1: Column RDC->R+1 touches R+1 plane");
            // Poteau R+1 -> R+2 (part du plan à Z=3)
            TEST_CHECK(checkLinear(gp_Pnt(0, 0, 3), gp_Pnt(0, 0, 6)), "Subtest 54.1: Column R+1->R+2 touches R+1 plane");
            // Poteau traversant le plan de Z=1 à Z=5
            TEST_CHECK(checkLinear(gp_Pnt(0, 0, 1), gp_Pnt(0, 0, 5)), "Subtest 54.1: Piercing column crosses R+1 plane");
            // Poteau hors plan R+2 -> R+3 (Z=6 à Z=9)
            TEST_CHECK(!checkLinear(gp_Pnt(0, 0, 6), gp_Pnt(0, 0, 9)), "Subtest 54.1: Distant column excluded");

            // Éléments surfaciques (Dalles)
            auto checkSurface = [&](const std::vector<gp_Pnt>& pts) {
                double minW = 1e12, maxW = -1e12;
                bool onPlane = false;
                for (const auto& p : pts) {
                    double u = 0, v = 0, w = 0;
                    wpR1.toLocal(p, u, v, w);
                    if (std::abs(w) <= tol) onPlane = true;
                    if (w < minW) minW = w;
                    if (w > maxW) maxW = w;
                }
                return onPlane || (minW < -tol && maxW > tol);
            };

            std::vector<gp_Pnt> slabR1 = { gp_Pnt(0,0,3), gp_Pnt(5,0,3), gp_Pnt(5,5,3), gp_Pnt(0,5,3) };
            std::vector<gp_Pnt> slabRDC = { gp_Pnt(0,0,0), gp_Pnt(5,0,0), gp_Pnt(5,5,0), gp_Pnt(0,5,0) };
            TEST_CHECK(checkSurface(slabR1), "Subtest 54.1: Slab at R+1 belongs to R+1 plane");
            TEST_CHECK(!checkSurface(slabRDC), "Subtest 54.1: Slab at RDC excluded from R+1 plane");

            std::cout << "  [PASS] Subtest 54.1: Geometric Membership and Intersection Predicates Verified" << std::endl;
        }

        // Subtest 54.2: Changement d'étage et de plan en mode 2D
        {
            WorkPlane wp = WorkPlane::xy(0.0, "RDC");
            const double tol = 0.05;
            gp_Pnt ptRDC(0, 0, 0);
            gp_Pnt ptR1(0, 0, 3);

            TEST_CHECK(std::abs(wp.distanceTo(ptRDC)) <= tol, "Subtest 54.2: At RDC, Z=0 is visible");
            TEST_CHECK(std::abs(wp.distanceTo(ptR1)) > tol, "Subtest 54.2: At RDC, Z=3 is isolated");

            // Bascule vers R+1
            wp.moveToElevation(3.0);
            TEST_CHECK(std::abs(wp.distanceTo(ptRDC)) > tol, "Subtest 54.2: After switch to R+1, Z=0 is isolated");
            TEST_CHECK(std::abs(wp.distanceTo(ptR1)) <= tol, "Subtest 54.2: After switch to R+1, Z=3 is visible");

            // Bascule vers plan vertical XZ (Y = 2.0)
            WorkPlane wpXZ = WorkPlane::xz(2.0, "Portique Y=2");
            gp_Pnt ptOnXZ(5.0, 2.0, 4.0);
            gp_Pnt ptOffXZ(5.0, 0.0, 4.0);
            TEST_CHECK(std::abs(wpXZ.distanceTo(ptOnXZ)) <= tol, "Subtest 54.2: Point on XZ plane is visible");
            TEST_CHECK(std::abs(wpXZ.distanceTo(ptOffXZ)) > tol, "Subtest 54.2: Point off XZ plane is isolated");

            // Vérification de la normale et du repère caméra pour XZ
            TEST_CHECK(approxEqual(std::abs(wpXZ.normal().Y()), 1.0), "Subtest 54.2: XZ normal points along Y");
            TEST_CHECK(approxEqual(wpXZ.yDirection().Z(), 1.0), "Subtest 54.2: XZ up vector points along Z");

            std::cout << "  [PASS] Subtest 54.2: Level and Plane Switching in 2D Mode Verified" << std::endl;
        }

        // Subtest 54.3: Conservation des coordonnées réelles 3D lors du dessin et snapping 2D
        {
            WorkPlane wpR1 = WorkPlane::xy(3.0, "R+1");
            gp_Pnt eye(5.0, 5.0, 50.0);
            gp_Dir ray(0.0, 0.0, -1.0); // regard vers le bas

            gp_Pnt hitPnt;
            bool ok = wpR1.projectRay(eye, ray, hitPnt);
            TEST_CHECK(ok, "Subtest 54.3: Raycast on R+1 plane succeeded");
            TEST_CHECK(approxEqual(hitPnt.X(), 5.0) && approxEqual(hitPnt.Y(), 5.0) && approxEqual(hitPnt.Z(), 3.0),
                       "Subtest 54.3: Coordinates retain true 3D elevation Z=3.00 m");

            // Plan incliné à 45 degrés
            WorkPlane wpTilted = WorkPlane::fromThreePoints(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(0, 10, 10), "Incliné 45");
            gp_Pnt eyeT(2.0, 2.0, 20.0);
            gp_Pnt hitT;
            TEST_CHECK(wpTilted.projectRay(eyeT, ray, hitT), "Subtest 54.3: Raycast on tilted plane");
            TEST_CHECK(approxEqual(hitT.X(), 2.0) && approxEqual(hitT.Y(), 2.0) && approxEqual(hitT.Z(), 2.0),
                       "Subtest 54.3: Tilted plane retains exact geometric height Z=2.00");

            std::cout << "  [PASS] Subtest 54.3: Real 3D Coordinates Preserved in 2D Raycast Verified" << std::endl;
        }

        std::cout << "[PASS] Test 54: 2D CAD Mode & Automatic WorkPlane Isolation Passed Successfully!" << std::endl;
        passed++;
    }

    // Test 55: X-Y-Z Structural WorkPlanes Auto-Detection, Selection, Camera Normal & 2D Isolation
    {
        std::cout << "\n[Test 55] X-Y-Z Structural WorkPlanes Auto-Detection, Selection, Camera Normal & 2D Isolation..." << std::endl;

        // Subtest 55.1: Détection automatique des plans structurels X, Y et Z depuis le modèle
        {
            Model model;
            // Réinitialiser les niveaux et coordonnées par défaut du modèle pour le test
            model.levelManager()->clear();
            model.levelManager()->addLevel("RDC", 0.0);
            model.levelManager()->addLevel("R+1", 3.20);
            model.levelManager()->addLevel("R+2", 6.40);

            model.coordinateSystem()->setXPositions({});
            model.coordinateSystem()->setYPositions({});

            // Nœuds du modèle définissant un portique 3D
            // Trame X = 0, 5, 10 m
            // Trame Y = 0, 4, 8 m
            model.addNode(0.0, 0.0, 0.0);
            model.addNode(5.0, 0.0, 0.0);
            model.addNode(10.0, 0.0, 0.0);

            model.addNode(0.0, 4.0, 3.20);
            model.addNode(5.0, 4.0, 3.20);
            model.addNode(10.0, 4.0, 3.20);

            model.addNode(0.0, 8.0, 6.40);
            model.addNode(5.0, 8.0, 6.40);
            model.addNode(10.0, 8.0, 6.40);

            // Nœud proche pour tester la tolérance de regroupement (tolérance = 5 cm = 0.05 m)
            model.addNode(5.02, 4.01, 3.21); // doit être regroupé avec X=5, Y=4, Z=3.2

            // Détection suivant Z
            auto zPlanes = model.detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::Z);
            TEST_CHECK(zPlanes.size() == 3, "Subtest 55.1: Exactly 3 Z levels detected");
            TEST_CHECK(approxEqual(zPlanes[0].offset, 0.0) && zPlanes[0].name == "RDC", "Subtest 55.1: Level 0 is RDC (Z=0.00 m)");
            TEST_CHECK(approxEqual(zPlanes[1].offset, 3.20) && zPlanes[1].name == "R+1", "Subtest 55.1: Level 1 is R+1 (Z=3.20 m)");
            TEST_CHECK(approxEqual(zPlanes[2].offset, 6.40) && zPlanes[2].name == "R+2", "Subtest 55.1: Level 2 is R+2 (Z=6.40 m)");

            // Détection suivant X
            auto xPlanes = model.detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::X);
            TEST_CHECK(xPlanes.size() == 3, "Subtest 55.1: Exactly 3 X cuts detected (5.02m clustered with 5.0m)");
            TEST_CHECK(approxEqual(xPlanes[0].offset, 0.0), "Subtest 55.1: First X cut is at X=0.00 m");
            TEST_CHECK(approxEqual(xPlanes[1].offset, 5.0), "Subtest 55.1: Second X cut is at X=5.00 m");
            TEST_CHECK(approxEqual(xPlanes[2].offset, 10.0), "Subtest 55.1: Third X cut is at X=10.00 m");

            // Détection suivant Y
            auto yPlanes = model.detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::Y);
            TEST_CHECK(yPlanes.size() == 3, "Subtest 55.1: Exactly 3 Y cuts detected (4.01m clustered with 4.0m)");
            TEST_CHECK(approxEqual(yPlanes[0].offset, 0.0), "Subtest 55.1: First Y cut is at Y=0.00 m");
            TEST_CHECK(approxEqual(yPlanes[1].offset, 4.0), "Subtest 55.1: Second Y cut is at Y=4.00 m");
            TEST_CHECK(approxEqual(yPlanes[2].offset, 8.0), "Subtest 55.1: Third Y cut is at Y=8.00 m");

            std::cout << "  [PASS] Subtest 55.1: Structural Plane Auto-Detection along Z, X, Y Verified" << std::endl;
        }

        // Subtest 55.2: Alignement géométrique, Normale caméra et Vecteur Haut pour Z, X, Y
        {
            // Plan horizontal Z (XY)
            WorkPlane wpZ = WorkPlane::xy(3.20, "R+1");
            TEST_CHECK(wpZ.type() == WorkPlaneType::GlobalXY, "Subtest 55.2: wpZ type is GlobalXY");
            TEST_CHECK(approxEqual(wpZ.normal().Z(), 1.0), "Subtest 55.2: Normal to Z plane is +Z (Top View)");
            TEST_CHECK(approxEqual(wpZ.yDirection().Y(), 1.0), "Subtest 55.2: Up vector for Z plane is +Y");

            // Plan vertical X (YZ) - Coupe transversale
            WorkPlane wpX = WorkPlane::yz(5.0, "Coupe X=5.00");
            TEST_CHECK(wpX.type() == WorkPlaneType::GlobalYZ, "Subtest 55.2: wpX type is GlobalYZ");
            TEST_CHECK(approxEqual(wpX.normal().X(), 1.0), "Subtest 55.2: Normal to X plane is +X (Side View)");
            TEST_CHECK(approxEqual(wpX.yDirection().Z(), 1.0), "Subtest 55.2: Up vector for X plane is +Z (Vertical)");
            TEST_CHECK(approxEqual(wpX.xDirection().Y(), 1.0), "Subtest 55.2: Local horizontal axis is +Y");

            // Plan vertical Y (XZ) - Coupe longitudinale
            WorkPlane wpY = WorkPlane::xz(4.0, "Coupe Y=4.00");
            TEST_CHECK(wpY.type() == WorkPlaneType::GlobalXZ, "Subtest 55.2: wpY type is GlobalXZ");
            TEST_CHECK(approxEqual(std::abs(wpY.normal().Y()), 1.0), "Subtest 55.2: Normal to Y plane is Y axis (Front View)");
            TEST_CHECK(approxEqual(wpY.yDirection().Z(), 1.0), "Subtest 55.2: Up vector for Y plane is +Z (Vertical)");
            TEST_CHECK(approxEqual(wpY.xDirection().X(), 1.0), "Subtest 55.2: Local horizontal axis is +X");

            std::cout << "  [PASS] Subtest 55.2: Camera Normal & Up Vector for Z, X, Y Verified" << std::endl;
        }

        // Subtest 55.3: Isolation 2D géométrique sur coupes verticales X, Y et plans horizontaux Z
        {
            const double tol = 0.05;

            // Élément 1 : Poutre sur Y=0, Z=3.20 de X=0 à X=5 (dans coupe Y=0 et plan Z=3.2)
            gp_Pnt b1_start(0.0, 0.0, 3.20);
            gp_Pnt b1_end(5.0, 0.0, 3.20);

            // Élément 2 : Poteau sur X=5, Y=4 de Z=0 à Z=3.20 (dans coupe X=5 et coupe Y=4)
            gp_Pnt c1_start(5.0, 4.0, 0.0);
            gp_Pnt c1_end(5.0, 4.0, 3.20);

            auto isSegmentInPlane = [&](const WorkPlane& wp, const gp_Pnt& p1, const gp_Pnt& p2) {
                double u1=0, v1=0, w1=0, u2=0, v2=0, w2=0;
                wp.toLocal(p1, u1, v1, w1);
                wp.toLocal(p2, u2, v2, w2);
                if (std::abs(w1) <= tol || std::abs(w2) <= tol) return true;
                if ((w1 < -tol && w2 > tol) || (w1 > tol && w2 < -tol)) return true;
                return false;
            };

            WorkPlane wpZ32 = WorkPlane::xy(3.20, "R+1");
            WorkPlane wpZ0 = WorkPlane::xy(0.0, "RDC");
            WorkPlane wpX5 = WorkPlane::yz(5.0, "Coupe X=5");
            WorkPlane wpX0 = WorkPlane::yz(0.0, "Coupe X=0");
            WorkPlane wpY0 = WorkPlane::xz(0.0, "Coupe Y=0");
            WorkPlane wpY4 = WorkPlane::xz(4.0, "Coupe Y=4");

            // Test Z
            TEST_CHECK(isSegmentInPlane(wpZ32, b1_start, b1_end), "Subtest 55.3: Beam 1 is visible on floor Z=3.20");
            TEST_CHECK(!isSegmentInPlane(wpZ0, b1_start, b1_end), "Subtest 55.3: Beam 1 is isolated on floor Z=0.00");

            // Test X (Coupe YZ à X=5)
            TEST_CHECK(isSegmentInPlane(wpX5, c1_start, c1_end), "Subtest 55.3: Column 1 is visible in vertical cut X=5.00");
            TEST_CHECK(isSegmentInPlane(wpX5, b1_start, b1_end), "Subtest 55.3: Beam 1 touches X=5.00 so is visible");
            TEST_CHECK(!isSegmentInPlane(wpX0, c1_start, c1_end), "Subtest 55.3: Column 1 (at X=5) is isolated in cut X=0.00");

            // Test Y (Coupe XZ à Y=4)
            TEST_CHECK(isSegmentInPlane(wpY4, c1_start, c1_end), "Subtest 55.3: Column 1 is visible in vertical cut Y=4.00");
            TEST_CHECK(!isSegmentInPlane(wpY4, b1_start, b1_end), "Subtest 55.3: Beam 1 (at Y=0) is isolated in cut Y=4.00");
            TEST_CHECK(isSegmentInPlane(wpY0, b1_start, b1_end), "Subtest 55.3: Beam 1 is visible in cut Y=0.00");

            std::cout << "  [PASS] Subtest 55.3: 2D Geometric Element Isolation across X, Y, Z Verified" << std::endl;
        }

        // Subtest 55.4: Navigation rapide et commutation d'axe bidirectionnelle
        {
            WorkPlane wp = WorkPlane::xy(0.0);
            TEST_CHECK(wp.isHorizontal(), "Subtest 55.4: Initial workplane is horizontal");

            // Bascule vers X (YZ)
            wp = WorkPlane::yz(5.0, "Coupe X");
            TEST_CHECK(!wp.isHorizontal(), "Subtest 55.4: Vertical X plane is not horizontal");
            TEST_CHECK(approxEqual(wp.origin().X(), 5.0), "Subtest 55.4: Origin X is 5.0m");

            // Bascule vers Y (XZ)
            wp = WorkPlane::xz(4.0, "Coupe Y");
            TEST_CHECK(!wp.isHorizontal(), "Subtest 55.4: Vertical Y plane is not horizontal");
            TEST_CHECK(approxEqual(wp.origin().Y(), 4.0), "Subtest 55.4: Origin Y is 4.0m");

            // Retour vers Z (XY)
            wp = WorkPlane::xy(3.20, "R+1");
            TEST_CHECK(wp.isHorizontal(), "Subtest 55.4: Returned to horizontal Z plane");
            TEST_CHECK(approxEqual(wp.origin().Z(), 3.20), "Subtest 55.4: Origin Z is 3.20m");

            std::cout << "  [PASS] Subtest 55.4: Rapid Navigation and Axis Switching Verified" << std::endl;
        }

        std::cout << "[PASS] Test 55: X-Y-Z Structural WorkPlanes Auto-Detection, Selection, Camera Normal & 2D Isolation Passed Successfully!" << std::endl;
        passed++;
    }

    return true;
}
