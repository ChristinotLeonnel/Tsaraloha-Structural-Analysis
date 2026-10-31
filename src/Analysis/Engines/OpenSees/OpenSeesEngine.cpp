#include "OpenSeesEngine.h"

#include "../../OpenSeesManager.h"
#include "../../OpenSeesSolver.h"

#include <QCoreApplication>
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
    i.description = QCoreApplication::translate("TSA::Analysis::OpenSeesEngine",
                        "Moteur éléments finis général (processus externe OpenSees.exe, scripts Tcl générés) : "
                        "ossatures spatiales, treillis, câbles, appuis élastiques.").toStdString();
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
    c.supportsTruss = true;              // truss
    c.supportsCable = true;              // corotTruss + InitStrain
    c.supportsSprings = true;            // zeroLength
    c.planarElementPolicy = UnsupportedElementPolicy::ExcludeWithWarning;   // comportement historique (BUG-002)
    c.supportsStatic = true;
    c.providesDisplacements = true;
    c.providesReactions = true;
    c.providesElementForces = true;
    c.providesElementStiffness = true;   // mode ADVANCED
    c.providesGlobalStiffness = true;    // mode ADVANCED, sous plafond de DDL
    c.providesDofMapping = true;         // mode ADVANCED
    c.supportsCustomOptions = true;
    c.supportsMesh = true;               // barres subdivisées (maillage 1D) : sous-éléments elasticBeamColumn
    c.supportsModal = true;              // eigen + modalProperties : fréquences, formes propres, participation
    c.supportsShell = true;              // dalles et voiles maillés : ShellMITC4 / ShellDKGT (formulation coque seule)
    return c;
}

EngineAvailability OpenSeesEngine::availability() const
{
    EngineAvailability a;
    a.available = OpenSeesManager::instance().isAvailable();
#ifdef Q_OS_WIN
    a.canProvision = !a.available;   // téléchargement de l'exécutable Windows officiel (OpenSeesManager::downloadAndInstall)
#else
    a.canProvision = false;
#endif
    if (!a.available)
    {
        const QString cause = OpenSeesManager::instance().lastCheck().message;   // cause précise (BUG-099)
        a.message = cause.isEmpty() ? QCoreApplication::translate("TSA::Analysis::OpenSeesEngine", "OpenSees.exe introuvable ou non vérifié.").toStdString()
                                    : cause.toStdString();
    }
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
        { "systemSolver", static_cast<int>(p.systemSolver) },
        { "constraintHandler", static_cast<int>(p.constraintHandler) },
        { "useKiloNewtons", p.useKiloNewtons },
        { "extractionLevel", static_cast<int>(p.extractionLevel) },
        { "maxGlobalStiffnessDofs", p.maxGlobalStiffnessDofs },
    };
}

std::vector<std::string> OpenSeesEngine::unsupportedSettings(const QJsonObject& settings) const
{
    // Valeurs enregistrées par les versions qui proposaient ces formulations. Elles changeaient le script d'un calcul :
    // geomTransf 1 PDelta, 2 Corotational ; trussFormulation 1 corotTruss, 3 corotTruss avec section. Les autres clés
    // de ces versions (algorithme, intégrateur, pas, tolérance…) étaient sans effet sur un calcul linéaire : ignorées.
    auto text = [](const char* source) { return QCoreApplication::translate("TSA::Analysis::OpenSeesEngine", source).toStdString(); };
    std::vector<std::string> features;
    const int geomTransf = settings.value("geomTransf").toInt(0);
    if (geomTransf == 1) features.push_back(text(QT_TRANSLATE_NOOP("TSA::Analysis::OpenSeesEngine", "transformation P-Delta (second ordre)")));
    if (geomTransf == 2) features.push_back(text(QT_TRANSLATE_NOOP("TSA::Analysis::OpenSeesEngine", "transformation corotationnelle (grands déplacements)")));
    const int truss = settings.value("trussFormulation").toInt(0);
    if (truss == 1 || truss == 3) features.push_back(text(QT_TRANSLATE_NOOP("TSA::Analysis::OpenSeesEngine", "treillis corotationnels (grands déplacements)")));
    return features;
}

