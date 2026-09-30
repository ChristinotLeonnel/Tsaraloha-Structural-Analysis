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
        int b1 = model.addBeam(n1, n2, 0.25, 0.40);

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

    return true;
}
