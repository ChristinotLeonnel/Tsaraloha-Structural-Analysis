#pragma once

#include <cmath>

namespace TSA::Model
{

/**
 * @brief Interface abstraite pour éléments de type câble dans les solveurs et l'analyse structurale.
 */
class ICableElement
{
public:
    virtual ~ICableElement() = default;

    virtual double length() const = 0;
    virtual bool tensionOnly() const = 0;
    virtual double initialTension() const = 0;
    virtual double metallicArea() const = 0;
    virtual double elasticModulus() const = 0;
};

/**
 * @brief Propriétés non-linéaires et comportement en traction pour les calculs EF.
 */
struct CableAnalysisProperties
{
    bool tensionOnly = true;            ///< Comportement unilatéral strict en traction (raideur nulle en compression)
    bool largeDisplacement = true;      ///< Prise en compte des grands déplacements / non-linéarité géométrique
    bool geometricNonlinearity = true;  ///< Matrice de rigidité géométrique Kg(T) fonction de la tension interne
    bool sagEffectEnabled = true;       ///< Prise en compte du mou / affaissement caténaire (formule d'Ernst)

    double minTensionThreshold = 10.0;  ///< Seuil de tension minimale en Newtons pour éviter les instabilités numériques

    /**
     * @brief Calcule le module d'élasticité sécant équivalent d'Ernst pour un câble fléchi sous son propre poids.
     * Formule classique de génie civil d'Ernst (utilisée pour les haubans et câbles de ponts) :
     * E_eq = E / [ 1 + ( (w * L_h)^2 * E * A ) / ( 12 * T^3 ) ]
     *
     * @param E Module d'Young élastique du matériau (Pa)
     * @param A Section métallique du câble (m²)
     * @param linearWeight Poids propre linéique w = rho * A * g (N/m)
     * @param horizontalSpan Portée horizontale projetée L_h (m)
     * @param tension Effort normal de traction interne T (N)
     * @return double Module sécant équivalent E_eq (Pa)
     */
    static double calculateErnstEquivalentModulus(double E, double A, double linearWeight, double horizontalSpan, double tension)
    {
        if (tension <= 1e-3 || horizontalSpan <= 1e-4 || E <= 1e-3 || A <= 1e-9)
        {
            return E;
        }

        double numerator = std::pow(linearWeight * horizontalSpan, 2.0) * E * A;
        double denominator = 12.0 * std::pow(tension, 3.0);
        double factor = 1.0 + (numerator / denominator);

        if (factor > 1e6)
        {
            // Câble détendu avec très faible raideur effective
            return E / 1e6;
        }

        return E / factor;
    }
};

} // namespace TSA::Model