AnalysisParameters OpenSeesEngine::parametersFromContext(const AnalysisContext& context)
{
    AnalysisParameters p;
    p.supportRestraints = context.common.supportRestraints;
    const QJsonObject s = context.settingsFor(kId);
    auto i = [&](const char* k, int d) { return s.value(k).toInt(d); };

    p.systemSolver = static_cast<SystemSolver>(i("systemSolver", static_cast<int>(p.systemSolver)));
    p.constraintHandler = static_cast<ConstraintHandler>(i("constraintHandler", static_cast<int>(p.constraintHandler)));
    p.useKiloNewtons = s.value("useKiloNewtons").toBool(p.useKiloNewtons);
    p.extractionLevel = static_cast<ExtractionLevel>(i("extractionLevel", static_cast<int>(p.extractionLevel)));
    p.maxGlobalStiffnessDofs = i("maxGlobalStiffnessDofs", p.maxGlobalStiffnessDofs);

    // Partie commune
    p.type = context.type;
    p.includeSelfWeight = context.common.includeSelfWeight;
    p.modeCount = context.common.modeCount;
    p.modalMassCaseIds = context.common.modalMassCaseIds;
    p.targetCombinationId = context.combinationId;
    p.targetLoadCaseId = (context.combinationId <= 0 && context.loadCaseIds.size() == 1) ? context.loadCaseIds.front() : 0;
    return p;
}

ValidationResult OpenSeesEngine::validate(const AnalysisContext& context, const AnalysisModel& model) const
{
    ValidationResult v;
    if (context.combinationId <= 0 && context.loadCaseIds.size() > 1)
        v.addError("OpenSees", QCoreApplication::translate("TSA::Analysis::OpenSeesEngine",
                                   "Le générateur OpenSees calcule un cas, tous les cas superposés ou une combinaison : "
                                   "créez une combinaison pour calculer plusieurs cas ensemble.").toStdString());

    if (!model.entireModel && context.scope.isPlanar())
        v.addWarning("OpenSees", QCoreApplication::translate("TSA::Analysis::OpenSeesEngine",
                                     "Calcul 3D d'une portée plane : la stabilité hors plan n'est assurée que par "
                                     "les appuis définis dans la portée.").toStdString());
    return v;
}

AnalysisRunResult OpenSeesEngine::run(const AnalysisContext& context, const AnalysisModel& model,
                                      const AnalysisRunCallbacks& callbacks)
{
    AnalysisRunResult r;
    OpenSeesSolver* solver = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_solverMutex);
        m_solver = std::make_unique<OpenSeesSolver>();
        solver = m_solver.get();
    }
    // Annulation reçue avant la création du solveur : cancel() n'a rien trouvé à arrêter.
    if (callbacks.cancelRequested && callbacks.cancelRequested()) solver->stop();
    if (callbacks.progress)
        QObject::connect(solver, &OpenSeesSolver::progressChanged,
                         [cb = callbacks.progress](int pct, const QString& s) { cb(pct, s.toStdString()); });
    if (callbacks.log)
        QObject::connect(solver, &OpenSeesSolver::logReceived,
                         [cb = callbacks.log](const QString& l) { cb(l.toStdString()); });

    QString err;
    r.success = solver->solveSnapshot(model.snapshot, parametersFromContext(context), &err);
    r.message = r.success ? QCoreApplication::translate("TSA::Analysis::OpenSeesEngine", "Calcul OpenSees terminé.").toStdString() : err.toStdString();
    r.results = solver->results();
    return r;
}

void OpenSeesEngine::cancel()
{
    std::lock_guard<std::mutex> lock(m_solverMutex);
    if (m_solver) m_solver->stop();
}

} // namespace TSA::Analysis
