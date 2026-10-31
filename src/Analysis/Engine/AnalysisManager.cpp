#include "AnalysisManager.h"

#include "../../Coordinate/GeometryTolerance.h"
#include "../../Model/Model.h"
#include "../../Standards/ModelValidator.h"
#include "../OpenSeesAnalysisBuilder.h"   // AnalysisParameters (paramètres communs de ModelValidator)

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace TSA::Analysis
{

namespace
{
/// Texte de l'interface (contexte de traduction « TSA::Analysis::AnalysisManager »).
std::string trText(const char* text)
{
    return QCoreApplication::translate("TSA::Analysis::AnalysisManager", text).toStdString();
}

std::string familyName(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Poutre"));
    case StructuralElementKind::Column: return trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Poteau"));
    case StructuralElementKind::Truss: return trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Treillis"));
    case StructuralElementKind::Cable: return trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Câble"));
    }
    return trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Barre"));
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

std::string dimensionLabel(AnalysisDimension d)
{
    return d == AnalysisDimension::Plane2D ? trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "2D (ossature plane)"))
                                           : trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "3D (ossature spatiale)"));
}

/// Texte de l'interface avec paramètres %1, %2… (chaînes déjà formatées). Le texte source est marqué
/// QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", …) à chaque appel pour lupdate.
std::string trArgs(const char* text, std::initializer_list<std::string> args)
{
    QString s = QCoreApplication::translate("TSA::Analysis::AnalysisManager", text);
    QStringList list;
    for (const auto& a : args) list << QString::fromStdString(a);
    switch (list.size())
    {
    case 1: return s.arg(list[0]).toStdString();
    case 2: return s.arg(list[0], list[1]).toStdString();
    case 3: return s.arg(list[0], list[1], list[2]).toStdString();
    case 4: return s.arg(list[0], list[1], list[2], list[3]).toStdString();
    default: return s.toStdString();
    }
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

    const std::string catScope = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Portée"));
    const std::string catEngine = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur"));
    const std::string catElements = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Éléments"));
    const std::string catSections = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Sections"));
    const std::string catMaterials = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Matériaux"));
    const std::string catSupports = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Appuis"));
    const std::string catLoads = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Charges"));
    const std::string catMasses = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Masses"));

    v.addInfo(catScope, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Portée : %1 — %2 nœud(s), %3 barre(s)."),
                               { model.scopeLabel, std::to_string(snap.nodeCount()), std::to_string(snap.elementCount()) }));

    // 1. Dimension et type d'analyse
    if (!caps.supportsDimension(context.dimension))
        v.addError(catEngine, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Le moteur %1 ne calcule pas en %2."),
                                     { engine, dimensionLabel(context.dimension) }));
    if (!caps.supportsAnalysisType(context.type))
        v.addError(catEngine, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Le moteur %1 ne propose pas ce type d'analyse."),
                                     { engine }));

    // 2. Plan d'une analyse 2D
    if (context.dimension == AnalysisDimension::Plane2D)
    {
        if (!model.plane)
            v.addError(catScope, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                          "Une analyse 2D nécessite une portée plane : choisissez un axe de grille, "
                                                          "un niveau ou un plan de travail.")));
        else if (!model.outOfPlaneNodes.empty())
        {
            std::vector<std::string> ids;
            for (int id : model.outOfPlaneNodes) ids.push_back("N" + std::to_string(id));
            v.addError(catScope, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Nœuds hors du plan d'analyse : %1."),
                                        { shortList(ids) }));
        }
        else
            v.addInfo(catScope, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Plan d'analyse défini.")));
    }

    // 3. Contenu calculable
    if (snap.elementCount() == 0 && context.surfaces.empty())
    {
        v.addError(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                         "La portée « %1 » ne contient aucun élément filaire calculable."),
                                       { model.scopeLabel }));
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
        v.addError(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                         "Le moteur %1 ne supporte pas les éléments de type %2 (%3)."),
                                       { engine, familyName(kind), shortList(labels) }));
    if (!zeroLength.empty())
        v.addError(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Barres de longueur nulle : %1."),
                                       { shortList(zeroLength) }));
    if (!badSection.empty())
        v.addError(catSections, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Sections sans aire ou sans inertie : %1."),
                                       { shortList(badSection) }));
    else
        v.addInfo(catSections, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Sections assignées.")));
    if (!badMaterial.empty())
        v.addError(catMaterials, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Matériaux sans module d'Young : %1."),
                                        { shortList(badMaterial) }));
    else
        v.addInfo(catMaterials, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Matériaux assignés.")));
    if (unsupported.empty() && zeroLength.empty())
        v.addInfo(catElements, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Éléments valides.")));

    // 5. Dalles / voiles (éléments surfaciques)
    if (model.hasPlanarElements() && !caps.supportsShell)
    {
        const std::string slabs = std::to_string(model.slabs.size());
        const std::string walls = std::to_string(model.walls.size());
        if (caps.planarElementPolicy == UnsupportedElementPolicy::Reject)
            v.addError(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                             "Le moteur %1 ne supporte pas les éléments de type Shell (%2 dalle(s) et "
                                                             "%3 voile(s) dans la portée). Sélectionnez un plan contenant uniquement des "
                                                             "éléments compatibles."),
                                           { engine, slabs, walls }));
        else if (!model.entireModel)   // modèle complet : déjà signalé par ModelValidator
            v.addWarning(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                               "%1 dalle(s) et %2 voile(s) de la portée ne sont pas pris en compte par "
                                                               "le moteur %3 (ni rigidité, ni charges) : seuls les éléments filaires "
                                                               "sont calculés."),
                                             { slabs, walls, engine }));
    }
    if (!model.foundations.empty())
        v.addInfo(catElements, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                        "%1 fondation(s) : seules les conditions d'appui des nœuds sont transmises."),
                                      { std::to_string(model.foundations.size()) }));

    // 6. Éléments reliés à la portée mais exclus
    if (!model.crossingElements.empty())
    {
        std::vector<std::string> labels;
        for (const auto& k : model.crossingElements) labels.push_back(k.label());
        v.addWarning(catScope, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                        "%1 barre(s) reliée(s) à la portée mais hors de celle-ci "
                                                        "sont exclues (leur rigidité et leurs charges ne sont pas transmises) : %2."),
                                      { std::to_string(labels.size()), shortList(labels) }));
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
        v.addInfo(catSupports, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                        "%1 nœud(s) du maillage des surfaces portés par des appuis linéiques de bord."),
                                      { std::to_string(context.surfaces.supports.size()) }));
    }
    if (!anySupport && !anySpring)
        v.addError(catSupports, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                         "Aucun appui dans la portée : la structure est instable.")));
    else if (anySpring && !caps.supportsSprings)
        v.addError(catSupports, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                         "Le moteur %1 ne supporte pas les appuis élastiques (ressorts)."),
                                       { engine }));
    else
        v.addInfo(catSupports, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Appuis définis.")));

    // 8. Chargement
    if (context.combinationId > 0)
    {
        if (!snap.combinations().count(context.combinationId))
            v.addError(catLoads, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Combinaison #%1 introuvable."),
                                        { std::to_string(context.combinationId) }));
    }
    for (int id : context.loadCaseIds)
        if (!snap.loadCases().count(id))
            v.addError(catLoads, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Cas de charge #%1 introuvable."),
                                        { std::to_string(id) }));

    // Charges surfaciques converties sur le maillage transmis (coques) : ce sont des charges du calcul
    const bool surfaceLoads = !context.surfaces.loads.empty() && context.common.useGeneratedMesh && caps.supportsShell;
    const bool anyLoad = !snap.nodalLoads().empty() || !snap.memberLoads().empty() || surfaceLoads;
    if (context.type == AnalysisType::Modal)
    {
        for (int id : context.common.modalMassCaseIds)
            if (!snap.loadCases().count(id))
                v.addError(catMasses, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                               "Cas de charge #%1 (converti en masses) introuvable."),
                                             { std::to_string(id) }));
        v.addInfo(catMasses, context.common.modalMassCaseIds.empty()
                                ? trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                           "Analyse modale : masses des éléments seulement (barres ρ·A, coques ρ·e) ; "
                                                           "les charges ne sont pas des masses."))
                                : trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                           "Analyse modale : masses des éléments + charges verticales descendantes des "
                                                           "cas choisis converties (m = F/g).")));
    }
    else if (!anyLoad && !context.common.includeSelfWeight)
        v.addWarning(catLoads, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                        "Aucune charge dans la portée et poids propre désactivé : résultats nuls.")));
    else
        v.addInfo(catLoads, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Charges valides.")));
    if (model.droppedNodalLoads + model.droppedMemberLoads > 0 && !model.entireModel)
        v.addInfo(catLoads, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                     "%1 charge(s) appliquée(s) hors portée ne sont pas transmises."),
                                   { std::to_string(model.droppedNodalLoads + model.droppedMemberLoads) }));
    return v;
}

