#pragma once

// Champs de résultats portés par un maillage TSA (docs/MESH_VISUALIZATION.md §6), séparés du maillage :
// valeurs par identifiant métier de nœud ou d'élément, jamais par indice d'affichage. Chaque solveur fournit
// ses grandeurs par un adaptateur (MeshResultAdapter) ; aucune valeur n'est inventée : une entité sans résultat
// calculé n'a pas de valeur (affichée comme telle).

#include "MeshTypes.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::Mesh
{

enum class FieldLocation
{
    Node,
    Element
};

struct MeshResultField
{
    std::string name;          ///< ex. « Déplacement »
    std::string unit;          ///< ex. « m »
    std::string source;        ///< solveur et cas, ex. « OpenSees 3.8.0 — CAS1 »
    std::string method;        ///< origine des valeurs (exactes aux nœuds, interpolées entre stations…)
    FieldLocation location = FieldLocation::Node;
    int components = 1;        ///< 1 : scalaire ; 3 : vecteur (repère global)
    std::unordered_map<int, std::array<double, 3>> values; ///< identifiant métier → valeur

    bool has(int id) const { return values.count(id) > 0; }
    /// Valeur scalaire (composante 0, ou norme d'un vecteur) ; faux si aucune valeur pour id.
    bool scalar(int id, double* out) const;
    /// Bornes des valeurs scalaires (0, 0 si vide).
    std::pair<double, double> range() const;
};

struct MeshResultSet
{
    std::vector<MeshResultField> fields;
    std::vector<std::string> notes;     ///< limites de transmission (entités sans valeur, approximations)
    const MeshResultField* find(const std::string& name) const;
};

/// Adaptateur commun : un solveur (ou un format de résultats) fournit ses champs sur le maillage.
class MeshResultAdapter
{
public:
    virtual ~MeshResultAdapter() = default;
    virtual std::string name() const = 0;
    virtual MeshResultSet extract(const FeMesh& mesh) const = 0;
};

/// Résultats des moteurs de barres de TSA (ResultsModel : OpenSees, Custom2D) reportés sur le maillage :
/// - nœud de maillage confondu avec un nœud TSA calculé : déplacement exact ;
/// - nœud intérieur d'une barre maillée (éléments 1D de la barre) : déplacement aux stations du solveur,
///   interpolé linéairement entre les deux stations encadrantes (position le long de la barre) ;
/// - nœuds des surfaces et volumes : aucun calcul de ces éléments → aucune valeur.
class ResultsModelMeshAdapter : public MeshResultAdapter
{
public:
    ResultsModelMeshAdapter(const TSA::Model::Model& model, const TSA::Analysis::ResultsModel& results)
        : m_model(model), m_results(results)
    {
    }
    std::string name() const override { return "Résultats des barres (ResultsModel)"; }
    MeshResultSet extract(const FeMesh& mesh) const override;

private:
    const TSA::Model::Model& m_model;
    const TSA::Analysis::ResultsModel& m_results;
};

} // namespace TSA::Mesh
