// Suite « benchmarks » : banc de validation tests/benchmarks (format tsa.benchmark/1, docs/BENCHMARKS.md).
//   367  tous les cas RDM et FEA par la chaîne de calcul réelle (OpenSees, Custom2D) : aucune vérification en échec ni
//        indisponible ; rapport écrit dans <build>/validation-reports (historique conservé) ;
//   368  outillage : évaluateur de formules, tolérance mixte, fichiers invalides refusés, références externes (CSV,
//        JSON) importées et comparées (une valeur fausse est détectée), rapports Markdown / CSV / JSON.

#include "test_common.h"

#include "App/ProductInfo.h"
#include "Validation/BenchmarkSuite.h"
#include "NDC/NDCExporter.h"
#include "NDC/ReportManager.h"
#include "Model/Model.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

using namespace TSA::Validation;

namespace
{
QString benchmarkDir() { return QStringLiteral(TSA_SOURCE_DIR "/tests/benchmarks"); }
} // namespace

bool runSuite_Benchmarks(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 367 : banc de validation complet
    // -------------------------------------------------------------------------
    {
        const auto files = benchmarkFiles(benchmarkDir());
        TEST_CHECK(files.size() >= 22, "Test 367: cas de référence présents (" << files.size() << ")");
        BenchmarkReport report;
        report.tsaVersion = TSA::Product::version().toStdString();
        report.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
        int rdm = 0, fea = 0;
        for (const auto& f : files)
        {
            BenchmarkCase c;
            std::vector<std::string> errors;
            TEST_CHECK(loadBenchmarkFile(f, &c, &errors), "Test 367: " << f.toStdString() << " valide (" << (errors.empty() ? "" : errors.front()) << ")");
            (c.category == "fea" ? fea : rdm)++;
            for (const auto& o : runBenchmark(c))
            {
                report.cases.push_back(o);
                if (o.skipped) continue;
                TEST_CHECK(o.ran, "Test 367: " << o.caseId << " (" << o.engine << ") calculé : " << o.message);
                for (const auto& k : o.checks)
                    TEST_CHECK(k.passed, "Test 367: " << o.caseId << " (" << o.engine << ") " << k.quantity << " = " << k.value << ", référence " << k.reference << " ("
                                                      << k.formula << ", tolérance " << k.tolerance.abs << " + " << k.tolerance.rel << "·|réf|)");
            }
        }
        QString err;
        TEST_CHECK(writeReport(report, QDir(QCoreApplication::applicationDirPath()).filePath("validation-reports"), &err), "Test 367: rapport écrit (" << err.toStdString() << ")");
        std::cout << "  " << rdm << " cas RDM, " << fea << " cas FEA ; " << report.checksPassed() << " vérifications réussies, " << report.checksFailed() << " en échec, "
                  << report.casesSkipped() << " cas ignoré(s)" << std::endl;
        TEST_CHECK(report.checksFailed() == 0 && report.checksUnavailable() == 0 && report.checksPassed() >= 100, "Test 367: toutes les vérifications réussies");
        std::cout << "[PASS] Test 367: Banc de validation RDM et FEA (" << report.checksPassed() << " vérifications)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 368 : outillage du banc
    // -------------------------------------------------------------------------
    {
        std::string e;
        TEST_CHECK(std::abs(evaluateExpression("-5*q*L^4/(384*E*I)", { { "q", 10 }, { "L", 6 }, { "E", 2.1e8 }, { "I", 1e-3 } }, &e) + 5.0 * 10 * 1296 / (384 * 2.1e8 * 1e-3)) < 1e-15 && e.empty(),
                   "Test 368: formule évaluée");
        TEST_CHECK(std::abs(evaluateExpression("-2^2", {}, &e) + 4.0) < 1e-15 && std::abs(evaluateExpression("2^3^2", {}, &e) - 512.0) < 1e-12 &&
                       std::abs(evaluateExpression("sqrt(16)+tanh(0)+pi", {}, &e) - (4.0 + 3.14159265358979323846)) < 1e-12,
                   "Test 368: priorités (−2² = −4, 2^3^2 = 512) et fonctions");
        evaluateExpression("2*x", {}, &e);
        TEST_CHECK(!e.empty(), "Test 368: paramètre inconnu signalé (" << e << ")");
        Tolerance t { 0.01, 1e-6 };
        TEST_CHECK(t.accepts(1.009, 1.0) && !t.accepts(1.02, 1.0) && t.accepts(5e-7, 0.0) && !t.accepts(std::nan(""), 1.0), "Test 368: tolérance mixte ε_abs + ε_rel·|réf|");

        // Fichier invalide : formule erronée, grandeur inconnue, sans source
        QJsonObject bad { { "format", "tsa.benchmark/1" }, { "id", "BAD" }, { "title", "x" }, { "model", QJsonObject { { "nodes", QJsonArray {} } } },
                          { "checks", QJsonArray { QJsonObject { { "quantity", "foo:1" }, { "expected", "2*y" } } } } };
        std::vector<std::string> errors;
        TEST_CHECK(!parseBenchmark(bad, nullptr, &errors) && errors.size() >= 3, "Test 368: cas invalide refusé (" << errors.size() << " erreurs)");

        // Références externes : CSV (une valeur juste, une fausse) et JSON
        QTemporaryDir tmp;
        const QString csv = tmp.filePath("ref.csv");
        {
            QFile f(csv);
            TEST_CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text), "Test 368: écriture CSV");
            f.write("case,quantity,value,unit,source,rel_tol,abs_tol\n"
                    "# référence indépendante (calcul manuel) et valeur volontairement fausse\n"
                    "RDM-04,reaction:1:fz,25,kN,calcul manuel,1e-6,1e-9\n"
                    "RDM-04,node:2:uz,-0.01,m,valeur fausse,0.01,0\n");
        }
        errors.clear();
        const auto refs = loadExternalReferences(csv, &errors);
        TEST_CHECK(refs.size() == 2 && errors.empty(), "Test 368: références CSV lues (" << refs.size() << ")");
        BenchmarkCase c;
        TEST_CHECK(loadBenchmarkFile(benchmarkDir() + "/rdm/RDM-04.json", &c, nullptr), "Test 368: RDM-04 lu");
        RunOptions opt;
        opt.engines = { "opensees" };
        opt.external = refs;
        const auto out = runBenchmark(c, opt);
        TEST_CHECK(out.size() == 1, "Test 368: un moteur");
        int good = 0, wrong = 0;
        for (const auto& k : out[0].checks)
        {
            if (k.source == "calcul manuel") good += k.passed;
            if (k.source == "valeur fausse") wrong += !k.passed && k.available;
        }
        TEST_CHECK(good == 1 && wrong == 1, "Test 368: comparaison externe — valeur juste acceptée, valeur fausse détectée");
        const QString json = tmp.filePath("ref.json");
        {
            QFile f(json);
            TEST_CHECK(f.open(QIODevice::WriteOnly), "Test 368: écriture JSON");
            f.write(QJsonDocument(QJsonObject { { "format", "tsa.reference/1" }, { "source", "calcul manuel" },
                                                { "references", QJsonArray { QJsonObject { { "case", "RDM-04" }, { "quantity", "reaction:3:fz" }, { "value", 25.0 } } } } })
                        .toJson());
        }
        TEST_CHECK(loadExternalReferences(json, nullptr).size() == 1, "Test 368: références JSON lues");

        BenchmarkReport r;
        r.tsaVersion = "test";
        r.cases = out;
        const QString md = reportMarkdown(r), cs = reportCsv(r);
        const QJsonObject js = reportJson(r);
        TEST_CHECK(md.contains("RÉUSSI") && md.contains("ÉCHEC") && md.contains("valeur fausse") && cs.startsWith("case,engine,quantity") &&
                       js.value("format").toString() == "tsa.validation-report/1" && js.value("summary").toObject().value("failed").toInt() == 1,
                   "Test 368: rapports Markdown, CSV et JSON");
        TEST_CHECK(writeReport(r, tmp.filePath("reports"), nullptr) && QDir(tmp.filePath("reports")).entryList({ "report-*.md" }).size() == 1 &&
                       QFile::exists(tmp.filePath("reports/latest.json")),
                   "Test 368: rapport horodaté et latest.json");
        // Note de calcul : chapitre de validation alimenté par le rapport (variable TSA_VALIDATION_REPORT), absence signalée sinon
        {
            Model m;
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&m);
            mgr.applyTemplate(TSA::NDC::ReportTemplateType::BureauEtudes);
            qputenv("TSA_VALIDATION_REPORT", tmp.filePath("absent.json").toUtf8());
            const QString none = TSA::NDC::NDCExporter::renderHtml(mgr.generateReport());
            qputenv("TSA_VALIDATION_REPORT", tmp.filePath("reports/latest.json").toUtf8());
            const QString with = TSA::NDC::NDCExporter::renderHtml(mgr.generateReport());
            qunsetenv("TSA_VALIDATION_REPORT");
            TEST_CHECK(with.contains("Validation du logiciel et traçabilité") && with.contains("RDM-04") && with.contains("Limites connues du logiciel"),
                       "Test 368: note de calcul — chapitre de validation (rapport repris)");
            TEST_CHECK(none.contains("AUCUN RAPPORT DE VALIDATION") || none.contains("RDM-0"), "Test 368: note de calcul — rapport absent signalé");
        }
        std::cout << "[PASS] Test 368: Outillage du banc (formules, tolérances, références externes, rapports)" << std::endl;
        ++passed;
    }
    return true;
}
