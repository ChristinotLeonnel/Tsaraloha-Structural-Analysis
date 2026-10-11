// TSA_Benchmark — banc de validation en ligne de commande (docs/BENCHMARKS.md).
//
//   TSA_Benchmark [--cases <dossier|fichier>] [--engine opensees|custom2d] [--reference <fichier.csv|.json>]
//                 [--out <dossier>] [--validate-only]
//
// Exécute les cas « tsa.benchmark/1 » par la chaîne de calcul réelle de TSA, les compare à leurs références
// analytiques et, si --reference est donné, à des résultats externes (ex. exportés d'un autre logiciel au format
// CSV « case,quantity,value,unit,source,rel_tol,abs_tol »). Écrit report-<date>.{md,csv,json} et latest.* dans
// --out (historique conservé). Code de retour : 0 si aucune vérification n'échoue, 1 sinon, 2 si un fichier est
// invalide.

#include "App/ProductInfo.h"
#include "Validation/BenchmarkSuite.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSysInfo>

#include <iostream>
#include <set>

using namespace TSA::Validation;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString cases = QStringLiteral(TSA_SOURCE_DIR "/tests/benchmarks"), reference, out = QDir::current().filePath("validation-reports");
    std::vector<std::string> engines;
    bool validateOnly = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        const QString a = args[i];
        auto next = [&]() { return i + 1 < args.size() ? args[++i] : QString(); };
        if (a == "--cases") cases = next();
        else if (a == "--engine") engines.push_back(next().toStdString());
        else if (a == "--reference") reference = next();
        else if (a == "--out") out = next();
        else if (a == "--validate-only") validateOnly = true;
        else
        {
            std::cout << "Usage : TSA_Benchmark [--cases <dossier|fichier>] [--engine <id>] [--reference <fichier>] [--out <dossier>] [--validate-only]\n";
            return a == "--help" ? 0 : 2;
        }
    }
    const std::vector<QString> files = QFileInfo(cases).isDir() ? benchmarkFiles(cases) : std::vector<QString> { cases };
    std::vector<BenchmarkCase> loaded;
    bool invalid = false;
    for (const auto& f : files)
    {
        BenchmarkCase c;
        std::vector<std::string> errors;
        if (!loadBenchmarkFile(f, &c, &errors))
        {
            invalid = true;
            for (const auto& e : errors) std::cerr << "[INVALIDE] " << f.toStdString() << " : " << e << "\n";
            continue;
        }
        loaded.push_back(c);
    }
    std::cout << loaded.size() << " cas valides sur " << files.size() << " fichier(s)\n";
    if (validateOnly) return invalid ? 2 : 0;

    RunOptions options;
    options.engines = engines;
    if (!reference.isEmpty())
    {
        std::vector<std::string> errors;
        options.external = loadExternalReferences(reference, &errors);
        for (const auto& e : errors) std::cerr << "[RÉFÉRENCE] " << e << "\n";
        std::cout << options.external.size() << " référence(s) externe(s) chargée(s)\n";
    }
    BenchmarkReport report;
    report.tsaVersion = TSA::Product::version().toStdString();
    report.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    report.machine = (QSysInfo::prettyProductName() + " / " + QSysInfo::currentCpuArchitecture()).toStdString();
    std::set<std::string> versions;
    for (const auto& c : loaded)
        for (auto& o : runBenchmark(c, options))
        {
            versions.insert(o.engine);
            std::cout << (o.skipped ? "[IGNORÉ] " : o.passed() ? "[RÉUSSI] " : "[ÉCHEC]  ") << o.caseId << " (" << o.engine << ")" << (o.ran ? "" : " : " + o.message) << "\n";
            for (const auto& k : o.checks)
                if (!k.passed)
                    std::cout << "    " << k.quantity << " : TSA " << (k.available ? std::to_string(k.value) : std::string("—")) << ", référence " << k.reference << " (" << k.source << ")\n";
            report.cases.push_back(o);
        }
    for (const auto& v : versions) report.engineVersions += (report.engineVersions.empty() ? "" : ", ") + v;
    QString err;
    if (!writeReport(report, out, &err)) std::cerr << err.toStdString() << "\n";
    std::cout << report.checksPassed() << " réussie(s), " << report.checksFailed() << " en échec, " << report.checksUnavailable() << " non disponible(s) — rapport : "
              << QDir(out).filePath("latest.md").toStdString() << "\n";
    return invalid ? 2 : (report.checksFailed() + report.checksUnavailable() > 0 ? 1 : 0);
}
