#pragma once

// Import IFC (IFC2X3 / IFC4 / IFC4X3, STEP) dans le modèle TSA (docs/IFC_MAPPING.md).
//  1. Objets physiques : tous les produits du fichier (porteurs ou non) avec leur géométrie, lue par le noyau
//     géométrique d'IfcOpenShell (extrusions, B-rep, maillages, booléens, géométries partagées), leur classe IFC
//     d'origine, leurs propriétés (Psets, quantités), matériaux, classification et étage. Un produit sans
//     élément de calcul est conservé « à préparer » (porteur) ou « non porteur » : rien n'est inventé.
//  2. Modèle de calcul : repris du modèle analytique du fichier (IfcStructuralAnalysisModel : nœuds, barres,
//     surfaces, appuis), relié aux produits par IfcRelAssignsToProduct (mapping 1:N conservé).
//  3. Déduction depuis la géométrie (axe ou extrusion des poutres, poteaux, dalles, voiles…) : seulement sur
//     demande explicite (deriveAnalyticalFromGeometry) ; chaque élément déduit est signalé « à vérifier ».
// Sans IfcOpenShell (compilation sans la bibliothèque), seul le lecteur STEP interne est utilisé : extrusions
// paramétriques et modèle analytique (capacités réduites, signalées dans le rapport).

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM::Ifc
{

struct IfcImportOptions
{
    double nodeTolerance = 1e-6;    ///< fusion des nœuds coïncidents (m)
    bool useAnalyticalModel = true; ///< reprendre le modèle analytique du fichier (IfcStructural*)
    /// Déduire des éléments de calcul de la géométrie des produits qui n'en ont pas (extrusions) : jamais par
    /// défaut, chaque élément déduit est signalé à vérifier (connexions, appuis, excentrements).
    bool deriveAnalyticalFromGeometry = false;
    bool importGeometry = true;     ///< géométrie physique des produits (noyau IfcOpenShell)
    /// Progression (0–100, étape en cours) ; retourne false pour annuler. Appelée depuis le fil d'exécution de
    /// l'import (ne pas toucher à l'interface directement).
    std::function<bool(int percent, const std::string& stage)> progress;
};

/// Devenir d'un produit du fichier, pour le rapport détaillé.
struct IfcImportItem
{
    enum class Outcome
    {
        Analytical,      ///< produit physique relié à des éléments de calcul (du fichier ou déduits)
        ToPrepare,       ///< produit porteur conservé sans élément de calcul (préparation requise)
        NotStructural,   ///< produit non porteur conservé (porte, revêtement…)
        Partial,         ///< conservé, une partie des données n'a pas pu être lue (voir note)
        Ignored          ///< non importé (voir note)
    };
    std::string globalId;
    std::string ifcClass;
    std::string name;
    Outcome outcome = Outcome::Ignored;
    std::string note;
};

struct IfcImportReport
{
    bool ok = false;
    bool canceled = false;
    std::string error;
    std::string schema;
    std::string engine;            ///< « IfcOpenShell 0.9.0post1 (OpenCASCADE) » ou « lecteur STEP interne »
    double lengthUnit = 1.0;       ///< facteur de l'unité de longueur du fichier vers le mètre
    int products = 0;              ///< produits physiques importés (toutes classes)
    int productsWithGeometry = 0;
    int toPrepare = 0;
    int notStructural = 0;
    int ignored = 0;
    int nodes = 0;
    int members = 0;
    int surfaces = 0;
    int foundations = 0;
    int storeys = 0;
    int derived = 0;               ///< éléments de calcul déduits de la géométrie (option explicite)
    std::map<std::string, int> byClass;   ///< produits importés par classe IFC
    std::vector<IfcImportItem> items;     ///< détail par produit
    std::vector<std::string> warnings;

    std::string summary() const;
};

class IfcImporter
{
public:
    /// Ajoute le contenu du fichier au modèle (généralement vide). L'appelant encadre l'opération
    /// (modèle temporaire ou pushUndoState / nouveau projet) et rafraîchit les vues.
    static IfcImportReport importFile(const std::string& path, TSA::Model::Model& model, const IfcImportOptions& options = {});
    /// Lecteur STEP interne seul (contenu en mémoire) : extrusions paramétriques et modèle analytique.
    static IfcImportReport importString(const std::string& content, TSA::Model::Model& model, const IfcImportOptions& options = {});
    /// Noyau IFC complet disponible (TSA compilé avec IfcOpenShell) et sa version.
    static bool fullEngineAvailable();
    static std::string engineName();
};

} // namespace TSA::BIM::Ifc