std::vector<std::string> AnalysisManager::unsupportedFeatures(const AnalysisContext& context) const
{
    std::vector<std::string> features;
    if (context.unsupportedAnalysisType == "nonlinear_static")
        features.push_back(trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "analyse statique non linéaire")));
    else if (!context.unsupportedAnalysisType.empty())
        features.push_back(trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "analyse de type « %1 »"), { context.unsupportedAnalysisType }));
    if (const AnalysisEngine* engine = m_registry.engine(context.engineId))
        for (const auto& setting : engine->unsupportedSettings(context.settingsFor(context.engineId))) features.push_back(setting);
    return features;
}

std::string AnalysisManager::unsupportedFeaturesMessage(const std::vector<std::string>& features)
{
    std::string list;
    for (std::size_t i = 0; i < features.size(); ++i) list += (i ? ", " : "") + features[i];
    return trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                    "Ce projet demande une fonctionnalité qui n'est plus prise en charge : %1. TSA prend actuellement en "
                                    "charge l'analyse structurale linéaire : ce calcul n'est pas lancé à la place d'un autre. Choisissez "
                                    "une analyse linéaire dans les paramètres d'analyse. Les autres données du projet sont conservées."),
                  { list });
}

PreparedAnalysis AnalysisManager::prepare(const TSA::Model::Model& model,
                                          const TSA::Grid::GridManager* grids,
                                          const AnalysisContext& context) const
{
    PreparedAnalysis p;
    const AnalysisEngine* engine = m_registry.engine(context.engineId);
    if (!engine)
    {
        p.validation.addError(trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur")),
                              trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur d'analyse inconnu : « %1 »."),
                                     { context.engineId }));
        return p;
    }

    const ResolvedScope scope = AnalysisScopeResolver::resolve(model, grids, context.scope);
    if (!scope.ok)
    {
        p.validation.addError(trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Portée")), scope.error);
        return p;
    }

    p.model = AnalysisModelExtractor::extract(model, scope, context.dimension, context.common.supportRestraints);
    p.extracted = true;

    p.validation = validateGeneric(context, p.model, engine->info(), engine->capabilities());
    if (const auto features = unsupportedFeatures(context); !features.empty())
        p.validation.addError(trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur")), unsupportedFeaturesMessage(features));

    // Calcul 3D : blocages ajoutés aux appuis articulés / simples (appui à fourche, structure plane),
    // listés pour que l'utilisateur sache quelles rotations le calcul a bloquées (CalculationSnapshot).
    if (context.dimension == AnalysisDimension::Space3D)
    {
        const auto& restraints = p.model.snapshot.automaticRestraints();
        const std::size_t shown = std::min<std::size_t>(restraints.size(), 8);
        const std::string catSupports = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Appuis"));
        for (std::size_t i = 0; i < shown; ++i)
            p.validation.addInfo(catSupports, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Blocage automatique : %1"),
                                                     { restraints[i].reason }));
        if (restraints.size() > shown)
            p.validation.addInfo(catSupports, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                       "%1 autre(s) blocage(s) automatique(s) d'appuis articulés ou simples."),
                                                     { std::to_string(restraints.size() - shown) }));
        for (const auto& n : p.model.snapshot.restraintNotes()) p.validation.addInfo(catSupports, n);
        // Risques des blocages (mode historique) : déjà ajoutés par ModelValidator pour le modèle complet.
        if (!p.model.entireModel)
            for (const auto& w : p.model.snapshot.restraintWarnings()) p.validation.addWarning(catSupports, w);
        if (context.common.supportRestraints == SupportRestraintMode::None)
            p.validation.addInfo(catSupports, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                       "Appuis articulés et simples : aucun blocage automatique (rotules "
                                                                       "sphériques réelles) ; un mécanisme éventuel est signalé avant ou "
                                                                       "après le calcul.")));
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
        const std::string catModel = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Modèle"));
        for (const auto& e : report.formattedErrors()) p.validation.addError(catModel, e);
        for (const auto& w : report.formattedWarnings()) p.validation.addWarning(catModel, w);
    }

    // Maillage 1D généré (paramètres du mailleur) → subdivision des barres, si le moteur la prend en charge
    // et si l'option du solveur le demande. Jamais de transmission partielle silencieuse.
    if (!context.memberSubdivisions.empty())
    {
        const std::string catMesh = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Maillage"));
        if (!context.common.useGeneratedMesh)
            p.validation.addInfo(catMesh, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                   "Maillage généré non transmis (option du calcul) : un élément fini par barre.")));
        else if (!engine->capabilities().supportsMesh)
            p.validation.addWarning(catMesh, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                      "Le moteur %1 ne prend pas en charge les barres subdivisées : "
                                                                      "maillage non transmis, un élément fini par barre."),
                                                    { engine->info().name }));
        else
        {
            std::vector<std::string> notes;
            p.model.snapshot = CalculationSnapshot::subdivided(p.model.snapshot, context.memberSubdivisions, &p.model.subdivisions, &notes);
            for (const auto& n : notes) p.validation.addWarning(catMesh, n);
            // Les éléments sont renumérotés : correspondance reconstruite entièrement.
            p.model.mapping = AnalysisMapping();
            for (const auto& [id, node] : p.model.snapshot.nodes()) p.model.mapping.addNode(id);
            for (const auto& [tag, el] : p.model.snapshot.elements()) p.model.mapping.addElement(el.key(), tag);
            const std::string counts = trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                "%1 barre(s) subdivisée(s) en %2 éléments finis (%3 nœuds intermédiaires)"),
                                              { std::to_string(p.model.subdivisions.parts.size()),
                                                std::to_string(p.model.subdivisions.subElementCount()),
                                                std::to_string(p.model.subdivisions.internalNodes.size()) });
            p.validation.addInfo(catMesh, counts + (context.meshLabel.empty() ? std::string() : " — " + context.meshLabel) + ".");
        }
    }

    // Éléments surfaciques maillés (coques) : ajoutés au modèle de calcul si le moteur les prend en charge.
    const std::string catSurfaces = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Surfaces"));
    if (!context.surfaces.empty())
    {
        if (!context.common.useGeneratedMesh)
            p.validation.addWarning(catSurfaces, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                          "Maillage non transmis (option du calcul) : dalles et voiles non calculés.")));
        else if (!engine->capabilities().supportsShell)
            p.validation.addError(catSurfaces, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                        "Le moteur %1 ne calcule pas les éléments surfaciques (coques)."),
                                                      { engine->info().name }));
        else
        {
            std::vector<std::string> notes;
            std::string error;
            p.model.snapshot = CalculationSnapshot::withSurfaces(p.model.snapshot, context.surfaces, &p.model.surfaceNodes, &notes, &error);
            if (!error.empty())
                p.validation.addError(catSurfaces, error);
            else
            {
                for (const auto& n : notes) p.validation.addInfo(catSurfaces, n);
                for (const auto& n : context.surfaces.notes) p.validation.addInfo(catSurfaces, n);
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
        p.validation.addError(catSurfaces, trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                    "%1 charge(s) surfacique(s) définie(s) mais aucune surface maillée "
                                                                    "transmise : générer le maillage (Analyse > Maillage) ou supprimer ces "
                                                                    "charges. Aucune charge n'est ignorée en silence."),
                                                  { std::to_string(calculated) }));
    else if (p.model.hasPlanarElements() && engine->capabilities().supportsShell)
        p.validation.addWarning(catSurfaces, trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager",
                                                                      "Dalles et voiles non maillés : ni leur rigidité ni leurs charges ne "
                                                                      "sont calculées. Générer le maillage (Analyse > Maillage) pour les "
                                                                      "transmettre en coques.")));

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
        r.message = trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur d'analyse inconnu : « %1 »."),
                           { context.engineId });
        return r;
    }
    // Contrôlé ici aussi : le contexte reçu peut différer de celui qui a servi à préparer le modèle.
    if (const auto features = unsupportedFeatures(context); !features.empty())
    {
        r.message = unsupportedFeaturesMessage(features);
        return r;
    }
    if (!prepared.canRun())
    {
        r.message = trText(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Le modèle d'analyse n'est pas valide :"));
        for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) r.message += "\n  • " + e;
        return r;
    }
    const EngineAvailability avail = engine->availability();
    if (!avail.available)
    {
        r.message = trArgs(QT_TRANSLATE_NOOP("TSA::Analysis::AnalysisManager", "Moteur %1 indisponible : %2"),
                           { engine->info().name, avail.message });
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

    // Traçabilité commune, renseignée ici pour qu'aucun moteur ne puisse l'omettre. Un calcul délégué à un moteur de
    // secours porte l'identité de ce moteur (AnalysisRunResult::producedBy), jamais celle du moteur choisi.
    const AnalysisEngine* producer = r.producedBy ? m_registry.engine(r.producedBy->id) : engine;
    const EngineInfo info = r.producedBy ? *r.producedBy : engine->info();
    auto& meta = r.results.executionMetadata();
    meta.engineId = info.id;
    meta.solverEngine = info.name;
    if (!info.version.empty()) meta.solverVersion = info.version;
    meta.analysisScope = prepared.model.entireModel ? std::string() : prepared.model.scopeLabel;
    meta.analysisDimension = dimensionKey(context.dimension);
    meta.preparationMessages.clear();
    for (const auto severity : { ValidationSeverity::Warning, ValidationSeverity::Info })
        for (const auto& m : prepared.validation.messages())
            if (m.severity == severity) meta.preparationMessages.push_back(m.category.empty() ? m.text : m.category + " : " + m.text);

    // Catégories annoncées = capacité déclarée ET données réellement présentes (capacités du moteur producteur).
    const AnalysisCapabilities caps = (producer ? producer : engine)->capabilities();
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
