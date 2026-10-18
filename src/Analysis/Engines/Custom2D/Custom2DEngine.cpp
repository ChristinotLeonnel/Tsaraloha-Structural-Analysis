#include "Custom2DEngine.h"

#include "Custom2DAdapter.h"

#include <QCoreApplication>

namespace TSA::Analysis
{

EngineInfo Custom2DEngine::info() const
{
    EngineInfo i;
    i.id = kId;
    i.name = "Custom2D";
    i.version = m_solver ? m_solver->version() : std::string();
    i.description = m_solver ? QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Moteur de calcul structurel 2D personnalisé TSA (%1).")
                                   .arg(QString::fromStdString(m_solver->name())).toStdString()
                             : QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Moteur de calcul structurel 2D personnalisé TSA — solveur non connecté.").toStdString();
    return i;
}

AnalysisCapabilities Custom2DEngine::capabilities() const
{
    // Capacités du contrat Custom2D::ISolver (ossature plane, statique) ; treillis et ressorts
    // seulement si le solveur branché les implémente (Custom2D::Features).
    AnalysisCapabilities c;
    const Custom2D::Features f = m_solver ? m_solver->features() : Custom2D::Features {};
    c.supports2D = true;
    c.supportsFrame = true;
    c.supportsTruss = f.truss;
    c.supportsSprings = f.springs;
    c.supportsStatic = true;
    c.supportsCustomOptions = true;
    c.planarElementPolicy = UnsupportedElementPolicy::Reject;
    c.providesDisplacements = true;
    c.providesReactions = true;
    c.providesElementForces = true;
    c.providesGlobalStiffness = f.linearSystem;   // option « exportSystem » (dock Données du calcul)
    c.providesDofMapping = f.linearSystem;
    return c;
}

EngineAvailability Custom2DEngine::availability() const
{
    EngineAvailability a;
    a.available = m_solver != nullptr;
    if (!a.available)
        a.message = QCoreApplication::translate("TSA::Analysis::Custom2DEngine",
                        "solveur Custom2D non connecté (adaptateur prêt : implémenter Custom2D::ISolver et "
                        "l'enregistrer dans registerBuiltInEngines). Aucun calcul n'est effectué.").toStdString();
    return a;
}

ValidationResult Custom2DEngine::validate(const AnalysisContext& context, const AnalysisModel& model) const
{
    ValidationResult v;
    if (!model.isPlanar()) return v;   // erreur déjà signalée par la validation générique
    // La conversion signale toute perte d'information (hors plan, section oblique, charges).
    Custom2D::buildInput(context, model, &v);
    if (!m_solver)
        v.addWarning("Custom2D", QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Solveur non connecté : la portée peut être validée mais pas calculée.").toStdString());
    return v;
}

AnalysisRunResult Custom2DEngine::run(const AnalysisContext& context, const AnalysisModel& model,
                                      const AnalysisRunCallbacks& callbacks)
{
    AnalysisRunResult r;
    if (!m_solver)
    {
        r.message = availability().message;
        return r;
    }
    if (callbacks.progress) callbacks.progress(10, QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Conversion de la portée en ossature plane…").toStdString());
    const Custom2D::Input input = Custom2D::buildInput(context, model, nullptr);

    if (callbacks.progress) callbacks.progress(30, QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Résolution Custom2D…").toStdString());
    const Custom2D::Output output = m_solver->solve(input);
    if (callbacks.log)
        for (const auto& line : output.log) callbacks.log(line);

    if (!output.success)
    {
        r.message = output.message.empty() ? QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Échec du solveur Custom2D.").toStdString() : output.message;
        return r;
    }
    if (callbacks.progress) callbacks.progress(90, QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Remappage des résultats vers le modèle TSA…").toStdString());
    r.results = Custom2D::mapResults(context, model, output);
    r.results.setSolverMesh(Custom2D::solverMesh(model, input));
    r.success = r.results.isValid();
    r.message = r.success ? QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Calcul Custom2D terminé.").toStdString()
                          : QCoreApplication::translate("TSA::Analysis::Custom2DEngine", "Résultats Custom2D inexploitables.").toStdString();
    return r;
}

void Custom2DEngine::cancel()
{
    if (m_solver) m_solver->cancel();
}

} // namespace TSA::Analysis
