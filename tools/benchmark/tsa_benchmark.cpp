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
#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "Model/Load/LoadManager.h"
#include "Model/Model.h"
#include "Validation/BenchmarkSuite.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QThread>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSysInfo>

#include <iostream>
#include <set>

using namespace TSA::Validation;

namespace
{
/// Mesures de performance (docs/AUDIT_CALCULS.md §Performances) : portiques 3D de taille croissante, poids propre et
/// charge nodale ; temps de construction, de préparation (snapshot, validation), de génération du script, de résolution
/// OpenSees (processus externe), de lecture ; écrit perf-<date>.md et .csv dans out.
struct Frame
{
    TSA::Model::Model model;
    int loadCase = 0, nodes = 0, members = 0, dofs = 0;
};

void buildFrame(Frame& f, int nx, int ny, int levels)
{
    auto& m = f.model;
    std::vector<int> grid;
    const auto mat = TSA::Model::Material::steelS355();
    auto id = [&](int i, int j, int k) { return grid[std::size_t((k * (ny + 1) + j) * (nx + 1) + i)]; };
    for (int k = 0; k <= levels; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
            {
                const int n = m.addNode(5.0 * i, 5.0 * j, 3.0 * k);
                if (k == 0) m.getNode(n)->setSupport(TSA::Model::SupportDefinition::fixed());
                grid.push_back(n);
            }
    for (int k = 1; k <= levels; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
            {
                m.addColumn(id(i, j, k - 1), id(i, j, k), TSA::Model::Section::heb(200), mat), ++f.members;
                if (i < nx) m.addBar(id(i, j, k), id(i + 1, j, k), TSA::Model::Section::ipe(300), mat, TSA::Model::BarRole::Beam), ++f.members;
                if (j < ny) m.addBar(id(i, j, k), id(i, j + 1, k), TSA::Model::Section::ipe(300), mat, TSA::Model::BarRole::Beam), ++f.members;
            }
    f.loadCase = m.loadManager().addLoadCase(TSA::Model::LoadCase(0, "G", TSA::Model::LoadCaseCategory::Dead, true, 1.0));
    m.loadManager().addNodalLoad(TSA::Model::NodalLoad(0, id(nx, ny, levels), f.loadCase, 10.0, 5.0, -20.0));
    f.nodes = int(m.nodes().size());
    f.dofs = (f.nodes - (nx + 1) * (ny + 1)) * 6;
}

TSA::Analysis::AnalysisRunResult solveFrame(Frame& f, int systemSolver, double* prepMs, double* runMs)
{
    TSA::Analysis::AnalysisEngineRegistry reg;
    TSA::Analysis::registerBuiltInEngines(reg);
    TSA::Analysis::AnalysisManager mgr(reg);
    TSA::Analysis::AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.loadCaseIds = { f.loadCase };
    if (systemSolver >= 0) ctx.engineSettings["opensees"] = QJsonObject { { "systemSolver", systemSolver } };
    TSA::Grid::GridManager gm;
    QElapsedTimer t;
    t.start();
    const auto prep = mgr.prepare(f.model, &gm, ctx);
    if (prepMs) *prepMs = t.nsecsElapsed() / 1e6;
    t.restart();
    auto run = prep.canRun() ? mgr.run(ctx, prep) : TSA::Analysis::AnalysisRunResult {};
    if (runMs) *runMs = t.nsecsElapsed() / 1e6;
    return run;
}

int runPerformance(const QString& out)
{
    struct Size { int nx, ny, levels; };
    const Size sizes[] = { { 2, 2, 2 }, { 4, 4, 3 }, { 8, 8, 5 }, { 12, 12, 8 }, { 16, 16, 10 } };
    QString md = QStringLiteral("# Mesures de performance — TSA %1\n\nPoste : %2, %3 cœurs logiques. Portiques 3D (travées de 5 m, niveaux de 3 m), "
                                "poteaux HEB 200, poutres IPE 300, poids propre, statique linéaire, OpenSees (BandGeneral, RCM).\n\n"
                                "| Nœuds | Barres | DDL | Construction (ms) | Préparation (ms) | Script (ms) | OpenSees (ms) | Lecture (ms) | Total (ms) |\n"
                                "|---|---|---|---|---|---|---|---|---|\n")
                     .arg(TSA::Product::version(), QSysInfo::prettyProductName())
                     .arg(QThread::idealThreadCount());
    QString csv = QStringLiteral("nodes,members,dofs,build_ms,prepare_ms,script_ms,solve_ms,read_ms,total_ms\n");
    for (const auto& sz : sizes)
    {
        QElapsedTimer t;
        t.start();
        Frame fr;
        buildFrame(fr, sz.nx, sz.ny, sz.levels);
        const double buildMs = t.nsecsElapsed() / 1e6;
        double prepMs = 0, runMs = 0;
        const auto run = solveFrame(fr, -1, &prepMs, &runMs);
        if (!run.success)
        {
            std::cerr << "échec du calcul de performance : " << run.message << "\n";
            return 1;
        }
        const int nodes = fr.nodes, members = fr.members, dofs = fr.dofs;
        const auto& meta = run.results.executionMetadata();
        md += QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 |\n")
                  .arg(nodes).arg(members).arg(dofs)
                  .arg(buildMs, 0, 'f', 0).arg(prepMs, 0, 'f', 0).arg(meta.scriptDurationMs, 0, 'f', 0).arg(meta.solveDurationMs, 0, 'f', 0)
                  .arg(meta.readDurationMs, 0, 'f', 0).arg(buildMs + prepMs + runMs, 0, 'f', 0);
        csv += QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9\n").arg(nodes).arg(members).arg(dofs).arg(buildMs).arg(prepMs).arg(meta.scriptDurationMs)
                   .arg(meta.solveDurationMs).arg(meta.readDurationMs).arg(buildMs + prepMs + runMs);
        std::cout << nodes << " nœuds, " << members << " barres : préparation " << prepMs << " ms, script " << meta.scriptDurationMs << " ms, OpenSees "
                  << meta.solveDurationMs << " ms, lecture " << meta.readDurationMs << " ms\n";
    }
    // Solveurs linéaires d'OpenSees : temps de résolution et écart des déplacements à BandGeneral (même modèle)
    const char* names[] = { "BandGeneral", "ProfileSPD", "SuperLU", "UmfPack", "BandSPD", "SparseGEN" };
    md += QStringLiteral("\n## Solveurs linéaires d'OpenSees\n\nTemps du processus OpenSees (lecture du script, assemblage, factorisation, écriture) ; écart = "
                         "max |u − u_BandGeneral| / max |u_BandGeneral| sur tous les nœuds.\n\n| Modèle (DDL) | Solveur | OpenSees (ms) | Écart des déplacements |\n|---|---|---|---|\n");
    for (const auto& sz : { Size { 8, 8, 5 }, Size { 16, 16, 10 } })
    {
        Frame fr;
        buildFrame(fr, sz.nx, sz.ny, sz.levels);
        std::map<int, TSA::Analysis::NodeDisplacement> ref;
        double umax = 0;
        for (int k = 0; k < 6; ++k)
        {
            const auto run = solveFrame(fr, k, nullptr, nullptr);
            if (!run.success)
            {
                md += QStringLiteral("| %1 | %2 | échec : %3 | — |\n").arg(fr.dofs).arg(names[k], QString::fromStdString(run.message).left(80));
                continue;
            }
            double dev = 0;
            if (k == 0)
            {
                ref = run.results.allDisplacements();
                for (const auto& [id, d] : ref) umax = std::max({ umax, std::abs(d.ux), std::abs(d.uy), std::abs(d.uz) });
            }
            else
                for (const auto& [id, d] : run.results.allDisplacements())
                    if (auto it = ref.find(id); it != ref.end())
                        dev = std::max({ dev, std::abs(d.ux - it->second.ux), std::abs(d.uy - it->second.uy), std::abs(d.uz - it->second.uz) });
            md += QStringLiteral("| %1 | %2 | %3 | %4 |\n").arg(fr.dofs).arg(names[k]).arg(run.results.executionMetadata().solveDurationMs, 0, 'f', 0).arg(umax > 0 ? dev / umax : 0.0, 0, 'g', 3);
            std::cout << fr.dofs << " DDL, " << names[k] << " : " << run.results.executionMetadata().solveDurationMs << " ms\n";
        }
    }
    QDir().mkpath(out);
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    for (const auto& [name, data] : { std::pair { QStringLiteral("perf-%1.md").arg(stamp), md }, std::pair { QStringLiteral("perf-%1.csv").arg(stamp), csv } })
    {
        QFile f(QDir(out).filePath(name));
        if (f.open(QIODevice::WriteOnly)) f.write(data.toUtf8());
    }
    std::cout << md.toStdString();
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // Paquet : <exe>/validation/cases ; poste de développement : sources
    const QString shipped = QDir(QCoreApplication::applicationDirPath()).filePath("validation/cases");
    QString cases = QFileInfo(shipped).isDir() ? shipped : QStringLiteral(TSA_SOURCE_DIR "/tests/benchmarks"), reference,
            out = QDir::current().filePath("validation-reports");
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
        else if (a == "--performance") return runPerformance(out);
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
