// Harnais d'audit (hors dépôt, aucun fichier de production modifié) : lié aux objets du build Release.
//   harness numerics    : essais ciblés sur le calcul et les vérifications
//   harness perf [n…]   : mesures sur l'API réelle (portiques de taille croissante)
#include "App/ProductInfo.h"
#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/ResultsModel.h"
#include "Analysis/CalculationSnapshot.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"
#include "Standards/ModelValidator.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Model/Load/LoadManager.h"
#include "Model/Model.h"
#include "Model/ModelDiff.h"
#include "Model/Cable/CableDefinition.h"
#include "Standards/Design/SteelDesignEC3.h"
#include "Analysis/Engines/Fem3D/Fem3DAdapter.h"
#include "Fem3D/Analysis/LinearStaticAnalysis.h"
#include "Fem3D/Core/Fem3DError.h"
#include "UndoRedo/EditTransaction.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonObject>

#include <windows.h>
#include <psapi.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace TSA::Model;
using namespace TSA::Analysis;

namespace
{
double workingSetMiB()
{
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
    return pmc.PrivateUsage / (1024.0 * 1024.0);
}

struct Frame
{
    Model model;
    int loadCase = 0, nodes = 0, members = 0, topNode = 0;
};

void buildFrame(Frame& f, int nx, int ny, int levels)
{
    auto& m = f.model;
    std::vector<int> grid;
    const auto mat = Material::steelS355();
    auto id = [&](int i, int j, int k) { return grid[std::size_t((k * (ny + 1) + j) * (nx + 1) + i)]; };
    for (int k = 0; k <= levels; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
            {
                const int n = m.addNode(5.0 * i, 5.0 * j, 3.0 * k);
                if (k == 0) m.getNode(n)->setSupport(SupportDefinition::fixed());
                grid.push_back(n);
            }
    for (int k = 1; k <= levels; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
            {
                m.addColumn(id(i, j, k - 1), id(i, j, k), Section::heb(200), mat), ++f.members;
                if (i < nx) m.addBar(id(i, j, k), id(i + 1, j, k), Section::ipe(300), mat, BarRole::Beam), ++f.members;
                if (j < ny) m.addBar(id(i, j, k), id(i, j + 1, k), Section::ipe(300), mat, BarRole::Beam), ++f.members;
            }
    f.loadCase = m.loadManager().addLoadCase(LoadCase(0, "AUDIT", LoadCaseCategory::Dead, true, 1.0));
    m.loadManager().addNodalLoad(NodalLoad(0, id(nx, ny, levels), f.loadCase, 10.0, 5.0, -20.0));
    f.nodes = int(m.nodes().size());
    f.topNode = id(nx, ny, levels);
}

AnalysisRunResult solve(Model& model, AnalysisContext ctx, std::string* err = nullptr, ValidationResult* validation = nullptr, double* prepMs = nullptr, double* runMs = nullptr)
{
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    TSA::Grid::GridManager gm;
    QElapsedTimer t;
    t.start();
    const auto prep = mgr.prepare(model, &gm, ctx);
    if (prepMs) *prepMs = t.nsecsElapsed() / 1e6;
    if (validation) *validation = prep.validation;
    if (!prep.canRun())
    {
        if (err)
        {
            err->clear();
            for (const auto& e : prep.validation.texts(ValidationSeverity::Error)) *err += e + " ; ";
        }
        return {};
    }
    t.restart();
    auto r = mgr.run(ctx, prep);
    if (runMs) *runMs = t.nsecsElapsed() / 1e6;
    if (err && !r.success) *err = r.message;
    return r;
}

double sumRz(const ResultsModel& r)
{
    double s = 0.0;
    for (const auto& [id, n] : r.allReactions()) s += n.rz * r.units().forceToKiloNewtons();
    return s;
}

// -------------------------------------------------------------------------------------------------------------------
// Essais numériques
// -------------------------------------------------------------------------------------------------------------------
struct Beam1
{
    Model m;
    int a = 0, b = 0, bar = 0;
    Beam1(double L = 8.0)
    {
        a = m.addNode(0, 0, 0);
        b = m.addNode(L, 0, 0);
        const auto pin = SupportDefinition::custom(DOFState::Fixed, DOFState::Fixed, DOFState::Fixed, DOFState::Fixed, DOFState::Free, DOFState::Free);
        const auto rol = SupportDefinition::custom(DOFState::Free, DOFState::Fixed, DOFState::Fixed, DOFState::Fixed, DOFState::Free, DOFState::Free);
        m.getNode(a)->setSupport(pin);
        m.getNode(b)->setSupport(rol);
        bar = m.addBar(a, b, Section::ipe(300), Material::steelS355(), BarRole::Beam);
    }
};

void selfWeightSemantics()
{
    std::cout << "\n=== A. Poids propre : drapeau du cas, coefficient, réglage du calcul ===\n";
    const double L = 8.0, A = Section::ipe(300).area(), rho = Material::steelS355().density, w = A * rho * 9.81 / 1000.0;
    std::cout << "poids linéique w = " << w << " kN/m ; w·L = " << w * L << " kN\n";
    auto run = [&](const char* label, double factor, bool caseFlag, bool ctxFlag, bool combo, const char* engine) {
        Beam1 b(L);
        auto& lm = b.m.loadManager();
        const int c = lm.addLoadCase(LoadCase(0, "SW", LoadCaseCategory::Dead, caseFlag, factor));
        AnalysisContext ctx;
        ctx.engineId = engine;
        ctx.common.includeSelfWeight = ctxFlag;
        if (std::string(engine) == "custom2d")
        {
            ctx.dimension = AnalysisDimension::Plane2D;
            ctx.scope.type = ScopeType::ModelPlane;
        }
        if (combo)
        {
            LoadCombination cmb(0, "ELU audit", LoadCombinationType::ULS_Fundamental);
            cmb.setFactor(c, 1.35);
            ctx.combinationId = lm.addCombination(cmb);
        }
        else
            ctx.loadCaseIds = { c };
        std::string err;
        const auto r = solve(b.m, ctx, &err);
        std::cout << std::left << std::setw(58) << label << " [" << engine << "] ΣRz = " << std::right << std::fixed << std::setprecision(3)
                  << (r.success ? sumRz(r.results) : std::numeric_limits<double>::quiet_NaN()) << " kN " << (r.success ? "" : err) << std::defaultfloat << "\n";
    };
    for (const char* e : { "opensees", "fem3d", "custom2d" })
        run("cas seul, drapeau=oui, coef=1.5, réglage=oui", 1.5, true, true, false, e);
    for (const char* e : { "opensees", "fem3d", "custom2d" })
        run("cas seul, drapeau=NON, coef=1.0, réglage=oui", 1.0, false, true, false, e);
    for (const char* e : { "opensees", "fem3d", "custom2d" })
        run("cas seul, drapeau=oui, coef=1.0, réglage=NON", 1.0, true, false, false, e);
    for (const char* e : { "opensees", "fem3d", "custom2d" })
        run("combinaison 1.35×cas, drapeau=oui, réglage=NON", 1.0, true, false, true, e);
    for (const char* e : { "opensees", "fem3d", "custom2d" })
        run("combinaison 1.35×cas, drapeau=NON, réglage=oui", 1.0, false, true, true, e);
}

void cableCompression()
{
    std::cout << "\n=== B. Câble comprimé (statique linéaire) ===\n";
    Model m;
    const int top = m.addNode(0, 0, 5), mid = m.addNode(0, 0, 0), side = m.addNode(4, 0, 0);
    m.getNode(top)->setSupport(SupportDefinition::fixed());
    m.getNode(side)->setSupport(SupportDefinition::fixed());
    const int c = m.addCable(top, mid, CableDefinition::strandY1860S7_15_7(), "Tirant");
    m.addBar(side, mid, Section::heb(200), Material::steelS355(), BarRole::Beam);
    auto& lm = m.loadManager();
    const int lc = lm.addLoadCase(LoadCase(0, "Soulèvement", LoadCaseCategory::Live, false, 1.0));
    lm.addNodalLoad(NodalLoad(0, mid, lc, 0.0, 0.0, 30.0));   // effort vers le HAUT au nœud : comprime le tirant suspendu
    AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = false;
    ctx.loadCaseIds = { lc };
    std::string err;
    ValidationResult v;
    const auto r = solve(m, ctx, &err, &v);
    std::cout << "calcul " << (r.success ? "réussi" : "échec : " + err) << "\n";
    if (r.success)
    {
        const auto* e = r.results.getElementResults(StructuralElementKind::Cable, c);
        std::cout << "N câble = " << (e ? e->startForces.N * r.results.units().forceToKiloNewtons() : 0.0) << " kN (négatif = câble COMPRIMÉ, impossible physiquement)\n";
        const auto meta = r.results.executionMetadata();
        std::cout << "avertissements du calcul : " << meta.warnings.size() << "\n";
        for (const auto& w : meta.warnings) std::cout << "  - " << w << "\n";
    }
    for (const auto& w : v.texts(ValidationSeverity::Warning)) std::cout << "  validation : " << w << "\n";
}

void angleSection()
{
    std::cout << "\n=== C. Cornière : axes géométriques et axes principaux ===\n";
    const Section s = Section::angle(0.100, 0.100, 0.010, "L100x100x10");
    const double Iy = s.iy(), Iz = s.iz(), Iyz = s.iyz();
    const double avg = 0.5 * (Iy + Iz), rad = std::sqrt(0.25 * (Iy - Iz) * (Iy - Iz) + Iyz * Iyz);
    const double Iu = avg + rad, Iv = avg - rad;
    std::cout << "Iy = " << Iy * 1e8 << " cm⁴, Iz = " << Iz * 1e8 << " cm⁴, Iyz = " << Iyz * 1e8 << " cm⁴ ; Iu = " << Iu * 1e8 << ", Iv = " << Iv * 1e8 << " cm⁴\n";
    const double L = 3.0, P = 1.0, E = 210e6;
    Model m;
    const int a = m.addNode(0, 0, 0), b = m.addNode(L, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.addBar(a, b, s, Material::steelS355(), BarRole::Beam);
    auto& lm = m.loadManager();
    const int lc = lm.addLoadCase(LoadCase(0, "P", LoadCaseCategory::Live, false, 1.0));
    lm.addNodalLoad(NodalLoad(0, b, lc, 0.0, 0.0, -P));
    AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = false;
    ctx.loadCaseIds = { lc };
    std::string err;
    const auto r = solve(m, ctx, &err);
    const auto* d = r.success ? r.results.getNodeDisplacement(b) : nullptr;
    // Flexion déviée sur axes principaux (section non symétrique par rapport à y, z)
    const double det = Iy * Iz - Iyz * Iyz;
    const double uzExact = -P * L * L * L / (3.0 * E) * Iz / det;
    const double uyExact = P * L * L * L / (3.0 * E) * Iyz / det;
    std::cout << "TSA : uz = " << (d ? d->uz : 0) << " m, uy = " << (d ? d->uy : 0) << " m\n";
    std::cout << "flexion déviée exacte : uz = " << uzExact << " m, uy = ±" << std::abs(uyExact) << " m (rapport TSA/exact sur uz = " << (d ? d->uz / uzExact : 0) << ")\n";
}

void bucklingAxis()
{
    std::cout << "\n=== D. Flambement EC3 : axe de vérification (note « Legacy », NDCGenerator.cpp:874) ===\n";
    for (const auto& [sec, name] : std::vector<std::pair<Section, const char*>> { { Section::ipe(200), "IPE 200" }, { Section::hea(200), "HEA 200" }, { Section::heb(300), "HEB 300" } })
        for (double L : { 3.0, 5.0 })
        {
            using TSA::Standards::Design::SteelDesignEC3;
            const auto strong = SteelDesignEC3::calculateFromBar(sec, Material::steelS355(), L, 1.0, 100e3, true);
            const auto weak = SteelDesignEC3::calculateFromBar(sec, Material::steelS355(), L, 1.0, 100e3, false);
            std::cout << name << " L=" << L << " m : axe fort Nb,Rd = " << strong.Nb_Rd / 1e3 << " kN (imprimé par la note) ; axe faible Nb,Rd = " << weak.Nb_Rd / 1e3
                      << " kN ; surestimation ×" << strong.Nb_Rd / weak.Nb_Rd << "\n";
        }
}

void nonFiniteCoordinates()
{
    std::cout << "\n=== E. Coordonnée non finie : l'exception de Fem3D est-elle contenue ? ===\n";
    Model m;
    const int a = m.addNode(0, 0, 0), b = m.addNode(std::numeric_limits<double>::quiet_NaN(), 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.addBar(a, b, Section::ipe(300), Material::steelS355(), BarRole::Beam);
    for (const char* engine : { "fem3d", "opensees" })
    {
        AnalysisContext ctx;
        ctx.engineId = engine;
        ctx.common.includeSelfWeight = true;
        try
        {
            std::string err;
            const auto r = solve(m, ctx, &err);
            std::cout << engine << " : aucune exception ; succès = " << r.success << " ; " << err.substr(0, 200) << "\n";
        }
        catch (const std::exception& e)
        {
            std::cout << engine << " : EXCEPTION NON CONTENUE (" << typeid(e).name() << ") : " << e.what() << "\n";
        }
        catch (...)
        {
            std::cout << engine << " : EXCEPTION NON CONTENUE (type inconnu)\n";
        }
    }
}

// -------------------------------------------------------------------------------------------------------------------
// Mesures
// -------------------------------------------------------------------------------------------------------------------
void perf(const QString& dir, int nx, int ny, int levels, bool withOpenSees)
{
    QElapsedTimer t;
    const double m0 = workingSetMiB();
    std::cerr<<"[construction]"<<std::endl;
    t.start();
    Frame f;
    buildFrame(f, nx, ny, levels);
    const double build = t.nsecsElapsed() / 1e6;
    const double m1 = workingSetMiB();
    auto& m = f.model;

    std::cerr<<"[instantané]"<<std::endl;
    t.restart();
    auto snap1 = m.createSnapshot("a");
    const double snap = t.nsecsElapsed() / 1e6;
    const double m2 = workingSetMiB();

    const std::string path = (dir + QStringLiteral("/perf_%1.tsa").arg(f.members)).toStdString();
    TSA::Grid::GridManager grids;
    TSA::IO::TSAFileWriter writer;
    std::string err;
    std::cerr<<"[enregistrement]"<<std::endl;
    t.restart();
    const bool saved = writer.saveToFile(path, m, &grids, "Audit", "Audit", &err);
    const double save = t.nsecsElapsed() / 1e6;
    const double size = QFileInfo(QString::fromStdString(path)).size() / 1024.0;

    Model back;
    TSA::Grid::GridManager backGrids;
    std::string name;
    TSA::IO::TSAFileReader reader;
    std::cerr<<"[ouverture] err="<<err<<std::endl;
    t.restart();
    const bool loaded = reader.loadFromFile(path, back, &backGrids, "", &name, nullptr, nullptr, &err);
    const double load = t.nsecsElapsed() / 1e6;

    std::cerr<<"[édition] err="<<err<<std::endl;
    // Édition d'un seul nœud par la voie de l'interface : transaction complète
    t.restart();
    {
        TSA::UndoRedo::EditTransaction tx(m, "Déplacer un nœud");
        m.getNode(f.topNode)->setX(m.getNode(f.topNode)->x() + 0.01);
        m.notifyNodeModified(f.topNode);
        tx.commit();
    }
    const double edit = t.nsecsElapsed() / 1e6;
    std::cerr<<"[annuler]"<<std::endl;
    t.restart();
    m.undo();
    const double undo = t.nsecsElapsed() / 1e6;
    t.restart();
    m.redo();
    const double redo = t.nsecsElapsed() / 1e6;
    auto snap2 = m.createSnapshot("b");
    t.restart();
    const auto diff = ModelDiff::compute(snap1, snap2);
    const double diffMs = t.nsecsElapsed() / 1e6;

    std::cerr<<"[calcul]"<<std::endl;
    double prepF = 0, runF = 0, prepO = 0, runO = 0;
    AnalysisContext ctx;
    ctx.engineId = "fem3d";
    ctx.loadCaseIds = { f.loadCase };
    std::string e1, e2;
    const auto rf = solve(m, ctx, &e1, nullptr, &prepF, &runF);
    if (withOpenSees)
    {
        ctx.engineId = "opensees";
        const auto ro = solve(m, ctx, &e2, nullptr, &prepO, &runO);
        if (!ro.success) std::cout << "  OpenSees : " << e2.substr(0, 160) << "\n";
    }
    if (!rf.success) std::cout << "  Fem3D : " << e1.substr(0, 160) << "\n";
    std::cout << std::fixed << std::setprecision(1) << f.nodes << " | " << f.members << " | " << build << " | " << snap << " | " << save << " | " << size << " | " << load << " | "
              << edit << " | " << undo << " | " << redo << " | " << diffMs << " | " << prepF << " | " << runF << " | " << prepO << " | " << runO << " | " << m1 - m0 << " | " << m2 - m1 << " | "
              << saved << loaded << " | " << diff.isEmpty() << std::defaultfloat << "\n";
}

// Détail du temps de Fem3D par étape
void perfFem3d(int nx, int ny, int levels, bool selfWeight)
{
    Frame f;
    buildFrame(f, nx, ny, levels);
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    TSA::Grid::GridManager gm;
    AnalysisContext ctx;
    ctx.engineId = "fem3d";
    ctx.common.includeSelfWeight = selfWeight;
    ctx.loadCaseIds = { f.loadCase };
    QElapsedTimer t;
    t.start();
    const auto prep = mgr.prepare(f.model, &gm, ctx);
    const double p = t.nsecsElapsed() / 1e6;
    t.restart();
    const auto c = Fem3DIntegration::convert(ctx, prep.model);
    const double conv = t.nsecsElapsed() / 1e6;
    t.restart();
    const TSA::Fem3D::LinearStaticAnalysis analysis(c.model);
    const double fact = t.nsecsElapsed() / 1e6;
    t.restart();
    const auto fr = analysis.run(1);
    const double run = t.nsecsElapsed() / 1e6;
    t.restart();
    const auto mapped = Fem3DIntegration::mapResults(c, prep.model, fr);
    const double map = t.nsecsElapsed() / 1e6;
    std::cout << std::fixed << std::setprecision(1) << f.members << " barres, poids propre=" << selfWeight << " : prépare " << p << " ms (dont une conversion), conversion " << conv
              << " ms, assemblage+factorisation " << fact << " ms, cas (charges, efforts, équilibre) " << run << " ms, remappage " << map << " ms\n" << std::defaultfloat;
}

// Charge un .tsa (éventuellement fuzzé) puis exerce instantané, validation, moteurs internes, sans OpenSees.
int fuzzLoad(const char* path)
{
    Model m;
    TSA::Grid::GridManager grids;
    std::string name, err;
    TSA::IO::TSAFileReader reader;
    if (!reader.loadFromFile(path, m, &grids, "", &name, nullptr, nullptr, &err)) return 1;
    auto s1 = m.createSnapshot("a");
    const auto d = ModelDiff::compute(s1, s1);
    int cases = 0;
    for (const auto& [id, lc] : m.loadManager().loadCases()) cases = id;
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    AnalysisContext ctx;
    ctx.engineId = "fem3d";
    if (cases) ctx.loadCaseIds = { cases };
    const auto prep = mgr.prepare(m, &grids, ctx);
    if (prep.canRun())
    {
        const auto r = mgr.run(ctx, prep);
        (void)r;
    }
    return 2;
}

// Tri d'un fichier fuzzé : dernière étape atteinte (stderr) et références pendantes
int fuzzDiag(const char* path)
{
    Model m;
    TSA::Grid::GridManager grids;
    std::string name, err;
    TSA::IO::TSAFileReader reader;
    std::cerr << "E1 lecture" << std::endl;
    if (!reader.loadFromFile(path, m, &grids, "", &name, nullptr, nullptr, &err)) { std::cerr << "refus lecture : " << err << std::endl; return 1; }
    int dangling = 0, nonFinite = 0;
    auto check = [&](int a, int b) { dangling += (m.getNode(a) == nullptr) + (m.getNode(b) == nullptr); };
    for (const auto& [id, e] : m.beams()) check(e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : m.columns()) check(e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : m.trussMembers()) check(e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : m.cables()) check(e.startNodeId(), e.endNodeId());
    for (const auto& [id, n] : m.nodes()) nonFinite += !(std::isfinite(n.x()) && std::isfinite(n.y()) && std::isfinite(n.z()));
    std::cerr << "chargé : nœuds=" << m.nodes().size() << " références pendantes=" << dangling << " coordonnées non finies=" << nonFinite << std::endl;
    std::cerr << "E2 instantané Undo" << std::endl;
    auto s1 = m.createSnapshot("a");
    std::cerr << "E3 CalculationSnapshot::capture" << std::endl;
    try { const auto snap = CalculationSnapshot::capture(m); std::cerr << "   noeuds=" << snap.nodeCount() << " éléments=" << snap.elementCount() << std::endl; }
    catch (const std::exception& e) { std::cerr << "   EXCEPTION " << typeid(e).name() << " : " << e.what() << std::endl; }
    std::cerr << "E4 ModelValidator" << std::endl;
    try { AnalysisParameters p; const auto rep = TSA::Standards::ModelValidator::validateForAnalysis(m, p); std::cerr << "   erreurs=" << rep.formattedErrors().size() << std::endl; }
    catch (const std::exception& e) { std::cerr << "   EXCEPTION " << typeid(e).name() << " : " << e.what() << std::endl; }
    int cases = 0;
    for (const auto& [id, lc] : m.loadManager().loadCases()) cases = id;
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    AnalysisContext ctx;
    ctx.engineId = "fem3d";
    if (cases) ctx.loadCaseIds = { cases };
    std::cerr << "E5 prepare" << std::endl;
    try
    {
        const auto prep = mgr.prepare(m, &grids, ctx);
        std::cerr << "   canRun=" << prep.canRun() << std::endl;
        if (prep.canRun())
        {
            std::cerr << "E6 run" << std::endl;
            const auto r = mgr.run(ctx, prep);
            std::cerr << "   succès=" << r.success << std::endl;
        }
    }
    catch (const std::exception& e) { std::cerr << "   EXCEPTION " << typeid(e).name() << " : " << e.what() << std::endl; }
    std::cerr << "FIN" << std::endl;
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    const std::string mode = argc > 1 ? argv[1] : "numerics";
    std::cout << std::unitbuf;
    if (mode == "fuzzload") return fuzzLoad(argv[2]);
    if (mode == "fuzzdiag") return fuzzDiag(argv[2]);
    if (mode == "perffem")
    {
        for (const auto& s : { std::array<int, 3> { 8, 8, 5 }, std::array<int, 3> { 16, 16, 10 }, std::array<int, 3> { 24, 24, 16 } })
        {
            perfFem3d(s[0], s[1], s[2], true);
            perfFem3d(s[0], s[1], s[2], false);
        }
        return 0;
    }
    if (mode == "numerics")
    {
        selfWeightSemantics();
        cableCompression();
        angleSection();
        bucklingAxis();
        nonFiniteCoordinates();
    }
    else
    {
        std::cout << "nœuds | barres | construction | instantané | enreg. | Kio | ouverture | édition 1 nœud | annuler | rétablir | diff | prép Fem3D | calcul Fem3D | prép OS | calcul OS | Mio modèle | Mio instantané | ok | diffvide\n";
        const QString dir = QDir::currentPath();
        const int repeat = argc > 2 ? std::atoi(argv[2]) : 3;
        struct Size { int nx, ny, lv; bool os; };
        for (const Size& s : { Size { 8, 8, 5, true }, Size { 16, 16, 10, true }, Size { 24, 24, 16, true }, Size { 32, 32, 20, false } })
            for (int k = 0; k < repeat; ++k) perf(dir, s.nx, s.ny, s.lv, s.os);
    }
    return 0;
}
