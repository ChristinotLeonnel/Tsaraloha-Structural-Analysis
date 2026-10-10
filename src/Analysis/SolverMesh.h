#pragma once

// Maillage numérique RÉELLEMENT transmis au moteur de calcul lors du dernier calcul : nœuds et
// éléments finis tels que le solveur les a reçus (tags du moteur), avec leur origine dans le modèle
// TSA. Neutre vis-à-vis des moteurs : chaque adaptateur le remplit à partir de SA propre description
// (OpenSees : OpenSeesModelMap ; Custom2D : entrée du solveur plan) et le joint aux résultats
// (ResultsModel::setSolverMesh). Rien n'est déduit ni subdivisé ici : un moteur qui ne fournit pas son
// maillage laisse cette structure vide et l'affichage le signale.
//
// Intégrer un futur moteur : construire un SolverMesh depuis ses données d'entrée (ou ce qu'il
// renvoie), un SolverMeshCell par élément fini, les nœuds internes (subdivisions, nœuds auxiliaires)
// avec tsaNodeId = 0, puis results.setSolverMesh(mesh) dans AnalysisEngine::run.

#include "AnalysisTypes.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace TSA::Analysis
{

enum class SolverCellType
{
    Line,           ///< élément linéaire à 2 nœuds (barre, treillis, câble)
    ZeroLength,     ///< élément de longueur nulle (ressort d'appui)
    Triangle,
    Quadrilateral,
    Tetrahedron,
    Hexahedron
};

const char* solverCellTypeName(SolverCellType t);

struct SolverMeshNode
{
    int tag = 0;                ///< numéro du nœud dans le moteur
    double x = 0.0, y = 0.0, z = 0.0;
    int tsaNodeId = 0;          ///< nœud TSA d'origine ; 0 : nœud propre au moteur (subdivision, auxiliaire)
    std::string role;           ///< ex. « auxiliaire (ressort N12) »
};

struct SolverMeshCell
{
    int tag = 0;                ///< numéro de l'élément dans le moteur
    SolverCellType type = SolverCellType::Line;
    std::string solverClass;    ///< classe d'élément du moteur (elasticBeamColumn, truss, Frame…)
    std::vector<int> nodes;     ///< tags de nœuds (ordre du moteur)
    std::optional<ElementKey> source; ///< élément TSA d'origine (absent : élément propre au moteur)
};

struct SolverMesh
{
    std::string engineId;       ///< moteur ayant produit le maillage
    std::string description;    ///< ex. « 1 élément fini par barre (elasticBeamColumn) »
    std::map<int, SolverMeshNode> nodes;
    std::vector<SolverMeshCell> cells;

    bool empty() const { return cells.empty(); }
    const SolverMeshNode* node(int tag) const;
    std::size_t cellCount(SolverCellType t) const;
    std::size_t internalNodeCount() const;          ///< nœuds sans nœud TSA d'origine
    /// Nombre d'éléments finis issus de l'élément TSA (subdivisions ; 0 s'il n'a pas été transmis).
    std::size_t cellsFor(const ElementKey& key) const;
    /// Résumé lisible : moteur, nœuds, éléments par type, subdivisions.
    std::string summary() const;
    /// Incohérences (nœud inconnu, élément sans nœud…). Vide = cohérent.
    std::vector<std::string> validate() const;
};

} // namespace TSA::Analysis
