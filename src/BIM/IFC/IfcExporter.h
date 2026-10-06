#pragma once

// Export IFC 4.3 (IFC4X3_ADD2, STEP) du modèle TSA : structure spatiale, produits physiques
// (géométrie, matériaux, profils, Psets, classifications) et modèle analytique
// (IfcStructuralAnalysisModel) relié aux produits par IfcRelAssignsToProduct.
// Implémentation partielle conçue selon IFC 4.3 / ISO 16739-1:2024 ; conformité à vérifier par
// un validateur (ex. IfcOpenShell, buildingSMART Validation Service). Référence : docs/IFC_MAPPING.md

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM::Ifc
{

struct IfcExportOptions
{
    bool includeAnalyticalModel = true;
    std::string projectName;      ///< vide = nom du projet BIM
    std::string author;
    std::string organization;
    std::string fileName;         ///< FILE_NAME de l'en-tête STEP
    std::string timeStamp;        ///< ISO 8601 ; vide = maintenant (fixer pour des exports reproductibles)
};

struct IfcExportReport
{
    bool ok = false;
    std::string error;
    int entities = 0;
    int products = 0;
    int storeys = 0;
    int analyticalNodes = 0;
    int analyticalMembers = 0;
    int relationships = 0;
    std::vector<std::string> warnings;

    std::string summary() const;
};

class IfcExporter
{
public:
    /// Construit le document STEP en mémoire (le modèle n'est jamais modifié).
    static IfcExportReport exportToString(const TSA::Model::Model& model, const IfcExportOptions& options, std::string& out);
    static IfcExportReport exportToFile(const TSA::Model::Model& model, const std::string& path, IfcExportOptions options = {});
};

} // namespace TSA::BIM::Ifc
