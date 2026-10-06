#pragma once

// Import IFC (IFC2X3 / IFC4 / IFC4X3, STEP) dans le modèle TSA.
//  1. Modèle analytique présent (IfcStructuralAnalysisModel) : nœuds, barres, surfaces et appuis
//     sont repris tels quels, reliés aux produits par IfcRelAssignsToProduct (mapping 1:N conservé).
//  2. Produits sans modèle analytique : élément analytique déduit de la géométrie (axe ou
//     extrusion) — poutres, poteaux, barres, dalles, voiles, semelles, pieux.
// GlobalId, noms, Psets, classifications, matériaux, profils et étages sont conservés. Les
// propriétés recalculables (Pset_TSA_Structural, LoadBearing…) ne sont pas dupliquées.
// Implémentation partielle : voir docs/IFC_MAPPING.md (entités non prises en charge signalées).

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
    double nodeTolerance = 1e-6;   ///< fusion des nœuds coïncidents (m)
    bool useAnalyticalModel = true; ///< false : toujours reconstruire depuis la géométrie physique
};

struct IfcImportReport
{
    bool ok = false;
    std::string error;
    std::string schema;
    int products = 0;
    int nodes = 0;
    int members = 0;
    int surfaces = 0;
    int foundations = 0;
    int storeys = 0;
    std::vector<std::string> warnings;

    std::string summary() const;
};

class IfcImporter
{
public:
    /// Ajoute le contenu du fichier au modèle (généralement vide). L'appelant encadre l'opération
    /// (pushUndoState / nouveau projet) et rafraîchit les vues.
    static IfcImportReport importString(const std::string& content, TSA::Model::Model& model, const IfcImportOptions& options = {});
    static IfcImportReport importFile(const std::string& path, TSA::Model::Model& model, const IfcImportOptions& options = {});
};

} // namespace TSA::BIM::Ifc
