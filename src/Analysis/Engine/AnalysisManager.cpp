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
    if (snap.elementCount() == 0 && context.surfaces.empty())
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
    if (!context.surfaces.supports.empty())
    {
        anySupport = true;
        v.addInfo("Appuis", std::to_string(context.surfaces.supports.size()) + " nœud(s) du maillage des surfaces portés par des appuis linéiques de bord.");
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

    // Charges surfaciques converties sur le maillage transmis (coques) : ce sont des charges du calcul
    const bool surfaceLoads = !context.surfaces.loads.empty() && context.common.useGeneratedMesh && caps.supportsShell;
    const bool anyLoad = !snap.nodalLoads().empty() || !snap.memberLoads().empty() || surfaceLoads;
    if (context.type == AnalysisType::Modal)
    {
        for (int id : context.common.modalMassCaseIds)
            if (!snap.loadCases().count(id)) v.addError("Masses", "Cas de charge #" + std::to_string(id) + " (converti en masses) introuvable.");
        v.addInfo("Masses", context.common.modalMassCaseIds.empty()
                                ? "Analyse modale : masses des éléments seulement (barres ρ·A, coques ρ·e) ; les charges ne sont pas des masses."
                                : "Analyse modale : masses des éléments + charges verticales descendantes des cas choisis converties (m = F/g).");
    }
    else if (!anyLoad && !context.common.includeSelfWeight)
        v.addWarning("Charges", "Aucune charge dans la portée et poids propre désactivé : résultats nuls.");
    else
        v.addInfo("Charges", "Charges valides.");
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

    p.model = AnalysisModelExtractor::extract(model, scope, context.dimension, context.common.supportRestraints);
    p.extracted = true;

    p.validation = validateGeneric(context, p.model, engine->info(), engine->capabilities());

    // Calcul 3D : blocages ajoutés aux appuis articulés / simples (appui à fourche, structure plane),
    // listés pour que l'utilisateur sache quelles rotations le calcul a bloquées (CalculationSnapshot).
    if (context.dimension == AnalysisDimension::Space3D)
    {
        const auto& restraints = p.model.snapshot.automaticRestraints();
        const std::size_t shown = std::min<std::size_t>(restraints.size(), 8);
        for (std::size_t i = 0; i < shown; ++i) p.validation.addInfo("Appuis", "Blocage automatique : " + restraints[i].reason);
        if (restraints.size() > shown)
            p.validation.addInfo("Appuis", std::to_string(restraints.size() - shown) + " autre(s) blocage(s) automatique(s) d'appuis articulés ou simples.");
        for (const auto& n : p.model.snapshot.restraintNotes()) p.validation.addInfo("Appuis", n);
        // Risques des blocages (mode historique) : déjà ajoutés par ModelValidator pour le modèle complet.
        if (!p.model.entireModel)
            for (const auto& w : p.model.snapshot.restraintWarnings()) p.validation.addWarning("Appuis", w);
        if (context.common.supportRestraints == SupportRestraintMode::None)
            p.validation.addInfo("Appuis", "Appuis articulés et simples : aucun blocage automatique (rotules sphériques réelles) ; "
                                           "un mécanisme éventuel est signalé avant ou après le calcul.");
    }

    // Modèle complet : contrôles normatifs historiques de TSA (communs à tous les moteurs).
    if (p.model.entireModel)
    {
        AnalysisParameters common;
        common.type = context.type;
        common.includeSelfWeight = context.common.includeSelfWeight;
        common.supportRestraints = context.common.supportRestraints;
        common.surfacesTransmitted = !context.surfaces.empty() && context.common.useGeneratedMesh && engine->capabilities().supportsShell;
        const auto report = TSA::Standards::ModelValidator::validateForAnalysis(model, common);
        for (const auto& e : report.formattedErrors()) p.validation.addError("Modèle", e);
        for (const auto& w : report.formattedWarnings()) p.validation.addWarning("Modèle", w);
    }

    // Maillage 1D généré (paramètres du mailleur) → subdivision des barres, si le moteur la prend en charge
    // et si l'option du solveur le demande. Jamais de transmission partielle silencieuse.
    if (!context.memberSubdivisions.empty())
    {
        if (!context.common.useGeneratedMesh)
            p.validation.addInfo("Maillage", "Maillage généré non transmis (option du calcul) : un élément fini par barre.");
        else if (!engine->capabilities().supportsMesh)
            p.validation.addWarning("Maillage", "Le moteur " + engine->info().name +
                                                    " ne prend pas en charge les barres subdivisées : maillage non transmis, un élément fini par barre.");
        else
        {
            std::vector<std::string> notes;
            p.model.snapshot = CalculationSnapshot::subdivided(p.model.snapshot, context.memberSubdivisions, &p.model.subdivisions, &notes);
            for (const auto& n : notes) p.validation.addWarning("Maillage", n);
            // Les éléments sont renumérotés : correspondance reconstruite entièrement.
            p.model.mapping = AnalysisMapping();
            for (const auto& [id, node] : p.model.snapshot.nodes()) p.model.mapping.addNode(id);
            for (const auto& [tag, el] : p.model.snapshot.elements()) p.model.mapping.addElement(el.key(), tag);
            p.validation.addInfo("Maillage", std::to_string(p.model.subdivisions.parts.size()) + " barre(s) subdivisée(s) en " +
                                                 std::to_string(p.model.subdivisions.subElementCount()) + " éléments finis (" +
                                                 std::to_string(p.model.subdivisions.internalNodes.size()) + " nœuds intermédiaires)" +
                                                 (context.meshLabel.empty() ? std::string() : " — " + context.meshLabel) + ".");
        }
    }

    // Éléments surfaciques maillés (coques) : ajoutés au modèle de calcul si le moteur les prend en charge.
    if (!context.surfaces.empty())
    {
        if (!context.common.useGeneratedMesh)
            p.validation.addWarning("Surfaces", "Maillage non transmis (option du calcul) : dalles et voiles non calculés.");
        else if (!engine->capabilities().supportsShell)
            p.validation.addError("Surfaces", "Le moteur " + engine->info().name + " ne calcule pas les éléments surfaciques (coques).");
        else
        {
            std::vector<std::string> notes;
            std::string error;
            p.model.snapshot = CalculationSnapshot::withSurfaces(p.model.snapshot, context.surfaces, &p.model.surfaceNodes, &notes, &error);
            if (!error.empty())
                p.validation.addError("Surfaces", error);
            else
            {
                for (const auto& n : notes) p.validation.addInfo("Surfaces", n);
                for (const auto& n : context.surfaces.notes) p.validation.addInfo("Surfaces", n);
            }
        }
    }
    else if (const std::size_t calculated = [&] {
                 // Charges surfaciques des cas réellement calculés : combinaison choisie, sinon cas choisis, sinon tous
                 std::size_t n = 0;
                 const auto& lm = model.loadManager();
                 const auto* combo = context.combinationId > 0 ? lm.getCombination(context.combinationId) : nullptr;
                 for (const auto& [id, sl] : lm.surfaceLoads())
                 {
                     const bool in = context.combinationId > 0 ? (combo && combo->hasCase(sl.loadCaseId))
                                     : context.loadCaseIds.empty()
                                         ? true
                                         : std::find(context.loadCaseIds.begin(), context.loadCaseIds.end(), sl.loadCaseId) != context.loadCaseIds.end();
                     n += in;
                 }
                 return n;
             }();
             calculated > 0)
        p.validation.addError("Surfaces", std::to_string(calculated) +
                                              " charge(s) surfacique(s) définie(s) mais aucune surface maillée transmise : générer le maillage "
                                              "(Analyse > Maillage) ou supprimer ces charges. Aucune charge n'est ignorée en silence.");
    else if (p.model.hasPlanarElements() && engine->capabilities().supportsShell)
        p.validation.addWarning("Surfaces", "Dalles et voiles non maillés : ni leur rigidité ni leurs charges ne sont calculées. "
                                            "Générer le maillage (Analyse > Maillage) pour les transmettre en coques.");

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

    // Barres subdivisées : résultats rattachés aux barres TSA (le reste de l'application ne voit jamais
    // les sous-éléments ni les nœuds intermédiaires).
    if (!prepared.model.subdivisions.empty())
    {
        std::map<ElementKey, std::vector<std::pair<ElementKey, double>>> parts;
        for (const auto& [parent, list] : prepared.model.subdivisions.parts)
            for (const auto& part : list) parts[parent].push_back({ part.key, part.x0 });
        r.results.mergeSubdivisions(parts, prepared.model.subdivisions.internalNodes);
    }

    // Coques : déplacements par nœud du maillage ; nœuds propres aux surfaces retirés des résultats nodaux de TSA.
    if (!prepared.model.surfaceNodes.empty())
    {
        r.results.attachSurfaceResults(prepared.model.surfaceNodes.meshToNode, prepared.model.surfaceNodes.surfaceOnlyNodes);
        r.results.setSurfaceMeshStamp(context.surfaces.meshStamp);
        r.results.setSurfaceMeshSummary(context.surfaces.mesh);
    }

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
