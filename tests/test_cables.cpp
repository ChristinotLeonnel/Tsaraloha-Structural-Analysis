#include "test_common.h"

bool runSuite_Cables(int& passed)
{
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
            TEST_CHECK(std::abs(actualArc - expectedApprox) < 1.0, "Subtest 36.3: Parabolic arc length accurate");

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


    // --- TEST 49: Cable System Audit & End-to-End Validation ---
    // Creation, Selection, Properties, Deletion, Move, Copy & Undo/Redo
    {
        std::cout << "\n--- TEST 49: Cable System Audit & End-to-End Validation ---" << std::endl;

        // 49.1: Unified Linear Element Role & Cable Creation Contract
        {
            TEST_CHECK(TSA::Model::BarRole::Cable != TSA::Model::BarRole::Beam, "Subtest 49.1: BarRole::Cable distinct role");
            TEST_CHECK(static_cast<int>(TSA::Model::BarRole::Cable) == 7, "Subtest 49.1: BarRole::Cable enum integration");

            TSA::Model::Model m1;
            int n1 = m1.addNode(0.0, 0.0, 0.0);
            int n2 = m1.addNode(10.0, 0.0, 5.0);

            int cId = m1.addCable(n1, n2, TSA::Model::CableType::StayCable);
            TEST_CHECK(cId > 0, "Subtest 49.1: Cable created with valid positive ID");
            const auto* cable = m1.getCable(cId);
            TEST_CHECK(cable != nullptr, "Subtest 49.1: Cable retrieved from model");
            if (cable)
            {
                TEST_CHECK(cable->startNodeId() == n1, "Subtest 49.1: Start node matching");
                TEST_CHECK(cable->endNodeId() == n2, "Subtest 49.1: End node matching");
                TEST_CHECK(cable->type() == TSA::Model::CableType::StayCable, "Subtest 49.1: Type StayCable confirmed");
                TEST_CHECK(cable->chordLength(m1) > 11.18 && cable->chordLength(m1) < 11.19, "Subtest 49.1: Chord length sqrt(100+25) ~ 11.18 m");
                TEST_CHECK(cable->diameter() > 0.0, "Subtest 49.1: Cable diameter is positive");
                TEST_CHECK(!cable->color().empty(), "Subtest 49.1: Cable has valid non-empty color");
                TEST_CHECK(cable->typeName() == "Cable", "Subtest 49.1: Cable typeName is Cable");
                TEST_CHECK(dynamic_cast<const TSA::Model::LinearElement*>(cable) != nullptr, "Subtest 49.1: Cable inherits from LinearElement");
            }
            std::cout << "  [PASS] Subtest 49.1: Unified Linear Element Role & Cable Creation Contract Verified" << std::endl;
        }

        // 49.2: Cable Deletion & Model Isolation
        {
            TSA::Model::Model m2;
            int n1 = m2.addNode(0.0, 0.0, 0.0);
            int n2 = m2.addNode(5.0, 0.0, 0.0);
            int n3 = m2.addNode(10.0, 0.0, 0.0);
            int n4 = m2.addNode(15.0, 0.0, 0.0);

            int c1 = m2.addCable(n1, n2);
            int c2 = m2.addCable(n2, n3);
            int c3 = m2.addCable(n3, n4);
            TEST_CHECK(m2.cables().size() == 3, "Subtest 49.2: 3 cables initially in model");

            // Deleting single cable
            m2.removeCable(c2);
            TEST_CHECK(m2.getCable(c2) == nullptr, "Subtest 49.2: Cable c2 successfully removed from model");
            TEST_CHECK(m2.cables().size() == 2, "Subtest 49.2: Model cable count decreased to 2");
            TEST_CHECK(m2.getCable(c1) != nullptr, "Subtest 49.2: Cable c1 remains intact");
            TEST_CHECK(m2.getCable(c3) != nullptr, "Subtest 49.2: Cable c3 remains intact");

            // Deleting remaining cables
            m2.removeCable(c1);
            m2.removeCable(c3);
            TEST_CHECK(m2.cables().empty(), "Subtest 49.2: All cables deleted successfully");

            // Deleting non-existent cable should not crash
            m2.removeCable(9999);
            TEST_CHECK(m2.cables().empty(), "Subtest 49.2: Deleting invalid ID handled gracefully");

            std::cout << "  [PASS] Subtest 49.2: Cable Deletion & Model Isolation Cleanliness Verified" << std::endl;
        }

        // 49.3: Cable Node Translation & Dynamic Recomputation
        {
            TSA::Model::Model m3;
            int n1 = m3.addNode(0.0, 0.0, 0.0);
            int n2 = m3.addNode(10.0, 0.0, 0.0);
            int cId = m3.addCable(n1, n2);

            auto* cable = m3.getCable(cId);
            TEST_CHECK(approxEqual(cable->chordLength(m3), 10.0), "Subtest 49.3: Initial chord length = 10.0 m");

            // Move node 2 to (12.0, 0.0, 5.0) -> chord length = sqrt(144 + 25) = 13.0 m
            auto* node2 = m3.getNode(n2);
            node2->setCoordinates(12.0, 0.0, 5.0);
            m3.notifyNodeModified(n2);

            TEST_CHECK(approxEqual(cable->chordLength(m3), 13.0), "Subtest 49.3: Updated chord length dynamically recomputed to 13.0 m");

            std::cout << "  [PASS] Subtest 49.3: Cable Node Translation & Dynamic Recomputation Verified" << std::endl;
        }

        // 49.4: Duplication via copyElements & copyAndRotateElements
        {
            TSA::Model::Model m4;
            int n1 = m4.addNode(0.0, 0.0, 0.0);
            int n2 = m4.addNode(5.0, 0.0, 10.0);
            int cId = m4.addCable(n1, n2, TSA::Model::CableType::StayCable);
            auto* origCable = m4.getCable(cId);
            origCable->setDiameter(0.040); // 40 mm
            origCable->setColor("#FF8800");
            origCable->prestress().initialTension = 150000.0; // 150 kN

            // 1. Translation copy (dy = +4.0m)
            std::set<int> selNodes = {n1, n2};
            std::set<int> selCables = {cId};
            auto created = m4.copyElements(selNodes, {}, {}, {}, 0.0, 4.0, 0.0, 1, selCables);
            TEST_CHECK(!created.empty(), "Subtest 49.4: copyElements returned created entity IDs");
            TEST_CHECK(m4.cables().size() == 2, "Subtest 49.4: Model now has 2 cables");

            int copyCableId = -1;
            for (const auto& [id, c] : m4.cables())
            {
                if (id != cId) { copyCableId = id; break; }
            }
            TEST_CHECK(copyCableId > 0, "Subtest 49.4: Duplicated cable identified");
            const auto* copiedCable = m4.getCable(copyCableId);
            TEST_CHECK(copiedCable != nullptr, "Subtest 49.4: Duplicated cable pointer valid");
            if (copiedCable)
            {
                TEST_CHECK(copiedCable->type() == TSA::Model::CableType::StayCable, "Subtest 49.4: Type preserved");
                TEST_CHECK(approxEqual(copiedCable->diameter(), 0.040), "Subtest 49.4: Diameter 40mm preserved");
                TEST_CHECK(copiedCable->color() == "#FF8800", "Subtest 49.4: Color #FF8800 preserved");
                TEST_CHECK(approxEqual(copiedCable->initialTension(), 150000.0), "Subtest 49.4: Prestress 150kN preserved");

                const auto* nStartCopy = m4.getNode(copiedCable->startNodeId());
                const auto* nEndCopy = m4.getNode(copiedCable->endNodeId());
                TEST_CHECK(approxEqual(nStartCopy->y(), 4.0), "Subtest 49.4: Start node shifted dy = 4.0");
                TEST_CHECK(approxEqual(nEndCopy->y(), 4.0), "Subtest 49.4: End node shifted dy = 4.0");
            }

            // 2. Rotation copy around Z axis by 90 degrees
            gp_Pnt center(0.0, 0.0, 0.0);
            gp_Dir axis(0.0, 0.0, 1.0);
            double angle = 1.5707963267948966; // 90 deg in rad
            auto rotCreated = m4.copyAndRotateElements(selNodes, {}, {}, {}, center, axis, angle, 1, selCables);
            TEST_CHECK(m4.cables().size() == 3, "Subtest 49.4: Model now has 3 cables after copyAndRotate");

            std::cout << "  [PASS] Subtest 49.4: copyElements & copyAndRotateElements for Cables Verified" << std::endl;
        }

        // 49.5: StructuralClipboard Integration (Copy & Paste to Another Model)
        {
            TSA::Model::Model sourceModel;
            int sN1 = sourceModel.addNode(1.0, 2.0, 3.0);
            int sN2 = sourceModel.addNode(6.0, 2.0, 3.0);
            int sC = sourceModel.addCable(sN1, sN2, TSA::Model::CableType::Generic);
            auto* cab = sourceModel.getCable(sC);
            cab->setDiameter(0.025);
            cab->setColor("#00AAFF");

            TSA::Model::StructuralClipboard clipboard;
            std::vector<int> selN = {sN1, sN2};
            std::vector<int> selC = {sC};
            std::vector<int> emptyVec;
            clipboard.copyFrom(sourceModel, selN, emptyVec, emptyVec, emptyVec, selC);
            TEST_CHECK(clipboard.hasData(), "Subtest 49.5: Clipboard hasData is true");
            TEST_CHECK(clipboard.cableCount() == 1, "Subtest 49.5: Clipboard contains 1 cable");
            TEST_CHECK(clipboard.nodeCount() == 2, "Subtest 49.5: Clipboard contains 2 nodes");

            TSA::Model::Model targetModel;
            auto pasteResult = clipboard.pasteTo(targetModel, 10.0, 20.0, 30.0);
            TEST_CHECK(!pasteResult.empty(), "Subtest 49.5: pasteTo returned non-empty result");
            TEST_CHECK(pasteResult.cableIds.size() == 1, "Subtest 49.5: 1 cable pasted in target model");
            TEST_CHECK(targetModel.cables().size() == 1, "Subtest 49.5: Target model has 1 cable");

            const auto* pastedCable = targetModel.getCable(pasteResult.cableIds[0]);
            TEST_CHECK(pastedCable != nullptr, "Subtest 49.5: Pasted cable retrieved");
            if (pastedCable)
            {
                TEST_CHECK(approxEqual(pastedCable->diameter(), 0.025), "Subtest 49.5: Pasted diameter 25mm preserved");
                TEST_CHECK(pastedCable->color() == "#00AAFF", "Subtest 49.5: Pasted color preserved");
                TEST_CHECK(approxEqual(pastedCable->chordLength(targetModel), 5.0), "Subtest 49.5: Pasted chord length 5.0m preserved");
            }

            std::cout << "  [PASS] Subtest 49.5: StructuralClipboard Cable Copy & Paste Integration Verified" << std::endl;
        }

        // 49.6: Full Undo/Redo & State Transaction Integrity
        {
            TSA::Model::Model m6;
            int n1 = m6.addNode(0.0, 0.0, 0.0);
            int n2 = m6.addNode(10.0, 0.0, 0.0);

            m6.pushUndoState("Creation Cable");
            int cId = m6.addCable(n1, n2);
            TEST_CHECK(m6.cables().size() == 1, "Subtest 49.6: 1 cable added");

            m6.pushUndoState("Modification Diametre");
            auto* cab = m6.getCable(cId);
            cab->setDiameter(0.060);
            cab->setColor("#123456");
            m6.notifyCableModified(cId);
            TEST_CHECK(approxEqual(m6.getCable(cId)->diameter(), 0.060), "Subtest 49.6: Diameter set to 60mm");

            m6.pushUndoState("Suppression Cable");
            m6.removeCable(cId);
            TEST_CHECK(m6.cables().empty(), "Subtest 49.6: Cable removed from model");

            // Undo deletion -> cable restored
            m6.undo();
            TEST_CHECK(m6.cables().size() == 1, "Subtest 49.6: Cable restored after undo deletion");
            auto* restoredCab = m6.getCable(cId);
            TEST_CHECK(restoredCab != nullptr, "Subtest 49.6: Restored cable pointer valid");
            if (restoredCab)
            {
                TEST_CHECK(approxEqual(restoredCab->diameter(), 0.060), "Subtest 49.6: Restored cable diameter is 60mm");
                TEST_CHECK(restoredCab->color() == "#123456", "Subtest 49.6: Restored cable color is #123456");
            }

            // Undo modification -> reverted diameter
            m6.undo();
            TEST_CHECK(m6.cables().size() == 1, "Subtest 49.6: Cable still exists after undo modification");
            TEST_CHECK(m6.getCable(cId)->diameter() < 0.059, "Subtest 49.6: Cable diameter reverted back to default");

            // Redo modification
            m6.redo();
            TEST_CHECK(approxEqual(m6.getCable(cId)->diameter(), 0.060), "Subtest 49.6: Cable diameter is 60mm after redo");

            // Redo deletion
            m6.redo();
            TEST_CHECK(m6.cables().empty(), "Subtest 49.6: Cable deleted after redo deletion");

            std::cout << "  [PASS] Subtest 49.6: Full Undo/Redo & State Transaction Integrity Verified" << std::endl;
        }

        // 49.7: Multi-Element Coexistence & Strict Separation (Cable != Beam != Column != Truss)
        {
            TSA::Model::Model m7;
            int nA = m7.addNode(0.0, 0.0, 0.0);
            int nB = m7.addNode(5.0, 0.0, 0.0);
            int nC = m7.addNode(0.0, 0.0, 4.0);
            int nD = m7.addNode(5.0, 0.0, 4.0);

            // 1. Poutre (Beam) — largeur 0.20m, hauteur 0.50m
            int beamId = m7.addBeam(nC, nD, 0.20, 0.50);
            // 2. Poteau (Column) — 0.30m x 0.30m
            int colId = m7.addColumn(nA, nC, 0.30, 0.30);
            // 3. Barre de Treillis (TrussMember) — diamètre 0.050m
            int trussId = m7.addTrussMember(nA, nD, 0.050);
            // 4. Câble (Cable)
            int cableId = m7.addCable(nB, nC, TSA::Model::CableType::StayCable);
            auto* cab = m7.getCable(cableId);
            cab->setDiameter(0.035);
            cab->setInitialTension(120000.0); // 120 kN

            TEST_CHECK(beamId > 0 && colId > 0 && trussId > 0 && cableId > 0, "Subtest 49.7: Created element IDs valid");
            TEST_CHECK(m7.beams().size() == 1, "Subtest 49.7: Exactly 1 beam in model");
            TEST_CHECK(m7.columns().size() == 1, "Subtest 49.7: Exactly 1 column in model");
            TEST_CHECK(m7.trussMembers().size() == 1, "Subtest 49.7: Exactly 1 truss member in model");
            TEST_CHECK(m7.cables().size() == 1, "Subtest 49.7: Exactly 1 cable in model");

            // Strict segregation checks
            const auto* beam = m7.getBeam(beamId);
            const auto* col = m7.getColumn(colId);
            TEST_CHECK(approxEqual(beam->width(), 0.20), "Subtest 49.7: Beam width is 0.20m");
            TEST_CHECK(approxEqual(col->width(), 0.30), "Subtest 49.7: Column width is 0.30m");
            TEST_CHECK(cab->type() == TSA::Model::CableType::StayCable, "Subtest 49.7: Cable is StayCable");
            TEST_CHECK(cab->tensionOnly() == true, "Subtest 49.7: Cable is tensionOnly");
            TEST_CHECK(approxEqual(cab->diameter(), 0.035), "Subtest 49.7: Cable diameter is 35mm");

            // Deleting Beam does not impact Cable or Column
            m7.removeBeam(beamId);
            TEST_CHECK(m7.beams().empty(), "Subtest 49.7: Beam removed");
            TEST_CHECK(m7.columns().size() == 1, "Subtest 49.7: Column remains intact");
            TEST_CHECK(m7.cables().size() == 1, "Subtest 49.7: Cable remains intact");
            TEST_CHECK(approxEqual(m7.getCable(cableId)->diameter(), 0.035), "Subtest 49.7: Cable properties unaffected by beam deletion");

            // Deleting Column does not impact Cable
            m7.removeColumn(colId);
            TEST_CHECK(m7.columns().empty(), "Subtest 49.7: Column removed");
            TEST_CHECK(m7.cables().size() == 1, "Subtest 49.7: Cable still exists");

            // Deleting Cable
            m7.removeCable(cableId);
            TEST_CHECK(m7.cables().empty(), "Subtest 49.7: Cable cleanly removed");
            TEST_CHECK(m7.trussMembers().size() == 1, "Subtest 49.7: Truss member remains");

            std::cout << "  [PASS] Subtest 49.7: Multi-Element Coexistence & Strict Separation (Cable != Beam != Column != Truss) Verified" << std::endl;
        }

        std::cout << "[PASS] Test 49: Cable System Audit & End-to-End Validation (7 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }


    return true;
}
