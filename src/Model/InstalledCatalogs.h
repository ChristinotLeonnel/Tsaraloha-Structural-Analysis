#pragma once

// Catalogues installés (bibliothèques .tsalib) vus par le noyau. La couche des bibliothèques
// (ExtensionSystem::LibraryRegistry) les fournit au démarrage du programme ; le noyau ne dépend pas d'elle
// (TSA Core, étape 1). Sans fournisseur (programme lié au seul TSA_Core), les catalogues intégrés sont utilisés.

#include "Section.h"
#include "Cable/CableDefinition.h"

#include <functional>
#include <vector>

namespace TSA::Model
{

struct InstalledCatalogs
{
    /// Sections des bibliothèques installées, dans l'ordre du registre ; vide : aucune bibliothèque installée.
    std::function<std::vector<Section>()> sections;
    /// Définitions de câbles des bibliothèques installées ; vide : aucune bibliothèque installée.
    std::function<std::vector<CableDefinition>()> cables;
};

/// Fournisseur des catalogues installés (remplace le précédent). Appelé par la couche des bibliothèques.
void setInstalledCatalogs(InstalledCatalogs catalogs);
/// Fournisseur courant ; ses fonctions sont vides si aucun n'a été installé.
const InstalledCatalogs& installedCatalogs();

} // namespace TSA::Model
