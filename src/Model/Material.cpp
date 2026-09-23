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

std::vector<Material> Material::defaultLibrary()
{
    return {
        concreteC25_30(),
        concreteC30_37(),
        steelS235(),
        steelS355()
    };
}

} // namespace TSA::Model
