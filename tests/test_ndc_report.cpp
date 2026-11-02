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
#include "Standards/Design/ConcreteDesignEC2.h"
#include "UI/Dock/ResultsDockWidget.h"
#include "UI/Properties/NodePropertiesView.h"
#include "Geometry/DiagramGeometry.h"
#include <QLabel>
#include "Standards/Design/SteelDesignEC3.h"

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
        MemberLoad ml = MemberLoad::pointOnMember(b1, lcId, -50.0, 0.5, LoadDirection::GlobalZ, true, "P_50kN");
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

        // Structure bois : EN 1995 cité (le bois était détecté mais jamais rapporté).
        {
            Model timber;
            const int t1 = timber.addNode(0.0, 0.0, 0.0);
            const int t2 = timber.addNode(4.0, 0.0, 0.0);
            timber.addBar(t1, t2, Section::rectangular(0.10, 0.20), Material::timberC24(), BarRole::Beam);
            bool hasEN1995 = false;
            for (const auto& r : NormativeReferenceDetector::detectApplicableStandards(timber))
                hasEN1995 |= r.code == "EN 1995-1-1";
            bool citesEN1995 = false;
            for (const auto& entry : NormativeReferenceDetector::generateBibliography(timber))
                citesEN1995 |= entry.citationKey == "[CEN-EN1995]";
            TEST_CHECK(hasEN1995 && citesEN1995, "EN 1995 (Calcul des structures en bois) must be detected and cited");
        }

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

    // TEST 492: vérifications EC2 / EC3 indépendantes des unités du calcul (kN ou N).
    // Mêmes efforts physiques exprimés en kN puis en N (OpenSees, option « kN » décochée) : la note de calcul
    // (présentation actuelle et historique) doit afficher les mêmes efforts et les mêmes ratios. Auparavant, les
    // efforts étaient multipliés par 1000 sans lire les unités : en N, ratios faux d'un facteur 1000.
    {
        Model model;
        const int n1 = model.addNode(0.0, 0.0, 0.0);
        const int n2 = model.addNode(6.0, 0.0, 0.0);
        const int beamId = model.addBar(n1, n2, Section::rectangular(0.30, 0.50, "R30x50"), Material::concreteC25_30(), BarRole::Beam);
        const int n3 = model.addNode(10.0, 0.0, 0.0);
        const int n4 = model.addNode(10.0, 0.0, 3.0);
        const int colId = model.addColumn(n3, n4);
        TEST_CHECK(beamId > 0 && colId > 0, "Test 492 : création des barres");
        model.getColumn(colId)->setSection(Section::hea(200));
        model.getColumn(colId)->setMaterial(Material::steelS355());

        // Efforts en kN / kN·m : poutre M_y max = 60,3 kN·m en travée, poteau N = −212,7 kN (compression)
        auto makeResults = [&](double scale, bool kN) {
            auto r = std::make_shared<ResultsModel>();
            r->setUnits(UnitSystem::fromKiloNewtons(kN));
            r->setAnalysisType(AnalysisType::LinearStatic);
            AnalysisExecutionMetadata meta;
            meta.solverEngine = "Test";
            meta.engineId = "test";
            r->setExecutionMetadata(meta);
            ElementResults eb;
            eb.elementId = beamId;
            eb.kind = StructuralElementKind::Beam;
            eb.length = 6.0;
            eb.endForces.position = 6.0;
            StationForces mid;
            mid.position = 3.0;
            mid.My = 60.3 * scale;
            eb.intermediateStations.push_back(mid);
            r->setElementResults(eb);
            ElementResults ec;
            ec.elementId = colId;
            ec.kind = StructuralElementKind::Column;
            ec.length = 3.0;
            ec.startForces.N = -212.7 * scale;
            ec.endForces.N = -212.7 * scale;
            ec.endForces.position = 3.0;
            r->setElementResults(ec);
            NodeReaction ra;
            ra.rz = 212.7 * scale;
            r->setNodeReaction(n3, ra);
            ra.rz = 40.2 * scale;
            r->setNodeReaction(n1, ra);
            r->setNodeDisplacement(n3, NodeDisplacement {});   // nœud calculé : le panneau du nœud affiche ses réactions
            GlobalEquilibrium eq;   // résultantes (dock Résultats : réaction totale)
            eq.reactionFz = 252.9 * scale;
            eq.appliedFz = -252.9 * scale;
            r->setEquilibrium(eq);
            r->setValid(true);
            return r;
        };
        const auto rKN = makeResults(1.0, true);
        const auto rN = makeResults(1000.0, false);
        TEST_CHECK(rKN->units().forceToNewtons() == 1000.0 && rN->units().forceToNewtons() == 1.0, "Test 492 : facteurs vers le newton");
        TEST_CHECK(rKN->units().forceToKiloNewtons() == 1.0 && rN->units().forceToKiloNewtons() == 1.0e-3, "Test 492 : facteurs vers le kilonewton");

        auto designTables = [](const NDCDocument& doc) {
            std::vector<NDCTable> out;
            for (const auto& ch : doc.chapters)
                for (const auto& sec : ch.sections)
                    for (const auto& t : sec.tables)
                        if (ch.title.contains(QStringLiteral("Vérifications réglementaires")) || t.caption.contains(QStringLiteral("(EC2)")) ||
                            t.caption.contains(QStringLiteral("(EC3)")))
                            out.push_back(t);
            return out;
        };
        for (const NoteLayout layout : { NoteLayout::Simplified, NoteLayout::Legacy })
        {
            ReportConfiguration cfg;
            cfg.noteLayout = layout;
            cfg.includeEurocodeDesignChecks = true;
            const auto tKN = designTables(NDCGenerator::generate(model, rKN, cfg));
            const auto tN = designTables(NDCGenerator::generate(model, rN, cfg));
            const char* name = layout == NoteLayout::Legacy ? "historique" : "actuelle";
            // Note entière (détaillée, et historique : présentation par défaut) : tous les tableaux (efforts, réactions,
            // contraintes…) affichés en kN quelle que soit l'unité du calcul.
            {
                cfg.noteLayout = layout == NoteLayout::Simplified ? NoteLayout::Detailed : NoteLayout::Legacy;
                const auto aKN = NDCGenerator::generate(model, rKN, cfg).allTables();
                const auto aN = NDCGenerator::generate(model, rN, cfg).allTables();
                TEST_CHECK(aKN.size() == aN.size(), std::string("Test 492 : même nombre de tableaux en kN et en N (note entière ") + name + ")");
                for (std::size_t i = 0; i < aKN.size() && i < aN.size(); ++i)
                    TEST_CHECK(aKN[i].rows == aN[i].rows, std::string("Test 492 : tableau « ") + aKN[i].caption.toStdString() + "» identique en kN et en N (note entière " + name + ")");
                cfg.noteLayout = layout;
            }
            TEST_CHECK(tKN.size() >= 2, std::string("Test 492 : tableaux EC2 et EC3 présents (note ") + name + ")");
            TEST_CHECK(tKN.size() == tN.size(), std::string("Test 492 : même nombre de tableaux en kN et en N (note ") + name + ")");
            for (std::size_t i = 0; i < tKN.size() && i < tN.size(); ++i)
                TEST_CHECK(tKN[i].rows == tN[i].rows, std::string("Test 492 : tableau « ") + tKN[i].caption.toStdString() + "» identique en kN et en N (note " + name + ")");
        }

        // BUG-117 (P2) : affichages des résultats indépendants de l'unité du calcul — dock Résultats, propriétés du nœud (réactions
        // en kN quelle que soit l'unité) ; unité des diagrammes = unité des valeurs tracées (kN·m ou N·m)
        for (const auto& results : { rKN, rN })
        {
            const std::string unitName = results->units().force;
            TSA::UI::ResultsDockWidget dock;
            dock.setModel(&model);
            dock.setResultsModel(results);
            TSA::UI::NodePropertiesView node(&model);
            node.setResultsModel(results);
            node.setElementId(n3);
            QStringList texts;
            for (const QWidget* w : { static_cast<const QWidget*>(&dock), static_cast<const QWidget*>(&node) })
                for (const QLabel* l : w->findChildren<QLabel*>()) texts << l->text();
            const QString all = texts.join(QLatin1Char('\n'));
            TEST_CHECK(all.contains(QStringLiteral("Réaction totale Rz : 252.9 kN")) && all.contains(QStringLiteral("212.70 kN")) &&
                           !all.contains(QStringLiteral("212700")) && !all.contains(QStringLiteral("252900")),
                       "Test 492 : dock Résultats et réactions du nœud en kN pour un calcul en " << unitName);
        }
        TEST_CHECK(TSA::Geometry::DiagramGeometry::diagramUnit(TSA::Geometry::DiagramType::BendingMy, rN->units().force != "N") == QStringLiteral("Nm") &&
                       TSA::Geometry::DiagramGeometry::diagramUnit(TSA::Geometry::DiagramType::BendingMy, rKN->units().force != "N") == QStringLiteral("kNm"),
                   "Test 492 : unité des diagrammes = unité du calcul");

        // Valeur attendue indépendante de la note : ratio EC2 calculé directement en SI
        const auto direct = TSA::Standards::Design::ConcreteDesignEC2::calculateFromModel(Section::rectangular(0.30, 0.50), Material::concreteC25_30(), 60.3e3);
        ReportConfiguration cfg;
        cfg.noteLayout = NoteLayout::Simplified;
        const QString expected = QString::number(direct.utilizationRatio * 100.0, 'f', 1);
        bool found = false;
        for (const auto& t : designTables(NDCGenerator::generate(model, rN, cfg)))
            for (const auto& row : t.rows)
                if (row.size() > 4 && row[0].contains(QStringLiteral("Poutre")) && row[4].startsWith(expected)) found = true;
        TEST_CHECK(direct.valid && found, "Test 492 : ratio EC2 de la note (calcul en N) = ratio calculé directement en SI");
        passed++;
    }

    // TEST 584 (BUG-152) : flambement EC3 vérifié sur les deux axes principaux, verdict de la note historique (« Legacy »,
    // présentation par défaut). Avant : axe fort seul, IPE 200 de 5 m sous 300 kN déclaré CONFORME (N_b,Rd = 809 kN imprimé
    // au lieu de 105 kN sur l'axe faible).
    {
        using TSA::Standards::Design::SteelDesignEC3;
        // Calcul à la main (S355, courbe b sur z-z) : i_z = 2,24 cm, λ̄ = 2,92, χ = 0,1045, N_b,Rd = 0,1045 × 28,5 cm² × 355 MPa ≈ 106 kN
        const auto g = SteelDesignEC3::calculateFromBarGoverning(Section::ipe(200), Material::steelS355(), 5.0, 1.0, 300e3);
        const auto strong = SteelDesignEC3::calculateFromBar(Section::ipe(200), Material::steelS355(), 5.0, 1.0, 300e3, true);
        TEST_CHECK(g.valid && g.axis == QStringLiteral("z-z") && g.Nb_Rd > 95e3 && g.Nb_Rd < 115e3 && !g.pass,
                   "Test 584 : IPE 200, 5 m, 300 kN — axe déterminant z-z, N_b,Rd = " << g.Nb_Rd / 1000.0 << " kN, non conforme");
        TEST_CHECK(strong.Nb_Rd > 700e3 && std::abs(g.Nb_Rd_otherAxis - strong.Nb_Rd) < 1.0,
                   "Test 584 : l'autre axe (y-y, " << strong.Nb_Rd / 1000.0 << " kN) est donné, non retenu");
        // Cornière : axes géométriques non principaux, flambement sur l'axe principal faible v-v (Iv < Iz)
        const Section angle = Section::angle(0.1, 0.1, 0.01);
        const auto ga = SteelDesignEC3::calculateFromBarGoverning(angle, Material::steelS235(), 2.0, 1.0, 50e3);
        const auto za = SteelDesignEC3::calculateFromBar(angle, Material::steelS235(), 2.0, 1.0, 50e3, false);
        TEST_CHECK(std::abs(angle.iyz()) > 0.0 && ga.valid && ga.axis == QStringLiteral("v-v") && ga.Nb_Rd < za.Nb_Rd,
                   "Test 584 : cornière — axe v-v déterminant (" << ga.Nb_Rd / 1000.0 << " kN < " << za.Nb_Rd / 1000.0 << " kN sur z-z)");

        // Note de calcul : poteau IPE 200 de 5 m comprimé à 300 kN → NON CONFORME dans la note historique et dans la note détaillée ;
        // poteau tendu : aucun flambement vérifié
        Model model;
        const int a = model.addNode(0.0, 0.0, 0.0), b = model.addNode(0.0, 0.0, 5.0);
        const int c = model.addNode(4.0, 0.0, 0.0), d = model.addNode(4.0, 0.0, 5.0);
        const int compressed = model.addColumn(a, b), tensioned = model.addColumn(c, d);
        for (int id : { compressed, tensioned })
        {
            model.getColumn(id)->setSection(Section::ipe(200));
            model.getColumn(id)->setMaterial(Material::steelS355());
        }
        auto r = std::make_shared<ResultsModel>();
        r->setUnits(UnitSystem::fromKiloNewtons(true));
        r->setAnalysisType(AnalysisType::LinearStatic);
        AnalysisExecutionMetadata meta;
        meta.solverEngine = "Test";
        meta.engineId = "test";
        r->setExecutionMetadata(meta);
        for (const auto& [id, n] : { std::pair<int, double>{ compressed, -300.0 }, std::pair<int, double>{ tensioned, 300.0 } })
        {
            ElementResults e;
            e.elementId = id;
            e.kind = StructuralElementKind::Column;
            e.length = 5.0;
            e.startForces.N = n;
            e.endForces.N = n;
            e.endForces.position = 5.0;
            r->setElementResults(e);
        }
        r->setValid(true);
        for (const NoteLayout layout : { NoteLayout::Legacy, NoteLayout::Detailed })
        {
            ReportConfiguration cfg;
            cfg.noteLayout = layout;
            cfg.includeEurocodeDesignChecks = true;
            int rows = 0, nonConforming = 0, conforming = 0;
            for (const auto& t : NDCGenerator::generate(model, r, cfg).allTables())
            {
                if (!t.caption.contains(QStringLiteral("EC3")) && !t.caption.contains(QStringLiteral("acier comprimées"))) continue;
                for (const auto& row : t.rows)
                {
                    ++rows;
                    const QString line = row.join(QLatin1Char(' '));
                    if (line.contains(QStringLiteral("NON CONFORME"))) ++nonConforming;
                    else if (line.contains(QStringLiteral("CONFORME"))) ++conforming;
                    TEST_CHECK(line.contains(QStringLiteral("z-z")), "Test 584 : axe déterminant affiché (" << line.toStdString() << ")");
                }
            }
            const char* name = layout == NoteLayout::Legacy ? "historique" : "détaillée";
            TEST_CHECK(rows == 1 && nonConforming == 1 && conforming == 0,
                       "Test 584 : note " << name << " — poteau comprimé NON CONFORME, poteau tendu non vérifié (" << rows << " ligne(s), "
                                          << nonConforming << " non conforme(s), " << conforming << " conforme(s))");
        }
        passed++;
    }

    return true;
}
