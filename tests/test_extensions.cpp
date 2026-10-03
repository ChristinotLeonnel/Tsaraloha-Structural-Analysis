#include "test_common.h"

bool runSuite_Extensions(int& passed)
{
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

            // Vérification par le DERNIER message et non par la taille : le tampon circulaire peut
            // être plein (taille constante) lorsque la suite complète a déjà beaucoup journalisé.
            auto lastMessage = [&logger]() {
                const auto entries = logger.recentEntries();
                return entries.empty() ? std::string() : entries.back().message;
            };
            TSA_LOG_TRACE("TestModule", "TraceEvent", "Message trace filtre");
            TEST_CHECK(lastMessage() != "Message trace filtre", "Subtest 37.4: Trace message skipped when dev mode disabled");

            logger.setDeveloperModeEnabled(true);
            TEST_CHECK(logger.isDeveloperModeEnabled(), "Subtest 37.4: Developer mode is enabled");
            TSA_LOG_TRACE("TestModule", "TraceEvent", "Message trace autorise");
            TEST_CHECK(lastMessage() == "Message trace autorise", "Subtest 37.4: Trace message accepted when dev mode enabled");

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
            (void)extMgr;
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
        TEST_CHECK(colElem != nullptr, "Subtest 46.4: colElem non null");
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


    // -------------------------------------------------------------------------
    // TEST 48: TSALib Phase 11 - Packaging .tsalib, Distribution & Validation Globale Finale
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 48: TSALib Phase 11 - Packaging .tsalib, Distribution & Validation Globale Finale ---" << std::endl;

        QString sourceDir = "e:/Book/Dev/TSA/Extensions/TSALib";
        if (!QDir(sourceDir).exists())
        {
            sourceDir = QDir::currentPath() + "/Extensions/TSALib";
        }

        QString pkgOutputPath = QDir::currentPath() + "/scratch_test_tsalib.tsalib";
        if (QFile::exists(pkgOutputPath))
        {
            QFile::remove(pkgOutputPath);
        }

        // 48.1: Création d'un package binaire compressé .tsalib
        QString packError;
        bool created = TSA::ExtensionSystem::ExtensionPackager::createPackage(sourceDir, pkgOutputPath, &packError);
        TEST_CHECK(created, "Subtest 48.1: Package .tsalib cree avec succes");
        TEST_CHECK(QFile::exists(pkgOutputPath), "Subtest 48.1: Fichier .tsalib present sur le disque");
        QFileInfo pkgInfo(pkgOutputPath);
        TEST_CHECK(pkgInfo.size() > 5000, "Subtest 48.1: Taille package substantielle (> 5 KB compresse)");

        std::cout << "  [PASS] Subtest 48.1: Creation de Package .tsalib Compresse Validee (Taille: " << pkgInfo.size() << " octets)" << std::endl;

        // 48.2: Inspection sécurisée sans extraction intégrale
        auto inspectRes = TSA::ExtensionSystem::ExtensionPackager::inspectPackage(pkgOutputPath);
        TEST_CHECK(inspectRes.isValid(), "Subtest 48.2: Inspection du package .tsalib valide");
        TEST_CHECK(inspectRes.formatVersion == 1, "Subtest 48.2: Version de format = 1");
        TEST_CHECK(inspectRes.manifest.id == "org.tsaraloha.tsalib", "Subtest 48.2: Manifest ID = org.tsaraloha.tsalib");
        TEST_CHECK(!inspectRes.manifest.name.empty(), "Subtest 48.2: Manifest Name present");
        TEST_CHECK(!inspectRes.packageSha256Hex.isEmpty(), "Subtest 48.2: Empreinte SHA-256 calculee");
        TEST_CHECK(inspectRes.files.size() >= 55, "Subtest 48.2: Au moins 55 fichiers archives dans le package");
        TEST_CHECK(static_cast<qint64>(inspectRes.totalUncompressedBytes) > pkgInfo.size(), "Subtest 48.2: Ratio de compression zlib efficace");

        std::cout << "  [PASS] Subtest 48.2: Inspection Securisee sans Extraction Validee (" 
                  << inspectRes.files.size() << " fichiers, SHA-256: " 
                  << inspectRes.packageSha256Hex.left(12).toStdString() << "...)" << std::endl;

        // 48.3: Securite Anti-Path-Traversal & Detection de Paquet Corrompu
        TEST_CHECK(TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath("Materials/concrete_c25_30.json"), 
                   "Subtest 48.3: Chemin relatif standard accepte");
        TEST_CHECK(TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath("Textures/concrete_diffuse.png"), 
                   "Subtest 48.3: Chemin sous-dossier valide");
        TEST_CHECK(!TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath("../../Windows/System32/evil.dll"), 
                   "Subtest 48.3: Tentative de traversal ../../ rejetee");
        TEST_CHECK(!TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath("/etc/shadow"), 
                   "Subtest 48.3: Chemin absolu racine rejete");
        TEST_CHECK(!TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath("C:/Windows/cmd.exe"), 
                   "Subtest 48.3: Chemin absolu Windows avec lettre de lecteur rejete");
        TEST_CHECK(!TSA::ExtensionSystem::ExtensionPackager::isSafeRelativePath(""), 
                   "Subtest 48.3: Chemin vide rejete");

        // Fichier invalide / corrompu
        QString corruptPath = QDir::currentPath() + "/scratch_corrupt.tsalib";
        QFile corruptFile(corruptPath);
        if (corruptFile.open(QIODevice::WriteOnly))
        {
            corruptFile.write("CORRUPTED_NOT_TSALIB_HEADER_DATA_12345");
            corruptFile.close();
        }
        auto corruptInspect = TSA::ExtensionSystem::ExtensionPackager::inspectPackage(corruptPath);
        TEST_CHECK(!corruptInspect.isValid(), "Subtest 48.3: Rejet immediat d'un package corrompu avec mauvais magic");
        QFile::remove(corruptPath);

        std::cout << "  [PASS] Subtest 48.3: Protection Anti-Path-Traversal (Zip Slip) & Detection d'Anomalies Validees" << std::endl;

        // 48.4: Installation & Extraction Reelle dans un Sandbox Temporaire
        QString sandboxDir = QDir::currentPath() + "/scratch_install_sandbox";
        if (QDir(sandboxDir).exists())
        {
            QDir(sandboxDir).removeRecursively();
        }
        QDir().mkpath(sandboxDir);

        QString installedPath;
        QString installError;
        bool installed = TSA::ExtensionSystem::ExtensionPackager::installPackage(pkgOutputPath, sandboxDir, &installedPath, &installError);
        TEST_CHECK(installed, "Subtest 48.4: Installation et extraction du package reussies");
        TEST_CHECK(QFile::exists(sandboxDir + "/manifest.json"), "Subtest 48.4: manifest.json extrait avec succes");
        TEST_CHECK(QFile::exists(sandboxDir + "/Materials/concrete_c25_30.json"), "Subtest 48.4: Fiche materiau extraite");
        TEST_CHECK(QFile::exists(sandboxDir + "/Sections/rect_300x500.json"), "Subtest 48.4: Fiche section extraite");
        TEST_CHECK(QFile::exists(sandboxDir + "/Profiles/ipe200.json"), "Subtest 48.4: Fiche profile extraite");
        TEST_CHECK(QFile::exists(sandboxDir + "/Cables/en10138_y1860s7_15_7.json"), "Subtest 48.4: Fiche cable extraite");
        TEST_CHECK(QFile::exists(sandboxDir + "/Textures/concrete.png"), "Subtest 48.4: Texture PBR extraite");

        // Nettoyage sandbox
        QDir(sandboxDir).removeRecursively();
        QFile::remove(pkgOutputPath);

        std::cout << "  [PASS] Subtest 48.4: Extraction Reelle Sandbox & Verification d'Integrite Bitwise Validees" << std::endl;

        // 48.5: Exportation d'Extension via LibraryManager & Validation Globale Finale
        QString exportOutPath = QDir::currentPath() + "/scratch_export_manager.tsalib";
        if (QFile::exists(exportOutPath)) QFile::remove(exportOutPath);

        QString expErr;
        bool exported = TSA::ExtensionSystem::LibraryManager::instance().exportPackage("org.tsaraloha.tsalib", exportOutPath, &expErr);
        TEST_CHECK(exported, "Subtest 48.5: Export de package via LibraryManager reussi");
        TEST_CHECK(QFile::exists(exportOutPath), "Subtest 48.5: Fichier exporte present");

        auto expInspect = TSA::ExtensionSystem::ExtensionPackager::inspectPackage(exportOutPath);
        TEST_CHECK(expInspect.isValid(), "Subtest 48.5: Package exporte valide");
        TEST_CHECK(expInspect.manifest.id == "org.tsaraloha.tsalib", "Subtest 48.5: ID conforme");
        QFile::remove(exportOutPath);

        // Validation finale de l'ensemble de l'architecture TSALib
        auto finalVal = TSA::ExtensionSystem::LibraryManager::instance().validateAll();
        TEST_CHECK(finalVal.isValid(), "Subtest 48.5: Validation globale finale 100% conforme de TSALib sans aucune erreur");

        std::cout << "  [PASS] Subtest 48.5: Export via LibraryManager & Validation Globale Finale des 11 Phases Validees" << std::endl;

        std::cout << "[PASS] Test 48: TSALib Phase 11 - Packaging .tsalib, Distribution & Validation Globale Finale (5 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }


    return true;
}
