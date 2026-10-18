#pragma once

// Passe « objets physiques » de l'import IFC, par IfcOpenShell (lecture des schémas IFC2X3 / IFC4 / IFC4X3_ADD2 et
// noyau géométrique OpenCASCADE). Interne à IfcImporter : voir IfcImporter.h pour le contrat de l'import.
// Compilé avec la bibliothèque si TSA_HAVE_IFCOPENSHELL (cmake/IfcOpenShell.cmake), sinon réponses « absent ».

#include "IfcImporter.h"

#include <string>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM::Ifc::detail
{

bool fullEngineAvailable();
std::string fullEngineName();   ///< « IfcOpenShell 0.9.0post1 (OpenCASCADE) »

/// Crée ou complète (par GlobalId) les produits physiques du modèle : classe d'origine, géométrie (mètres, repère du
/// projet), propriétés et quantités, matériaux, classification, étage, état de préparation. Les produits déjà créés
/// par la passe du modèle analytique gardent leurs éléments de calcul. Retourne false si le fichier est illisible
/// ou l'import annulé (report.error / report.canceled renseignés).
bool importPhysicalProducts(const std::string& path, TSA::Model::Model& model, const IfcImportOptions& options,
                            IfcImportReport& report);

/// Relecture indépendante d'un fichier IFC (contrôle après export : autre lecteur que l'écrivain de TSA). Renseigne le
/// schéma et le nombre de produits physiques (IfcElement) ; false si le fichier est illisible.
bool verifyFile(const std::string& path, std::string& schema, int& elements, std::string& error);

} // namespace TSA::BIM::Ifc::detail
