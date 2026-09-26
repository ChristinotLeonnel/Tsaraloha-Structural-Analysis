#pragma once

#include <string>
#include <vector>

namespace TSA::Model
{

enum class MaterialType
{
    Concrete = 0,
    Steel = 1,
    Timber = 2,
    Masonry = 3,
    Custom = 4,
    ReinforcedConcrete = 5,
    RebarSteel = 6,
    GalvanizedSteel = 7,
    Aluminum = 8,
    Brick = 9,
    Glass = 10,
    Soil = 11,
    Sand = 12,
    Gravel = 13,
    Rock = 14
};

/**
 * @brief Propriétés physiques et mécaniques intrinsèques (destinées aux calculs EF / analyse structurale).
 */
struct MechanicalProperties
{
    double youngModulus = 31.0e9;       // E en Pa (ex: 31 GPa)
    double poissonRatio = 0.20;         // nu
    double density = 2500.0;            // rho en kg/m³
    double yieldStrength = 25.0e6;      // fk / fck / fy en Pa
    double thermalCoeff = 1.0e-5;       // Dilatation thermique 1/K

    // Accesseurs de commodité
    double E() const { return youngModulus; }
    double nu() const { return poissonRatio; }
    double rho() const { return density; }
    double fk() const { return yieldStrength; }
};

/**
 * @brief Propriétés visuelles et de rendu réaliste dans le viewport (destinées à OCCT PBR / ombrage).
 */
struct VisualProperties
{
    std::string baseColor = "#A0A0A0"; // Albedo / Couleur de base hexadécimale
    double roughness = 0.85;           // Rugosité [0.0 = lisse miroir, 1.0 = mat/rugueux]
    double metallic = 0.0;            // Aspect métallique [0.0 = diélectrique, 1.0 = métal]
    double transparency = 0.0;        // Transparence [0.0 = opaque, 1.0 = transparent]
    double shininess = 0.10;          // Brillance spéculaire Phong [0.0 à 1.0]
    std::string textureName;          // Identifiant ou nom de texture (ex: "concrete", "steel", "wood")
    std::string texturePath;          // Chemin optionnel vers une texture personnalisée
    double textureScaleU = 1.0;
    double textureScaleV = 1.0;
};

/**
 * @brief Définition complète d'un matériau d'ingénierie combinant physique et aspect visuel.
 */
struct Material
{
    int id = 0;
    std::string name = "Concrete C25/30";
    MaterialType type = MaterialType::Concrete;

    // Découplage strict : Physique vs Visuel
    MechanicalProperties mechanical;
    VisualProperties visual;

    // Champs de compatibilité directe pour les modules existants et sérialisation binaire
    double E = 31.0e9;            // Module d'Young en Pa
    double nu = 0.20;            // Coefficient de Poisson
    double density = 2500.0;      // Masse volumique kg/m³
    double fk = 25.0e6;           // Résistance caractéristique en Pa (fck ou fy)
    double thermalCoeff = 1.0e-5; // Dilatation thermique 1/K

    void syncMechanical()
    {
        mechanical.youngModulus = E;
        mechanical.poissonRatio = nu;
        mechanical.density = density;
        mechanical.yieldStrength = fk;
        mechanical.thermalCoeff = thermalCoeff;
    }

    void syncFromMechanical()
    {
        E = mechanical.youngModulus;
        nu = mechanical.poissonRatio;
        density = mechanical.density;
        fk = mechanical.yieldStrength;
        thermalCoeff = mechanical.thermalCoeff;
    }

    // Méthodes usines pour matériaux du génie civil / Eurocodes
    static Material concreteC25_30();
    static Material concreteC30_37();
    static Material reinforcedConcrete();
    static Material steelS235();
    static Material steelS355();
    static Material rebarSteel();
    static Material galvanizedSteel();
    static Material aluminum();
    static Material timberC24();
    static Material brick();
    static Material masonry();
    static Material glass();
    static Material soil();
    static Material sand();
    static Material gravel();
    static Material rock();

    static std::vector<Material> defaultLibrary();
    static Material findByName(const std::string& name);
    static Material findById(int id);
    static Material findByType(MaterialType type);
};

} // namespace TSA::Model

