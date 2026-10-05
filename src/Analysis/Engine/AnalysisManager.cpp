#include "AnalysisManager.h"

#include "../../Coordinate/GeometryTolerance.h"
#include "../../Model/Model.h"
#include "../../Standards/ModelValidator.h"
#include "../OpenSeesAnalysisBuilder.h"   // AnalysisParameters (paramètres communs de ModelValidator)

#include <algorithm>
#include <cmath>

namespace TSA::Analysis
{

namespace
{
std::string familyName(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return "Poutre";
    case StructuralElementKind::Column: return "Poteau";
    case StructuralElementKind::Truss: return "Treillis";
    case StructuralElementKind::Cable: return "Câble";
    }
    return "Barre";
}

bool familySupported(StructuralElementKind k, const AnalysisCapabilities& c)
{
    switch (k)
    {
    case StructuralElementKind::Beam:
    case StructuralElementKind::Column: return c.supportsFrame;
    case StructuralElementKind::Truss: return c.supportsTruss;
    case StructuralElementKind::Cable: return c.supportsCable;
    }
    return false;
}

/// « B1, B2, C4 (+3) » : liste courte d'éléments pour les messages.
std::string shortList(const std::vector<std::string>& labels, std::size_t max = 6)
{
    std::string s;
    for (std::size_t i = 0; i < labels.size() && i < max; ++i) s += (i ? ", " : "") + labels[i];
    if (labels.size() > max) s += " (+" + std::to_string(labels.size() - max) + ")";
    return s;
}

const char* dimensionLabel(AnalysisDimension d)
{
    return d == AnalysisDimension::Plane2D ? "2D (ossature plane)" : "3D (ossature spatiale)";
}
} // namespace

