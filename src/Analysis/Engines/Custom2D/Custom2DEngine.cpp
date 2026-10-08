#include "Custom2DEngine.h"

#include "Custom2DAdapter.h"

namespace TSA::Analysis
{

EngineInfo Custom2DEngine::info() const
{
    EngineInfo i;
    i.id = kId;
    i.name = "Custom2D";
    i.version = m_solver ? m_solver->version() : std::string();
    i.description = m_solver ? "Moteur de calcul structurel 2D personnalisé TSA (" + m_solver->name() + ")."
                             : "Moteur de calcul structurel 2D personnalisé TSA — solveur non connecté.";
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
        a.message = "solveur Custom2D non connecté (adaptateur prêt : implémenter Custom2D::ISolver et "
                    "l'enregistrer dans registerBuiltInEngines). Aucun calcul n'est effectué.";
    return a;
}

ValidationResult Custom2DEngine::validate(const AnalysisContext& context, const AnalysisModel& model) const
{
    ValidationResult v;
    if (!model.isPlanar()) return v;   // erreur déjà signalée par la validation générique
    // La conversion signale toute perte d'information (hors plan, section oblique, charges).
    Custom2D::buildInput(context, model, &v);
    if (!m_solver)
        v.addWarning("Custom2D", "Solveur non connecté : la portée peut être validée mais pas calculée.");
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
    if (callbacks.progress) callbacks.progress(10, "Conversion de la portée en ossature plane…");
    const Custom2D::Input input = Custom2D::buildInput(context, model, nullptr);

    if (callbacks.progress) callbacks.progress(30, "Résolution Custom2D…");
    const Custom2D::Output output = m_solver->solve(input);
    if (callbacks.log)
        for (const auto& line : output.log) callbacks.log(line);

    if (!output.success)
    {
        r.message = output.message.empty() ? "Échec du solveur Custom2D." : output.message;
        return r;
    }
    if (callbacks.progress) callbacks.progress(90, "Remappage des résultats vers le modèle TSA…");
    r.results = Custom2D::mapResults(context, model, output);
    r.success = r.results.isValid();
    r.message = r.success ? "Calcul Custom2D terminé." : "Résultats Custom2D inexploitables.";
    return r;
}

void Custom2DEngine::cancel()
{
    if (m_solver) m_solver->cancel();
}

} // namespace TSA::Analysis
