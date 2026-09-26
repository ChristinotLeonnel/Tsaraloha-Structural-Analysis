#include "CableDefinition.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TSA::Model
{

CableDefinition::CableDefinition()
    : m_id("CAB_DEF_1")
    , m_name("Standard Cable 20mm")
    , m_type(CableType::Generic)
{
    setNominalDiameter(0.020);
}

CableDefinition::CableDefinition(const std::string& name, CableType type)
    : m_id("CAB_DEF_" + name)
    , m_name(name)
    , m_type(type)
{
    setNominalDiameter(0.020);
}

void CableDefinition::setNominalDiameter(double d)
{
    m_nominalDiameter = std::max(0.001, d);
    // Aire d'un disque circulaire plein comme référence si non spécifiée
    m_area = (M_PI * m_nominalDiameter * m_nominalDiameter) / 4.0;
    m_linearMass = m_density * m_area;
}

CableDefinition CableDefinition::fromStandardProduct(const StandardCableProduct& p)
{
    CableDefinition def;
    def.setId("DEF_" + p.designation);
    def.setName(p.designation);
    def.setType(p.productType);
    def.setStandardCode(p.standard);
    def.setStandardName(p.standardName);
    def.setStandardVersion(p.standardVersion);
    def.setGrade(p.grade);

    def.m_nominalDiameter = p.nominalDiameter;
    def.m_area = p.nominalCrossSection;
    def.m_linearMass = p.linearMass;
    def.m_elasticModulus = p.elasticModulus;
    def.m_characteristicStrength = p.characteristicStrength;
    def.m_ultimateStrength = p.characteristicStrength;
    def.m_minimumBreakingForce = p.minimumBreakingForce;
    def.m_density = (p.linearMass > 0.0 && p.nominalCrossSection > 0.0) 
                    ? (p.linearMass / p.nominalCrossSection) 
                    : 7850.0;
    def.m_defaultInitialTension = 0.40 * p.minimumBreakingForce; // Tension de service usuelle ~40% Fpk
    def.m_tensionOnly = true;

    return def;
}

CableDefinition CableDefinition::strandY1860S7_15_7()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("EN10138-Y1860S7-15.7");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Y1860S7 15.7mm", CableType::Strand);
    d.setNominalDiameter(0.0157);
    d.setMetallicArea(150.0e-6);
    d.setElasticModulus(195.0e9);
    d.setCharacteristicStrength(1860.0e6);
    d.setMinimumBreakingForce(279.0e3);
    d.setDefaultInitialTension(100.0e3);
    return d;
}

CableDefinition CableDefinition::strandY1860S7_15_2()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("EN10138-Y1860S7-15.2");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Y1860S7 15.2mm", CableType::Strand);
    d.setNominalDiameter(0.0152);
    d.setMetallicArea(139.0e-6);
    d.setElasticModulus(195.0e9);
    d.setCharacteristicStrength(1860.0e6);
    d.setMinimumBreakingForce(259.0e3);
    d.setDefaultInitialTension(90.0e3);
    return d;
}

CableDefinition CableDefinition::stayCablePSS_19x15_7()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("Stay-PSS-19-15.7");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Stay PSS 19x15.7mm", CableType::StayCable);
    d.setNominalDiameter(0.090);
    d.setMetallicArea(19 * 150.0e-6);
    d.setElasticModulus(195.0e9);
    d.setCharacteristicStrength(1860.0e6);
    d.setMinimumBreakingForce(19 * 279.0e3);
    d.setDefaultInitialTension(1500.0e3);
    return d;
}

CableDefinition CableDefinition::stayCablePSS_37x15_7()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("Stay-PSS-37-15.7");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Stay PSS 37x15.7mm", CableType::StayCable);
    d.setNominalDiameter(0.125);
    d.setMetallicArea(37 * 150.0e-6);
    d.setElasticModulus(195.0e9);
    d.setCharacteristicStrength(1860.0e6);
    d.setMinimumBreakingForce(37 * 279.0e3);
    d.setDefaultInitialTension(3000.0e3);
    return d;
}

CableDefinition CableDefinition::suspensionCableFLC_120()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("EN1993-FLC-1570-120");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Suspension FLC 120mm", CableType::SuspensionCable);
    d.setNominalDiameter(0.120);
    d.setMetallicArea(9700.0e-6);
    d.setElasticModulus(165.0e9);
    d.setCharacteristicStrength(1570.0e6);
    d.setMinimumBreakingForce(13700.0e3);
    d.setDefaultInitialTension(5000.0e3);
    return d;
}

CableDefinition CableDefinition::hangerCable_30()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("EN1993-Hanger-30");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Hanger Spiral 30mm", CableType::Hanger);
    d.setNominalDiameter(0.030);
    d.setMetallicArea(565.0e-6);
    d.setElasticModulus(150.0e9);
    d.setCharacteristicStrength(1770.0e6);
    d.setMinimumBreakingForce(850.0e3);
    d.setDefaultInitialTension(200.0e3);
    return d;
}

CableDefinition CableDefinition::prestressingBar_32()
{
    auto opt = CableStandardsRegistry::instance().findByDesignation("EN10138-Bar-Y1030-32");
    if (opt.has_value())
    {
        return fromStandardProduct(opt.value());
    }
    CableDefinition d("Bar Y1030 32mm", CableType::PrestressingBar);
    d.setNominalDiameter(0.032);
    d.setMetallicArea(804.2e-6);
    d.setElasticModulus(205.0e9);
    d.setCharacteristicStrength(1030.0e6);
    d.setMinimumBreakingForce(828.0e3);
    d.setDefaultInitialTension(450.0e3);
    return d;
}

std::vector<CableDefinition> CableDefinition::defaultDefinitions()
{
    std::vector<CableDefinition> list;
    const auto& stdList = CableStandardsRegistry::instance().allProducts();
    for (const auto& prod : stdList)
    {
        list.push_back(CableDefinition::fromStandardProduct(prod));
    }
    return list;
}

std::vector<CableDefinition> CableDefinition::defaultLibrary()
{
    return defaultDefinitions();
}

} // namespace TSA::Model
