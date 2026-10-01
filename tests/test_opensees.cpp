#include "test_common.h"

#include "Analysis/OpenSeesManager.h"
#include "Analysis/CalculationSnapshot.h"
#include "Analysis/ResultsModel.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"
#include "Analysis/OpenSeesSolver.h"
#include "Model/Model.h"
#include "Model/Section.h"
#include "Model/Material.h"
#include "Model/Load/LoadManager.h"
#include "Geometry/DeformedGeometry.h"
#include "Geometry/DiagramGeometry.h"
#include "NDC/NDCDocumentModel.h"
#include "NDC/NDCGenerator.h"
#include "NDC/NDCExporter.h"

using namespace TSA::Analysis;
using namespace TSA::Model;

bool runSuite_OpenSees(int& passed)
{
    // TEST 65: OpenSees Manager Detection and Verification
    {
        auto& mgr = OpenSeesManager::instance();
        std::cout << "  Search directory: " << mgr.defaultSearchDirectory().toStdString() << std::endl;
        std::cout << "  Executable path: " << mgr.executablePath().toStdString() << std::endl;
        QString err;
        bool verified = mgr.verifyExecutable(mgr.executablePath(), &err);
        std::cout << "  Verified: " << (verified ? "YES" : "NO") << " | Err: " << err.toStdString() << std::endl;
        TEST_CHECK(!mgr.defaultSearchDirectory().isEmpty(), "Default OpenSees directory should not be empty");
        TEST_CHECK(mgr.isAvailable(), "OpenSees executable must be detected and available");
        TEST_CHECK(!mgr.executablePath().isEmpty(), "OpenSees executable path must be valid");

        OpenSeesVersionInfo v = mgr.versionInfo();
        TEST_CHECK(v.isValid, "OpenSees version info must be valid");
        TEST_CHECK(v.major >= 3, "OpenSees version major should be >= 3");
        passed++;
    }

    // TEST 66: Calculation Snapshot Data Security & Immutability
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed);
        model.addBeam(n1, n2, 0.25, 0.40);

        CalculationSnapshot snap = CalculationSnapshot::capture(model);
        TEST_CHECK(snap.nodeCount() == 2, "Snapshot must have 2 nodes");
        TEST_CHECK(snap.elementCount() == 1, "Snapshot must have 1 element");

        const auto* sn1 = snap.getNode(n1);
        TEST_CHECK(sn1 != nullptr, "Node 1 must exist in snapshot");
        TEST_CHECK(sn1->fixTx && sn1->fixTy && sn1->fixTz && sn1->fixRx && sn1->fixRy && sn1->fixRz,
                   "Fixed node must have all 6 DOFs restrained in snapshot");

        // Modification du modèle source : le snapshot ne doit pas changer
        model.addNode(10.0, 10.0, 10.0);
        TEST_CHECK(snap.nodeCount() == 2, "Snapshot must remain immutable after model modification");
        passed++;
    }

    // TEST 67: Simply Supported Beam with Point Load (Analytical verification)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Pinned);

        int n2 = model.addNode(5.0, 0.0, 0.0);
        model.getNode(n2)->setSupportType(SupportType::Roller);

        Section sec = Section::ipe(200);
        Material mat = Material::steelS235();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);
        TEST_CHECK(b1 > 0, "Beam creation failed");

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "Charge_Ponctuelle", LoadCaseCategory::Live, false, 1.0));

        // Fz = 10 kN au centre (x = 2.5 m, relatif = 0.5)
        MemberLoad ml = MemberLoad::pointOnMember(b1, lcId, 10.0, 0.5, LoadDirection::GlobalZ, LoadCoordSystem::Global, true, "Pt_10kN");
        lm.addMemberLoad(ml);

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        if (!ok) {
            std::cout << "  Solver error: " << err.toStdString() << "\n";
            std::cout << "  Solver log: \n" << solver.results().journalLog() << "\n";
        }
        TEST_CHECK(ok, "Solver execution failed for beam with point load");

        const auto& res = solver.results();
        TEST_CHECK(res.isValid(), "ResultsModel must be valid after solution");

        // Réactions : 5 kN à chaque appui
        const auto* r1 = res.getNodeReaction(n1);
        const auto* r2 = res.getNodeReaction(n2);
        TEST_CHECK(r1 != nullptr && r2 != nullptr, "Reaction records must be present");
        TEST_CHECK(std::abs(r1->rz - 5.0) < 0.2, "Reaction at N1 must equal 5.0 kN");
        TEST_CHECK(std::abs(r2->rz - 5.0) < 0.2, "Reaction at N2 must equal 5.0 kN");

        // Moment fléchissant max analytique : P * L / 4 = 10 * 5 / 4 = 12.5 kNm
        const auto* eb = res.getElementResults(b1);
        TEST_CHECK(eb != nullptr, "Element results must be present");
        double maxM = eb->maxBendingMoment();
        TEST_CHECK(std::abs(maxM - 12.5) < 0.5, "Maximum bending moment must be close to 12.5 kNm");

        // Équilibre global
        TEST_CHECK(res.equilibrium().isBalanced(0.05), "Global equilibrium must be verified");
        passed++;
    }

    // TEST 68: Simply Supported Beam with Uniform Load (q = 20 kN/m)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Pinned);

        int n2 = model.addNode(6.0, 0.0, 0.0);
        model.getNode(n2)->setSupportType(SupportType::Roller);

        Section sec = Section::ipe(300);
        Material mat = Material::steelS355();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(2, "Charge_Uniforme", LoadCaseCategory::Live, false, 1.0));

        // q = 20 kN/m -> R = 20 * 6 / 2 = 60 kN
        MemberLoad ml = MemberLoad::uniform(b1, lcId, 20.0, LoadDirection::GlobalZ, "UDL_20kN");
        lm.addMemberLoad(ml);

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for uniform load");

        const auto& res = solver.results();
        TEST_CHECK(res.isValid(), "ResultsModel must be valid");

        const auto* r1 = res.getNodeReaction(n1);
        const auto* r2 = res.getNodeReaction(n2);
        TEST_CHECK(r1 != nullptr && r2 != nullptr, "Reactions must be computed");
        TEST_CHECK(std::abs(r1->rz - 60.0) < 1.0, "Reaction at N1 must equal 60 kN");
        TEST_CHECK(std::abs(r2->rz - 60.0) < 1.0, "Reaction at N2 must equal 60 kN");
        passed++;
    }

    // TEST 69: Modal Analysis
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed);
        int n2 = model.addNode(0.0, 0.0, 4.0); // Poteau console 4m

        Section sec = Section::rectangular(0.30, 0.30);
        Material mat = Material::concreteC25_30();
        model.addBar(n1, n2, sec, mat, BarRole::Column);

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::Modal;
        params.numEigenmodes = 3;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        if (!ok) {
            std::cout << "  Modal error: " << err.toStdString() << "\n";
            std::cout << "  Modal journal log: \n" << solver.results().journalLog() << "\n";
        }
        TEST_CHECK(ok, "Modal analysis solve failed");

        const auto& res = solver.results();
        TEST_CHECK(res.isValid(), "Modal ResultsModel must be valid");
        TEST_CHECK(!res.modalModes().empty(), "Modal modes list must not be empty");

        for (const auto& m : res.modalModes())
        {
            TEST_CHECK(m.frequency > 0.0, "Mode frequency must be positive");
            TEST_CHECK(m.period > 0.0, "Mode period must be positive");
        }
        passed++;
    }

    // TEST 70: 3D Deformed Geometry Generation (Displacement amplification)
    {
        gp_Pnt p1(0.0, 0.0, 0.0);
        gp_Pnt p2(5.0, 0.0, 0.0);
        NodeDisplacement d1{ 0.0, 0.0, 0.0, 0.0, 0.01, 0.0 };
        NodeDisplacement d2{ 0.0, 0.0, -0.005, 0.0, -0.01, 0.0 };
        double scale = 50.0;

        gp_Pnt defP1 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p1, d1, scale);
        gp_Pnt defP2 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p2, d2, scale);

        TEST_CHECK(std::abs(defP1.X() - 0.0) < 1e-5 && std::abs(defP1.Z() - 0.0) < 1e-5, "Node 1 remains at origin");
        TEST_CHECK(std::abs(defP2.Z() - (-0.25)) < 1e-4, "Node 2 deformed Z is scaled by 50 (-0.005 * 50 = -0.25)");

        Section sec = Section::ipe(200);
        TopoDS_Shape defShape = TSA::Geometry::DeformedGeometry::createDeformedBeamShape(p1, p2, d1, d2, sec, scale);
        TEST_CHECK(!defShape.IsNull(), "Deformed beam 3D shape must not be null");

        TopoDS_Shape wire = TSA::Geometry::DeformedGeometry::createDeformedCenterline(p1, p2, d1, d2, scale, 10);
        TEST_CHECK(!wire.IsNull(), "Deformed centerline wire must not be null");
        passed++;
    }

    // TEST 71: 3D Force Diagram Ribbon Geometry (Bending Moment & Shear)
    {
        gp_Pnt p1(0.0, 0.0, 0.0);
        gp_Pnt p2(6.0, 0.0, 0.0);
        std::vector<StationForces> stations;
        // Parabolic moment Mz: 0 at ends, 18 kNm at midspan
        for (int i = 0; i <= 10; ++i) {
            double s = i / 10.0;
            double x = s * 6.0;
            double m = 4.0 * 18.0 * s * (1.0 - s);
            double v = 12.0 - 4.0 * x;
            StationForces st;
            st.position = x;
            st.Mz = m;
            st.Vz = v;
            st.N = -50.0;
            stations.push_back(st);
        }

        TopoDS_Shape diagMz = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, stations, TSA::Geometry::DiagramType::BendingMz, 0.05, true);
        TEST_CHECK(!diagMz.IsNull(), "Bending moment Mz 3D diagram shape must not be null");

        TopoDS_Shape diagN = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, stations, TSA::Geometry::DiagramType::AxialForceN, 0.02, true);
        TEST_CHECK(!diagN.IsNull(), "Axial force N 3D diagram shape must not be null");
        passed++;
    }

    // TEST 72: Note de Calcul (NDC) Generation, HTML & PDF Exporter
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Pinned);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        model.getNode(n2)->setSupportType(SupportType::Roller);
        Section sec = Section::hea(200);
        Material mat = Material::steelS355();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "G_Permanent", LoadCaseCategory::Dead, true, 1.35));
        lm.addMemberLoad(MemberLoad::uniform(b1, lcId, 15.0, LoadDirection::GlobalZ));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        if (!ok) {
            std::cout << "  Test 72 Solver error: " << err.toStdString() << "\n";
            std::cout << "  Test 72 Journal log: \n" << solver.results().journalLog() << "\n";
        }
        TEST_CHECK(ok, "OpenSees solve for NDC test must succeed");

        auto resultsPtr = std::make_shared<ResultsModel>(solver.results());
        TSA::NDC::NDCDocument doc = TSA::NDC::NDCGenerator::generate(model, resultsPtr, "Tour d'essai", "Christinot");

        TEST_CHECK(!doc.chapters.empty(), "NDC must contain structured chapters");
        TEST_CHECK(doc.chapters.size() >= 7, "NDC must contain at least 7 comprehensive technical chapters");

        QString html = doc.toHtml();
        TEST_CHECK(!html.isEmpty(), "Generated HTML for NDC must not be empty");
        TEST_CHECK(html.contains("NOTE DE CALCUL DE STRUCTURE"), "HTML must contain header title");
        TEST_CHECK(html.contains("OpenSees"), "HTML must reference OpenSees solver engine");
        TEST_CHECK(html.contains("Eurocode"), "HTML must reference Eurocode standards");

        QString plain = doc.toPlainText();
        TEST_CHECK(!plain.isEmpty(), "Plain text representation must not be empty");
        passed++;
    }

    // TEST 73: Free Node & Supported Node Identification in Model
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        int n3 = model.addNode(8.0, 0.0, 0.0); // Nœud libre
        int n4 = model.addNode(0.0, 4.0, 0.0); // Nœud avec appui seul

        model.getNode(n1)->setSupportType(SupportType::Fixed);
        model.getNode(n4)->setSupportType(SupportType::Pinned);

        // Barre reliant n1 et n2
        model.addBeam(n1, n2, 0.30, 0.40);

        TEST_CHECK(!model.isNodeFree(n1), "Node 1 connected to beam is not free");
        TEST_CHECK(!model.isNodeFree(n2), "Node 2 connected to beam is not free");
        TEST_CHECK(model.isNodeFree(n3), "Node 3 unconnected to any element is free");
        TEST_CHECK(model.isNodeFree(n4), "Node 4 unconnected to any element is free (even if supported)");

        auto freeList = model.freeNodeIds();
        TEST_CHECK(freeList.size() == 2, "Model should have exactly 2 free nodes (n3, n4)");
        TEST_CHECK(std::find(freeList.begin(), freeList.end(), n3) != freeList.end(), "Free list contains n3");
        TEST_CHECK(std::find(freeList.begin(), freeList.end(), n4) != freeList.end(), "Free list contains n4");

        auto suppList = model.supportedNodeIds();
        TEST_CHECK(suppList.size() == 2, "Model should have exactly 2 supported nodes (n1, n4)");
        TEST_CHECK(std::find(suppList.begin(), suppList.end(), n1) != suppList.end(), "Supp list contains n1");
        TEST_CHECK(std::find(suppList.begin(), suppList.end(), n4) != suppList.end(), "Supp list contains n4");
        passed++;
    }

    // TEST 74: Extended Diagram Geometries & Multi-Step Results
    {
        gp_Pnt p1(0.0, 0.0, 0.0);
        gp_Pnt p2(5.0, 0.0, 0.0);
        std::vector<StationForces> stations;
        for (int i = 0; i <= 5; ++i) {
            StationForces st;
            st.position = i * 1.0;
            st.Mz = 10.0;
            st.My = 5.0;
            st.Mx = 2.0;
            st.Vz = -3.0;
            st.Vy = 1.5;
            st.N = 25.0;
            st.ux = 0.001;
            st.uy = 0.002;
            st.uz = -0.005;
            stations.push_back(st);
        }

        // Test Deflection UZ diagram
        TopoDS_Shape diagDef = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, stations, TSA::Geometry::DiagramType::DeflectionUz, 100.0, true);
        TEST_CHECK(!diagDef.IsNull(), "Deflection UZ 3D diagram shape must be valid");

        // Test Rotation RY diagram
        TopoDS_Shape diagRot = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, stations, TSA::Geometry::DiagramType::RotationRy, 200.0, true);
        TEST_CHECK(!diagRot.IsNull(), "Rotation RY 3D diagram shape must be valid");

        // Test Nonlinear algorithm & integrator Tcl conversion helpers
        TEST_CHECK(std::string(TSA::Analysis::toTclString(TSA::Analysis::NonlinearAlgorithm::NewtonLineSearch)).find("NewtonLineSearch") != std::string::npos, "NewtonLineSearch Tcl match");
        TEST_CHECK(std::string(TSA::Analysis::toTclString(TSA::Analysis::NonlinearAlgorithm::KrylovNewton)) == "KrylovNewton", "KrylovNewton Tcl match");
        TEST_CHECK(std::string(TSA::Analysis::toTclString(TSA::Analysis::IntegratorType::ArcLength)) == "ArcLength", "ArcLength Tcl match");
        TEST_CHECK(std::string(TSA::Analysis::toTclString(TSA::Analysis::IntegratorType::DisplacementControl)) == "DisplacementControl", "DisplacementControl Tcl match");

        // Test Multi-step tracking in ResultsModel
        ResultsModel rm;
        rm.setValid(true);
        StepResults step0;
        step0.stepNumber = 0;
        step0.factorOrTime = 0.5;
        step0.displacements[1] = NodeDisplacement{0.001, 0.0, -0.002, 0.0, 0.0, 0.0};
        step0.reactions[1] = NodeReaction{0.0, 0.0, 10.0, 0.0, 0.0, 0.0};

        StepResults step1;
        step1.stepNumber = 1;
        step1.factorOrTime = 1.0;
        step1.displacements[1] = NodeDisplacement{0.002, 0.0, -0.004, 0.0, 0.0, 0.0};
        step1.reactions[1] = NodeReaction{0.0, 0.0, 20.0, 0.0, 0.0, 0.0};

        rm.addStepResults(step0);
        rm.addStepResults(step1);
        rm.setActiveStep(1);

        TEST_CHECK(rm.stepCount() == 2, "ResultsModel must have 2 steps");
        TEST_CHECK(rm.activeStep() == 1, "Default active step is 1");
        TEST_CHECK(approxEqual(rm.nodeDisplacement(1).uz, -0.004), "Step 1 displacement");
        TEST_CHECK(approxEqual(rm.nodeReaction(1).rz, 20.0), "Step 1 reaction");

        // Switch to step 0
        rm.setActiveStep(0);
        TEST_CHECK(rm.activeStep() == 0, "Active step should now be 0");
        TEST_CHECK(approxEqual(rm.nodeDisplacement(1).uz, -0.002), "Step 0 displacement");
        TEST_CHECK(approxEqual(rm.nodeReaction(1).rz, 10.0), "Step 0 reaction");

        passed++;
    }

    // TEST 76: Axial Normal Force Sign & Station Uniformity (RDM convention)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed);
        int n2 = model.addNode(5.0, 0.0, 0.0); // Bar along X, 5m

        Section sec = Section::ipe(200);
        Material mat = Material::steelS235();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "Traction", LoadCaseCategory::Live, false, 1.0));
        // Tension: Nodal load +50 kN along X at node 2
        lm.addNodalLoad(NodalLoad(0, n2, lcId, 50.0, 0.0, 0.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "Traction_50kN"));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for bar in axial tension");

        const auto& res = solver.results();
        const auto* eb = res.getElementResults(b1);
        TEST_CHECK(eb != nullptr, "Element results must be present for beam");
        TEST_CHECK(std::abs(eb->startForces.N - 50.0) < 1.0, "Start normal force must be ~50 kN in tension");
        TEST_CHECK(std::abs(eb->endForces.N - 50.0) < 1.0, "End normal force must be ~50 kN in tension");

        // Stations intermédiaires : doivent toutes être ~50 kN (et non 0 au milieu !)
        TEST_CHECK(!eb->intermediateStations.empty(), "Intermediate stations must be populated");
        for (const auto& st : eb->intermediateStations)
        {
            TEST_CHECK(std::abs(st.N - 50.0) < 1.0, "Station normal force must be ~50 kN throughout the bar");
        }

        passed++;
    }

    return true;
}
