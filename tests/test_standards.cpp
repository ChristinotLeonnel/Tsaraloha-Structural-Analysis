#include "test_common.h"
#include "../src/Standards/NormativeTypes.h"
#include "../src/Standards/RequirementsCatalog.h"
#include "../src/Standards/NationalAnnexConfig.h"
#include "../src/Standards/ExternalLibraryCatalog.h"
#include "../src/Standards/DataDefinition.h"
#include "../src/Standards/ModelValidator.h"
#include "../src/Standards/AnalyticalBenchmark.h"
#include "../src/Standards/Design/ConcreteDesignEC2.h"
#include "../src/Standards/Design/SteelDesignEC3.h"
#include "../src/Analysis/OpenSeesSolver.h"
#include "../src/Analysis/OpenSeesResultsReader.h"
#include "../src/Analysis/CalculationSnapshot.h"
#include "../src/Analysis/ResultsModel.h"
#include "../src/NDC/NDCGenerator.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

using namespace TSA::Standards;
using namespace TSA::Standards::Design;

// assert() est sans effet en Release (NDEBUG) : vérifications actives dans toutes les configurations (audit).
#define STD_CHECK(cond) TEST_CHECK(cond, "Test Standards : condition non vérifiée")

bool runSuite_Standards(int& passed)
{
    std::cout << "\n=== [Suite Standards] Normative Requirements & Model Validator Tests ===" << std::endl;

    // Test 1 : Catalogue d'exigences normatives (ISO/IEC 25010 & ISO 12207)
    {
        std::cout << "Test Standards.1 : RequirementsCatalog completeness and indexing... ";
        auto& cat = RequirementsCatalog::instance();
        STD_CHECK(cat.totalCount() >= 15);

        auto reqArch = cat.findById("REQ-SW-ARCH-001");
        STD_CHECK(reqArch.has_value());
        STD_CHECK(reqArch->standard == StandardFramework::ISO_IEC_25010);
        STD_CHECK(reqArch->status == RequirementStatus::Implemented);

        auto reqSnap = cat.findById("REQ-CALC-SNAP-001");
        STD_CHECK(reqSnap.has_value());
        STD_CHECK(reqSnap->domain == RequirementDomain::AnalysisFEM);

        auto reqEC0 = cat.findById("REQ-CALC-EC0-001");
        STD_CHECK(reqEC0.has_value());
        STD_CHECK(reqEC0->standard == StandardFramework::EN_1990);

        auto ecReqs = cat.filterByStandard(StandardFramework::EN_1990);
        STD_CHECK(!ecReqs.empty());

        auto implementedReqs = cat.filterByStatus(RequirementStatus::Implemented);
        STD_CHECK(implementedReqs.size() >= 10);

        QString md = cat.generateMatrixMarkdown();
        STD_CHECK(!md.isEmpty());
        STD_CHECK(md.contains("REQ-SW-ARCH-001"));

        QString html = cat.generateTraceabilityReportHtml();
        STD_CHECK(!html.isEmpty());
        STD_CHECK(html.contains("<table>"));

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 2 : Annexes Nationales des Eurocodes (EN 1990 / EN 1992 / EN 1993)
    {
        std::cout << "Test Standards.2 : NationalAnnexConfig safety factors and psi factors... ";
        auto& nac = NationalAnnexConfig::instance();

        // France NF
        nac.setAnnex(NationalAnnexCode::France_NF);
        auto fNF = nac.safetyFactors();
        STD_CHECK(std::abs(fNF.gammaG_sup - 1.35) < 1e-6);
        STD_CHECK(std::abs(fNF.gammaQ - 1.50) < 1e-6);
        STD_CHECK(std::abs(fNF.gammaC - 1.50) < 1e-6);
        STD_CHECK(std::abs(fNF.gammaS - 1.15) < 1e-6);
        STD_CHECK(std::abs(fNF.gammaM0 - 1.00) < 1e-6);
        STD_CHECK(std::abs(fNF.gammaM1 - 1.00) < 1e-6);

        // Germany DIN (spécificité gammaM1 = 1.10)
        nac.setAnnex(NationalAnnexCode::Germany_DIN);
        auto fDIN = nac.safetyFactors();
        STD_CHECK(std::abs(fDIN.gammaM1 - 1.10) < 1e-6);

        // Restauration France NF
        nac.setAnnex(NationalAnnexCode::France_NF);

        // Facteurs psi EN 1990
        auto psiA = nac.psiForCategory(BuildingCategory::CatA_Domestic);
        STD_CHECK(std::abs(psiA.psi0 - 0.70) < 1e-6);
        STD_CHECK(std::abs(psiA.psi1 - 0.50) < 1e-6);
        STD_CHECK(std::abs(psiA.psi2 - 0.30) < 1e-6);

        auto psiE = nac.psiForCategory(BuildingCategory::CatE_Storage);
        STD_CHECK(std::abs(psiE.psi0 - 1.00) < 1e-6);
        STD_CHECK(std::abs(psiE.psi1 - 0.90) < 1e-6);
        STD_CHECK(std::abs(psiE.psi2 - 0.80) < 1e-6);

        auto psiSnowPlaine = nac.psiForSnow(200.0);
        STD_CHECK(std::abs(psiSnowPlaine.psi0 - 0.50) < 1e-6);
        STD_CHECK(std::abs(psiSnowPlaine.psi2 - 0.00) < 1e-6);

        auto psiSnowMontagne = nac.psiForSnow(1500.0);
        STD_CHECK(std::abs(psiSnowMontagne.psi0 - 0.70) < 1e-6);
        STD_CHECK(std::abs(psiSnowMontagne.psi2 - 0.20) < 1e-6);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 3 : Répertoire des bibliothèques externes tierces
    {
        std::cout << "Test Standards.3 : ExternalLibraryCatalog documentation and constraints... ";
        auto& libCat = ExternalLibraryCatalog::instance();
        STD_CHECK(libCat.allLibraries().size() >= 3);

        auto qtInfo = libCat.findByName("Qt");
        STD_CHECK(qtInfo.has_value());
        STD_CHECK(!qtInfo->responsibility.empty());
        STD_CHECK(!qtInfo->constraints.empty());

        auto occInfo = libCat.findByName("OpenCASCADE");
        STD_CHECK(occInfo.has_value());
        STD_CHECK(occInfo->version == "8.0.1");

        auto opsInfo = libCat.findByName("OpenSees");
        STD_CHECK(opsInfo.has_value());
        STD_CHECK(!opsInfo->interfaceUsed.empty());

        QString doc = libCat.generateDocumentationMarkdown();
        STD_CHECK(doc.contains("OpenCASCADE"));
        STD_CHECK(doc.contains("OpenSees"));

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 4 : Définition des propriétés physiques normatives
    {
        std::cout << "Test Standards.4 : StandardPropertyDefinitions and domain checks... ";
        auto fckDef = StandardPropertyDefinitions::concreteCompressiveStrength();
        STD_CHECK(fckDef.propertyName == "fck");
        STD_CHECK(fckDef.isValid(25.0e6));
        STD_CHECK(!fckDef.isValid(-5.0));
        STD_CHECK(!fckDef.isValid(std::numeric_limits<double>::quiet_NaN()));
        STD_CHECK(!fckDef.isValid(std::numeric_limits<double>::infinity()));

        auto eSteelDef = StandardPropertyDefinitions::youngModulusSteel();
        STD_CHECK(eSteelDef.isValid(210.0e9));
        STD_CHECK(!eSteelDef.isValid(50.0e9));

        auto nuConc = StandardPropertyDefinitions::poissonRatioConcrete();
        STD_CHECK(nuConc.isValid(0.20));
        STD_CHECK(!nuConc.isValid(0.55)); // Non physique pour béton

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 5 : Validateur de modèle structural (ModelValidator)
    {
        std::cout << "Test Standards.5 : ModelValidator on valid and invalid models... ";
        
        // Modèle vide -> doit échouer
        TSA::Model::Model emptyModel;
        auto repEmpty = ModelValidator::validate(emptyModel);
        STD_CHECK(!repEmpty.isValid());
        STD_CHECK(repEmpty.errorCount() >= 1);

        // Modèle avec 2 nœuds et une poutre sans appui -> doit échouer pour instabilité cinématique
        TSA::Model::Model testModel;
        int n1 = testModel.addNode(0.0, 0.0, 0.0);
        int n2 = testModel.addNode(5.0, 0.0, 0.0);
        testModel.addBar(n1, n2, TSA::Model::Section::rectangular(0.30, 0.50), TSA::Model::Material::concreteC25_30());

        auto repNoSupport = ModelValidator::validate(testModel);
        STD_CHECK(!repNoSupport.isValid());
        STD_CHECK(repNoSupport.summary().contains("Échec"));

        // Ajout d'un appui encastré au nœud 1 -> devient valide
        testModel.getNode(n1)->setSupportType(TSA::Model::SupportType::Fixed);

        auto repValid = ModelValidator::validate(testModel);
        STD_CHECK(repValid.isValid());

        // Test section invalide (aire négative)
        TSA::Model::Section invalidSec;
        invalidSec.width = -0.3;
        invalidSec.height = 0.5;
        std::string secErr;
        STD_CHECK(!ModelValidator::validateSection(invalidSec, &secErr));

        // Test matériau invalide (Poisson >= 0.5)
        TSA::Model::Material invalidMat;
        invalidMat.nu = 0.65;
        std::string matErr;
        STD_CHECK(!ModelValidator::validateMaterial(invalidMat, &matErr));

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 6 : Validation préalable avant calcul (ModelValidator::validateForAnalysis)
    {
        std::cout << "Test Standards.6 : ModelValidator::validateForAnalysis pre-analysis guards... ";
        
        TSA::Model::Model emptyModel;
        TSA::Analysis::AnalysisParameters params;
        params.type = TSA::Analysis::AnalysisType::LinearStatic;
        auto repEmpty = ModelValidator::validateForAnalysis(emptyModel, params);
        STD_CHECK(!repEmpty.isValid());

        // Modèle sans aucun appui
        TSA::Model::Model unstableModel;
        int n1 = unstableModel.addNode(0.0, 0.0, 0.0);
        int n2 = unstableModel.addNode(4.0, 0.0, 0.0);
        unstableModel.addBar(n1, n2, TSA::Model::Section::rectangular(0.3, 0.5), TSA::Model::Material::concreteC25_30());
        auto repUnstable = ModelValidator::validateForAnalysis(unstableModel, params);
        STD_CHECK(!repUnstable.isValid());
        bool hasSupportIssue = false;
        for (const auto& iss : repUnstable.issues())
        {
            if (iss.category == "Conditions aux Limites") hasSupportIssue = true;
        }
        STD_CHECK(hasSupportIssue);

        // Modèle stable mais cas de charge cible inexistant
        unstableModel.getNode(n1)->setSupportType(TSA::Model::SupportType::Fixed);
        params.targetLoadCaseId = 999;
        auto repBadLC = ModelValidator::validateForAnalysis(unstableModel, params);
        STD_CHECK(!repBadLC.isValid());
        bool hasLCIssue = false;
        for (const auto& iss : repBadLC.issues())
        {
            if (iss.category == "Charges") hasLCIssue = true;
        }
        STD_CHECK(hasLCIssue);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 7 : Blocage solveur OpenSees et détection de singularités numériques (NaN/Inf)
    {
        std::cout << "Test Standards.7 : OpenSeesSolver pre-check and OpenSeesResultsReader singularity detection... ";

        // Solveur bloque le calcul d'un modèle non contraint
        TSA::Model::Model unstableModel;
        int n1 = unstableModel.addNode(0.0, 0.0, 0.0);
        int n2 = unstableModel.addNode(4.0, 0.0, 0.0);
        unstableModel.addBar(n1, n2, TSA::Model::Section::rectangular(0.3, 0.5), TSA::Model::Material::concreteC25_30());
        TSA::Analysis::OpenSeesSolver solver;
        TSA::Analysis::AnalysisParameters params;
        QString solverErr;
        bool ok = solver.solveSynchronous(unstableModel, params, &solverErr);
        STD_CHECK(!ok);
        STD_CHECK(solverErr.contains("validation normative avant calcul"));

        // Détection de singularité numérique (NaN/Inf) dans OpenSeesResultsReader
        QString tmpDir = QDir::tempPath() + "/tsa_singularity_test_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tmpDir);

        QString dispFile = tmpDir + "/displacements.out";
        QFile f(dispFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&f);
            out << "0.0 0.0 0.0 0.0 0.0 0.0 NaN 0.0 1.5 0.0 0.0 0.0\n";
            f.close();
        }

        unstableModel.getNode(n1)->setSupportType(TSA::Model::SupportType::Fixed);
        auto snap = TSA::Analysis::CalculationSnapshot::capture(unstableModel);
        TSA::Analysis::AnalysisParameters readParams;
        readParams.dispOutputFile = "displacements.out";
        readParams.reactOutputFile = "reactions.out";
        readParams.forceOutputFile = "forces.out";

        TSA::Analysis::ResultsModel res;
        std::string readerErr;
        bool readOk = TSA::Analysis::OpenSeesResultsReader::readResults(tmpDir.toStdString(), snap, readParams, res, &readerErr);
        STD_CHECK(!readOk);
        STD_CHECK(!res.isValid());
        STD_CHECK(readerErr.find("Instabilité numérique") != std::string::npos);

        // Nettoyage
        QFile::remove(dispFile);
        QDir().rmdir(tmpDir);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 8 : Métadonnées d'exécution et traçabilité normative des résultats (ResultsModel)
    {
        std::cout << "Test Standards.8 : AnalysisExecutionMetadata in ResultsModel... ";
        TSA::Analysis::ResultsModel res;
        auto meta = res.executionMetadata();
        // BUG-111 (étape 0) : un résultat neuf ne porte aucune valeur inventée (auparavant « OpenSees 3.8.0 », « France NF »)
        STD_CHECK(meta.solverEngine.empty() && meta.solverVersion.empty());
        STD_CHECK(meta.nationalAnnex.empty());
        STD_CHECK(meta.normativeFramework.empty());

        meta.maxResidualForce = 0.00045;
        meta.isEquilibriumVerified = true;
        res.setExecutionMetadata(meta);

        STD_CHECK(res.executionMetadata().isEquilibriumVerified);
        STD_CHECK(res.executionMetadata().maxResidualForce < 0.001);

        res.clear();
        STD_CHECK(!res.executionMetadata().isEquilibriumVerified);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 9 : Cas de référence analytiques et benchmarks (AnalyticalBenchmarkRegistry)
    {
        std::cout << "Test Standards.9 : AnalyticalBenchmarkRegistry and reference cases... ";
        auto& reg = AnalyticalBenchmarkRegistry::instance();
        STD_CHECK(reg.totalCount() >= 4);
        STD_CHECK(reg.passedCount() >= 4);

        // Évaluation analytique d'une poutre bi-appuyée sous P = 10 kN, L = 5 m (M_théorique = 12.5 kNm)
        auto bc1 = AnalyticalBenchmarkRegistry::evaluateBeamPointLoad("TEST_POINT_LOAD", 10.0, 5.0, 12.502);
        STD_CHECK(bc1.passed);
        STD_CHECK(std::abs(bc1.expectedValue - 12.5) < 1e-9);
        STD_CHECK(bc1.relativeError() < 0.001);

        // Évaluation avec écart supérieur à la tolérance
        auto bcFail = AnalyticalBenchmarkRegistry::evaluateBeamPointLoad("TEST_FAIL", 10.0, 5.0, 15.0);
        STD_CHECK(!bcFail.passed);

        // Rapport Markdown généré
        std::string reportMd = reg.generateReportMarkdown();
        STD_CHECK(!reportMd.empty());
        STD_CHECK(reportMd.find("EC3_STEEL_BEAM_001") != std::string::npos);
        STD_CHECK(reportMd.find("PASSED") != std::string::npos);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 10 : Dimensionnement flexion simple béton armé selon Eurocode 2 (ConcreteDesignEC2)
    {
        std::cout << "Test Standards.10 : ConcreteDesignEC2 (EN 1992-1-1 §6.1)... ";

        EC2BeamBendingInput in;
        in.b = 0.30;
        in.h = 0.50;
        in.d = 0.45;
        in.fck = 25.0e6;  // C25/30
        in.fyk = 500.0e6; // B500B
        in.Med = 150.0e3; // 150 kNm
        in.gammaC = 1.50;
        in.gammaS = 1.15;

        auto res = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        STD_CHECK(res.valid);
        STD_CHECK(!res.requiresCompressionSteel);
        STD_CHECK(res.effectiveDepth == 0.45);
        STD_CHECK(std::abs(res.fcd - 16.666667e6) < 1e3);
        STD_CHECK(std::abs(res.fyd - 434.782608e6) < 1e3);
        // mu_cu ~ 0.1481
        STD_CHECK(res.mu_cu > 0.14 && res.mu_cu < 0.16);
        STD_CHECK(res.utilizationRatio < 1.0);
        // Section As_provided en m² (~ 8.3 cm²)
        STD_CHECK(res.As_provided > 8.0e-4 && res.As_provided < 9.0e-4);
        STD_CHECK(res.As_provided >= res.As_min);

        // Cas sous-dimensionné nécessitant armatures comprimées
        in.Med = 500.0e3; // 500 kNm -> dépasse mu_lu = 0.372
        auto resHeavy = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        STD_CHECK(resHeavy.valid);
        STD_CHECK(resHeavy.requiresCompressionSteel);
        STD_CHECK(resHeavy.utilizationRatio > 1.0);

        // Calcul direct depuis les objets du modèle TSA (Section & Material)
        auto sec = TSA::Model::Section::rectangular(0.30, 0.50);
        auto mat = TSA::Model::Material::concreteC25_30();
        auto resModel = ConcreteDesignEC2::calculateFromModel(sec, mat, 120.0e3, 500.0e6, 0.05);
        STD_CHECK(resModel.valid);
        STD_CHECK(!resModel.requiresCompressionSteel);
        STD_CHECK(resModel.As_provided > 6.0e-4);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 11 : Justification des barres acier au flambement selon Eurocode 3 (SteelDesignEC3)
    {
        std::cout << "Test Standards.11 : SteelDesignEC3 (EN 1993-1-1 §6.2 & §6.3)... ";

        // 1. Résistances de section (§6.2)
        double A = 2.848e-3; // IPE 200 (28.48 cm²)
        double fy = 235.0e6; // S235
        double Wel = 1.943e-4; // 194.3 cm³

        double Nt_Rd = SteelDesignEC3::tensionResistance(A, fy);
        STD_CHECK(std::abs(Nt_Rd - 669.28e3) < 1e2);

        double Mc_Rd = SteelDesignEC3::bendingResistance(Wel, fy);
        STD_CHECK(std::abs(Mc_Rd - 45.66e3) < 1e2);

        // 2. Flambement (§6.3) : Poteau IPE 200, L = 4.0 m, articulé (beta = 1.0), axe faible (Iz = 1.42e-6 m4)
        EC3BucklingInput in;
        in.Lcr = 4.0;
        in.A = A;
        in.I = 1.42e-6; // Iz
        in.E = 210.0e9;
        in.fy = fy;
        in.Ned = 100.0e3; // 100 kN sollicitant
        in.curve = EC3BucklingCurve::b;
        in.gammaM1 = 1.00;

        auto res = SteelDesignEC3::calculateBuckling(in);
        STD_CHECK(res.valid);
        STD_CHECK(res.radiusOfGyration > 0.02 && res.radiusOfGyration < 0.025); // ~ 2.23 cm
        STD_CHECK(res.slenderness > 170.0 && res.slenderness < 190.0);         // ~ 179
        STD_CHECK(res.reducedSlenderness > 1.8 && res.reducedSlenderness < 2.1); // ~ 1.91
        STD_CHECK(res.chi > 0.20 && res.chi < 0.26);                          // ~ 0.228
        STD_CHECK(res.Nb_Rd > 140.0e3 && res.Nb_Rd < 170.0e3);                // ~ 152 kN
        STD_CHECK(res.utilizationRatio < 1.0);
        STD_CHECK(res.pass);

        // Cas charge excessive au-delà de Nb,Rd
        in.Ned = 200.0e3; // 200 kN > 152 kN
        auto resFail = SteelDesignEC3::calculateBuckling(in);
        STD_CHECK(resFail.valid);
        STD_CHECK(resFail.utilizationRatio > 1.0);
        STD_CHECK(!resFail.pass);

        // Calcul direct depuis une barre du modèle TSA
        auto secIpe = TSA::Model::Section::ipe(200);
        auto matSteel = TSA::Model::Material::steelS235();
        auto resBar = SteelDesignEC3::calculateFromBar(secIpe, matSteel, 4.0, 1.0, 100.0e3, false);
        STD_CHECK(resBar.valid);
        STD_CHECK(resBar.pass);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // TEST 503 : informations de calcul fiables (BUG-111, étape 0) — aucune valeur inventée dans les métadonnées ; la note
    // de calcul décrit le moteur et la version enregistrés avec les résultats, jamais le moteur installé ; plus de
    // métadonnées « certifiées » ; horodatage en temps universel.
    {
        const TSA::Analysis::AnalysisExecutionMetadata blank;
        TEST_CHECK(blank.solverEngine.empty() && blank.solverVersion.empty() && blank.nationalAnnex.empty() && blank.normativeFramework.empty()
                       && blank.modelBuilder.empty() && blank.numberer.empty() && blank.preparationMessages.empty(),
                   "Test 503: métadonnées neuves vides (ni « OpenSees 3.8.0 », ni « France NF »)");

        TSA::Model::Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        m.getNode(a)->setSupport(TSA::Model::SupportDefinition::fixed());
        m.addBar(a, b, TSA::Model::Section::ipe(300), TSA::Model::Material::steelS235());
        auto results = std::make_shared<TSA::Analysis::ResultsModel>();
        results->setValid(true);
        results->updateTimestamp();
        auto meta = results->executionMetadata();
        meta.engineId = "opensees";
        meta.solverEngine = "OpenSees";
        meta.solverVersion = "9.9.9-enregistree";   // version simulée, différente de toute installation
        meta.preparationMessages = { "Charges : composante hors plan ignorée" };
        results->setExecutionMetadata(meta);
        const QString html = TSA::NDC::NDCGenerator::generate(m, results, QStringLiteral("P"), QStringLiteral("I")).toHtml();
        TEST_CHECK(html.contains(QStringLiteral("OpenSees 9.9.9-enregistree")) && !html.contains(QStringLiteral("OpenSees v3."))
                       && !html.contains(QStringLiteral("OpenSees 3.")),
                   "Test 503: note régénérée : version du moteur enregistrée avec les résultats, pas la version installée");
        TEST_CHECK(!html.contains(QStringLiteral("certifiées")) && html.contains(QStringLiteral("ni une certification")),
                   "Test 503: métadonnées présentées comme enregistrées, non certifiées");
        TEST_CHECK(html.contains(QStringLiteral("aucune (coefficients des combinaisons saisis dans le modèle)"))
                       && html.contains(QStringLiteral("Charges : composante hors plan ignorée")),
                   "Test 503: annexe du calcul « aucune » ; messages de validation avant calcul repris");
        meta.solverVersion.clear();
        results->setExecutionMetadata(meta);
        const QString unread = TSA::NDC::NDCGenerator::generate(m, results, QStringLiteral("P"), QStringLiteral("I")).toHtml();
        TEST_CHECK(unread.contains(QStringLiteral("OpenSees (version non lue)")) && !unread.contains(QStringLiteral("OpenSees 3.")),
                   "Test 503: version non lue affichée comme telle (aucune version de repli)");
        const std::string& stamp = results->timestamp();
        TEST_CHECK(stamp.size() == 23 && stamp.compare(stamp.size() - 4, 4, " UTC") == 0, "Test 503: horodatage UTC (" << stamp << ")");
        std::cout << "[PASS] Test 503: Informations de calcul sans valeur inventée (métadonnées, note, horodatage)" << std::endl;
        ++passed;
    }

    return true;
}
