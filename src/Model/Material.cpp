#include "Material.h"
#include <algorithm>

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
    m.syncMechanical();

    m.visual.baseColor = "#9EA0A2"; // Gris béton mat
    m.visual.roughness = 0.88;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.08;
    m.visual.textureName = "concrete";
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
    m.syncMechanical();

    m.visual.baseColor = "#8F9295"; // Gris béton moyen
    m.visual.roughness = 0.85;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.10;
    m.visual.textureName = "concrete";
    return m;
}

Material Material::reinforcedConcrete()
{
    Material m;
    m.id = 6;
    m.name = "Reinforced Concrete";
    m.type = MaterialType::ReinforcedConcrete;
    m.E = 32.0e9;
    m.nu = 0.20;
    m.density = 2500.0;
    m.fk = 30.0e6;
    m.thermalCoeff = 1.0e-5;
    m.syncMechanical();

    m.visual.baseColor = "#7E8489"; // Gris béton armé légèrement texturé
    m.visual.roughness = 0.82;
    m.visual.metallic = 0.05;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.12;
    m.visual.textureName = "reinforced_concrete";
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
    m.syncMechanical();

    m.visual.baseColor = "#4682B4"; // Acier bleuté classique / SteelBlue
    m.visual.roughness = 0.35;
    m.visual.metallic = 0.90;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.65;
    m.visual.textureName = "steel";
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
    m.syncMechanical();

    m.visual.baseColor = "#3A5B72"; // Acier structural sombre haute résistance
    m.visual.roughness = 0.30;
    m.visual.metallic = 0.95;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.70;
    m.visual.textureName = "steel";
    return m;
}

Material Material::rebarSteel()
{
    Material m;
    m.id = 7;
    m.name = "Rebar Steel B500B";
    m.type = MaterialType::RebarSteel;
    m.E = 200.0e9;
    m.nu = 0.30;
    m.density = 7850.0;
    m.fk = 500.0e6;     // fy = 500 MPa
    m.thermalCoeff = 1.2e-5;
    m.syncMechanical();

    m.visual.baseColor = "#2B3036"; // Acier sombre nervuré pour armature
    m.visual.roughness = 0.45;
    m.visual.metallic = 0.88;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.50;
    m.visual.textureName = "rebar";
    return m;
}

Material Material::galvanizedSteel()
{
    Material m;
    m.id = 8;
    m.name = "Galvanized Steel";
    m.type = MaterialType::GalvanizedSteel;
    m.E = 210.0e9;
    m.nu = 0.30;
    m.density = 7850.0;
    m.fk = 275.0e6;
    m.thermalCoeff = 1.2e-5;
    m.syncMechanical();

    m.visual.baseColor = "#B4BAC0"; // Acier galvanisé clair
    m.visual.roughness = 0.40;
    m.visual.metallic = 0.82;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.55;
    m.visual.textureName = "galvanized";
    return m;
}

Material Material::aluminum()
{
    Material m;
    m.id = 9;
    m.name = "Aluminum";
    m.type = MaterialType::Aluminum;
    m.E = 70.0e9;
    m.nu = 0.33;
    m.density = 2700.0;
    m.fk = 200.0e6;
    m.thermalCoeff = 2.3e-5;
    m.syncMechanical();

    m.visual.baseColor = "#D8DCE0"; // Aspect aluminium métallique clair
    m.visual.roughness = 0.25;
    m.visual.metallic = 0.92;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.75;
    m.visual.textureName = "aluminum";
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
    m.syncMechanical();

    m.visual.baseColor = "#BA8C53"; // Couleur bois naturel chaleureux
    m.visual.roughness = 0.75;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.15;
    m.visual.textureName = "wood";
    return m;
}

Material Material::brick()
{
    Material m;
    m.id = 10;
    m.name = "Brick Masonry";
    m.type = MaterialType::Brick;
    m.E = 5.0e9;
    m.nu = 0.15;
    m.density = 1800.0;
    m.fk = 10.0e6;
    m.thermalCoeff = 6.0e-6;
    m.syncMechanical();

    m.visual.baseColor = "#A64B35"; // Rouge brique terre cuite mat
    m.visual.roughness = 0.90;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.05;
    m.visual.textureName = "brick";
    return m;
}

Material Material::masonry()
{
    Material m;
    m.id = 11;
    m.name = "Concrete Block Masonry";
    m.type = MaterialType::Masonry;
    m.E = 4.0e9;
    m.nu = 0.20;
    m.density = 2000.0;
    m.fk = 8.0e6;
    m.thermalCoeff = 8.0e-6;
    m.syncMechanical();

    m.visual.baseColor = "#8E887E"; // Gris bloc de béton / parpaing
    m.visual.roughness = 0.88;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.05;
    m.visual.textureName = "masonry";
    return m;
}

Material Material::glass()
{
    Material m;
    m.id = 12;
    m.name = "Structural Glass";
    m.type = MaterialType::Glass;
    m.E = 70.0e9;
    m.nu = 0.22;
    m.density = 2500.0;
    m.fk = 45.0e6;
    m.thermalCoeff = 9.0e-6;
    m.syncMechanical();

    m.visual.baseColor = "#C2E2E8"; // Verre translucide bleuté
    m.visual.roughness = 0.05;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.65;
    m.visual.shininess = 0.95;
    m.visual.textureName = "glass";
    return m;
}

