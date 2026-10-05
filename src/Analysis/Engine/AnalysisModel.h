#pragma once

// Modèle d'analyse : représentation DÉRIVÉE du modèle TSA, restreinte à la portée demandée et
// neutre vis-à-vis des moteurs. Le modèle TSA reste l'unique source de vérité : un AnalysisModel
// est reconstruit à chaque préparation de calcul et n'est jamais modifié par un moteur.
//
//   Model TSA ──(AnalysisScopeResolver)──▶ ResolvedScope ──(AnalysisModelExtractor)──▶ AnalysisModel
//                                                                         └▶ adaptateur du moteur
//
// Le contenu structurel réutilise CalculationSnapshot (nœuds, barres, sections, matériaux, appuis,
// charges, cas, combinaisons) ; AnalysisModel y ajoute la portée, le plan 2D éventuel, la
// correspondance TSA ↔ analyse et l'inventaire de ce qui n'est pas transmissible.

#include "../CalculationSnapshot.h"
#include "AnalysisContext.h"
#include "../../Model/SelectionQuery.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Analysis
{

/// Plan d'une portée plane : repère orthonormé direct (u, v, n), n normal au plan.
/// Pour un axe de grille, u suit la ligne d'axe (horizontale) et v = +Z global.
struct AnalysisPlane
{
    gp_Pnt origin;
    gp_Dir u { 1, 0, 0 };
    gp_Dir v { 0, 0, 1 };
    gp_Dir n { 0, -1, 0 };

    /// Coordonnées dans le plan (u, v) d'un point global.
    std::pair<double, double> toPlane(double x, double y, double z) const;
    /// Distance signée au plan (le long de n).
    double distance(double x, double y, double z) const;
};

/// Correspondance TSA ↔ modèle d'analyse. Les indices d'analyse sont contigus à partir de 1 ;
/// l'indice d'élément est le tag unique du CalculationSnapshot.
class AnalysisMapping
{
public:
    void addNode(int tsaNodeId);
    void addElement(const ElementKey& key, int analysisIndex);

    /// Indice d'analyse du nœud TSA ; 0 si le nœud n'est pas dans le modèle d'analyse.
    int analysisNode(int tsaNodeId) const;
    /// Nœud TSA de l'indice d'analyse ; 0 si inconnu.
    int tsaNode(int analysisIndex) const;
    /// Indice d'analyse de l'élément TSA ; 0 s'il n'est pas dans le modèle d'analyse.
    int analysisElement(const ElementKey& key) const;
    /// Élément TSA de l'indice d'analyse.
    std::optional<ElementKey> tsaElement(int analysisIndex) const;

    std::size_t nodeCount() const { return m_nodesByIndex.size(); }
    std::size_t elementCount() const { return m_elementsByIndex.size(); }
    const std::vector<int>& nodesByIndex() const { return m_nodesByIndex; }        ///< [i-1] = nœud TSA
    const std::map<int, ElementKey>& elementsByIndex() const { return m_elementsByIndex; }

private:
    std::map<int, int> m_nodeIndex;
    std::vector<int> m_nodesByIndex;
    std::map<ElementKey, int> m_elementIndex;
    std::map<int, ElementKey> m_elementsByIndex;
};

/// Portée résolue : identifiants TSA concernés et plan éventuel. Produite par AnalysisScopeResolver.
struct ResolvedScope
{
    bool ok = false;
    std::string error;
    std::string label;                         ///< « Axe B (Grille 1) », « Niveau 2 (+6.00 m) »…
    bool entireModel = false;
    TSA::Model::ElementSet elements;           ///< objets TSA de la portée
    std::optional<AnalysisPlane> plane;        ///< défini pour une portée plane
};

class AnalysisModel
{
public:
    AnalysisDimension dimension = AnalysisDimension::Space3D;
    std::string scopeLabel;
    bool entireModel = false;
    std::uint64_t sourceRevision = 0;          ///< Model::revision() au moment de l'extraction

    CalculationSnapshot snapshot;              ///< contenu structurel de la portée (barres uniquement)
    AnalysisMapping mapping;
    std::optional<AnalysisPlane> plane;
    std::map<int, std::pair<double, double>> planarCoordinates;   ///< nœud TSA → (u, v), si 2D

    // Inventaire de la portée non transmis en tant qu'éléments filaires (contrôlé par la validation
    // générique selon les capacités du moteur).
    std::vector<int> slabs, walls, foundations;
    /// Barres reliées à la portée par une seule extrémité (exclues : ni leur rigidité ni leurs
    /// charges ne sont transmises).
    std::vector<ElementKey> crossingElements;
    /// Nœuds dont la distance au plan dépasse la tolérance (2D seulement).
    std::vector<int> outOfPlaneNodes;
    std::size_t droppedNodalLoads = 0;         ///< charges nodales hors portée
    std::size_t droppedMemberLoads = 0;        ///< charges sur barres hors portée

    bool isPlanar() const { return dimension == AnalysisDimension::Plane2D && plane.has_value(); }
    bool hasPlanarElements() const { return !slabs.empty() || !walls.empty(); }
};

class AnalysisScopeResolver
{
public:
    /// Résout une portée en objets TSA. Grilles : GridDefinition (positions, origine, rotation,
    /// libellés) ; appartenance au plan : SelectionQuery + GeometryTolerance::planeMembership.
    static ResolvedScope resolve(const TSA::Model::Model& model,
                                 const TSA::Grid::GridManager* grids,
                                 const AnalysisScope& scope);

    struct ScopeOption
    {
        AnalysisScope scope;
        std::string label;
    };
    /// Portées proposées par le modèle : tout le modèle, chaque axe de chaque grille cartésienne,
    /// chaque niveau, chaque plan de travail. (La sélection est ajoutée par l'UI, qui la connaît.)
    static std::vector<ScopeOption> availableScopes(const TSA::Model::Model& model,
                                                    const TSA::Grid::GridManager* grids);
};

class AnalysisModelExtractor
{
public:
    static AnalysisModel extract(const TSA::Model::Model& model,
                                 const ResolvedScope& scope,
                                 AnalysisDimension dimension);
};

} // namespace TSA::Analysis
