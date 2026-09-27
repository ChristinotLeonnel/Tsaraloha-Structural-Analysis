#pragma once

#include "ExtensionTypes.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <QJsonObject>
#include <QJsonArray>

namespace TSA::Model
{
    struct Material;
    struct Section;
    class CableDefinition;
}

namespace TSA::ExtensionSystem
{

/**
 * @brief Référence à un standard normatif (Eurocode EN, ISO, ASTM, etc.).
 */
struct StandardReference
{
    std::string name;       // ex: "EN 1992-1-1" ou "EN 1993-1-1"
    std::string edition;    // ex: "2004" ou "2005+A1:2014"
    std::string clause;     // ex: "Tableau 3.1"
    std::string source;     // ex: "CEN"

    static StandardReference fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
};

/**
 * @brief Propriétés visuelles PBR (Physically-Based Rendering) découplées de la physique.
 */
struct VisualDefinition
{
    std::string baseColor = "#808080"; // Couleur albedo hexadécimale (ex: "#A0A0A0")
    double roughness = 0.85;           // [0.0 = poli, 1.0 = rugueux mat]
    double metallic = 0.0;            // [0.0 = diélectrique/béton, 1.0 = métal]
    double transparency = 0.0;        // [0.0 = opaque, 1.0 = transparent]
    double shininess = 0.10;

    std::map<std::string, std::string> textures; // "albedo" -> "textures/concrete_albedo.png"
    double textureScaleU = 1.0;
    double textureScaleV = 1.0;

    static VisualDefinition fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
};

/**
 * @brief Définition externe d'un matériau d'ingénierie (JSON).
 */
struct MaterialDefinition
{
    DefinitionReference ref;
    std::string id;             // ex: "concrete.c25_30"
    std::string name;           // ex: "C25/30"
    std::string category;       // "Concrete", "Steel", "Timber", "Masonry", "Soil", "Glass", "Aluminum"
    SemanticVersion version;    // Version de la définition (ex: 1.0)

    StandardReference standard;

    // Propriétés mécaniques avec unités physiques explicites
    PhysicalValue density;      // ex: { 2500, "kg/m3" }
    PhysicalValue youngModulus; // ex: { 31000, "MPa" }
    double poissonRatio = 0.20;
    PhysicalValue thermalCoeff; // ex: { 1.0e-5, "1/K" }

    // Résistances mécaniques
    PhysicalValue fck;          // Résistance compression caractéristique (Béton)
    PhysicalValue fy;           // Limite d'élasticité (Acier)
    PhysicalValue ft;           // Résistance en traction

    VisualDefinition visual;

    // Génération du snapshot immuable de calcul
    MechanicalSnapshot createSnapshot() const;

    // Bridge de conversion avec TSA::Model::Material
    TSA::Model::Material toModelMaterial(int fallbackId = 0) const;
    static MaterialDefinition fromModelMaterial(const TSA::Model::Material& mat, const std::string& libraryId = "org.tsaraloha.tsalib");

    static std::optional<MaterialDefinition> fromJson(const QJsonObject& json, std::string* outError = nullptr);
    QJsonObject toJson() const;
};

/**
 * @brief Définition d'une section transversale ou profilé métallique (JSON).
 */
struct SectionDefinition
{
    DefinitionReference ref;
    std::string id;             // ex: "steel.ipe200" ou "concrete.rect_400x400"
    std::string name;           // ex: "IPE 200" ou "Rect 400x400"
    std::string category;       // "Steel", "Concrete", "Timber", "Composite"
    std::string shapeType;      // "IShape", "Rectangular", "Circular", "Pipe", "BoxHollow", "UPN", "Angle", "TSection"
    SemanticVersion version;

    StandardReference standard;

    // Dimensions en mètres
    double width = 0.0;         // b
    double height = 0.0;        // h
    double diameter = 0.0;      // D
    double webThickness = 0.0;  // tw (âme)
    double flangeThickness = 0.0;// tf (aile)
    double filletRadius = 0.0;  // r (rayon de congé)

    // Caractéristiques géométriques calculées ou tabulées (SI: m, m², m³, m⁴)
    double area = 0.0;          // A (m²)
    double ix = 0.0;            // Inertie de flexion axe fort (m⁴)
    double iy = 0.0;            // Inertie de flexion axe faible (m⁴)
    double it = 0.0;            // Inertie de torsion (m⁴)
    double wx = 0.0;            // Module de résistance élastique fort (m³)
    double wy = 0.0;            // Module de résistance élastique faible (m³)

    std::string defaultMaterialId; // ex: "steel.s235"
    VisualDefinition visual;

    // Bridge de conversion avec TSA::Model::Section
    TSA::Model::Section toModelSection(int fallbackId = 0) const;
    static SectionDefinition fromModelSection(const TSA::Model::Section& sec, const std::string& libraryId = "org.tsaraloha.tsalib");

    static std::optional<SectionDefinition> fromJson(const QJsonObject& json, std::string* outError = nullptr);
    QJsonObject toJson() const;
};

/**
 * @brief Définition d'un câble, toron ou hauban d'ingénierie (JSON).
 */
struct CableCatalogDefinition
{
    DefinitionReference ref;
    std::string id;             // ex: "cable.strand_y1860s7_15_7"
    std::string name;           // ex: "Toron 7 fils T15.7 Y1860"
    std::string category;       // "Prestressing", "StayCable", "Suspension", "Generic"
    SemanticVersion version;

    StandardReference standard;
    std::string grade;          // ex: "Y1860"

    double nominalDiameter = 0.0;  // m (ex: 0.0157)
    double metallicArea = 0.0;     // m² (ex: 150e-6)
    double linearMass = 0.0;        // kg/m (ex: 1.18)

    double elasticModulus = 195.0e9;// Pa (195 GPa)
    double density = 7850.0;        // kg/m³
    double characteristicStrength = 1860.0e6; // Pa
    double minimumBreakingForce = 279.0e3;    // N (279 kN)

    double defaultInitialTension = 100.0e3;   // N
    bool tensionOnly = true;

    VisualDefinition visual;

    // Génération du snapshot immuable de calcul
    MechanicalSnapshot createSnapshot() const;

    // Bridge de conversion avec TSA::Model::CableDefinition
    TSA::Model::CableDefinition toModelCableDefinition() const;
    static CableCatalogDefinition fromModelCableDefinition(const TSA::Model::CableDefinition& cable, const std::string& libraryId = "org.tsaraloha.tsalib");

    static std::optional<CableCatalogDefinition> fromJson(const QJsonObject& json, std::string* outError = nullptr);
    QJsonObject toJson() const;
};

} // namespace TSA::ExtensionSystem
