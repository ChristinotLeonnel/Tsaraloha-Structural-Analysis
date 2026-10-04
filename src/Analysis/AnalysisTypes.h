#pragma once

// Types de base des résultats de calcul, indépendants d'OpenSees et de Qt :
// identification des éléments, matrices, mapping des degrés de liberté, unités.
// Référence : docs/OPENSEES_RESULTS.md

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis
{

/// Famille d'élément TSA. Les identifiants TSA ne sont uniques qu'à l'intérieur d'une famille
/// (Model : compteurs séparés pour poutres, poteaux, treillis, câbles) : un élément calculé est
/// donc toujours désigné par le couple (famille, id), jamais par l'id seul.
enum class StructuralElementKind
{
    Beam,
    Column,
    Truss,
    Cable
};

inline const char* elementKindName(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return "Beam";
    case StructuralElementKind::Column: return "Column";
    case StructuralElementKind::Truss: return "Truss";
    case StructuralElementKind::Cable: return "Cable";
    }
    return "Beam";
}

/// Préfixe d'affichage : B12, C3, T7, K2.
inline const char* elementKindPrefix(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return "B";
    case StructuralElementKind::Column: return "C";
    case StructuralElementKind::Truss: return "T";
    case StructuralElementKind::Cable: return "K";
    }
    return "B";
}

struct ElementKey
{
    StructuralElementKind kind = StructuralElementKind::Beam;
    int id = 0;

    bool operator<(const ElementKey& o) const
    {
        return kind != o.kind ? static_cast<int>(kind) < static_cast<int>(o.kind) : id < o.id;
    }
    bool operator==(const ElementKey& o) const { return kind == o.kind && id == o.id; }
    bool operator!=(const ElementKey& o) const { return !(*this == o); }

    std::string label() const { return std::string(elementKindPrefix(kind)) + std::to_string(id); }
};

/// Niveau d'extraction des résultats.
/// Light    : déplacements, réactions, efforts d'éléments (calcul courant).
/// Advanced : + forces globales/basiques, mapping DDL, rigidités élémentaires, K globale.
enum class ExtractionLevel
{
    Light,
    Advanced
};

/// Unités réellement utilisées par le script OpenSees généré (AnalysisParameters::useKiloNewtons).
/// Aucune conversion n'est appliquée aux résultats : ils sont stockés dans ces unités.
struct UnitSystem
{
    std::string force = "kN";
    std::string length = "m";
    std::string moment = "kN·m";
    std::string translationalStiffness = "kN/m";
    std::string rotationalStiffness = "kN·m/rad";
    std::string stress = "kPa";

    static UnitSystem fromKiloNewtons(bool kN)
    {
        UnitSystem u;
        if (!kN)
        {
            u.force = "N";
            u.moment = "N·m";
            u.translationalStiffness = "N/m";
            u.rotationalStiffness = "N·m/rad";
            u.stress = "Pa";
        }
        return u;
    }
};

/// Matrice dense, stockage ligne par ligne.
struct DenseMatrix
{
    int rows = 0;
    int cols = 0;
    std::vector<double> data;

    DenseMatrix() = default;
    DenseMatrix(int r, int c) : rows(r), cols(c), data(static_cast<std::size_t>(r) * c, 0.0) {}

    bool empty() const { return rows == 0 || cols == 0; }
    double& operator()(int i, int j) { return data[static_cast<std::size_t>(i) * cols + j]; }
    double operator()(int i, int j) const { return data[static_cast<std::size_t>(i) * cols + j]; }
};

/// Matrice creuse au format COO trié (ligne puis colonne), sans doublon.
struct SparseMatrix
{
    int rows = 0;
    int cols = 0;
    std::vector<int> rowIndex;
    std::vector<int> colIndex;
    std::vector<double> values;

    std::size_t nonZeros() const { return values.size(); }
    /// Valeur (i, j) ; 0 si absente. Recherche dichotomique : O(log nnz).
    double at(int i, int j) const;
    /// y = A·x (x de taille cols).
    std::vector<double> multiply(const std::vector<double>& x) const;
    std::size_t memoryBytes() const
    {
        return values.size() * (sizeof(double) + 2 * sizeof(int));
    }
    /// Construit depuis une matrice dense en ne gardant que les coefficients non nuls.
    static SparseMatrix fromDense(const DenseMatrix& m);
};

/// Traçabilité obligatoire de toute matrice exposée par TSA.
struct MatrixMetadata
{
    std::string name;              ///< ex. "K_global", "k_basic B12"
    std::string source;            ///< "OpenSees API: …" ou "Reconstructed: …"
    std::string matrixType;        ///< "Initial tangent", "Basic", …
    std::string coordinateSystem;  ///< "Global", "Local", "Basic", "Reduced global (equations)"
    std::string dofOrdering;       ///< description de l'ordre des lignes/colonnes
    std::string constraints;       ///< traitement des conditions aux limites
    std::string solver;            ///< système d'équations OpenSees concerné
    std::string storage;           ///< "dense", "sparse COO"
    std::string units;
    bool symmetric = true;
    bool exact = false;            ///< true : valeurs d'OpenSees ou transformation algébrique exacte de celles-ci
    int significantDigits = 0;     ///< précision des valeurs transmises par OpenSees
    std::string notes;
};

/// Correspondance équation OpenSees → (nœud TSA, DDL).
struct DofEquation
{
    int equation = -1;   ///< numéro d'équation OpenSees (0-based, après numberer)
    int nodeId = 0;      ///< nœud TSA (== tag OpenSees du nœud)
    int dof = 0;         ///< 0..ndf-1 (0=UX … 5=RZ en 3D/6 DDL)
};

/// Mapping complet des degrés de liberté, lu dans OpenSees (commande nodeDOFs).
struct DofMap
{
    int ndm = 3;
    int ndf = 6;
    std::vector<std::string> dofLabels { "UX", "UY", "UZ", "RX", "RY", "RZ" };
    /// nœud TSA → numéros d'équation par DDL (-1 = DDL contraint, éliminé du système)
    std::map<int, std::vector<int>> nodeEquations;
    /// équations triées par numéro (index = numéro d'équation)
    std::vector<DofEquation> equations;
    std::string numberer;
    std::string constraintHandler;
    std::string source;

    int equationCount() const { return static_cast<int>(equations.size()); }
    bool empty() const { return equations.empty(); }
    /// Numéro d'équation du DDL (nœud, dof) ; -1 si contraint ou inconnu.
    int equationOf(int nodeId, int dof) const;
    /// Libellé "N12.UZ".
    std::string equationLabel(int equation) const;
};

} // namespace TSA::Analysis
