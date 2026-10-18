#pragma once

// Service de création des éléments dessinés (nœud, barre, poutre, poteau, treillis, câble, dalle,
// voile, fondation), sans vue : utilisé par les modes Dessin de la vue 3D et par les dialogues de
// création, testé sans fenêtre (suite « drawing »).
//
// Règles (docs/BIM_ARCHITECTURE.md, « Création des éléments ») :
// - le tracé ne fait que mémoriser des points (DrawPoint) : aucun nœud n'est créé avant la
//   validation, un tracé abandonné ne laisse rien dans le modèle ;
// - validation géométrique complète AVANT toute modification ; refus motivé, modèle intact ;
// - nœuds manquants + élément créés dans UNE transaction (une entrée Annuler) ; le produit BIM
//   (GlobalId, classe IFC, étage) suit par la synchronisation du BimModel ;
// - un point confondu avec un nœud (GeometryTolerance::nodeCoincidence) le réutilise ; aucune autre
//   fusion. Une extrémité posée sur une barre existante sans nœud n'est PAS une connexion :
//   avertissement, la connexion reste une décision explicite (outils Diviser / Intersecter).

#include "../Model/Beam.h"
#include "../Model/Cable/Cable.h"
#include "../Model/CreationPresets.h"
#include "../Model/Slab.h"

#include <gp_Pnt.hxx>

#include <functional>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Interaction
{

/// Point désigné pendant un tracé : position (accrochée ou libre) et nœud visé par l'accrochage.
struct DrawPoint
{
    gp_Pnt point;
    int nodeId = -1;   ///< > 0 : nœud existant accroché (prioritaire sur la position)

    DrawPoint() = default;
    DrawPoint(const gp_Pnt& p, int node = -1) : point(p), nodeId(node) {}
};

struct CreationResult
{
    bool success = false;
    int elementId = -1;                ///< élément créé (nœud pour createNode)
    std::vector<int> nodeIds;          ///< nœuds de l'élément, dans l'ordre du tracé
    std::vector<int> createdNodeIds;   ///< nœuds créés par l'opération (les autres existaient)
    std::string message;               ///< motif du refus (vide en cas de succès)
    std::vector<std::string> warnings; ///< réserves sur un élément créé (extrémité non connectée…)
};

/// Réglages complémentaires appliqués dans la même transaction que la création (propriétés d'un
/// dialogue) ; reçoit le modèle et l'id de l'élément créé.
using CreationSetup = std::function<void(TSA::Model::Model&, int)>;

class ElementCreation
{
public:
    explicit ElementCreation(TSA::Model::Model& model) : m_model(model) {}

    /// Nœud qu'un point désigne SANS rien créer : nœud accroché, sinon nœud confondu ; -1 sinon.
    int existingNode(const DrawPoint& p) const;
    /// Position effective d'un point (celle du nœud désigné s'il y en a un).
    gp_Pnt position(const DrawPoint& p) const;

    CreationResult createNode(const DrawPoint& p);
    /// Barre générique, poutre ou poteau selon props.role (BarRole::Column → poteau).
    CreationResult createBar(const DrawPoint& a, const DrawPoint& b, const TSA::Model::BarProperties& props,
                             const CreationSetup& setup = {});
    CreationResult createTrussMember(const DrawPoint& a, const DrawPoint& b, const TSA::Model::TrussPreset& preset,
                                     const CreationSetup& setup = {});
    CreationResult createCable(const DrawPoint& a, const DrawPoint& b, TSA::Model::CableType type,
                               const CreationSetup& setup = {});
    /// Contour fermé implicitement (ne pas répéter le premier point).
    CreationResult createSlab(const std::vector<DrawPoint>& contour, const TSA::Model::SlabPreset& preset,
                              const std::string& name = "", TSA::Model::SlabType type = TSA::Model::SlabType::TwoWay,
                              const CreationSetup& setup = {});
    CreationResult createWall(const DrawPoint& a, const DrawPoint& b, const TSA::Model::WallPreset& preset,
                              const std::string& name = "", const CreationSetup& setup = {});
    CreationResult createFoundation(const DrawPoint& p, const TSA::Model::FoundationPreset& preset,
                                    const CreationSetup& setup = {});

    /// Validations seules (aucune modification) : message de refus, vide si la création est possible.
    std::string checkLinear(const DrawPoint& a, const DrawPoint& b) const;
    std::string checkSlab(const std::vector<DrawPoint>& contour) const;

private:
    enum class LinearFamily { Bar, Truss, Cable, Wall };

    std::string checkDuplicateLinear(LinearFamily family, const DrawPoint& a, const DrawPoint& b) const;
    /// Points nouveaux posés sur l'intérieur d'un élément linéaire existant, et croisements du
    /// segment [a, b] avec les éléments linéaires existants : aucune connexion, avertissement.
    std::vector<std::string> unconnectedWarnings(const std::vector<DrawPoint>& points, bool segment) const;
    /// Nœud du point, créé s'il n'existe pas (à appeler DANS la transaction).
    int resolveNode(const DrawPoint& p, CreationResult& r);

    TSA::Model::Model& m_model;
};

} // namespace TSA::Interaction
