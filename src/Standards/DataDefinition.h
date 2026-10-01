#pragma once

#include "NormativeTypes.h"
#include <string>
#include <vector>
#include <limits>
#include <cmath>

namespace TSA::Standards
{

/**
 * @brief Définition descriptive et métadonnées d'une propriété de données normative.
 * [NORM: ISO/IEC 25010 §4.2.5, EN 1990 §1.6]
 */
struct NormativePropertyDefinition
{
    std::string propertyName;           ///< Identifiant canonique (ex: "fck", "E", "rho", "Iy")
    std::string displayName;            ///< Nom complet lisible (ex: "Résistance caractéristique à la compression du béton")
    std::string unit;                   ///< Unité du Système International (ex: "MPa", "Pa", "m", "m4", "kg/m3")
    double minValue = -std::numeric_limits<double>::infinity();
    double maxValue = std::numeric_limits<double>::infinity();
    double defaultValue = 0.0;
    StandardFramework standard = StandardFramework::EN_1990;
    std::string standardClause;         ///< Clause exacte de la norme (ex: "EN 1992-1-1 Tableau 3.1")
    std::string validatorName;          ///< Validateur associé (ex: "MaterialValidator", "SectionValidator")

    /// Vérifie si une valeur numérique est conforme au domaine de validité physique
    bool isValid(double value) const
    {
        if (std::isnan(value) || std::isinf(value)) return false;
        return (value >= minValue && value <= maxValue);
    }
};

/**
 * @brief Usines de métadonnées pour les propriétés usuelles des matériaux et profilés.
 */
class StandardPropertyDefinitions
{
public:
    static NormativePropertyDefinition concreteCompressiveStrength()
    {
        return {
            "fck",
            "Résistance caractéristique en compression du béton à 28 jours",
            "Pa",
            12.0e6,     // Min C12/15 (12 MPa)
            90.0e6,     // Max C90/105 (90 MPa)
            25.0e6,     // Défaut C25/30
            StandardFramework::EN_1992,
            "EN 1992-1-1:2004 Tableau 3.1",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition steelYieldStrength()
    {
        return {
            "fy",
            "Limite d'élasticité nominale de l'acier de construction",
            "Pa",
            195.0e6,    // Min (195 MPa)
            960.0e6,    // Max acier haute résistance (960 MPa)
            235.0e6,    // Défaut S235
            StandardFramework::EN_1993,
            "EN 1993-1-1:2005 Tableau 3.1",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition youngModulusSteel()
    {
        return {
            "E_steel",
            "Module d'élasticité longitudinale de l'acier",
            "Pa",
            180.0e9,
            220.0e9,
            210.0e9,    // 210 GPa
            StandardFramework::EN_1993,
            "EN 1993-1-1:2005 §3.2.6",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition youngModulusConcrete()
    {
        return {
            "Ecm",
            "Module sécant d'élasticité du béton à 28 jours",
            "Pa",
            20.0e9,
            45.0e9,
            31.0e9,    // 31 GPa pour C25/30
            StandardFramework::EN_1992,
            "EN 1992-1-1:2004 Tableau 3.1",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition poissonRatioConcrete()
    {
        return {
            "nu_concrete",
            "Coefficient de Poisson du béton non fissuré",
            "-",
            0.0,
            0.25,
            0.20,
            StandardFramework::EN_1992,
            "EN 1992-1-1:2004 §3.1.3 (4)",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition densityConcrete()
    {
        return {
            "rho_concrete",
            "Masse volumique du béton armé durci",
            "kg/m3",
            2200.0,
            2600.0,
            2500.0,
            StandardFramework::EN_1991,
            "EN 1991-1-1:2002 Annexe A Tableau A.1",
            "MaterialValidator"
        };
    }

    static NormativePropertyDefinition sectionArea()
    {
        return {
            "A",
            "Aire de la section transversale",
            "m2",
            1.0e-7,     // Strictement positif
            100.0,
            0.15,
            StandardFramework::EN_1990,
            "RDM classique",
            "SectionValidator"
        };
    }

    static NormativePropertyDefinition sectionMomentOfInertia()
    {
        return {
            "I",
            "Moment quadratique principal de flexion",
            "m4",
            1.0e-12,    // Strictement positif
            1000.0,
            3.125e-3,
            StandardFramework::EN_1990,
            "RDM classique",
            "SectionValidator"
        };
    }
};

} // namespace TSA::Standards
