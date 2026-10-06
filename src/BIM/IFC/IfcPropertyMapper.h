#pragma once

// Jeux de propriétés IFC d'un produit : Psets standard (Pset_BeamCommon…), Pset_TSA_Structural
// (données de calcul propres à TSA) et Psets saisis par l'utilisateur (prioritaires à nom égal).
// Les propriétés BIM ne sont jamais codées dans l'interface : elles sont calculées ici depuis le modèle.

#include "IfcExportContext.h"
#include "../Core/BimModel.h"

#include <vector>

namespace TSA::BIM::Ifc
{

class IfcPropertyMapper
{
public:
    explicit IfcPropertyMapper(IfcExportContext& ctx) : m_ctx(ctx) {}

    /// Psets effectifs du produit (standard + TSA + utilisateur fusionnés).
    static std::vector<PropertySet> propertySets(const TSA::Model::Model& model, const PhysicalElement& e);
    /// Écrit les IfcPropertySet et retourne leurs ids (relations créées par IfcRelationshipMapper).
    std::vector<int> write(const PhysicalElement& e);
    /// Valeur STEP typée (IFCLABEL('x'), IFCLENGTHMEASURE(3.)…).
    static std::string stepValue(const PropertyValue& v);

private:
    IfcExportContext& m_ctx;
};

} // namespace TSA::BIM::Ifc
