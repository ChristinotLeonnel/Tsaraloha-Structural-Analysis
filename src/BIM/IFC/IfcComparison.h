#pragma once

// Comparaison de deux modèles TSA issus d'un même contenu IFC : aller-retour import → export → import (tsa-ifc
// roundtrip, essais sur fichiers réels). Mesure ce qui est conservé : identités (GlobalId), classes, noms, jeux de
// propriétés, état vis-à-vis du calcul, géométrie (emprise de chaque produit, à une tolérance donnée) et modèle de
// calcul. Tolérance géométrique et classement conservé / converti / approché / perdu : docs/IFC_MAPPING.md.

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM::Ifc
{

struct IfcComparison
{
    double tolerance = 1e-3;       ///< écart d'emprise admis (m)
    int productsA = 0;
    int productsB = 0;
    int guidsPreserved = 0;        ///< produits de A retrouvés dans B par GlobalId
    int classesPreserved = 0;      ///< … de même classe IFC d'origine
    int namesPreserved = 0;
    int propertySetsPreserved = 0; ///< … dont chaque jeu de propriétés de A est retrouvé (noms, valeurs)
    int statusPreserved = 0;       ///< … de même état (relié au calcul, à préparer, non porteur)
    int geometryCompared = 0;      ///< produits avec géométrie dans A et dans B
    int geometryWithinTolerance = 0;
    double maxBoundsDeviation = 0.0;   ///< plus grand écart d'emprise constaté (m)
    int levelsA = 0, levelsB = 0;   ///< niveaux du projet (exportés en IfcBuildingStorey)
    int storeysA = 0;               ///< niveaux de A liés à un étage IFC (GlobalId)
    int storeysPreserved = 0;       ///< … retrouvés dans B par GlobalId, même nom et même cote
    int nodesA = 0, nodesB = 0, membersA = 0, membersB = 0, surfacesA = 0, surfacesB = 0, foundationsA = 0, foundationsB = 0;
    std::vector<std::string> differences;   ///< premières différences relevées (50 au plus)

    /// Tout est conservé : mêmes produits et identités, classes, noms, propriétés, états, géométrie dans la tolérance,
    /// même modèle de calcul.
    bool identical() const;
    std::string summary() const;
};

IfcComparison compareModels(const TSA::Model::Model& a, const TSA::Model::Model& b, double tolerance = 1e-3);

} // namespace TSA::BIM::Ifc
