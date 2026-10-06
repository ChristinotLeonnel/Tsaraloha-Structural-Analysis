#pragma once

// Relations IFC (IfcRel*) collectées pendant l'export puis écrites en une fois, regroupées
// (une relation de contenance par étage, une association par matériau…). Les relations n'ont
// pas d'identité persistante dans TSA : leur GlobalId est créé à chaque export.

#include "IfcExportContext.h"

#include <map>
#include <string>
#include <vector>

namespace TSA::BIM::Ifc
{

class IfcRelationshipMapper
{
public:
    explicit IfcRelationshipMapper(IfcExportContext& ctx) : m_ctx(ctx) {}

    void aggregate(int parent, int child) { m_aggregates[parent].push_back(child); }
    void contain(int spatial, int product) { m_contained[spatial].push_back(product); }
    void associateMaterial(int materialSelect, int object) { m_materials[materialSelect].push_back(object); }
    void defineByProperties(int pset, int object) { m_psets.push_back({ pset, object }); }
    void classify(int reference, int object) { m_classifications[reference].push_back(object); }
    void assignToGroup(int group, int object) { m_groups[group].push_back(object); }
    /// Objets analytiques → produit physique (PhysicalToAnalyticalMap à l'échange).
    void assignToProduct(int product, const std::vector<int>& analytical) { m_products[product] = analytical; }
    void connect(int member, int connection) { m_connections.push_back({ member, connection }); }

    /// Écrit toutes les relations ; retourne leur nombre.
    int write();

private:
    IfcExportContext& m_ctx;
    std::map<int, std::vector<int>> m_aggregates, m_contained, m_materials, m_classifications, m_groups, m_products;
    std::vector<std::pair<int, int>> m_psets, m_connections;
};

} // namespace TSA::BIM::Ifc