Material Material::soil()
{
    Material m;
    m.id = 13;
    m.name = "Soil / Earth";
    m.type = MaterialType::Soil;
    m.E = 50.0e6;       // 50 MPa
    m.nu = 0.35;
    m.density = 1900.0;
    m.fk = 0.25e6;      // Capacité portante ~250 kPa
    m.thermalCoeff = 1.0e-5;
    m.syncMechanical();

    m.visual.baseColor = "#735137"; // Terre brune mate
    m.visual.roughness = 0.95;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.02;
    m.visual.textureName = "soil";
    return m;
}

Material Material::sand()
{
    Material m;
    m.id = 14;
    m.name = "Sand";
    m.type = MaterialType::Sand;
    m.E = 30.0e6;
    m.nu = 0.30;
    m.density = 1600.0;
    m.fk = 0.15e6;
    m.thermalCoeff = 1.0e-5;
    m.syncMechanical();

    m.visual.baseColor = "#D2B57B"; // Beige sable mat
    m.visual.roughness = 0.92;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.04;
    m.visual.textureName = "sand";
    return m;
}

Material Material::gravel()
{
    Material m;
    m.id = 15;
    m.name = "Gravel";
    m.type = MaterialType::Gravel;
    m.E = 80.0e6;
    m.nu = 0.25;
    m.density = 2000.0;
    m.fk = 0.40e6;
    m.thermalCoeff = 1.0e-5;
    m.syncMechanical();

    m.visual.baseColor = "#6E6962"; // Gris/brun gravier concassé
    m.visual.roughness = 0.90;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.05;
    m.visual.textureName = "gravel";
    return m;
}

Material Material::rock()
{
    Material m;
    m.id = 16;
    m.name = "Rock";
    m.type = MaterialType::Rock;
    m.E = 20.0e9;       // 20 GPa
    m.nu = 0.20;
    m.density = 2600.0;
    m.fk = 50.0e6;
    m.thermalCoeff = 8.0e-6;
    m.syncMechanical();

    m.visual.baseColor = "#44464A"; // Roche dure gris sombre
    m.visual.roughness = 0.85;
    m.visual.metallic = 0.0;
    m.visual.transparency = 0.0;
    m.visual.shininess = 0.10;
    m.visual.textureName = "rock";
    return m;
}

std::vector<Material> Material::defaultLibrary()
{
    return {
        concreteC25_30(),
        concreteC30_37(),
        reinforcedConcrete(),
        steelS235(),
        steelS355(),
        rebarSteel(),
        galvanizedSteel(),
        aluminum(),
        timberC24(),
        brick(),
        masonry(),
        glass(),
        soil(),
        sand(),
        gravel(),
        rock()
    };
}

Material Material::findByName(const std::string& name)
{
    for (const auto& m : defaultLibrary())
    {
        if (m.name == name)
            return m;
    }
    // Recherche intelligente par mot-clé
    if (name.find("30/37") != std::string::npos) return concreteC30_37();
    if (name.find("Armé") != std::string::npos || name.find("Reinforced") != std::string::npos) return reinforcedConcrete();
    if (name.find("355") != std::string::npos) return steelS355();
    if (name.find("235") != std::string::npos) return steelS235();
    if (name.find("Rebar") != std::string::npos || name.find("Armature") != std::string::npos || name.find("Ferraillage") != std::string::npos) return rebarSteel();
    if (name.find("Galva") != std::string::npos) return galvanizedSteel();
    if (name.find("Alum") != std::string::npos) return aluminum();
    if (name.find("Timber") != std::string::npos || name.find("Bois") != std::string::npos) return timberC24();
    if (name.find("Brick") != std::string::npos || name.find("Brique") != std::string::npos) return brick();
    if (name.find("Masonry") != std::string::npos || name.find("Parpaing") != std::string::npos || name.find("Bloc") != std::string::npos) return masonry();
    if (name.find("Glass") != std::string::npos || name.find("Verre") != std::string::npos) return glass();
    if (name.find("Sand") != std::string::npos || name.find("Sable") != std::string::npos) return sand();
    if (name.find("Gravel") != std::string::npos || name.find("Gravier") != std::string::npos) return gravel();
    if (name.find("Rock") != std::string::npos || name.find("Roche") != std::string::npos) return rock();
    if (name.find("Soil") != std::string::npos || name.find("Sol") != std::string::npos || name.find("Terre") != std::string::npos) return soil();
    return concreteC25_30();
}

Material Material::findById(int id)
{
    for (const auto& m : defaultLibrary())
    {
        if (m.id == id)
            return m;
    }
    return concreteC25_30();
}

Material Material::findByType(MaterialType type)
{
    for (const auto& m : defaultLibrary())
    {
        if (m.type == type)
            return m;
    }
    return concreteC25_30();
}

} // namespace TSA::Model

