#pragma once

#include <string>
#include <vector>

namespace TSA::Model
{

enum class MaterialType
{
    Concrete,
    Steel,
    Timber,
    Masonry,
    Custom
};

struct Material
{
    int id = 0;
    std::string name = "Concrete C25/30";
    MaterialType type = MaterialType::Concrete;
    double E = 31.0e9;       // Module d'Young en Pa (ex: 31 GPa)
    double nu = 0.2;         // Coefficient de Poisson
    double density = 2500.0; // Masse volumique kg/m³
    double fk = 25.0e6;      // Résistance caractéristique en Pa (fck ou fy)
    double thermalCoeff = 1.0e-5; // Dilatation thermique 1/K

    // Méthodes usines pour matériaux Eurocodes
    static Material concreteC25_30();
    static Material concreteC30_37();
    static Material steelS235();
    static Material steelS355();
    static Material timberC24();
    static std::vector<Material> defaultLibrary();
};

} // namespace TSA::Model
