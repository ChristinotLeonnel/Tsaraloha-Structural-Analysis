#include "test_common.h"
#include <limits>

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

        // Fz = 10 kN gravitaire au centre (x = 2.5 m, relatif = 0.5)
        MemberLoad ml = MemberLoad::pointOnMember(b1, lcId, 10.0, 0.5, LoadDirection::Gravity, LoadCoordSystem::Global, true, "Pt_10kN");
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

        // q = 20 kN/m gravitaire -> R = 20 * 6 / 2 = 60 kN
        MemberLoad ml = MemberLoad::uniform(b1, lcId, 20.0, LoadDirection::Gravity, "UDL_20kN");
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

    // TEST 77: Load Combination Factoring (1.35*G + 1.50*Q)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed);
        int n2 = model.addNode(4.0, 0.0, 0.0);

        Section sec = Section::ipe(200);
        Material mat = Material::steelS235();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lc1 = lm.addLoadCase(LoadCase(1, "Dead_G", LoadCaseCategory::Dead, false, 1.0));
        int lc2 = lm.addLoadCase(LoadCase(2, "Live_Q", LoadCaseCategory::Live, false, 1.0));

        // Nodal load on node 2: 10 kN along X for Case 1, 20 kN along X for Case 2
        lm.addNodalLoad(NodalLoad(0, n2, lc1, 10.0, 0.0, 0.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "G_10kN"));
        lm.addNodalLoad(NodalLoad(0, n2, lc2, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "Q_20kN"));

        // Combination: 1.35 * G + 1.50 * Q = 1.35 * 10 + 1.50 * 20 = 13.5 + 30.0 = 43.5 kN
        std::map<int, double> factors = { {lc1, 1.35}, {lc2, 1.50} };
        int comboId = lm.addCombination(LoadCombination(1, "ELU_Comb", LoadCombinationType::ULS_Fundamental, factors, "1.35G + 1.50Q"));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetCombinationId = comboId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for load combination");

        const auto& res = solver.results();
        const auto* eb = res.getElementResults(b1);
        TEST_CHECK(eb != nullptr, "Element results must be present for combination");
        TEST_CHECK(std::abs(eb->startForces.N - 43.5) < 0.5, "Combination normal force must equal 43.5 kN (1.35*10 + 1.50*20)");
        TEST_CHECK(std::abs(eb->endForces.N - 43.5) < 0.5, "End normal force must equal 43.5 kN");

        // Réaction au nœud 1
        const auto* r1 = res.getNodeReaction(n1);
        TEST_CHECK(r1 != nullptr, "Reaction at support node 1 must exist");
        TEST_CHECK(std::abs(r1->rx - (-43.5)) < 0.5, "Reaction Rx at support must equal -43.5 kN to balance the combination");

        passed++;
    }

    // TEST 78: Global Equilibrium with Member Uniform Loads (No Nodal Loads)
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
        int lcId = lm.addLoadCase(LoadCase(1, "UDL_Case", LoadCaseCategory::Live, false, 1.0));
        // Uniform load q = 25 kN/m downwards along Gravity -> Total applied force = 25 * 6 = 150 kN
        lm.addMemberLoad(MemberLoad::uniform(b1, lcId, 25.0, LoadDirection::Gravity, "UDL_25kNm"));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for member uniform load equilibrium");

        const auto& res = solver.results();
        const auto& eq = res.equilibrium();
        TEST_CHECK(std::abs(eq.appliedFz - (-150.0)) < 1.0, "Total applied vertical force must equal -150 kN from member load");
        TEST_CHECK(std::abs(eq.reactionFz - 150.0) < 1.0, "Total reaction vertical force must equal +150 kN");
        TEST_CHECK(eq.isBalanced(0.05), "Global equilibrium must balance member uniform loads");

        passed++;
    }

    // TEST 79: ElementResults Stations Local Displacements & Rotations
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed); // Cantilever
        int n2 = model.addNode(5.0, 0.0, 0.0);

        Section sec = Section::ipe(240);
        Material mat = Material::steelS235();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "TipLoad", LoadCaseCategory::Live, false, 1.0));
        // Tip load Fz = -10 kN
        lm.addNodalLoad(NodalLoad(0, n2, lcId, 0.0, 0.0, -10.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "Tip_10kN"));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for cantilever beam");

        const auto& res = solver.results();
        const auto* eb = res.getElementResults(b1);
        TEST_CHECK(eb != nullptr, "Element results must be present");
        TEST_CHECK(std::abs(eb->startForces.uz) < 1e-6, "Deflection at fixed end must be 0");
        TEST_CHECK(eb->endForces.uz < -1e-4, "Deflection at tip must be negative");

        // Intermediate stations: deflection must monotonically decrease from fixed end to tip
        TEST_CHECK(!eb->intermediateStations.empty(), "Intermediate stations must exist");
        double prevUz = eb->startForces.uz;
        for (const auto& st : eb->intermediateStations)
        {
            TEST_CHECK(st.uz <= prevUz + 1e-9, "Deflection must decrease monotonically towards tip");
            prevUz = st.uz;
        }

        passed++;
    }

    // TEST 80: Comprehensive NDC Document Generation with Cables and Trusses
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        int n3 = model.addNode(4.0, 0.0, 3.0);
        int n4 = model.addNode(0.0, 0.0, 3.0);

        model.getNode(n1)->setSupportType(SupportType::Fixed);
        model.getNode(n2)->setSupportType(SupportType::Pinned);

        model.addColumn(n1, n4, 0.30, 0.30);
        model.addBeam(n4, n3, 0.20, 0.40);
        model.addTrussMember(n1, n3, 0.10);
        model.addCable(n2, n4, 0.020);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "Charge_Test", LoadCaseCategory::Dead, false, 1.0));
        lm.addNodalLoad(NodalLoad(0, n3, lcId, 0.0, 0.0, -20.0, 0.0, 0.0, 0.0));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution failed for 4-element structure");

        auto resultsPtr = std::make_shared<ResultsModel>(solver.results());
        TSA::NDC::NDCDocument doc = TSA::NDC::NDCGenerator::generate(model, resultsPtr, "Tour Mixte", "Christinot");

        QString html = doc.toHtml();
        TEST_CHECK(html.contains("Poutre"), "NDC HTML must contain Poutre");
        TEST_CHECK(html.contains("Poteau"), "NDC HTML must contain Poteau");
        TEST_CHECK(html.contains("Treillis"), "NDC HTML must contain Treillis");
        TEST_CHECK(html.contains("Câble"), "NDC HTML must contain Câble");

        passed++;
    }

    // TEST 81: DeformedGeometry Robustness & Numerical Guards
    {
        gp_Pnt p1(0.0, 0.0, 0.0);
        gp_Pnt p2(4.0, 0.0, 0.0);
        NodeDisplacement dNan{ std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0, 0.0, 0.0 };
        NodeDisplacement dInf{ 0.0, std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0, 0.0 };

        // computeDeformedPoint handles NaN/Inf safely without producing NaN coordinates
        gp_Pnt pDefNan = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p1, dNan, 50.0);
        TEST_CHECK(!std::isnan(pDefNan.X()) && !std::isnan(pDefNan.Y()) && !std::isnan(pDefNan.Z()),
                   "computeDeformedPoint handles NaN disp safely");

        gp_Pnt pDefInf = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p1, dInf, 50.0);
        TEST_CHECK(!std::isinf(pDefInf.X()) && !std::isinf(pDefInf.Y()) && !std::isinf(pDefInf.Z()),
                   "computeDeformedPoint handles Inf disp safely");

        // Scale factor NaN/Inf handled safely
        gp_Pnt pDefScaleNan = TSA::Geometry::DeformedGeometry::computeDeformedPoint(
            p1, NodeDisplacement{0.01, 0.02, 0.03, 0, 0, 0}, std::numeric_limits<double>::quiet_NaN());
        TEST_CHECK(std::abs(pDefScaleNan.X() - p1.X()) < 1e-6,
                   "computeDeformedPoint with NaN scale yields original coordinates");

        // Degenerate element (coincident nodes)
        Section sec = Section::rectangular(0.2, 0.4);
        TopoDS_Shape degShape = TSA::Geometry::DeformedGeometry::createDeformedBeamShape(p1, p1, dNan, dInf, sec, 1.0);
        TEST_CHECK(degShape.IsNull(), "createDeformedBeamShape on coincident points returns null shape");

        // Sphere with invalid radius
        TopoDS_Shape sph = TSA::Geometry::DeformedGeometry::createDeformedNodeSphere(p1, dNan, 1.0, -1.0);
        TEST_CHECK(!sph.IsNull(), "createDeformedNodeSphere with negative radius defaults safely to standard sphere");

        // Modal deformed beam shape with NaN scale and phase
        TopoDS_Shape modalShape = TSA::Geometry::DeformedGeometry::createModalDeformedBeamShape(
            p1, p2, dNan, dInf, sec, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity());
        TEST_CHECK(!modalShape.IsNull(), "createModalDeformedBeamShape handles NaN/Inf modal scale and phase safely");

        passed++;
    }

    // TEST 82: DiagramGeometry Robustness & Numerical Guards
    {
        gp_Pnt p1(0.0, 0.0, 0.0);
        gp_Pnt p2(5.0, 0.0, 0.0);

        // Coincident points -> null shape
        std::vector<StationForces> dummyStations(3);
        dummyStations[0].position = 0.0; dummyStations[0].Mz = 10.0;
        dummyStations[1].position = 2.5; dummyStations[1].Mz = 20.0;
        dummyStations[2].position = 5.0; dummyStations[2].Mz = 10.0;
        TopoDS_Shape shCoincident = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p1, 0.0, dummyStations, TSA::Geometry::DiagramType::BendingMz, 0.1);
        TEST_CHECK(shCoincident.IsNull(), "Diagram on coincident points must be null");

        // Type None -> null shape
        TopoDS_Shape shNone = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, dummyStations, TSA::Geometry::DiagramType::None, 0.1);
        TEST_CHECK(shNone.IsNull(), "Diagram of type None must be null");

        // Stations with NaN/Inf values
        std::vector<StationForces> nanStations = dummyStations;
        nanStations[1].Mz = std::numeric_limits<double>::quiet_NaN();
        nanStations[2].Mz = std::numeric_limits<double>::infinity();
        TopoDS_Shape shNan = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, nanStations, TSA::Geometry::DiagramType::BendingMz, 0.1);
        TEST_CHECK(!shNan.IsNull(), "Diagram with NaN/Inf station forces is built safely");

        // Negative or NaN scale factor
        TopoDS_Shape shNegScale = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, dummyStations, TSA::Geometry::DiagramType::BendingMz, -5.0);
        TEST_CHECK(!shNegScale.IsNull(), "Diagram with negative scale factor defaults to zero safely");

        TopoDS_Shape shNanScale = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, dummyStations, TSA::Geometry::DiagramType::BendingMz, std::numeric_limits<double>::quiet_NaN());
        TEST_CHECK(!shNanScale.IsNull(), "Diagram with NaN scale factor defaults to zero safely");

        // Diagram crossing zero (positive to negative Mz)
        std::vector<StationForces> crossStations(3);
        crossStations[0].position = 0.0; crossStations[0].Mz = 15.0;
        crossStations[1].position = 2.5; crossStations[1].Mz = 0.0;
        crossStations[2].position = 5.0; crossStations[2].Mz = -15.0;
        TopoDS_Shape shCross = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, 0.0, crossStations, TSA::Geometry::DiagramType::BendingMz, 0.05);
        TEST_CHECK(!shCross.IsNull(), "Diagram crossing zero must produce valid compound shape");

        passed++;
    }

    return true;
}
