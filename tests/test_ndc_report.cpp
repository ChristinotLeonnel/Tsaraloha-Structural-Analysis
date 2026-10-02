#include "test_common.h"

#include "NDC/NDCDocumentModel.h"
#include "NDC/ReportConfiguration.h"
#include "NDC/ReportTemplate.h"
#include "NDC/NormativeReferenceDetector.h"
#include "NDC/ResultAnalyzer.h"
#include "NDC/NDCGenerator.h"
#include "NDC/NDCExporter.h"
#include "NDC/ReportManager.h"

#include "Analysis/OpenSeesSolver.h"
#include "Analysis/ResultsModel.h"
#include "Model/Model.h"
#include "Model/Section.h"
#include "Model/Material.h"
#include "Model/Load/LoadManager.h"
#include "Model/Beam.h"
#include "Model/Column.h"
#include "Model/SupportDefinition.h"

#include <QFile>
#include <QDir>
#include <cmath>

using namespace TSA::Model;
using namespace TSA::Analysis;
using namespace TSA::NDC;

/**
 * @brief Suite de tests unitaires et de vérification analytique du module Note de Calcul (NDC).
 * Conforme aux exigences IEEE Std 1063 et Eurocodes (EN 1990/1991/1992/1993).
 */
bool runSuite_NDCReport(int& passed)
{
    // TEST 85: Benchmark Analytique & Localisation Spatiale 3D des Extrema
    // Poutre bi-appuyée L = 6.0 m sous charge ponctuelle centrée P = 50.0 kN
    // Solution analytique exacte :
    // - Réactions aux appuis : Ra = Rb = P / 2 = 25.0 kN
    // - Moment fléchissant maximal : Mmax = P * L / 4 = 75.0 kNm à x = 3.0 m
    // - Effort tranchant maximal : Vmax = 25.0 kN
    // - Coordonnées 3D globales du moment max : (3.0, 0.0, 0.0)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Pinned);

        int n2 = model.addNode(6.0, 0.0, 0.0);
        model.getNode(n2)->setSupportType(SupportType::Roller);

        Section sec = Section::ipe(300);
        Material mat = Material::steelS355();
        int b1 = model.addBar(n1, n2, sec, mat, BarRole::Beam);
        TEST_CHECK(b1 > 0, "Benchmark beam creation failed");

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "Charge_Concentree_50kN", LoadCaseCategory::Live, false, 1.0));

        // Charge ponctuelle 50 kN au milieu de la travée (relatif 0.5 -> x = 3.0 m)
        MemberLoad ml = MemberLoad::pointOnMember(b1, lcId, 50.0, 0.5, LoadDirection::Gravity, LoadCoordSystem::Global, true, "P_50kN");
        lm.addMemberLoad(ml);

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.includeSelfWeight = false;
        params.targetLoadCaseId = lcId;

        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        if (!ok)
        {
            std::cout << "  Solver error: " << err.toStdString() << "\n";
            std::cout << "  Journal log:\n" << solver.results().journalLog() << "\n";
        }
        TEST_CHECK(ok, "OpenSees solve for benchmark beam must succeed");

        const auto& res = solver.results();
        TEST_CHECK(res.isValid(), "ResultsModel must be valid");

        // Vérification des réactions
        const auto* r1 = res.getNodeReaction(n1);
        const auto* r2 = res.getNodeReaction(n2);
        TEST_CHECK(r1 != nullptr && r2 != nullptr, "Reactions must be computed");
        TEST_CHECK(std::abs(r1->rz - 25.0) < 0.5, "Reaction at N1 must equal 25.0 kN");
        TEST_CHECK(std::abs(r2->rz - 25.0) < 0.5, "Reaction at N2 must equal 25.0 kN");

        // Analyse détaillée des extrema via ResultAnalyzer
        auto resultsPtr = std::make_shared<ResultsModel>(solver.results());
        MostStressedSummary summary = ResultAnalyzer::analyzeExtrema(model, resultsPtr);

        // 1. Moment fléchissant maximal (My dans le plan vertical sous charge gravitaire)
        const auto& extM = (summary.absMaxBendingMy.value > summary.absMaxBendingMz.value) 
                            ? summary.absMaxBendingMy 
                            : summary.absMaxBendingMz;
        double computedMmax = extM.value;
        std::cout << "  [Benchmark] Computed M_max = " << computedMmax << " kNm (Analytical = 75.0 kNm)\n";
        TEST_CHECK(std::abs(computedMmax - 75.0) < 1.0, "Computed M_max must match analytical value of 75.0 kNm");
        TEST_CHECK(std::abs(extM.localPositionX - 3.0) < 0.1, "Extremum station must be at x = 3.0 m");
        TEST_CHECK(std::abs(extM.relativePosition - 0.5) < 0.05, "Relative station must be 0.5");

        // 2. Coordonnées globales 3D (X, Y, Z)
        gp_Pnt pt3d = extM.globalCoords;
        std::cout << "  [Extremum 3D] Coords: (" << pt3d.X() << ", " << pt3d.Y() << ", " << pt3d.Z() << ")\n";
        TEST_CHECK(std::abs(pt3d.X() - 3.0) < 0.1, "Global X coordinate of Mmax must be at 3.0 m");
        TEST_CHECK(std::abs(pt3d.Y() - 0.0) < 1e-4, "Global Y coordinate of Mmax must be 0.0");
        TEST_CHECK(std::abs(pt3d.Z() - 0.0) < 1e-4, "Global Z coordinate of Mmax must be 0.0");

        // 3. Effort tranchant maximal (Vz ou Vy)
        double computedVmax = std::max(summary.absMaxShearVz.value, summary.absMaxShearVy.value);
        std::cout << "  [Benchmark] Computed V_max = " << computedVmax << " kN (Analytical = 25.0 kN)\n";
        TEST_CHECK(std::abs(computedVmax - 25.0) < 1.0, "Computed V_max must match analytical value of 25.0 kN");

        passed++;
    }

    // TEST 86: Sérialisation JSON & Gestion des Fichiers Templates (.tsareport)
    {
        ReportConfiguration cfg;
        cfg.projectTitle = "Viaduc de Tsaraloha";
        cfg.engineerName = "Antigravity Structural Lead";
        cfg.organization = "Tsaraloha Engineering Group";
        cfg.projectNumber = "PRJ-2026-001";
        cfg.pageFormat = PageFormat::A3;
        cfg.pageOrientation = PageOrientation::Landscape;
        cfg.primaryColor = "#0055bb";
        cfg.includeExtremaSpatialTable = true;
        cfg.includeDetailedElementTables = true;
        cfg.includeModalAnalysis = true;
        cfg.includeEurocodeDesignChecks = true;

        QString tmpPath = QDir::tempPath() + "/test_roundtrip.tsareport";
        TEST_CHECK(cfg.saveToFile(tmpPath), "Saving .tsareport configuration must succeed");

        ReportConfiguration loadedCfg;
        TEST_CHECK(loadedCfg.loadFromFile(tmpPath), "Loading .tsareport configuration must succeed");

        TEST_CHECK(loadedCfg.projectTitle == "Viaduc de Tsaraloha", "Loaded title must match");
        TEST_CHECK(loadedCfg.engineerName == "Antigravity Structural Lead", "Loaded engineer must match");
        TEST_CHECK(loadedCfg.projectNumber == "PRJ-2026-001", "Loaded project number must match");
        TEST_CHECK(loadedCfg.pageFormat == PageFormat::A3, "Loaded paper size must match");
        TEST_CHECK(loadedCfg.pageOrientation == PageOrientation::Landscape, "Loaded orientation must match");
        TEST_CHECK(loadedCfg.primaryColor == "#0055bb", "Loaded primary color must match");
        TEST_CHECK(loadedCfg.includeExtremaSpatialTable == true, "Loaded extrema toggle must match");
        TEST_CHECK(loadedCfg.includeDetailedElementTables == true, "Loaded detailed tables toggle must match");

        QFile::remove(tmpPath);
        passed++;
    }

    // TEST 87: Presets de Modèles (Bureau d'Études, Eurocode, Universitaire, Minimal)
    {
        // 1. Bureau d'Études
        auto be = ReportTemplate::createTemplate(ReportTemplateType::BureauEtudes);
        TEST_CHECK(be.includeCoverPage, "Bureau d'études template must include cover page");
        TEST_CHECK(be.includeToc, "Bureau d'études template must include TOC");
        TEST_CHECK(be.includeMostStressedSummary, "Bureau d'études template must include most stressed summary");
        TEST_CHECK(!be.primaryColor.isEmpty(), "Primary color must be set");

        // 2. Eurocode
        auto ec = ReportTemplate::createTemplate(ReportTemplateType::Eurocode);
        TEST_CHECK(ec.includeStandards, "Eurocode template must include standards chapter");
        TEST_CHECK(ec.includeMaterials, "Eurocode template must include materials chapter");

        // 3. Universitaire
        auto uni = ReportTemplate::createTemplate(ReportTemplateType::Universitaire);
        TEST_CHECK(uni.includeCalculationMethod, "University template must include calculation method");
        TEST_CHECK(uni.includeModelVerification, "University template must include model verification");

        // 4. Minimal
        auto min = ReportTemplate::createTemplate(ReportTemplateType::Minimal);
        TEST_CHECK(!min.includeToc, "Minimal template should omit TOC");
        TEST_CHECK(!min.includeLof, "Minimal template should omit LOF");
        TEST_CHECK(!min.includeStandards, "Minimal template should omit standards");

        passed++;
    }

    // TEST 88: Détecteur Automatique de Références Normatives (Eurocodes & IEEE Std 1063)
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        int n2 = model.addNode(0.0, 0.0, 3.5);
        int n3 = model.addNode(5.0, 0.0, 3.5);

        Section secPoteau = Section::rectangular(0.30, 0.30);
        Material matBeton = Material::concreteC25_30();
        model.addBar(n1, n2, secPoteau, matBeton, BarRole::Column);

        Section secPoutre = Section::ipe(240);
        Material matAcier = Material::steelS355();
        model.addBar(n2, n3, secPoutre, matAcier, BarRole::Beam);

        auto refs = NormativeReferenceDetector::detectApplicableStandards(model);
        TEST_CHECK(!refs.empty(), "Normative detector must identify applicable standards");

        bool hasEN1990 = false;
        bool hasEN1992 = false;
        bool hasEN1993 = false;
        for (const auto& r : refs)
        {
            if (r.code.contains("1990")) hasEN1990 = true;
            if (r.code.contains("1992")) hasEN1992 = true;
            if (r.code.contains("1993")) hasEN1993 = true;
        }

        TEST_CHECK(hasEN1990, "EN 1990 (Base de calcul) must be detected");
        TEST_CHECK(hasEN1992, "EN 1992 (Calcul des structures en béton) must be detected");
        TEST_CHECK(hasEN1993, "EN 1993 (Calcul des structures en acier) must be detected");

        auto bib = NormativeReferenceDetector::generateBibliography(model);
        TEST_CHECK(!bib.empty(), "Official bibliography must be generated");
        bool hasEurocodeRef = false;
        bool hasOpenSeesRef = false;
        for (const auto& entry : bib)
        {
            TEST_CHECK(!entry.title.isEmpty(), "Bibliographic entry title must not be empty");
            TEST_CHECK(!entry.url.isEmpty(), "Bibliographic entry URL must not be empty");
            TEST_CHECK(!entry.publisher.isEmpty(), "Bibliographic publisher must not be empty");
            if (entry.citationKey.contains("EN1990") || entry.citationKey.contains("EN1992") || entry.citationKey.contains("EN1993"))
            {
                hasEurocodeRef = true;
                TEST_CHECK(entry.url.contains("europa.eu") || entry.url.contains("cen.eu"), "Eurocode URL must point to official portal");
            }
            if (entry.citationKey.contains("OpenSees"))
            {
                hasOpenSeesRef = true;
            }
        }
        TEST_CHECK(hasEurocodeRef, "Bibliography must contain Eurocode official reference");
        TEST_CHECK(hasOpenSeesRef, "Bibliography must contain OpenSees technical reference");

        passed++;
    }

    // TEST 89: ReportManager, Ingestion de Snapshots & Export Vectoriel PDF / HTML
    {
        Model model;
        int n1 = model.addNode(0.0, 0.0, 0.0);
        model.getNode(n1)->setSupportType(SupportType::Fixed);
        int n2 = model.addNode(4.0, 0.0, 0.0);
        Section sec = Section::rectangular(0.20, 0.40);
        Material mat = Material::concreteC30_37();
        model.addBar(n1, n2, sec, mat, BarRole::Beam);

        auto& lm = model.loadManager();
        int lcId = lm.addLoadCase(LoadCase(1, "Permanent", LoadCaseCategory::Dead, true, 1.0));

        OpenSeesSolver solver;
        AnalysisParameters params;
        params.type = AnalysisType::LinearStatic;
        params.useKiloNewtons = true;
        params.targetLoadCaseId = lcId;
        QString err;
        bool ok = solver.solveSynchronous(model, params, &err);
        TEST_CHECK(ok, "Solver execution must succeed for report test");

        auto resultsPtr = std::make_shared<ResultsModel>(solver.results());

        ReportManager mgr;
        mgr.setModel(&model);
        mgr.setResultsModel(resultsPtr);

        // Injection d'un provider de capture 3D synthétique (découplage de l'UI)
        int snapshotCount = 0;
        mgr.setSnapshotProvider([&snapshotCount](const QString& /*viewType*/, int w, int h) -> QImage {
            snapshotCount++;
            QImage img(w, h, QImage::Format_RGB32);
            img.fill(qRgb(245, 248, 255));
            return img;
        });

        NDCDocument doc = mgr.generateReport();
        TEST_CHECK(!doc.chapters.empty(), "ReportManager must generate populated chapters");
        TEST_CHECK(snapshotCount > 0, "Snapshot provider must have been invoked for 3D views");
        TEST_CHECK(!doc.allFigures().empty(), "Generated document must contain registered 3D figures");
        TEST_CHECK(!doc.allTables().empty(), "Generated document must contain registered technical tables");

        // Export HTML
        QString htmlFile = QDir::tempPath() + "/tsa_report_test.html";
        QString htmlErr;
        bool htmlOk = mgr.exportHtml(htmlFile, &htmlErr);
        TEST_CHECK(htmlOk, "HTML export via ReportManager must succeed");
        QFile fHtml(htmlFile);
        TEST_CHECK(fHtml.exists() && fHtml.size() > 500, "Exported HTML file must exist and have content");
        fHtml.remove();

        // Export PDF vectoriel
        QString pdfFile = QDir::tempPath() + "/tsa_report_test.pdf";
        QString pdfErr;
        bool pdfOk = mgr.exportPdf(pdfFile, &pdfErr);
        TEST_CHECK(pdfOk, "PDF export via ReportManager must succeed");
        QFile fPdf(pdfFile);
        TEST_CHECK(fPdf.exists() && fPdf.size() > 500, "Exported PDF file must exist and have content");
        fPdf.remove();

        passed++;
    }

    return true;
}
