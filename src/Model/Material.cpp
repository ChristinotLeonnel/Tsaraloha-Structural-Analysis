#include "Material.h"

namespace TSA::Model
{

Material Material::concreteC25_30()
{
    Material m;
    m.id = 1;
    m.name = "Concrete C25/30";
    m.type = MaterialType::Concrete;
    m.E = 31.0e9;       // 31 GPa
    m.nu = 0.20;
    m.density = 2500.0; // 25 kN/m³
    m.fk = 25.0e6;      // fck = 25 MPa
    m.thermalCoeff = 1.0e-5;
    return m;
}

Material Material::concreteC30_37()
{
    Material m;
    m.id = 2;
    m.name = "Concrete C30/37";
    m.type = MaterialType::Concrete;
    m.E = 33.0e9;
    m.nu = 0.20;
    m.density = 2500.0;
    m.fk = 30.0e6;
    m.thermalCoeff = 1.0e-5;
    return m;
}

Material Material::steelS235()
{
    Material m;
    m.id = 3;
    m.name = "Steel S235";
    m.type = MaterialType::Steel;
    m.E = 210.0e9;      // 210 GPa
    m.nu = 0.30;
    m.density = 7850.0; // 78.5 kN/m³
    m.fk = 235.0e6;     // fy = 235 MPa
    m.thermalCoeff = 1.2e-5;
    return m;
}

Material Material::steelS355()
{
    Material m;
    m.id = 4;
    m.name = "Steel S355";
    m.type = MaterialType::Steel;
    m.E = 210.0e9;
    m.nu = 0.30;
    m.density = 7850.0;
    m.fk = 355.0e6;     // fy = 355 MPa
    m.thermalCoeff = 1.2e-5;
    return m;
}

Material Material::timberC24()
{
    Material m;
    m.id = 5;
    m.name = "Timber C24";
    m.type = MaterialType::Timber;
    m.E = 11.0e9;       // 11 GPa
    m.nu = 0.30;
    m.density = 420.0;  // 420 kg/m³
    m.fk = 24.0e6;      // 24 MPa
    m.thermalCoeff = 5.0e-6;
    return m;
}

std::vector<Material> Material::defaultLibrary()
{
    return {
        concreteC25_30(),
        concreteC30_37(),
        steelS235(),
        steelS355(),
        timberC24()
    };
}

Material Material::findByName(const std::string& name)
{
    for (const auto& m : defaultLibrary())
    {
        if (m.name == name)
            return m;
    }
    // Recherche par mot-clé
    if (name.find("30/37") != std::string::npos) return concreteC30_37();
    if (name.find("355") != std::string::npos) return steelS355();
    if (name.find("235") != std::string::npos) return steelS235();
    if (name.find("Timber") != std::string::npos || name.find("Bois") != std::string::npos) return timberC24();
    return concreteC25_30();
}

} // namespace TSA::Model
