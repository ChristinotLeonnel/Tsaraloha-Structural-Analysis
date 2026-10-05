#include "OpenSeesEngine.h"

#include "../../OpenSeesManager.h"
#include "../../OpenSeesSolver.h"

#include <QObject>

namespace TSA::Analysis
{

OpenSeesEngine::OpenSeesEngine() = default;
OpenSeesEngine::~OpenSeesEngine() = default;

EngineInfo OpenSeesEngine::info() const
{
    EngineInfo i;
    i.id = kId;
    i.name = "OpenSees";
    i.description = "Moteur éléments finis général (processus externe OpenSees.exe, scripts Tcl générés) : "
                    "ossatures spatiales, treillis, câbles, appuis élastiques.";
    if (!m_version)
    {
        auto& mgr = OpenSeesManager::instance();
        if (mgr.isAvailable()) m_version = mgr.versionInfo().versionString.toStdString();
    }
    i.version = m_version.value_or(std::string());
    return i;
}

AnalysisCapabilities OpenSeesEngine::capabilities() const
{
    // Strictement ce que OpenSeesAnalysisBuilder / OpenSeesResultsReader produisent aujourd'hui.
    AnalysisCapabilities c;
    c.supports3D = true;                 // BasicBuilder -ndm 3 -ndf 6 uniquement
    c.supportsFrame = true;              // elasticBeamColumn
    c.supportsTruss = true;              // truss / corotTruss
    c.supportsCable = true;              // corotTruss + InitStrain
    c.supportsSprings = true;            // zeroLength
    c.planarElementPolicy = UnsupportedElementPolicy::ExcludeWithWarning;   // comportement historique (BUG-002)
    c.supportsStatic = true;
    c.supportsNonlinear = true;          // NonLinearStatic (algorithme / intégrateur configurables)
    c.supportsModal = true;              // eigen -fullGenLapack
    // Pushover et temporel : le générateur produit aujourd'hui un calcul statique linéaire pour
    // ces types (BUG-017) → non déclarés.
    c.providesDisplacements = true;
    c.providesReactions = true;
    c.providesElementForces = true;
    c.providesElementStiffness = true;   // mode ADVANCED
    c.providesGlobalStiffness = true;    // mode ADVANCED, sous plafond de DDL
    c.providesDofMapping = true;         // mode ADVANCED
    c.supportsCustomOptions = true;
    return c;
}

EngineAvailability OpenSeesEngine::availability() const
{
    EngineAvailability a;
    a.available = OpenSeesManager::instance().isAvailable();
    a.canProvision = !a.available;
    if (!a.available) a.message = "OpenSees.exe introuvable ou non vérifié.";
    return a;
}

bool OpenSeesEngine::provision(std::string* error)
{
    QString err;
    const bool ok = OpenSeesManager::instance().downloadAndInstall(nullptr, &err);
    if (!ok && error) *error = err.toStdString();
    m_version.reset();
    return ok;
}

QJsonObject OpenSeesEngine::defaultSettings() const
{
    return settingsFromParameters(AnalysisParameters{});
}

QJsonObject OpenSeesEngine::settingsFromParameters(const AnalysisParameters& p)
{
    return QJsonObject{
        { "algorithm", static_cast<int>(p.algorithmType) },
        { "integrator", static_cast<int>(p.integratorType) },
        { "systemSolver", static_cast<int>(p.systemSolver) },
        { "constraintHandler", static_cast<int>(p.constraintHandler) },
        { "trussFormulation", static_cast<int>(p.trussFormulation) },
        { "geomTransf", static_cast<int>(p.geomTransf) },
        { "maxIterations", p.maxIterations },
        { "tolerance", p.tolerance },
        { "numSteps", p.numSteps },
        { "stepSize", p.stepSize },
        { "controlNodeId", p.controlNodeId },
        { "controlDof", p.controlDof },
        { "dispIncrement", p.dispIncrement },
        { "saveAllSteps", p.saveAllSteps },
        { "useKiloNewtons", p.useKiloNewtons },
        { "extractionLevel", static_cast<int>(p.extractionLevel) },
        { "maxGlobalStiffnessDofs", p.maxGlobalStiffnessDofs },
    };
}

AnalysisParameters OpenSeesEngine::parametersFromContext(const AnalysisContext& context)
{
    AnalysisParameters p;
    const QJsonObject s = context.settingsFor(kId);
    auto i = [&](const char* k, int d) { return s.value(k).toInt(d); };
    auto d = [&](const char* k, double v) { return s.value(k).toDouble(v); };

    // Les deux jeux de champs historiques (algorithm / algorithmType…) sont alignés : le générateur
    // utilise *Type, les journaux le champ court.
    p.algorithmType = p.algorithm = static_cast<NonlinearAlgorithm>(i("algorithm", static_cast<int>(p.algorithmType)));
    p.integratorType = p.integrator = static_cast<IntegratorType>(i("integrator", static_cast<int>(p.integratorType)));
    p.systemSolver = static_cast<SystemSolver>(i("systemSolver", static_cast<int>(p.systemSolver)));
    p.constraintHandler = static_cast<ConstraintHandler>(i("constraintHandler", static_cast<int>(p.constraintHandler)));
    p.trussFormulation = static_cast<TrussFormulation>(i("trussFormulation", static_cast<int>(p.trussFormulation)));
    p.geomTransf = static_cast<GeomTransfType>(i("geomTransf", static_cast<int>(p.geomTransf)));
    p.maxIterations = i("maxIterations", p.maxIterations);
    p.tolerance = d("tolerance", p.tolerance);
    p.numSteps = i("numSteps", p.numSteps);
    p.stepSize = d("stepSize", p.stepSize);
    p.controlNodeId = i("controlNodeId", p.controlNodeId);
    p.controlDof = i("controlDof", p.controlDof);
    p.dispIncrement = d("dispIncrement", p.dispIncrement);
    p.saveAllSteps = s.value("saveAllSteps").toBool(p.saveAllSteps);
    p.useKiloNewtons = s.value("useKiloNewtons").toBool(p.useKiloNewtons);
    p.extractionLevel = static_cast<ExtractionLevel>(i("extractionLevel", static_cast<int>(p.extractionLevel)));
    p.maxGlobalStiffnessDofs = i("maxGlobalStiffnessDofs", p.maxGlobalStiffnessDofs);

    // Partie commune
    p.type = context.type;
    p.includeSelfWeight = context.common.includeSelfWeight;
    p.numEigenmodes = context.common.modeCount;
    p.targetCombinationId = context.combinationId;
    p.targetLoadCaseId = (context.combinationId <= 0 && context.loadCaseIds.size() == 1) ? context.loadCaseIds.front() : 0;
    return p;
}

ValidationResult OpenSeesEngine::validate(const AnalysisContext& context, const AnalysisModel& model) const
{
    ValidationResult v;
    if (context.combinationId <= 0 && context.loadCaseIds.size() > 1)
        v.addError("OpenSees", "Le générateur OpenSees calcule un cas, tous les cas superposés ou une combinaison : "
                               "créez une combinaison pour calculer plusieurs cas ensemble.");

    const AnalysisParameters p = parametersFromContext(context);
    if (p.type == AnalysisType::NonLinearStatic && p.integratorType == IntegratorType::DisplacementControl &&
        model.mapping.analysisNode(p.controlNodeId) == 0)
        v.addError("OpenSees", "Le nœud de contrôle N" + std::to_string(p.controlNodeId)
                                   + " (Displacement Control) n'appartient pas à la portée calculée.");

    if (!model.entireModel && context.scope.isPlanar())
        v.addWarning("OpenSees", "Calcul 3D d'une portée plane : la stabilité hors plan n'est assurée que par "
                                 "les appuis définis dans la portée.");
    return v;
}

AnalysisRunResult OpenSeesEngine::run(const AnalysisContext& context, const AnalysisModel& model,
                                      const AnalysisRunCallbacks& callbacks)
{
    AnalysisRunResult r;
    m_solver = std::make_unique<OpenSeesSolver>();
    if (callbacks.progress)
        QObject::connect(m_solver.get(), &OpenSeesSolver::progressChanged,
                         [cb = callbacks.progress](int pct, const QString& s) { cb(pct, s.toStdString()); });
    if (callbacks.log)
        QObject::connect(m_solver.get(), &OpenSeesSolver::logReceived,
                         [cb = callbacks.log](const QString& l) { cb(l.toStdString()); });

    QString err;
    r.success = m_solver->solveSnapshot(model.snapshot, parametersFromContext(context), &err);
    r.message = r.success ? "Calcul OpenSees terminé." : err.toStdString();
    r.results = m_solver->results();
    return r;
}

void OpenSeesEngine::cancel()
{
    if (m_solver) m_solver->stop();
}

} // namespace TSA::Analysis
