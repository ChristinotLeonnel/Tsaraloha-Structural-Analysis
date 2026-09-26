#pragma once

#include "CableTypes.h"
#include "CableStandards.h"
#include <string>
#include <vector>

namespace TSA::Model
{

/**
 * @brief Définition catalogue / gabarit d'un type de câble (Section + Matériau + Norme associés).
 */
class CableDefinition
{
public:
    CableDefinition();
    explicit CableDefinition(const std::string& name, CableType type = CableType::Generic);

    const std::string& id() const { return m_id; }
    void setId(const std::string& id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    CableType type() const { return m_type; }
    void setType(CableType type) { m_type = type; }

    // Norme de référence
    StandardCode standardCode() const { return m_standardCode; }
    void setStandardCode(StandardCode code) { m_standardCode = code; }

    const std::string& standardName() const { return m_standardName; }
    void setStandardName(const std::string& std) { m_standardName = std; }

    const std::string& standardVersion() const { return m_standardVersion; }
    void setStandardVersion(const std::string& ver) { m_standardVersion = ver; }

    const std::string& grade() const { return m_grade; }
    void setGrade(const std::string& grade) { m_grade = grade; }

    // Caractéristiques géométriques (en mètres et m²)
    double nominalDiameter() const { return m_nominalDiameter; }
    void setNominalDiameter(double d);

    double metallicArea() const { return m_area; }
    void setMetallicArea(double a) { m_area = a; }
    double area() const { return m_area; }
    void setArea(double a) { setMetallicArea(a); }

    double linearMass() const { return m_linearMass; }
    void setLinearMass(double mass) { m_linearMass = mass; }

    // Caractéristiques mécaniques (en Pascals et Newtons)
    double elasticModulus() const { return m_elasticModulus; }
    void setElasticModulus(double E) { m_elasticModulus = E; }

    double density() const { return m_density; }
    void setDensity(double rho) { m_density = rho; }

    double characteristicStrength() const { return m_characteristicStrength; }
    void setCharacteristicStrength(double fpk) { m_characteristicStrength = fpk; }

    double ultimateStrength() const { return m_ultimateStrength; }
    void setUltimateStrength(double fu) { m_ultimateStrength = fu; }

    double minimumBreakingForce() const { return m_minimumBreakingForce; }
    void setMinimumBreakingForce(double f) { m_minimumBreakingForce = f; }

    // Précontrainte / Tension par défaut du gabarit
    double defaultInitialTension() const { return m_defaultInitialTension; }
    void setDefaultInitialTension(double t) { m_defaultInitialTension = t; }
    double initialTension() const { return m_defaultInitialTension; }
    void setInitialTension(double t) { setDefaultInitialTension(t); }

    bool tensionOnly() const { return m_tensionOnly; }
    void setTensionOnly(bool to) { m_tensionOnly = to; }

    // Usines à partir du référentiel normatif certifié
    static CableDefinition fromStandardProduct(const StandardCableProduct& product);
    static CableDefinition strandY1860S7_15_7();
    static CableDefinition strandY1860S7_15_2();
    static CableDefinition stayCablePSS_19x15_7();
    static CableDefinition stayCablePSS_37x15_7();
    static CableDefinition suspensionCableFLC_120();
    static CableDefinition hangerCable_30();
    static CableDefinition prestressingBar_32();

    // Bibliothèque par défaut
    static std::vector<CableDefinition> defaultDefinitions();
    static std::vector<CableDefinition> defaultLibrary();

private:
    std::string m_id;
    std::string m_name;
    CableType m_type = CableType::Generic;

    StandardCode m_standardCode = StandardCode::EN_1993_1_11;
    std::string m_standardName = "EN 1993-1-11";
    std::string m_standardVersion = "2006";
    std::string m_grade = "S460";

    double m_nominalDiameter = 0.020;       // 20 mm par défaut (0.02 m)
    double m_area = 3.14159265e-4;          // Section en m²
    double m_linearMass = 2.46;             // kg/m

    double m_elasticModulus = 195.0e9;      // 195 GPa
    double m_density = 7850.0;              // kg/m³
    double m_characteristicStrength = 1770.0e6; // Pa
    double m_ultimateStrength = 1860.0e6;   // Pa
    double m_minimumBreakingForce = 500.0e3;// N

    double m_defaultInitialTension = 100.0e3; // 100 kN par défaut
    bool m_tensionOnly = true;
};

} // namespace TSA::Model
