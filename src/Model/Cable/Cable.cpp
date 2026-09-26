#include "Cable.h"
#include "../Model.h"
#include "../Node.h"
#include <cmath>

namespace TSA::Model
{

Cable::Cable()
    : m_id(0)
    , m_name("Cable_1")
    , m_startNodeId(0)
    , m_endNodeId(0)
    , m_type(CableType::Generic)
    , m_section(Section::circular(0.020, "Cable 20mm"))
    , m_material(Material::steelS235())
{
    m_material.name = "Cable Steel EN 1993-1-11";
    m_material.E = 195.0e9;
    m_material.syncMechanical();
    m_definition.setNominalDiameter(0.020);
    m_prestress.initialTension = m_definition.defaultInitialTension();
}

Cable::Cable(int id, int startNodeId, int endNodeId, const std::string& name, CableType type)
    : m_id(id)
    , m_name(name.empty() ? ("Cable_" + std::to_string(id)) : name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_type(type)
    , m_section(Section::circular(0.020, "Cable 20mm"))
    , m_material(Material::steelS235())
{
    m_material.name = "Cable Steel EN 1993-1-11";
    m_material.E = 195.0e9;
    m_material.syncMechanical();
    m_definition.setType(type);
    m_definition.setNominalDiameter(0.020);
    m_prestress.initialTension = m_definition.defaultInitialTension();
}

Cable::Cable(int id, int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name)
    : m_id(id)
    , m_name(name.empty() ? ("Cable_" + std::to_string(id)) : name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_type(definition.type())
    , m_definition(definition)
    , m_section(Section::circular(definition.nominalDiameter(), definition.name()))
    , m_material(Material::steelS235())
{
    m_section.diameter = definition.nominalDiameter();
    m_section.width = definition.nominalDiameter();
    m_section.height = definition.nominalDiameter();

    m_material.name = definition.grade() + " (" + definition.standardName() + ")";
    m_material.E = definition.elasticModulus();
    m_material.density = definition.density();
    m_material.fk = definition.characteristicStrength();
    m_material.syncMechanical();

    m_prestress.initialTension = definition.defaultInitialTension();
    m_analysis.tensionOnly = definition.tensionOnly();
}

std::string Cable::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    return "Cable_" + std::to_string(m_id);
}

void Cable::setDefinition(const CableDefinition& def)
{
    m_definition = def;
    m_type = def.type();

    m_section.shape = SectionShape::Circular;
    m_section.diameter = def.nominalDiameter();
    m_section.width = def.nominalDiameter();
    m_section.height = def.nominalDiameter();
    m_section.name = def.name();

    m_material.name = def.grade() + " (" + def.standardName() + ")";
    m_material.E = def.elasticModulus();
    m_material.density = def.density();
    m_material.fk = def.characteristicStrength();
    m_material.syncMechanical();

    if (m_prestress.initialTension <= 0.0)
    {
        m_prestress.initialTension = def.defaultInitialTension();
    }
    m_analysis.tensionOnly = def.tensionOnly();
}

void Cable::setDiameter(double d)
{
    d = std::max(0.001, d);
    m_section.diameter = d;
    m_section.width = d;
    m_section.height = d;
    m_definition.setNominalDiameter(d);
}

double Cable::metallicArea() const
{
    if (m_definition.metallicArea() > 1e-10)
    {
        return m_definition.metallicArea();
    }
    return m_section.area();
}

double Cable::elasticModulus() const
{
    if (m_definition.elasticModulus() > 1e6)
    {
        return m_definition.elasticModulus();
    }
    return m_material.E;
}

double Cable::length() const
{
    return m_geometry.curveLength();
}

double Cable::chordLength(const Model& model) const
{
    const auto* nA = model.getNode(m_startNodeId);
    const auto* nB = model.getNode(m_endNodeId);
    if (!nA || !nB)
        return 0.0;

    double dx = nB->x() - nA->x();
    double dy = nB->y() - nA->y();
    double dz = nB->z() - nA->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Cable::length(const Model& model) const
{
    const auto* nA = model.getNode(m_startNodeId);
    const auto* nB = model.getNode(m_endNodeId);
    if (!nA || !nB)
        return 0.0;

    if (m_geometry.mode() == CableGeometryMode::Straight)
    {
        return chordLength(model);
    }

    CableGeometry localGeom = m_geometry;
    localGeom.setStartPoint(gp_Pnt(nA->x(), nA->y(), nA->z()));
    localGeom.setEndPoint(gp_Pnt(nB->x(), nB->y(), nB->z()));
    return localGeom.curveLength();
}

double Cable::equivalentErnstModulus(const Model& model) const
{
    double E = elasticModulus();
    double A = metallicArea();
    double T = initialTension();

    const auto* nA = model.getNode(m_startNodeId);
    const auto* nB = model.getNode(m_endNodeId);
    if (!nA || !nB)
        return E;

    double dx = nB->x() - nA->x();
    double dy = nB->y() - nA->y();
    double Lh = std::sqrt(dx * dx + dy * dy);

    // Poids linéique en N/m : w = linearMass * g
    double linearMass = m_definition.linearMass();
    if (linearMass <= 1e-4)
    {
        linearMass = m_material.density * A;
    }
    double w = linearMass * 9.81;

    return CableAnalysisProperties::calculateErnstEquivalentModulus(E, A, w, Lh, T);
}

std::vector<gp_Pnt> Cable::sampleWorldPoints(const Model& model, int numSamples) const
{
    const auto* nA = model.getNode(m_startNodeId);
    const auto* nB = model.getNode(m_endNodeId);
    if (!nA || !nB)
    {
        return {};
    }

    CableGeometry localGeom = m_geometry;
    localGeom.setStartPoint(gp_Pnt(nA->x(), nA->y(), nA->z()));
    localGeom.setEndPoint(gp_Pnt(nB->x(), nB->y(), nB->z()));

    double linearMass = m_definition.linearMass();
    if (linearMass <= 1e-4) linearMass = m_material.density * metallicArea();
    localGeom.setLinearWeightW(linearMass * 9.81);
    localGeom.setHorizontalTensionH(std::max(100.0, m_prestress.initialTension));

    return localGeom.samplePoints(numSamples);
}

double Cable::weight(const Model& model) const
{
    double len = length(model);
    double linMass = m_definition.linearMass();
    if (linMass <= 1e-4)
    {
        double area = metallicArea();
        double rho = m_material.density > 0.0 ? m_material.density : 7850.0;
        linMass = area * rho;
    }
    return linMass * 9.80665 * len;
}

} // namespace TSA::Model
