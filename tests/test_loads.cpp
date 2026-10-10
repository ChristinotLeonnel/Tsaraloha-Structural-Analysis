#include "test_common.h"
#include "Model/Load/LoadEnums.h"
#include "Model/Load/NodalLoad.h"
#include "Model/Load/MemberLoad.h"
#include "Model/Load/LoadCase.h"
#include "Model/Load/LoadCombination.h"
#include "Model/Load/LoadManager.h"
#include "Analysis/LoadResolver.h"
#include "Analysis/CalculationSnapshot.h"
#include "Geometry/MemberLoadGlyph.h"
#include "Model/Load/MemberLoadCommands.h"
#include "IO/TSAFile.h"
#include <limits>
#include "Analysis/LoadValidation.h"
#include "Analysis/OpenSeesAdapter.h"
#include "Model/Beam.h"
#include "Model/Column.h"

using namespace TSA::Model;
using namespace TSA::Analysis;

bool runSuite_Loads(int& passed)
{
    std::cout << "\n=================================================" << std::endl;
    std::cout << "SUITE: Structural Loads & OpenSees Solver Integration" << std::endl;
    std::cout << "=================================================" << std::endl;

    // =========================================================================
    // TEST 56: Nodal Load CRUD, Components & Global/Local Coordinates
    // =========================================================================
    {
        std::cout << "\n--- TEST 56: Nodal Load CRUD, Components & Magnitudes ---" << std::endl;

        LoadManager lm;
        TEST_CHECK(lm.nodalLoads().empty(), "Subtest 56.1: Initial nodal loads list is empty");

        // 1. Création d'une charge nodale complète
        NodalLoad nl1(0, 1, 1, 10.0, -5.0, -25.0, 1.5, -2.0, 3.0, LoadCoordSystem::Global, "NL_Test1");
        TEST_CHECK(nl1.nodeId() == 1, "Subtest 56.1: Node ID is 1");
        TEST_CHECK(nl1.loadCaseId() == 1, "Subtest 56.1: Load case ID is 1");
        TEST_CHECK(approxEqual(nl1.fx(), 10.0) && approxEqual(nl1.fy(), -5.0) && approxEqual(nl1.fz(), -25.0),
                   "Subtest 56.1: Forces match");
        TEST_CHECK(approxEqual(nl1.mx(), 1.5) && approxEqual(nl1.my(), -2.0) && approxEqual(nl1.mz(), 3.0),
                   "Subtest 56.1: Moments match");

        // 2. Vérification de la résultante
        // R_force = sqrt(100 + 25 + 625) = sqrt(750) = 27.386127875
        TEST_CHECK(approxEqual(nl1.resultantForce(), std::sqrt(750.0)), "Subtest 56.2: Resultant force calculation");
        // R_moment = sqrt(2.25 + 4 + 9) = sqrt(15.25) = 3.905124837
        TEST_CHECK(approxEqual(nl1.resultantMoment(), std::sqrt(15.25)), "Subtest 56.2: Resultant moment calculation");

        // 3. Ajout dans le LoadManager
        int id1 = lm.addNodalLoad(nl1);
        TEST_CHECK(id1 == 1, "Subtest 56.3: First load assigned ID 1");
        TEST_CHECK(lm.nodalLoads().size() == 1, "Subtest 56.3: LoadManager has 1 load");
        auto* retrieved = lm.getNodalLoad(id1);
        TEST_CHECK(retrieved != nullptr && retrieved->name() == "NL_Test1", "Subtest 56.3: Retrieve load by ID");

        // 4. Ajout d'une deuxième charge sur un autre nœud
        NodalLoad nl2(0, 2, 2, 0.0, 0.0, -50.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "NL_Test2");
        int id2 = lm.addNodalLoad(nl2);
        TEST_CHECK(id2 == 2, "Subtest 56.4: Second load assigned ID 2");

        // 5. Filtres par nœud et par cas de charge
        auto loadsN1 = lm.getNodalLoadsForNode(1);
        TEST_CHECK(loadsN1.size() == 1 && loadsN1[0].id() == 1, "Subtest 56.5: Filter by node ID");
        auto loadsCase2 = lm.getNodalLoadsForCase(2);
        TEST_CHECK(loadsCase2.size() == 1 && loadsCase2[0].id() == 2, "Subtest 56.5: Filter by load case ID");

        // 6. Suppression
        bool rem = lm.removeNodalLoad(id1);
        TEST_CHECK(rem && lm.nodalLoads().size() == 1, "Subtest 56.6: Remove nodal load");
        TEST_CHECK(lm.getNodalLoad(id1) == nullptr, "Subtest 56.6: Removed load is nullptr");

        std::cout << "[PASS] Test 56: Nodal Load CRUD & Magnitudes Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 57: Member Distributed & Concentrated Loads
    // =========================================================================
    {
        std::cout << "\n--- TEST 57: Member Loads: Uniform, Trapezoidal, Point ---" << std::endl;

        LoadManager lm;

        // 1. Charge uniforme
        MemberLoad uLoad = MemberLoad::uniform(10, 1, -12.5, LoadDirection::GlobalZ, "UDL_Span1");
        TEST_CHECK(uLoad.isUniform(), "Subtest 57.1: Load is recognized as uniform");
        TEST_CHECK(!uLoad.isTrapezoidal() && !uLoad.isPointOnMember(), "Subtest 57.1: Not trapezoidal or point");
        TEST_CHECK(approxEqual(uLoad.q1(), -12.5) && approxEqual(uLoad.q2(), -12.5), "Subtest 57.1: Intensities match");
        TEST_CHECK(uLoad.direction() == LoadDirection::GlobalZ, "Subtest 57.1: Direction is GlobalZ");

        int id1 = lm.addMemberLoad(uLoad);
        TEST_CHECK(id1 == 1, "Subtest 57.1: Member load ID 1");

        // 2. Charge trapézoïdale / triangulaire
        MemberLoad tLoad = MemberLoad::trapezoidal(10, 2, 0.0, -20.0, 1.0, 5.0, LoadDirection::LocalZ, "Tri_Load");
        TEST_CHECK(tLoad.isTrapezoidal(), "Subtest 57.2: Recognized as trapezoidal");
        TEST_CHECK(approxEqual(tLoad.q1(), 0.0) && approxEqual(tLoad.q2(), -20.0), "Subtest 57.2: Intensities match");
        TEST_CHECK(approxEqual(tLoad.x1(), 1.0) && approxEqual(tLoad.x2(), 5.0), "Subtest 57.2: Positions match");

        int id2 = lm.addMemberLoad(tLoad);
        TEST_CHECK(id2 == 2, "Subtest 57.2: Member load ID 2");

        // 3. Charge ponctuelle sur barre
        MemberLoad pLoad = MemberLoad::pointOnMember(10, 1, -75.0, 3.0, LoadDirection::GlobalZ, "Point_Midspan");
        TEST_CHECK(pLoad.isPointOnMember(), "Subtest 57.3: Recognized as point on member");
        TEST_CHECK(approxEqual(pLoad.q1(), -75.0), "Subtest 57.3: Point force magnitude");
        TEST_CHECK(approxEqual(pLoad.x1(), 3.0), "Subtest 57.3: Point force position");

        int id3 = lm.addMemberLoad(pLoad);
        TEST_CHECK(id3 == 3, "Subtest 57.3: Member load ID 3");

        // 4. Recherche par élément
        auto elemLoads = lm.getMemberLoadsForElement(10);
        TEST_CHECK(elemLoads.size() == 3, "Subtest 57.4: 3 member loads found for element 10");

        std::cout << "[PASS] Test 57: Member Loads: Uniform, Trapezoidal, Point Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 58: Automated Self-Weight Calculation
    // =========================================================================
    {
        std::cout << "\n--- TEST 58: Automated Element Self-Weight ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(6.0, 0.0, 0.0);

        // Section rectangulaire 0.30 x 0.50 m -> Aire = 0.15 m2
        Section sec = Section::rectangular(0.30, 0.50, "R30x50");
        // Béton C25/30 : masse volumique = 2500 kg/m3
        Material mat = Material::concreteC25_30();

        int beamId = model.addBar(n1, n2, sec, mat, BarRole::Beam);
        TEST_CHECK(beamId > 0, "Subtest 58.1: Beam added to model");

        // Formule théorique : q = rho * A * g / 1000 (kN/m)
        // rho = 2500 kg/m3, A = 0.15 m2, g = 9.80665 m/s2
        // q = 2500 * 0.15 * 9.80665 / 1000 = 3.67749375 kN/m
        double expectedQ = (2500.0 * 0.15 * 9.80665) / 1000.0;
        double computedQ = LoadManager::computeElementSelfWeight(model, beamId);

        TEST_CHECK(approxEqual(computedQ, expectedQ, 1e-4), "Subtest 58.1: Beam self-weight matches analytical formula");

        // Poids total pour L = 6m : W = q * L = 3.67749375 * 6 = 22.0649625 kN
        double expectedTotalWeight = expectedQ * 6.0;
        double computedTotalWeight = LoadManager::computeTotalModelSelfWeight(model);

        TEST_CHECK(approxEqual(computedTotalWeight, expectedTotalWeight, 1e-3),
                   "Subtest 58.2: Total structure self-weight matches analytical weight");

        std::cout << "[PASS] Test 58: Automated Self-Weight Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 59: Load Cases & Multi-Factor Combinations
    // =========================================================================
    {
        std::cout << "\n--- TEST 59: Load Cases & Combinations (Eurocodes) ---" << std::endl;

        LoadManager lm;
        lm.initializeEurocodeDefaults();

        // 1. Vérification des 5 cas de charges par défaut
        TEST_CHECK(lm.loadCases().size() == 5, "Subtest 59.1: 5 Eurocode default load cases initialized");
        const auto* caseG = lm.getLoadCase(1);
        TEST_CHECK(caseG != nullptr && caseG->name() == "G" && caseG->isSelfWeightIncluded(),
                   "Subtest 59.1: Case G has self-weight enabled");
        const auto* caseQ = lm.getLoadCase(2);
        TEST_CHECK(caseQ != nullptr && caseQ->name() == "Q" && !caseQ->isSelfWeightIncluded(),
                   "Subtest 59.1: Case Q has self-weight disabled");

        // 2. Combinaisons d'actions (ELU / ELS)
        TEST_CHECK(lm.loadCombinations().size() == 3, "Subtest 59.2: 3 default combinations initialized");
        LoadCombination customCombo(10, "Combinaison Vent", LoadCombinationType::ULS,
                                    {{1, 1.35}, {3, 1.50}});
        lm.addCombination(customCombo);
        TEST_CHECK(lm.loadCombinations().size() == 4, "Subtest 59.2: 4 combinations after adding custom");

        // 3. Formule lisible automatique
        std::string formulaStr = customCombo.formula(lm.loadCases());
        TEST_CHECK(formulaStr == "1.35*G + 1.50*W", "Subtest 59.3: Combination formula string representation");

        std::cout << "[PASS] Test 59: Load Cases & Combinations Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 60: Coordinate Transformation (LoadResolver)
    // =========================================================================
    {
        std::cout << "\n--- TEST 60: Coordinate Transformation (LoadResolver) ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(5.0, 0.0, 0.0); // Poutre horizontale selon X

        Section sec = Section::rectangular(0.30, 0.50, "R30x50");
        Material mat = Material::concreteC25_30();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);

        // 1. Charge verticale globale Z (-10 kN/m) sur barre horizontale selon X
        MemberLoad mlGlobalZ = MemberLoad::uniform(b1, 1, -10.0, LoadDirection::GlobalZ);
        ResolvedMemberLoad resZ = LoadResolver::resolveMemberLoadLocal(model, mlGlobalZ);

        TEST_CHECK(approxEqual(resZ.wx, 0.0), "Subtest 60.1: Axial local force wx is 0");
        TEST_CHECK(approxEqual(resZ.wy, 0.0), "Subtest 60.1: Transverse local force wy is 0");
        // Local z correspond à Z global pour une barre selon X
        TEST_CHECK(approxEqual(resZ.wz, -10.0), "Subtest 60.1: Transverse local force wz is -10 kN/m");

        // 2. Barre inclinée à 45° dans le plan X-Z
        int n3 = model.addNode(0.0, 0.0, 0.0);
        int n4 = model.addNode(4.0, 0.0, 4.0); // L = sqrt(32) ~= 5.65685 m
        int b2 = model.addBar(n3, n4, sec, mat, BarRole::Beam);

        MemberLoad mlInclined = MemberLoad::uniform(b2, 1, -10.0, LoadDirection::GlobalZ);
        ResolvedMemberLoad resInc = LoadResolver::resolveMemberLoadLocal(model, mlInclined);

        // Décomposition selon angle 45° : wx = -10 * sin(45°) ~= -7.071, wz = -10 * cos(45°) ~= -7.071
        double expectedComp = -10.0 * std::sin(M_PI / 4.0);
        TEST_CHECK(approxEqual(resInc.wx, expectedComp, 1e-3), "Subtest 60.2: Decomposed axial load wx");
        TEST_CHECK(approxEqual(resInc.wz, expectedComp, 1e-3), "Subtest 60.2: Decomposed transverse load wz");

        std::cout << "[PASS] Test 60: Coordinate Transformation (LoadResolver) Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 61: Pre-Analysis Structural Validation (LoadValidation)
    // =========================================================================
    {
        std::cout << "\n--- TEST 61: Structural Model Validation for OpenSees ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        Section sec = Section::rectangular(0.30, 0.50, "R30x50");
        Material mat = Material::concreteC25_30();
        model.addBar(n1, n2, sec, mat, BarRole::Beam);

        // 1. Modèle sans aucun appui -> doit produire une erreur
        ValidationReport rep1 = LoadValidation::validate(model);
        TEST_CHECK(rep1.hasErrors(), "Subtest 61.1: Model without boundary conditions produces error");

        // 2. Fixation d'un appui
        auto* node1 = model.getNode(n1);
        if (node1) node1->setSupportType(SupportType::Fixed);

        // 3. Charge orpheline (sur nœud inexistant 999)
        NodalLoad orphanLoad(0, 999, 1, 0, 0, -50.0);
        model.loadManager().addNodalLoad(orphanLoad);

        ValidationReport rep2 = LoadValidation::validate(model);
        TEST_CHECK(rep2.hasErrors(), "Subtest 61.2: Orphan load produces error");

        // 4. Correction de la charge et validation complète
        model.loadManager().clear();
        model.loadManager().initializeEurocodeDefaults();
        NodalLoad validLoad(0, n2, 1, 0, 0, -25.0);
        model.loadManager().addNodalLoad(validLoad);

        ValidationReport rep3 = LoadValidation::validate(model);
        TEST_CHECK(!rep3.hasErrors(), "Subtest 61.3: Valid structure passes validation with 0 errors");

        std::cout << "[PASS] Test 61: Model Validation Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 62: OpenSees Tcl Script Generation (OpenSeesAdapter)
    // =========================================================================
    {
        std::cout << "\n--- TEST 62: OpenSees Tcl Script Generation ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(5.0, 0.0, 0.0);
        int n3 = model.addNode(5.0, 0.0, 3.0);

        auto* node1 = model.getNode(n1);
        if (node1) node1->setSupportType(SupportType::Fixed);

        Section sec = Section::rectangular(0.30, 0.50, "R30x50");
        Material mat = Material::concreteC25_30();

        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);
        int c1 = model.addColumn(n2, n3, sec, mat);
        TEST_CHECK(b1 > 0 && c1 > 0, "Subtest 62.0: Elements created");

        model.loadManager().initializeEurocodeDefaults();

        // Ajout d'une charge uniforme et d'une force ponctuelle
        MemberLoad udl = MemberLoad::uniform(b1, 1, -15.0, LoadDirection::GlobalZ, "UDL_Beam");
        model.loadManager().addMemberLoad(udl);
        NodalLoad pt = NodalLoad(0, n3, 2, 20.0, 0.0, 0.0, 0, 0, 0, LoadCoordSystem::Global, "Wind_N3");
        model.loadManager().addNodalLoad(pt);

        OpenSeesAdapterOptions opts;
        opts.includeAnalysisCommands = true;
        opts.includeSelfWeight = true;

        std::string tclScript = OpenSeesAdapter::generateScript(model, opts);

        // Vérifications de la syntaxe Tcl générée
        TEST_CHECK(tclScript.find("-ndm 3 -ndf 6") != std::string::npos,
                   "Subtest 62.1: Model 3D 6DOF statement present");
        TEST_CHECK(tclScript.find("node 1") != std::string::npos,
                   "Subtest 62.2: Node 1 coordinates present");
        TEST_CHECK(tclScript.find("fix 1 1 1 1 1 1") != std::string::npos,
                   "Subtest 62.3: Fixed base condition present");
        TEST_CHECK(tclScript.find("geomTransf Linear") != std::string::npos,
                   "Subtest 62.4: Geometric transformation present");
        TEST_CHECK(tclScript.find("element elasticBeamColumn") != std::string::npos,
                   "Subtest 62.5: Elastic beam-column element present");
        TEST_CHECK(tclScript.find("pattern Plain") != std::string::npos,
                   "Subtest 62.6: Load pattern present");
        TEST_CHECK(tclScript.find("eleLoad -ele") != std::string::npos,
                   "Subtest 62.7: Member load eleLoad command present");
        TEST_CHECK(tclScript.find("load 3 20 0 0 0 0 0") != std::string::npos, // 17 chiffres significatifs (plus de format fixe)
                   "Subtest 62.8: Nodal load command present");
        TEST_CHECK(tclScript.find("analysis Static") != std::string::npos,
                   "Subtest 62.9: Static analysis solver block present");
        TEST_CHECK(tclScript.find("analyze 1") != std::string::npos,
                   "Subtest 62.10: Solve command analyze 1 present");

        // Test d'export vers fichier temporaire
        std::string testPath = "test_opensees_export.tcl";
        bool expOk = OpenSeesAdapter::exportToFile(model, testPath, opts);
        TEST_CHECK(expOk && std::filesystem::exists(testPath), "Subtest 62.11: Export to file succeeds");
        std::filesystem::remove(testPath);

        std::cout << "[PASS] Test 62: OpenSees Tcl Script Generation Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 63: Full Undo/Redo & Model Snapshots for Load System
    // =========================================================================
    {
        std::cout << "\n--- TEST 63: Full Undo/Redo & Snapshots for Loads ---" << std::endl;

        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(6.0, 0.0, 0.0);

        Section sec = Section::rectangular(0.30, 0.50, "R30x50");
        Material mat = Material::concreteC25_30();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);
        TEST_CHECK(b1 > 0, "Subtest 63.0: Beam created");

        // 1. Ajout de charges initiales
        model.loadManager().initializeEurocodeDefaults();
        int nlId1 = model.loadManager().addNodalLoad(NodalLoad(0, n1, 1, 0, 0, -30.0, 0, 0, 0, LoadCoordSystem::Global, "NL_Base"));
        int mlId1 = model.loadManager().addMemberLoad(MemberLoad::uniform(b1, 1, -18.0, LoadDirection::GlobalZ, "UDL_1"));

        TEST_CHECK(model.loadManager().nodalLoads().size() == 1, "Subtest 63.1: 1 nodal load before snapshot");
        TEST_CHECK(model.loadManager().memberLoads().size() == 1, "Subtest 63.1: 1 member load before snapshot");

        // 2. Prise du snapshot d'état
        auto snap = model.createSnapshot();

        // 3. Modification de l'état : ajout d'une nouvelle charge après le snapshot
        int nlId2 = model.loadManager().addNodalLoad(NodalLoad(0, n2, 2, 0, 0, -100.0, 0, 0, 0, LoadCoordSystem::Global, "NL_New"));
        TEST_CHECK(nlId2 != nlId1, "Subtest 63.2: New load has distinct ID");
        TEST_CHECK(model.loadManager().nodalLoads().size() == 2, "Subtest 63.3: 2 nodal loads in model");

        // 4. Restauration de l'état via le snapshot
        model.applySnapshotData(snap);

        // Vérification de la restauration exacte
        TEST_CHECK(model.loadManager().nodalLoads().size() == 1, "Subtest 63.4: Restored exactly 1 nodal load");
        auto* restoredNl = model.loadManager().getNodalLoad(nlId1);
        TEST_CHECK(restoredNl != nullptr && approxEqual(restoredNl->fz(), -30.0) && restoredNl->name() == "NL_Base",
                   "Subtest 63.4: Restored nodal load values match original snapshot");

        TEST_CHECK(model.loadManager().memberLoads().size() == 1, "Subtest 63.5: Restored exactly 1 member load");
        auto* restoredMl = model.loadManager().getMemberLoad(mlId1);
        TEST_CHECK(restoredMl != nullptr && approxEqual(restoredMl->q1(), -18.0) && restoredMl->name() == "UDL_1",
                   "Subtest 63.5: Restored member load values match original snapshot");

        // La charge ajoutée après le snapshot ne doit plus exister
        TEST_CHECK(model.loadManager().getNodalLoad(nlId2) == nullptr,
                   "Subtest 63.6: Non-snapshot load is gone after snapshot restoration");

        std::cout << "[PASS] Test 63: Load Snapshots & Undo/Redo Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 64: Cascade Load Deletions, ID Coexistence & Observer Routing
    // =========================================================================
    {
        std::cout << "\n--- TEST 64: Cascade Deletions, ID Coexistence & Observers ---" << std::endl;

        class MockObserver : public TSA::Model::IModelObserver
        {
        public:
            std::vector<int> nodalAdded, nodalRemoved;
            std::vector<int> memberAdded, memberRemoved;
            void onNodalLoadAdded(int loadId) override { nodalAdded.push_back(loadId); }
            void onNodalLoadRemoved(int loadId) override { nodalRemoved.push_back(loadId); }
            void onMemberLoadAdded(int loadId) override { memberAdded.push_back(loadId); }
            void onMemberLoadRemoved(int loadId) override { memberRemoved.push_back(loadId); }
        };

        Model model;
        MockObserver obs;
        model.addObserver(&obs);

        int n1 = model.addNode(0, 0, 0);
        int n2 = model.addNode(5, 0, 0);
        int n3 = model.addNode(5, 0, 3);
        int b1 = model.addBar(n1, n2, Section::rectangular(0.3, 0.5), Material::concreteC25_30(), BarRole::Beam);
        int tr1 = model.addTrussMember(n2, n3, 0.1);

        // 1. Coexistence of Nodal Load ID=1 and Member Load ID=1
        int nl1 = model.loadManager().addNodalLoad(NodalLoad(0, n1, 1, 0, 0, -50.0, 0, 0, 0, LoadCoordSystem::Global, "NL1"));
        int ml1 = model.loadManager().addMemberLoad(MemberLoad::uniform(b1, 1, -10.0, LoadDirection::GlobalZ, "ML1", MemberTargetType::Beam));
        int mlTruss = model.loadManager().addMemberLoad(MemberLoad::uniform(tr1, 1, -5.0, LoadDirection::GlobalZ, "ML_Truss", MemberTargetType::Truss));

        TEST_CHECK(nl1 == 1 && ml1 == 1, "Subtest 64.1: NodalLoad and MemberLoad both have ID 1 without conflict");
        TEST_CHECK(model.loadManager().getNodalLoad(1) != nullptr, "Subtest 64.1: NodalLoad #1 exists");
        TEST_CHECK(model.loadManager().getMemberLoad(1) != nullptr, "Subtest 64.1: MemberLoad #1 exists");

        // Observers notify without ambiguity
        model.notifyNodalLoadAdded(nl1);
        model.notifyMemberLoadAdded(ml1);
        TEST_CHECK(obs.nodalAdded.size() == 1 && obs.nodalAdded[0] == 1, "Subtest 64.2: Observer received nodalAdded(1)");
        TEST_CHECK(obs.memberAdded.size() == 1 && obs.memberAdded[0] == 1, "Subtest 64.2: Observer received memberAdded(1)");

        // 2. Cascade deletion of loads when a beam is removed
        bool remBeam = model.removeBeam(b1);
        TEST_CHECK(remBeam, "Subtest 64.3: Beam removed");
        TEST_CHECK(model.loadManager().getMemberLoad(ml1) == nullptr,
                   "Subtest 64.3: MemberLoad on deleted beam was automatically cascade deleted");
        TEST_CHECK(obs.memberRemoved.size() == 1 && obs.memberRemoved[0] == ml1,
                   "Subtest 64.3: Observer received memberRemoved notification for cascade deleted load");

        // 3. Cascade deletion of loads when a node is removed
        bool remNode = model.removeNode(n1);
        TEST_CHECK(remNode, "Subtest 64.4: Node removed");
        TEST_CHECK(model.loadManager().getNodalLoad(nl1) == nullptr,
                   "Subtest 64.4: NodalLoad on deleted node was automatically cascade deleted");
        TEST_CHECK(obs.nodalRemoved.size() == 1 && obs.nodalRemoved[0] == nl1,
                   "Subtest 64.4: Observer received nodalRemoved notification for cascade deleted load");

        // 4. Cascade deletion when truss member is removed
        bool remTruss = model.removeTrussMember(tr1);
        TEST_CHECK(remTruss, "Subtest 64.5: Truss member removed");
        TEST_CHECK(model.loadManager().getMemberLoad(mlTruss) == nullptr,
                   "Subtest 64.5: MemberLoad on deleted truss member was cascade deleted");

        // 5. OpenSees Tcl generation with truss self-weight and equivalent end loads
        Model model2;
        int mN1 = model2.addNode(0, 0, 0);
        int mN2 = model2.addNode(4, 0, 3);
        model2.getNode(mN1)->setSupportType(SupportType::Fixed);
        model2.getNode(mN2)->setSupportType(SupportType::Pinned);
        int mTr = model2.addTrussMember(mN1, mN2, 0.1);
        model2.loadManager().initializeEurocodeDefaults();
        model2.loadManager().addMemberLoad(MemberLoad::uniform(mTr, 1, -10.0, LoadDirection::GlobalZ, "TrussLoad"));

        OpenSeesOptions opt;
        opt.includeSelfWeight = true;
        opt.includeAnalysisCommands = true;
        opt.targetLoadCaseId = 1;
        std::string script = OpenSeesAdapter::generateTclScript(model2, opt);
        TEST_CHECK(script.find("Treillis #" + std::to_string(mTr)) != std::string::npos,
                   "Subtest 64.6: OpenSees generated equivalent end loads for truss member");
        TEST_CHECK(script.find("Poids propre treillis #" + std::to_string(mTr)) != std::string::npos,
                   "Subtest 64.6: OpenSees generated self-weight for truss member");

        model.removeObserver(&obs);
        std::cout << "[PASS] Test 64: Cascade Deletions & ID Coexistence Validated!" << std::endl;
        passed++;
    }

    // =========================================================================
    // TEST 65: True-Direction Force Vectors, Right-Hand Rule Moments & 3D Load Orientations
    // =========================================================================
    {
        std::cout << "\n--- TEST 65: True-Direction Force Vectors & Right-Hand Rule Moments ---" << std::endl;

        // 1. Vecteur force global avec signe préservé (+Z soulèvement, -Z gravité)
        NodalLoad nlDown(1, 1, 1, 0.0, 0.0, -25.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "Gravity");
        NodalLoad nlUp(2, 1, 1, 0.0, 0.0, +15.0, 0.0, 0.0, 0.0, LoadCoordSystem::Global, "Uplift");

        TEST_CHECK(nlDown.fz() == -25.0, "Subtest 65.1: Negative Fz preserved");
        TEST_CHECK(nlUp.fz() == +15.0, "Subtest 65.1: Positive Fz preserved");

        // Direction unitaire des forces
        gp_Vec vecDown(nlDown.fx(), nlDown.fy(), nlDown.fz());
        gp_Vec vecUp(nlUp.fx(), nlUp.fy(), nlUp.fz());
        TEST_CHECK(vecDown.Z() < 0.0, "Subtest 65.2: Gravity force vector points strictly downward in Z");
        TEST_CHECK(vecUp.Z() > 0.0, "Subtest 65.2: Uplift force vector points strictly upward in Z");

        // 2. Moments 3D et règle de la main droite (Right-Hand Rule)
        NodalLoad nlMomZ(3, 2, 1, 0.0, 0.0, 0.0, 0.0, 0.0, 50.0, LoadCoordSystem::Global, "MomZ_Pos");
        NodalLoad nlMomZNeg(4, 2, 1, 0.0, 0.0, 0.0, 0.0, 0.0, -30.0, LoadCoordSystem::Global, "MomZ_Neg");
        TEST_CHECK(approxEqual(nlMomZ.resultantMoment(), 50.0), "Subtest 65.3: Moment resultant magnitude");
        TEST_CHECK(approxEqual(nlMomZNeg.resultantMoment(), 30.0), "Subtest 65.3: Negative moment resultant magnitude");

        // L'axe de rotation du moment positif Mz est (0, 0, 1) -> rotation de +X vers +Y
        gp_Vec axisMomPos(nlMomZ.mx(), nlMomZ.my(), nlMomZ.mz());
        axisMomPos.Normalize();
        TEST_CHECK(approxEqual(axisMomPos.X(), 0.0) && approxEqual(axisMomPos.Y(), 0.0) && approxEqual(axisMomPos.Z(), 1.0),
                   "Subtest 65.4: Positive Mz axis aligned with +Z (right-hand rule CCW)");

        // L'axe de rotation du moment négatif Mz est (0, 0, -1) -> rotation inversée
        gp_Vec axisMomNeg(nlMomZNeg.mx(), nlMomZNeg.my(), nlMomZNeg.mz());
        axisMomNeg.Normalize();
        TEST_CHECK(approxEqual(axisMomNeg.Z(), -1.0),
                   "Subtest 65.4: Negative Mz axis aligned with -Z (clockwise)");

        // 3. Charge combinée Force + Moment sans ambiguïté
        NodalLoad nlCombined(5, 3, 1, 10.0, 0.0, -40.0, 0.0, 15.0, 0.0, LoadCoordSystem::Global, "Combined");
        TEST_CHECK(nlCombined.resultantForce() > 0.0, "Subtest 65.5: Combined load has valid force resultant");
        TEST_CHECK(nlCombined.resultantMoment() > 0.0, "Subtest 65.5: Combined load has valid moment resultant");
        TEST_CHECK(approxEqual(nlCombined.resultantForce(), std::sqrt(10.0*10.0 + 40.0*40.0)), "Subtest 65.5: Force resultant value");
        TEST_CHECK(approxEqual(nlCombined.resultantMoment(), 15.0), "Subtest 65.5: Moment resultant value");

        // 4. LoadResolver : projection d'une charge répartie positive GlobalZ (+Z soulèvement) sur poutre horizontale
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(6.0, 0.0, 0.0);
        int beamId = model.addBeam(n1, n2, 0.3, 0.5);

        MemberLoad mlUplift = MemberLoad::uniform(beamId, 1, +12.0, LoadDirection::GlobalZ, "UpliftBeam");
        auto compUplift = LoadResolver::resolveMemberLoadToLocal(mlUplift, model);
        // Sur une poutre horizontale orientée selon +X, l'axe vertical Z correspond à l'axe local Z
        // Le signe positif doit être préservé (+12.0) et non forcé négatif (-12.0)
        TEST_CHECK(compUplift.wz > 0.0 && approxEqual(compUplift.wz, 12.0),
                   "Subtest 65.6: Uplift load (+Z) resolved to positive local wz, not forced negative");

        MemberLoad mlGravity = MemberLoad::uniform(beamId, 1, -20.0, LoadDirection::GlobalZ, "GravBeam");
        auto compGrav = LoadResolver::resolveMemberLoadToLocal(mlGravity, model);
        TEST_CHECK(compGrav.wz < 0.0 && approxEqual(compGrav.wz, -20.0),
                   "Subtest 65.7: Gravity load (-Z) resolved to negative local wz");

        // 5. Poteau vertical (0,0,0) -> (0,0,4) avec charge de vent GlobalX
        int n3 = model.addNode(0.0, 0.0, 4.0);
        int colId = model.addColumn(n1, n3, 0.4, 0.4);
        MemberLoad mlWind = MemberLoad::uniform(colId, 1, 8.0, LoadDirection::GlobalX, LoadCoordSystem::Global, "WindCol", MemberTargetType::Column);
        auto compWind = LoadResolver::resolveMemberLoadToLocal(mlWind, model);
        // Sur un poteau vertical, le vent selon GlobalX est transversal
        double transversalMag = std::sqrt(compWind.wy * compWind.wy + compWind.wz * compWind.wz);
        TEST_CHECK(approxEqual(transversalMag, 8.0), "Subtest 65.8: Wind on vertical column is purely transversal of magnitude 8.0");
        TEST_CHECK(approxEqual(compWind.wx, 0.0), "Subtest 65.8: Wind on vertical column has zero axial component");

        // 6. Barre inclinée (0,0,0) -> (3,4,0) : longueur = 5.0
        int n4 = model.addNode(3.0, 4.0, 0.0);
        int diagId = model.addBeam(n1, n4, 0.2, 0.2);
        MemberLoad mlDiagY = MemberLoad::uniform(diagId, 1, 10.0, LoadDirection::GlobalY, "GlobalYDiag");
        auto compDiag = LoadResolver::resolveMemberLoadToLocal(mlDiagY, model);
        // Le vecteur de la barre est (0.6, 0.8, 0).
        // Le produit scalaire du vecteur (0, 10, 0) avec dirX (0.6, 0.8, 0) est 8.0 (wx axial).
        TEST_CHECK(approxEqual(compDiag.wx, 8.0), "Subtest 65.9: Axial component of GlobalY on 3-4-5 inclined member is 8.0");

        std::cout << "[PASS] Test 65: True-Direction Force Vectors & Right-Hand Rule Moments Validated!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 209 : représentation des charges réparties (MemberLoadGlyph) — densité, forme, sens
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Geometry;
        using TSA::Model::MemberLoad;
        const gp_Pnt a(0, 0, 3), b(6, 0, 3);          // poutre horizontale de 6 m
        MemberLoadGlyphSettings st;
        st.arrowLength = 0.6;
        st.spacing = 0.3;
        auto regular = [](const MemberLoadGlyph& g) {
            for (std::size_t i = 2; i < g.arrows.size(); ++i)
                if (std::abs((g.arrows[i].position - g.arrows[i - 1].position) - (g.arrows[1].position - g.arrows[0].position)) > 1e-9)
                    return false;
            return true;
        };
        auto down = [](const LoadArrow& r) { return r.direction.Z() < -0.999999; };

        // Uniforme positive (Gravité) : 21 flèches régulières, toutes égales et vers le bas.
        const MemberLoad g1 = MemberLoad::uniform(1, 1, 1, 10.0);
        const MemberLoadGlyph u = buildMemberLoadGlyph(g1, a, b, 0.0, st);
        TEST_CHECK(u.kind == MemberLoadGlyphKind::Distributed && u.arrows.size() == 21, "Test 209: uniforme 6 m → 21 flèches (" << u.arrows.size() << ")");
        TEST_CHECK(regular(u), "Test 209: espacement régulier");
        TEST_CHECK(std::all_of(u.arrows.begin(), u.arrows.end(), [&](const LoadArrow& r) { return std::abs(r.length - 0.6) < 1e-12 && down(r); }),
                   "Test 209: uniforme = longueurs égales, flèches vers le bas");
        TEST_CHECK(u.arrows.front().tip.Distance(a) < 1e-12 && u.arrows.back().tip.Distance(b) < 1e-12, "Test 209: pointes sur la barre, de a à b");
        bool parallel = true;
        for (const auto& p : u.envelope) parallel &= std::abs(p.Z() - 3.6) < 1e-12;
        TEST_CHECK(parallel && u.envelope.size() == 21, "Test 209: enveloppe parallèle à la barre (charge uniforme)");

        // Uniforme négative : Gravité reste vers le bas (module) ; selon Global Z, le signe donne le sens.
        TEST_CHECK(down(buildMemberLoadGlyph(MemberLoad::uniform(1, 1, 1, -10.0), a, b, 0.0, st).arrows.front()),
                   "Test 209: Gravité négative toujours vers le bas (convention des moteurs)");
        const MemberLoadGlyph up = buildMemberLoadGlyph(MemberLoad::uniform(1, 1, 1, -10.0, TSA::Model::LoadDirection::GlobalZ), a, b, 0.0, st);
        TEST_CHECK(up.arrows.front().direction.Z() < -0.999999, "Test 209: Global Z négative → vers le bas");
        TEST_CHECK(buildMemberLoadGlyph(MemberLoad::uniform(1, 1, 1, 10.0, TSA::Model::LoadDirection::GlobalZ), a, b, 0.0, st)
                       .arrows.front().direction.Z() > 0.999999, "Test 209: Global Z positive → vers le haut");

        // Triangulaire 0 → 12 sur toute la barre : pas de flèche en a, longueur ∝ intensité, maximum en b.
        const MemberLoadGlyph tri = buildMemberLoadGlyph(MemberLoad::trapezoidal(1, 1, 1, 0.0, 12.0, 0.0, 6.0), a, b, 0.0, st);
        bool proportional = true;
        for (const auto& r : tri.arrows) proportional &= std::abs(r.length - 0.6 * std::abs(r.intensity) / 12.0) < 1e-9;
        TEST_CHECK(tri.arrows.size() == 20 && tri.envelope.size() == 21 && tri.envelope.front().Distance(a) < 1e-12,
                   "Test 209: triangulaire : extrémité nulle sans flèche, enveloppe partant de la barre");
        TEST_CHECK(proportional && std::abs(tri.arrows.back().length - 0.6) < 1e-12, "Test 209: triangulaire : longueur ∝ intensité locale");

        // Trapézoïdale 4 → 12 sur [1, 4] m : intervalle respecté, longueurs de 0,2 à 0,6.
        const MemberLoadGlyph trap = buildMemberLoadGlyph(MemberLoad::trapezoidal(1, 1, 1, 4.0, 12.0, 1.0, 4.0), a, b, 0.0, st);
        TEST_CHECK(std::abs(trap.start - 1.0) < 1e-12 && std::abs(trap.end - 4.0) < 1e-12 && trap.arrows.size() == 11,
                   "Test 209: trapézoïdale partielle : [1, 4] m, 11 flèches");
        TEST_CHECK(std::abs(trap.arrows.front().length - 0.2) < 1e-9 && std::abs(trap.arrows.back().length - 0.6) < 1e-9
                       && std::abs(trap.arrows.front().tip.X() - 1.0) < 1e-12 && std::abs(trap.arrows.back().tip.X() - 4.0) < 1e-12,
                   "Test 209: trapézoïdale : longueurs 4/12 → 12/12 aux bornes de l'intervalle");

        // Changement de signe (Global Z, −6 → +6) : zéro inséré, sens des flèches inversé à mi-portée.
        const MemberLoadGlyph flip = buildMemberLoadGlyph(
            MemberLoad::trapezoidal(1, 1, 1, -6.0, 6.0, 0.0, 6.0, TSA::Model::LoadDirection::GlobalZ), a, b, 0.0, st);
        TEST_CHECK(flip.arrows.front().direction.Z() < 0.0 && flip.arrows.back().direction.Z() > 0.0, "Test 209: sens suivant le signe local");

        // Barre inclinée, repère local z : flèches perpendiculaires à la barre, sens = LoadResolver.
        const gp_Pnt c(0, 0, 0), d(4, 0, 3);
        const MemberLoad loc = MemberLoad::uniform(1, 1, 1, -5.0, TSA::Model::LoadDirection::LocalZ, TSA::Model::LoadCoordSystem::Local);
        const MemberLoadGlyph incl = buildMemberLoadGlyph(loc, c, d, 0.0, st);
        const gp_Vec ref = TSA::Analysis::LoadResolver::memberLoadVector(loc, -5.0, c, d, 0.0);
        TEST_CHECK(incl.arrows.size() == 18 && std::abs(gp_Vec(incl.arrows[3].direction).Dot(gp_Vec(c, d).Normalized())) < 1e-9
                       && gp_Vec(incl.arrows[3].direction).IsParallel(ref, 1e-9) && gp_Vec(incl.arrows[3].direction).Dot(ref) > 0.0,
                   "Test 209: barre inclinée, local z : perpendiculaire, sens du résolveur (" << incl.arrows.size() << " flèches)");

        // Global X sur poutre horizontale : flèches axiales (pas de flèche dégénérée).
        const MemberLoadGlyph ax = buildMemberLoadGlyph(MemberLoad::uniform(1, 1, 1, 3.0, TSA::Model::LoadDirection::GlobalX), a, b, 0.0, st);
        TEST_CHECK(ax.arrows.front().direction.X() > 0.999999, "Test 209: charge selon X");

        // Intensité nulle : aucune flèche ; segment très court : 2 flèches sans superposition ; barre très longue : plafond.
        TEST_CHECK(buildMemberLoadGlyph(MemberLoad::uniform(1, 1, 1, 0.0), a, b, 0.0, st).arrows.empty(), "Test 209: intensité nulle → aucune flèche");
        const MemberLoadGlyph shortSeg = buildMemberLoadGlyph(MemberLoad::trapezoidal(1, 1, 1, 5.0, 5.0, 2.0, 2.1), a, b, 0.0, st);
        TEST_CHECK(shortSeg.arrows.size() == 2 && shortSeg.arrows[0].tip.Distance(shortSeg.arrows[1].tip) > 0.099,
                   "Test 209: segment de 0,1 m : 2 flèches distinctes");
        const MemberLoadGlyph longBar = buildMemberLoadGlyph(g1, gp_Pnt(0, 0, 0), gp_Pnt(500, 0, 0), 0.0, st);
        TEST_CHECK(longBar.arrows.size() == 60 && regular(longBar), "Test 209: barre de 500 m : plafond de 60 flèches régulières");
        TEST_CHECK(distributedStationCount(6.0, st) == 21 && distributedStationCount(0.001, st) == 1, "Test 209: règle de densité");

        // Force ponctuelle et moment réparti.
        const MemberLoadGlyph pt = buildMemberLoadGlyph(MemberLoad::pointOnMember(1, 1, 1, 20.0, 2.5), a, b, 0.0, st);
        TEST_CHECK(pt.kind == MemberLoadGlyphKind::Point && pt.arrows.size() == 1 && std::abs(pt.arrows[0].tip.X() - 2.5) < 1e-12 && down(pt.arrows[0]),
                   "Test 209: force ponctuelle : une flèche à x = 2,5 m");
        MemberLoad mm = g1;
        mm.setType(TSA::Model::LoadType::MemberMoment);
        TEST_CHECK(buildMemberLoadGlyph(mm, a, b, 0.0, st).arrows.empty(), "Test 209: moment réparti : aucune flèche de force");
        std::cout << "[PASS] Test 209: Représentation des charges réparties" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 210 : règles métier communes (intervalle, intensité, validation, sens unique) et calcul
    // -------------------------------------------------------------------------
    {
        using TSA::Model::MemberLoad;
        using TSA::Model::LoadDirection;
        using TSA::Model::LoadCoordSystem;
        const MemberLoad full = MemberLoad::uniform(1, 1, 1, 10.0);
        TEST_CHECK(full.coversFullLength(6.0) && full.appliedRange(6.0).second == 6.0, "Test 210: uniforme ancien format (x1 = x2 = 0) = toute la barre");
        MemberLoad part = full;
        part.setX1(1.0);
        part.setX2(3.0);
        TEST_CHECK(!part.coversFullLength(6.0) && part.appliedRange(6.0).first == 1.0, "Test 210: uniforme partielle [1, 3]");
        const MemberLoad lin = MemberLoad::trapezoidal(1, 1, 1, 0.0, 12.0, 0.0, 0.0);
        TEST_CHECK(std::abs(lin.intensityAt(3.0, 6.0) - 6.0) < 1e-12, "Test 210: x2 = 0 → jusqu'au nœud j, intensité interpolée");

        TEST_CHECK(full.validate(6.0).empty() && part.validate(6.0).empty(), "Test 210: charges valides");
        MemberLoad bad = part;
        bad.setX2(0.5);
        TEST_CHECK(!bad.validate(6.0).empty(), "Test 210: fin avant le début refusée");
        bad = part;
        bad.setX2(7.0);
        TEST_CHECK(!bad.validate(6.0).empty(), "Test 210: intervalle hors de la barre refusé");
        bad = full;
        bad.setQ1(std::numeric_limits<double>::quiet_NaN());
        TEST_CHECK(!bad.validate(6.0).empty(), "Test 210: valeur non finie refusée");
        TEST_CHECK(!MemberLoad::uniform(1, 1, 1, 0.0).validate(6.0).empty(), "Test 210: intensité nulle refusée");
        TEST_CHECK(MemberLoad::coordSystemFor(LoadDirection::LocalY) == LoadCoordSystem::Local
                       && MemberLoad::coordSystemFor(LoadDirection::Gravity) == LoadCoordSystem::Global,
                   "Test 210: repère déduit de la direction");

        // Sens unique : chemin modèle = chemin calcul (auparavant −q contre −|q| pour une direction non
        // locale en repère local).
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(4, 0, 3);
        const int bar = m.addBar(n1, n2, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        MemberLoad odd(1, bar, 1, TSA::Model::LoadType::MemberUniform, -8.0, -8.0, LoadDirection::Gravity, LoadCoordSystem::Local);
        const auto viaModel = TSA::Analysis::LoadResolver::resolveMemberLoadToLocal(odd, m);
        const auto viaSnapshot = TSA::Analysis::LoadResolver::resolveMemberLoadToLocal(odd, TSA::Analysis::CalculationSnapshot::capture(m));
        TEST_CHECK(viaModel.wz == viaSnapshot.wz && viaModel.wz == -8.0, "Test 210: modèle et calcul lisent le même sens");
        const MemberLoad gl = MemberLoad::uniform(1, bar, 1, 5.0, LoadDirection::GlobalX);
        const auto comp = TSA::Analysis::LoadResolver::resolveMemberLoadToLocal(gl, m);
        const gp_Vec back = TSA::Analysis::LoadResolver::localVectorToGlobal(comp.wx, comp.wy, comp.wz, gp_Pnt(0, 0, 0), gp_Pnt(4, 0, 3));
        TEST_CHECK(back.IsEqual(TSA::Analysis::LoadResolver::memberLoadVector(gl, 5.0, gp_Pnt(0, 0, 0), gp_Pnt(4, 0, 3)), 1e-12, 1e-12),
                   "Test 210: composantes locales = vecteur global (barre inclinée)");

        std::cout << "[PASS] Test 210: Règles communes des charges sur barre" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 211 : application au modèle (tout ou rien, doublons, Annuler), modification, sauvegarde
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Model;
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0), n3 = m.addNode(5, 0, 3);
        const int b1 = m.addBar(n1, n2, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int b2 = m.addBar(n2, n3, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        MemberLoad proto = MemberLoad::trapezoidal(0, 0, lc, 2.0, 8.0, 1.0, 4.0);
        std::string err;

        // Tout ou rien : x2 = 4 m dépasse la barre de 3 m → aucune charge ajoutée.
        TEST_CHECK(applyMemberLoad(m, proto, { { b1 }, { b2 } }, "test", &err).empty() && m.loadManager().memberLoads().empty() && !err.empty(),
                   "Test 211: une cible invalide → aucune application partielle (" << err << ")");
        proto.setX2(2.5);
        const auto ids = applyMemberLoad(m, proto, { { b1 }, { b2 } }, "Charge sur 2 barres", &err);
        TEST_CHECK(ids.size() == 2 && m.loadManager().memberLoads().size() == 2, "Test 211: appliquée aux deux barres");
        MemberLoad again = proto;
        again.setElementId(b1);
        TEST_CHECK(hasEquivalentMemberLoad(m, again), "Test 211: doublon détecté");
        again.setQ2(9.0);
        TEST_CHECK(!hasEquivalentMemberLoad(m, again), "Test 211: charge différente : pas un doublon");
        TEST_CHECK(applyMemberLoad(m, proto, { { 999 } }, "x", &err).empty(), "Test 211: barre supprimée refusée");

        TEST_CHECK(m.undo() && m.loadManager().memberLoads().empty(), "Test 211: une seule entrée Annuler retire les deux charges");
        TEST_CHECK(m.redo() && m.loadManager().memberLoads().size() == 2, "Test 211: Rétablir");

        // Modification puis suppression avec Annuler.
        m.pushUndoState("modifier");
        m.loadManager().getMemberLoad(ids[0])->setQ2(10.0);
        m.notifyMemberLoadModified(ids[0]);
        TEST_CHECK(m.loadManager().getMemberLoad(ids[0])->q2() == 10.0, "Test 211: modification");
        m.pushUndoState("supprimer");
        m.loadManager().removeMemberLoad(ids[1]);
        TEST_CHECK(m.undo() && m.loadManager().getMemberLoad(ids[1]) != nullptr, "Test 211: suppression annulée");

        // Sauvegarde et réouverture : forme, intervalle, direction, repère conservés.
        const QString file = QDir::temp().filePath("tsa_test_211.tsa");
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 211: enregistrement");
        Model back;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, back, nullptr, &err), "Test 211: réouverture");
        const MemberLoad* r = back.loadManager().getMemberLoad(ids[0]);
        TEST_CHECK(r && r->type() == LoadType::MemberLinear && r->q1() == 2.0 && r->q2() == 10.0 && r->x1() == 1.0 && r->x2() == 2.5
                       && r->direction() == LoadDirection::Gravity && r->coordSystem() == LoadCoordSystem::Global,
                   "Test 211: charge relue à l'identique");
        QFile::remove(file);
        std::cout << "[PASS] Test 211: Application, modification et sauvegarde des charges sur barre" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 219 : suppression des charges sélectionnées (Suppr dans la vue 3D, BUG-065)
    // -------------------------------------------------------------------------
    {
        class RemovalObserver : public TSA::Model::IModelObserver
        {
        public:
            std::vector<int> nodalRemoved, memberRemoved;
            void onNodalLoadRemoved(int loadId) override { nodalRemoved.push_back(loadId); }
            void onMemberLoadRemoved(int loadId) override { memberRemoved.push_back(loadId); }
        };
        Model model;
        RemovalObserver obs;
        model.addObserver(&obs);
        const int n1 = model.addNode(0, 0, 0);
        const int n2 = model.addNode(6, 0, 0);
        const int b1 = model.addBar(n1, n2, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int nl = model.loadManager().addNodalLoad(NodalLoad(0, n2, 1, 0, 0, -20.0, 0, 0, 0, LoadCoordSystem::Global, "NL"));
        const int ml = model.loadManager().addMemberLoad(MemberLoad::uniform(b1, 1, -5.0, LoadDirection::Gravity, "ML", MemberTargetType::Beam));
        const int keep = model.loadManager().addMemberLoad(MemberLoad::uniform(b1, 1, -2.0, LoadDirection::Gravity, "Keep", MemberTargetType::Beam));
        const bool couldUndo = model.canUndo();

        TEST_CHECK(removeLoads(model, {}, {}, "rien") == 0 && model.canUndo() == couldUndo,
                   "Test 219: sélection vide → modèle inchangé, pas d'entrée Annuler");
        TEST_CHECK(removeLoads(model, { 999 }, { 999 }, "inconnues") == 0 && model.canUndo() == couldUndo,
                   "Test 219: identifiants inconnus ignorés");

        const int removed = removeLoads(model, { nl, 999 }, { ml }, "Suppression de charge(s)");
        TEST_CHECK(removed == 2, "Test 219: deux charges supprimées");
        TEST_CHECK(!model.loadManager().getNodalLoad(nl) && !model.loadManager().getMemberLoad(ml),
                   "Test 219: charge nodale et charge sur barre retirées du modèle");
        TEST_CHECK(model.loadManager().getMemberLoad(keep) && model.getBeam(b1) && model.getNode(n2),
                   "Test 219: autres charges, barre et nœuds conservés");
        TEST_CHECK(obs.nodalRemoved == std::vector<int>{ nl } && obs.memberRemoved == std::vector<int>{ ml },
                   "Test 219: vue 3D notifiée (formes retirées)");
        TEST_CHECK(model.canUndo() && model.undo(), "Test 219: une entrée Annuler");
        TEST_CHECK(model.loadManager().getNodalLoad(nl) && model.loadManager().getMemberLoad(ml),
                   "Test 219: Annuler rétablit les deux charges");
        model.removeObserver(&obs);
        std::cout << "[PASS] Test 219: Suppression des charges sélectionnées (une entrée Annuler)" << std::endl;
        passed++;
    }

    return true;
}
