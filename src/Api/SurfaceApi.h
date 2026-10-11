#pragma once

// API interne des éléments surfaciques (dalles, voiles), indépendante de l'interface graphique (docs/SURFACES.md §API).
// Destinée aux extensions, scripts et échanges (IFC, JSON) : description structurée et stable des surfaces, capacités du
// moteur à interroger AVANT toute demande de calcul, modifications validées (rien n'est créé si une règle est violée).
// Les identifiants sont ceux du modèle (jamais réutilisés) et le GlobalId IFC de la couche BIM.

#include "../Model/Load/SurfaceLoad.h"
#include "../Model/SurfaceFormulation.h"
#include "../Model/SurfaceSupport.h"

#include <QJsonObject>

#include <string>
#include <utility>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class AnalysisEngine;
}

namespace TSA::Api
{

struct SurfaceInfo
{
    TSA::Model::SurfaceTargetKind kind = TSA::Model::SurfaceTargetKind::Slab;
    int id = 0;
    std::string ref;            ///< SL2, W5 (référence du maillage et des résultats)
    std::string globalId;       ///< GlobalId IFC du produit physique (vide si la couche BIM ne le connaît pas)
    std::string name;
    TSA::Model::SurfaceFormulation formulation = TSA::Model::SurfaceFormulation::Shell;
    double thickness = 0.0;     ///< m
    std::string material;
    double grossArea = 0.0, netArea = 0.0;   ///< m²
    std::size_t openings = 0, edgeSupports = 0, loads = 0;
};

/// Ce qu'un moteur sait réellement calculer pour les surfaces (à interroger avant utilisation).
struct SurfaceCapabilities
{
    std::string engineId;
    bool shell = false;          ///< coques (membrane + flexion)
    bool plate = false;          ///< plaque seule
    bool membrane = false;       ///< membrane seule
    bool modal = false;          ///< analyse modale (masses des surfaces comprises)
    bool edgeSupports = false;   ///< appuis linéiques de bord
    bool zoneLoads = false;      ///< charges sur zone et à variation linéaire
    bool pointLoads = false;     ///< charges ponctuelles sur surface
    bool thermal = false;        ///< charges thermiques (non prises en charge)
    bool supports(TSA::Model::SurfaceFormulation f) const;
};

class SurfaceApi
{
public:
    static std::vector<SurfaceInfo> list(const TSA::Model::Model& model);
    static SurfaceCapabilities capabilities(const TSA::Analysis::AnalysisEngine& engine);

    /// Description complète (surfaces, contours, ouvertures, appuis, charges), format versionné « tsa.surfaces/1 ».
    static QJsonObject toJson(const TSA::Model::Model& model);

    /// Ajout d'une ouverture (repère local) ; refusée si elle sort de la surface ou en chevauche une autre.
    static bool addOpening(TSA::Model::Model& model, TSA::Model::SurfaceTargetKind kind, int id, const std::vector<std::pair<double, double>>& polygon,
                           const std::string& name, std::string* error = nullptr);
    /// Ajout d'un appui linéique ; refusé si la surface ou le bord n'existe pas, ou s'il n'a aucun effet.
    static bool addEdgeSupport(TSA::Model::Model& model, TSA::Model::SurfaceTargetKind kind, int id, const TSA::Model::SurfaceEdgeSupport& support,
                               std::string* error = nullptr);
    /// Ajout d'une charge surfacique ; refusée si la surface ou le cas n'existe pas, ou si la zone est vide. Retourne l'id (0 si refus).
    static int addLoad(TSA::Model::Model& model, const TSA::Model::SurfaceLoad& load, std::string* error = nullptr);
    static bool setFormulation(TSA::Model::Model& model, TSA::Model::SurfaceTargetKind kind, int id, TSA::Model::SurfaceFormulation f, std::string* error = nullptr);
};

} // namespace TSA::Api