ValidationResult AnalysisManager::validateGeneric(const AnalysisContext& context,
                                                  const AnalysisModel& model,
                                                  const EngineInfo& info,
                                                  const AnalysisCapabilities& caps)
{
    ValidationResult v;
    const auto& snap = model.snapshot;
    const std::string engine = info.name;

    v.addInfo("Portée", "Portée : " + model.scopeLabel + " — " + std::to_string(snap.nodeCount()) + " nœud(s), "
                            + std::to_string(snap.elementCount()) + " barre(s).");

    // 1. Dimension et type d'analyse
    if (!caps.supportsDimension(context.dimension))
        v.addError("Moteur", "Le moteur " + engine + " ne calcule pas en " + dimensionLabel(context.dimension) + ".");
    if (!caps.supportsAnalysisType(context.type))
        v.addError("Moteur", "Le moteur " + engine + " ne propose pas ce type d'analyse.");
    if (context.type == AnalysisType::Modal && context.common.modeCount < 1)
        v.addError("Analyse", "Le nombre de modes propres demandé doit être supérieur ou égal à 1.");

    // 2. Plan d'une analyse 2D
    if (context.dimension == AnalysisDimension::Plane2D)
    {
        if (!model.plane)
            v.addError("Portée", "Une analyse 2D nécessite une portée plane : choisissez un axe de grille, "
                                 "un niveau ou un plan de travail.");
        else if (!model.outOfPlaneNodes.empty())
        {
            std::vector<std::string> ids;
            for (int id : model.outOfPlaneNodes) ids.push_back("N" + std::to_string(id));
            v.addError("Portée", "Nœuds hors du plan d'analyse : " + shortList(ids) + ".");
        }
        else
            v.addInfo("Portée", "Plan d'analyse défini.");
    }

    // 3. Contenu calculable
    if (snap.elementCount() == 0)
    {
        v.addError("Éléments", "La portée « " + model.scopeLabel + " » ne contient aucun élément filaire calculable.");
        return v;
    }

    // 4. Familles d'éléments supportées
    std::map<StructuralElementKind, std::vector<std::string>> unsupported;
    std::vector<std::string> zeroLength, badSection, badMaterial;
    for (const auto& [tag, el] : snap.elements())
    {
        if (!familySupported(el.type, caps)) unsupported[el.type].push_back(el.key().label());
        if (el.length <= TSA::Coordinate::GeometryTolerance::pointCoincidence) zeroLength.push_back(el.key().label());
        const bool bending = el.type == StructuralElementKind::Beam || el.type == StructuralElementKind::Column;
        if (el.section.area() <= 0.0 || (bending && (el.section.iy() <= 0.0 || el.section.iz() <= 0.0)))
            badSection.push_back(el.key().label());
        if (el.material.mechanical.youngModulus <= 0.0) badMaterial.push_back(el.key().label());
    }
    for (const auto& [kind, labels] : unsupported)
        v.addError("Éléments", "Le moteur " + engine + " ne supporte pas les éléments de type " + familyName(kind)
                                   + " (" + shortList(labels) + ").");
    if (!zeroLength.empty()) v.addError("Éléments", "Barres de longueur nulle : " + shortList(zeroLength) + ".");
    if (!badSection.empty())
        v.addError("Sections", "Sections sans aire ou sans inertie : " + shortList(badSection) + ".");
    else
        v.addInfo("Sections", "Sections assignées.");
    if (!badMaterial.empty())
        v.addError("Matériaux", "Matériaux sans module d'Young : " + shortList(badMaterial) + ".");
    else
        v.addInfo("Matériaux", "Matériaux assignés.");
    if (unsupported.empty() && zeroLength.empty()) v.addInfo("Éléments", "Éléments valides.");

    // 5. Dalles / voiles (éléments surfaciques)
    if (model.hasPlanarElements() && !caps.supportsShell)
    {
        const std::string what = std::to_string(model.slabs.size()) + " dalle(s) et "
                                 + std::to_string(model.walls.size()) + " voile(s)";
        if (caps.planarElementPolicy == UnsupportedElementPolicy::Reject)
            v.addError("Éléments", "Le moteur " + engine + " ne supporte pas les éléments de type Shell (" + what
                                       + " dans la portée). Sélectionnez un plan contenant uniquement des "
                                         "éléments compatibles.");
        else if (!model.entireModel)   // modèle complet : déjà signalé par ModelValidator
            v.addWarning("Éléments", what + " de la portée ne sont pas pris en compte par le moteur " + engine
                                         + " (ni rigidité, ni charges) : seuls les éléments filaires sont calculés.");
    }
    if (!model.foundations.empty())
        v.addInfo("Éléments", std::to_string(model.foundations.size())
                                  + " fondation(s) : seules les conditions d'appui des nœuds sont transmises.");

    // 6. Éléments reliés à la portée mais exclus
    if (!model.crossingElements.empty())
    {
        std::vector<std::string> labels;
        for (const auto& k : model.crossingElements) labels.push_back(k.label());
        v.addWarning("Portée", std::to_string(labels.size()) + " barre(s) reliée(s) à la portée mais hors de celle-ci "
                                   "sont exclues (leur rigidité et leurs charges ne sont pas transmises) : "
                                   + shortList(labels) + ".");
    }

    // 7. Appuis
    bool anySupport = false, anySpring = false;
    for (const auto& [id, n] : snap.nodes())
    {
        anySupport |= std::any_of(n.definedFix.begin(), n.definedFix.end(), [](bool f) { return f; });
        anySpring |= (n.kTx > 0 || n.kTy > 0 || n.kTz > 0 || n.kRx > 0 || n.kRy > 0 || n.kRz > 0);
    }
    if (!anySupport && !anySpring)
        v.addError("Appuis", "Aucun appui dans la portée : la structure est instable.");
    else if (anySpring && !caps.supportsSprings)
        v.addError("Appuis", "Le moteur " + engine + " ne supporte pas les appuis élastiques (ressorts).");
    else
        v.addInfo("Appuis", "Appuis définis.");

    // 8. Chargement
    if (context.combinationId > 0)
    {
        if (!snap.combinations().count(context.combinationId))
            v.addError("Charges", "Combinaison #" + std::to_string(context.combinationId) + " introuvable.");
    }
    for (int id : context.loadCaseIds)
        if (!snap.loadCases().count(id)) v.addError("Charges", "Cas de charge #" + std::to_string(id) + " introuvable.");

    if (context.type != AnalysisType::Modal)
    {
        const bool anyLoad = !snap.nodalLoads().empty() || !snap.memberLoads().empty();
        if (!anyLoad && !context.common.includeSelfWeight)
            v.addWarning("Charges", "Aucune charge dans la portée et poids propre désactivé : résultats nuls.");
        else
            v.addInfo("Charges", "Charges valides.");
    }
    if (model.droppedNodalLoads + model.droppedMemberLoads > 0 && !model.entireModel)
        v.addInfo("Charges", std::to_string(model.droppedNodalLoads + model.droppedMemberLoads)
                                 + " charge(s) appliquée(s) hors portée ne sont pas transmises.");
    return v;
}

