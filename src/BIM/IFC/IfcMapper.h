#pragma once

// Correspondances TSA ↔ IFC 4.3 : entités et types prédéfinis, profils paramétrés, matériaux.
// Référence : docs/IFC_MAPPING.md

#include "IfcExportContext.h"
#include "../Core/BimTypes.h"
#include "../../Model/Material.h"
#include "../../Model/Section.h"

#include <map>
#include <string>

namespace TSA::BIM::Ifc
{

class IfcMapper
{
public:
    explicit IfcMapper(IfcExportContext& ctx) : m_ctx(ctx) {}

    /// Nom STEP de l'entité (IFCBEAM…) ; vide si la catégorie n'est pas exportable comme produit.
    static std::string stepEntity(BimCategory c);
    /// Type prédéfini accepté par l'entité ; NOTDEFINED sinon.
    static std::string predefinedType(BimCategory c, const std::string& requested);

    /// IfcProfileDef paramétré (IfcIShapeProfileDef, IfcRectangleProfileDef…), mis en cache.
    int profile(const TSA::Model::Section& s);
    /// IfcMaterial + propriétés mécaniques (Pset_MaterialMechanical / Pset_MaterialCommon), mis en cache.
    int material(const TSA::Model::Material& m);
    /// IfcMaterialProfileSet (matériau + profil) pour les éléments linéaires, mis en cache.
    int materialProfileSet(const TSA::Model::Material& m, const TSA::Model::Section& s);

    static std::string profileKey(const TSA::Model::Section& s);

private:
    IfcExportContext& m_ctx;
    std::map<std::string, int> m_profiles;
    std::map<std::string, int> m_materials;
    std::map<std::string, int> m_profileSets;
};

} // namespace TSA::BIM::Ifc
