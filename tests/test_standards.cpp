#include "test_common.h"
#include "../src/Standards/NormativeTypes.h"
#include "../src/Standards/RequirementsCatalog.h"
#include "../src/Standards/NationalAnnexConfig.h"
#include "../src/Standards/ExternalLibraryCatalog.h"
#include "../src/Standards/DataDefinition.h"
#include "../src/Standards/ModelValidator.h"

using namespace TSA::Standards;

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

    return true;
}