PreparedAnalysis AnalysisManager::prepare(const TSA::Model::Model& model,
                                          const TSA::Grid::GridManager* grids,
                                          const AnalysisContext& context) const
{
    PreparedAnalysis p;
    const AnalysisEngine* engine = m_registry.engine(context.engineId);
    if (!engine)
    {
        p.validation.addError("Moteur", "Moteur d'analyse inconnu : « " + context.engineId + " ».");
        return p;
    }

    const ResolvedScope scope = AnalysisScopeResolver::resolve(model, grids, context.scope);
    if (!scope.ok)
    {
        p.validation.addError("Portée", scope.error);
        return p;
    }

    p.model = AnalysisModelExtractor::extract(model, scope, context.dimension);
    p.extracted = true;

    p.validation = validateGeneric(context, p.model, engine->info(), engine->capabilities());

    // Modèle complet : contrôles normatifs historiques de TSA (communs à tous les moteurs).
    if (p.model.entireModel)
    {
        AnalysisParameters common;
        common.type = context.type;
        common.numEigenmodes = context.common.modeCount;
        common.includeSelfWeight = context.common.includeSelfWeight;
        const auto report = TSA::Standards::ModelValidator::validateForAnalysis(model, common);
        for (const auto& e : report.formattedErrors()) p.validation.addError("Modèle", e);
        for (const auto& w : report.formattedWarnings()) p.validation.addWarning("Modèle", w);
    }

    p.validation.merge(engine->validate(context, p.model));
    return p;
}

AnalysisRunResult AnalysisManager::run(const AnalysisContext& context,
                                       const PreparedAnalysis& prepared,
                                       const AnalysisRunCallbacks& callbacks)
{
    AnalysisRunResult r;
    AnalysisEngine* engine = m_registry.engine(context.engineId);
    if (!engine)
    {
        r.message = "Moteur d'analyse inconnu : « " + context.engineId + " ».";
        return r;
    }
    if (!prepared.canRun())
    {
        r.message = "Le modèle d'analyse n'est pas valide :";
        for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) r.message += "\n  • " + e;
        return r;
    }
    const EngineAvailability avail = engine->availability();
    if (!avail.available)
    {
        r.message = "Moteur " + engine->info().name + " indisponible : " + avail.message;
        return r;
    }

    r = engine->run(context, prepared.model, callbacks);
    if (!r.success) return r;

    // Traçabilité commune, renseignée ici pour qu'aucun moteur ne puisse l'omettre.
    const EngineInfo info = engine->info();
    auto& meta = r.results.executionMetadata();
    meta.engineId = info.id;
    meta.solverEngine = info.name;
    if (!info.version.empty()) meta.solverVersion = info.version;
    meta.analysisScope = prepared.model.entireModel ? std::string() : prepared.model.scopeLabel;
    meta.analysisDimension = dimensionKey(context.dimension);

    // Catégories annoncées = capacité déclarée ET données réellement présentes.
    const AnalysisCapabilities caps = engine->capabilities();
    ResultAvailability a = r.results.availabilityFromData();
    a.displacements &= caps.providesDisplacements;
    a.reactions &= caps.providesReactions;
    a.elementForces &= caps.providesElementForces;
    a.modal &= caps.supportsModal;
    a.elementStiffness &= caps.providesElementStiffness;
    a.globalStiffness &= caps.providesGlobalStiffness;
    a.dofMapping &= caps.providesDofMapping;
    r.results.setAvailability(a);
    return r;
}

void AnalysisManager::cancel(const EngineId& id)
{
    if (auto* e = m_registry.engine(id)) e->cancel();
}

} // namespace TSA::Analysis
