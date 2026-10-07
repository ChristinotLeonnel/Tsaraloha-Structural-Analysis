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
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

using namespace TSA::Standards;
using namespace TSA::Standards::Design;

bool runSuite_Standards(int& passed)
{
    std::cout << "\n=== [Suite Standards] Normative Requirements & Model Validator Tests ===" << std::endl;

    // Test 1 : Catalogue d'exigences normatives (ISO/IEC 25010 & ISO 12207)
    {
        std::cout << "Test Standards.1 : RequirementsCatalog completeness and indexing... ";
        auto& cat = RequirementsCatalog::instance();
        assert(cat.totalCount() >= 15);

        auto reqArch = cat.findById("REQ-SW-ARCH-001");
        assert(reqArch.has_value());
        assert(reqArch->standard == StandardFramework::ISO_IEC_25010);
        assert(reqArch->status == RequirementStatus::Implemented);

        auto reqSnap = cat.findById("REQ-CALC-SNAP-001");
        assert(reqSnap.has_value());
        assert(reqSnap->domain == RequirementDomain::AnalysisFEM);

        auto reqEC0 = cat.findById("REQ-CALC-EC0-001");
        assert(reqEC0.has_value());
        assert(reqEC0->standard == StandardFramework::EN_1990);

        auto ecReqs = cat.filterByStandard(StandardFramework::EN_1990);
        assert(!ecReqs.empty());

        auto implementedReqs = cat.filterByStatus(RequirementStatus::Implemented);
        assert(implementedReqs.size() >= 10);

        QString md = cat.generateMatrixMarkdown();
        assert(!md.isEmpty());
        assert(md.contains("REQ-SW-ARCH-001"));

        QString html = cat.generateTraceabilityReportHtml();
        assert(!html.isEmpty());
        assert(html.contains("<table>"));

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
        assert(std::abs(fNF.gammaG_sup - 1.35) < 1e-6);
        assert(std::abs(fNF.gammaQ - 1.50) < 1e-6);
        assert(std::abs(fNF.gammaC - 1.50) < 1e-6);
        assert(std::abs(fNF.gammaS - 1.15) < 1e-6);
        assert(std::abs(fNF.gammaM0 - 1.00) < 1e-6);
        assert(std::abs(fNF.gammaM1 - 1.00) < 1e-6);

        // Germany DIN (spécificité gammaM1 = 1.10)
        nac.setAnnex(NationalAnnexCode::Germany_DIN);
        auto fDIN = nac.safetyFactors();
        assert(std::abs(fDIN.gammaM1 - 1.10) < 1e-6);

        // Restauration France NF
        nac.setAnnex(NationalAnnexCode::France_NF);

        // Facteurs psi EN 1990
        auto psiA = nac.psiForCategory(BuildingCategory::CatA_Domestic);
        assert(std::abs(psiA.psi0 - 0.70) < 1e-6);
        assert(std::abs(psiA.psi1 - 0.50) < 1e-6);
        assert(std::abs(psiA.psi2 - 0.30) < 1e-6);

        auto psiE = nac.psiForCategory(BuildingCategory::CatE_Storage);
        assert(std::abs(psiE.psi0 - 1.00) < 1e-6);
        assert(std::abs(psiE.psi1 - 0.90) < 1e-6);
        assert(std::abs(psiE.psi2 - 0.80) < 1e-6);

        auto psiSnowPlaine = nac.psiForSnow(200.0);
        assert(std::abs(psiSnowPlaine.psi0 - 0.50) < 1e-6);
        assert(std::abs(psiSnowPlaine.psi2 - 0.00) < 1e-6);

        auto psiSnowMontagne = nac.psiForSnow(1500.0);
        assert(std::abs(psiSnowMontagne.psi0 - 0.70) < 1e-6);
        assert(std::abs(psiSnowMontagne.psi2 - 0.20) < 1e-6);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 3 : Répertoire des bibliothèques externes tierces
    {
        std::cout << "Test Standards.3 : ExternalLibraryCatalog documentation and constraints... ";
        auto& libCat = ExternalLibraryCatalog::instance();
        assert(libCat.allLibraries().size() >= 3);

        auto qtInfo = libCat.findByName("Qt");
        assert(qtInfo.has_value());
        assert(!qtInfo->responsibility.empty());
        assert(!qtInfo->constraints.empty());

        auto occInfo = libCat.findByName("OpenCASCADE");
        assert(occInfo.has_value());
        assert(occInfo->version == "8.0.1");

        auto opsInfo = libCat.findByName("OpenSees");
        assert(opsInfo.has_value());
        assert(!opsInfo->interfaceUsed.empty());

        QString doc = libCat.generateDocumentationMarkdown();
        assert(doc.contains("OpenCASCADE"));
        assert(doc.contains("OpenSees"));

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 4 : Définition des propriétés physiques normatives
    {
        std::cout << "Test Standards.4 : StandardPropertyDefinitions and domain checks... ";
        auto fckDef = StandardPropertyDefinitions::concreteCompressiveStrength();
        assert(fckDef.propertyName == "fck");
        assert(fckDef.isValid(25.0e6));
        assert(!fckDef.isValid(-5.0));
        assert(!fckDef.isValid(std::numeric_limits<double>::quiet_NaN()));
        assert(!fckDef.isValid(std::numeric_limits<double>::infinity()));

        auto eSteelDef = StandardPropertyDefinitions::youngModulusSteel();
        assert(eSteelDef.isValid(210.0e9));
        assert(!eSteelDef.isValid(50.0e9));

        auto nuConc = StandardPropertyDefinitions::poissonRatioConcrete();
        assert(nuConc.isValid(0.20));
        assert(!nuConc.isValid(0.55)); // Non physique pour béton

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 5 : Validateur de modèle structural (ModelValidator)
    {
        std::cout << "Test Standards.5 : ModelValidator on valid and invalid models... ";
        
        // Modèle vide -> doit échouer
        TSA::Model::Model emptyModel;
        auto repEmpty = ModelValidator::validate(emptyModel);
        assert(!repEmpty.isValid());
        assert(repEmpty.errorCount() >= 1);

        // Modèle avec 2 nœuds et une poutre sans appui -> doit échouer pour instabilité cinématique
        TSA::Model::Model testModel;
        int n1 = testModel.addNode(0.0, 0.0, 0.0);
        int n2 = testModel.addNode(5.0, 0.0, 0.0);
        testModel.addBar(n1, n2, TSA::Model::Section::rectangular(0.30, 0.50), TSA::Model::Material::concreteC25_30());

        auto repNoSupport = ModelValidator::validate(testModel);
        assert(!repNoSupport.isValid());
        assert(repNoSupport.summary().contains("Échec"));

        // Ajout d'un appui encastré au nœud 1 -> devient valide
        testModel.getNode(n1)->setSupportType(TSA::Model::SupportType::Fixed);

        auto repValid = ModelValidator::validate(testModel);
        assert(repValid.isValid());

        // Test section invalide (aire négative)
        TSA::Model::Section invalidSec;
        invalidSec.width = -0.3;
        invalidSec.height = 0.5;
        std::string secErr;
        assert(!ModelValidator::validateSection(invalidSec, &secErr));

        // Test matériau invalide (Poisson >= 0.5)
        TSA::Model::Material invalidMat;
        invalidMat.nu = 0.65;
        std::string matErr;
        assert(!ModelValidator::validateMaterial(invalidMat, &matErr));

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
        assert(!repEmpty.isValid());

        // Modèle sans aucun appui
        TSA::Model::Model unstableModel;
        int n1 = unstableModel.addNode(0.0, 0.0, 0.0);
        int n2 = unstableModel.addNode(4.0, 0.0, 0.0);
        unstableModel.addBar(n1, n2, TSA::Model::Section::rectangular(0.3, 0.5), TSA::Model::Material::concreteC25_30());
        auto repUnstable = ModelValidator::validateForAnalysis(unstableModel, params);
        assert(!repUnstable.isValid());
        bool hasSupportIssue = false;
        for (const auto& iss : repUnstable.issues())
        {
            if (iss.category == "Conditions aux Limites") hasSupportIssue = true;
        }
        assert(hasSupportIssue);

        // Modèle stable mais cas de charge cible inexistant
        unstableModel.getNode(n1)->setSupportType(TSA::Model::SupportType::Fixed);
        params.targetLoadCaseId = 999;
        auto repBadLC = ModelValidator::validateForAnalysis(unstableModel, params);
        assert(!repBadLC.isValid());
        bool hasLCIssue = false;
        for (const auto& iss : repBadLC.issues())
        {
            if (iss.category == "Charges") hasLCIssue = true;
        }
        assert(hasLCIssue);

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
        assert(!ok);
        assert(solverErr.contains("validation normative avant calcul"));

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
        assert(!readOk);
        assert(!res.isValid());
        assert(readerErr.find("Instabilité numérique") != std::string::npos);

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
        assert(meta.solverEngine == "OpenSees");
        assert(!meta.nationalAnnex.empty());
        assert(!meta.normativeFramework.empty());

        meta.maxResidualForce = 0.00045;
        meta.isEquilibriumVerified = true;
        res.setExecutionMetadata(meta);

        assert(res.executionMetadata().isEquilibriumVerified);
        assert(res.executionMetadata().maxResidualForce < 0.001);

        res.clear();
        assert(!res.executionMetadata().isEquilibriumVerified);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    // Test 9 : Cas de référence analytiques et benchmarks (AnalyticalBenchmarkRegistry)
    {
        std::cout << "Test Standards.9 : AnalyticalBenchmarkRegistry and reference cases... ";
        auto& reg = AnalyticalBenchmarkRegistry::instance();
        assert(reg.totalCount() >= 4);
        assert(reg.passedCount() >= 4);

        // Évaluation analytique d'une poutre bi-appuyée sous P = 10 kN, L = 5 m (M_théorique = 12.5 kNm)
        auto bc1 = AnalyticalBenchmarkRegistry::evaluateBeamPointLoad("TEST_POINT_LOAD", 10.0, 5.0, 12.502);
        assert(bc1.passed);
        assert(std::abs(bc1.expectedValue - 12.5) < 1e-9);
        assert(bc1.relativeError() < 0.001);

        // Évaluation avec écart supérieur à la tolérance
        auto bcFail = AnalyticalBenchmarkRegistry::evaluateBeamPointLoad("TEST_FAIL", 10.0, 5.0, 15.0);
        assert(!bcFail.passed);

        // Rapport Markdown généré
        std::string reportMd = reg.generateReportMarkdown();
        assert(!reportMd.empty());
        assert(reportMd.find("EC3_STEEL_BEAM_001") != std::string::npos);
        assert(reportMd.find("PASSED") != std::string::npos);

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
        assert(res.valid);
        assert(!res.requiresCompressionSteel);
        assert(res.effectiveDepth == 0.45);
        assert(std::abs(res.fcd - 16.666667e6) < 1e3);
        assert(std::abs(res.fyd - 434.782608e6) < 1e3);
        // mu_cu ~ 0.1481
        assert(res.mu_cu > 0.14 && res.mu_cu < 0.16);
        assert(res.utilizationRatio < 1.0);
        // Section As_provided en m² (~ 8.3 cm²)
        assert(res.As_provided > 8.0e-4 && res.As_provided < 9.0e-4);
        assert(res.As_provided >= res.As_min);

        // Cas sous-dimensionné nécessitant armatures comprimées
        in.Med = 500.0e3; // 500 kNm -> dépasse mu_lu = 0.372
        auto resHeavy = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        assert(resHeavy.valid);
        assert(resHeavy.requiresCompressionSteel);
        assert(resHeavy.utilizationRatio > 1.0);

        // Calcul direct depuis les objets du modèle TSA (Section & Material)
        auto sec = TSA::Model::Section::rectangular(0.30, 0.50);
        auto mat = TSA::Model::Material::concreteC25_30();
        auto resModel = ConcreteDesignEC2::calculateFromModel(sec, mat, 120.0e3, 500.0e6, 0.05);
        assert(resModel.valid);
        assert(!resModel.requiresCompressionSteel);
        assert(resModel.As_provided > 6.0e-4);

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
        assert(std::abs(Nt_Rd - 669.28e3) < 1e2);

        double Mc_Rd = SteelDesignEC3::bendingResistance(Wel, fy);
        assert(std::abs(Mc_Rd - 45.66e3) < 1e2);

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
        assert(res.valid);
        assert(res.radiusOfGyration > 0.02 && res.radiusOfGyration < 0.025); // ~ 2.23 cm
        assert(res.slenderness > 170.0 && res.slenderness < 190.0);         // ~ 179
        assert(res.reducedSlenderness > 1.8 && res.reducedSlenderness < 2.1); // ~ 1.91
        assert(res.chi > 0.20 && res.chi < 0.26);                          // ~ 0.228
        assert(res.Nb_Rd > 140.0e3 && res.Nb_Rd < 170.0e3);                // ~ 152 kN
        assert(res.utilizationRatio < 1.0);
        assert(res.pass);

        // Cas charge excessive au-delà de Nb,Rd
        in.Ned = 200.0e3; // 200 kN > 152 kN
        auto resFail = SteelDesignEC3::calculateBuckling(in);
        assert(resFail.valid);
        assert(resFail.utilizationRatio > 1.0);
        assert(!resFail.pass);

        // Calcul direct depuis une barre du modèle TSA
        auto secIpe = TSA::Model::Section::ipe(200);
        auto matSteel = TSA::Model::Material::steelS235();
        auto resBar = SteelDesignEC3::calculateFromBar(secIpe, matSteel, 4.0, 1.0, 100.0e3, false);
        assert(resBar.valid);
        assert(resBar.pass);

        passed++;
        std::cout << "PASSED" << std::endl;
    }

    return true;
}
