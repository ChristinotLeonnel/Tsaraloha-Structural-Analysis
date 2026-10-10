#pragma once

// Adaptateur maillage → OpenSees (docs/MESH.md §J), distinct du moteur de maillage (Gmsh) et du moteur de
// calcul. Avant toute conversion, il vérifie le type et l'ordre des éléments, la connectivité et l'ordre
// des nœuds, les degrés de liberté, les sections et matériaux, les transformations géométriques et
// orientations locales, les appuis, les charges et les groupes. Seuls les éléments compatibles sont
// convertis ; les autres sont listés avec la raison et une stratégie compatible. Rien n'est converti
// silencieusement.
//
// Conversion réalisée : barres fléchies (poutres, poteaux) en éléments 1D du premier ordre → sous-éléments
// elasticBeamColumn, par CalculationSnapshot::subdivided (charges réparties exactement, relâchements aux
// extrémités, orientation de la barre conservée), résultats recomposés par barre TSA.

#include "../MeshGeometry.h"
#include "../MeshTypes.h"

#include "../../Analysis/Engine/AnalysisEngine.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace TSA::Mesh
{

struct SolverCheckItem
{
    enum class Level
    {
        Ok,
        Warning,
        Blocking   ///< le maillage ne peut pas être transmis tel quel
    };
    Level level = Level::Ok;
    std::string category;              ///< « Types d'éléments », « Ordre », « DDL »…
    std::string message;
    std::vector<std::string> objects;  ///< objets concernés (références)
};

struct SolverCompatibility
{
    std::string engine;
    std::vector<SolverCheckItem> items;
    std::vector<std::string> strategies;   ///< stratégies compatibles proposées
    /// Subdivision transmissible (poutres et poteaux, éléments 1D du premier ordre) : positions relatives.
    std::map<TSA::Analysis::ElementKey, std::vector<double>> memberSubdivisions;
    std::size_t transmittedElements = 0, ignoredElements = 0;
    std::size_t estimatedDofs = 0;

    bool blocking() const;
    bool hasWarnings() const;
    std::string summary() const;
};

class OpenSeesMeshAdapter
{
public:
    /// @param dofLimit limite raisonnable de degrés de liberté (ressources) ; au-delà : bloquant
    static SolverCompatibility check(const FeMesh& mesh, const MeshGeometry& geometry, const std::string& engineName,
                                     const TSA::Analysis::AnalysisCapabilities& caps, std::size_t dofLimit = 3000000);

    /// Élément OpenSees correspondant (ou raison de l'absence de conversion) pour un type d'élément du
    /// maillage ; family : 0 poutre, 1 poteau, 2 treillis, 3 câble, -1 surface ou volume.
    static std::string openSeesElementFor(FeType type, int family, bool* convertible);
};

/// Référence de barre (B12, C3…) → clé d'élément d'analyse ; std::nullopt pour une surface ou un volume.
std::optional<TSA::Analysis::ElementKey> memberKeyFromRef(const std::string& ref);

} // namespace TSA::Mesh
