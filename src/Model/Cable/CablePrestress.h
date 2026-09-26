#pragma once

#include <cmath>
#include <string>

namespace TSA::Model
{

/**
 * @brief Paramètres de précontrainte, tension initiale et estimation des pertes selon EN 1992-1-1 et EN 1993-1-11.
 */
struct CablePrestress
{
    // Tension initiale appliquée (à la mise en tension au vérin)
    double initialTension = 0.0;    // P0 en Newtons (N)
    double initialStrain = 0.0;     // Déformation initiale sans dimension (epsilon_0)
    double stressAtTransfer = 0.0;  // sigma_p0 en Pascals (Pa)

    // Paramètres de pertes instantanées (Eurocode 2 / EN 1992-1-1 §5.10.5)
    double anchorageSlip = 0.006;   // Rentrée de mors Delta L (m), typiquement 4 à 6 mm
    double frictionCoeff = 0.19;    // Coefficient de frottement mu (rad^-1) pour gaines métalliques / PEHD
    double wobbleCoeff = 0.005;     // Coefficient de déviation parasite k (rad/m)

    // Pertes différées à long terme (Relaxation, Fluage, Retrait)
    double relaxationLossRatio = 0.025; // Perte par relaxation de l'acier (Delta sigma_pr / sigma_pi) ~2.5%
    double elasticShorteningLoss = 0.0; // Perte par raccourcissement élastique (N)
    double timeDependentLosses = 0.0;   // Pertes totales différées estimées (N)

    /**
     * @brief Calcule la contrainte initiale sigma_p0 = P0 / Area
     */
    double calculateStress(double metallicArea) const
    {
        if (metallicArea > 1e-12)
        {
            return initialTension / metallicArea;
        }
        return 0.0;
    }

    /**
     * @brief Calcule la déformation élastique initiale epsilon_0 = sigma_p0 / E
     */
    double calculateStrain(double metallicArea, double elasticModulus) const
    {
        double stress = calculateStress(metallicArea);
        if (elasticModulus > 1e-6)
        {
            return stress / elasticModulus;
        }
        return 0.0;
    }

    /**
     * @brief Calcule la perte par frottement le long du tracé selon la formule normalisée EN 1992-1-1 :
     * Delta P_mu(x) = P0 * (1 - exp(-mu * (theta + k * x)))
     * @param distanceX Distance depuis l'ancrage actif (m)
     * @param totalAngleChange Somme des déviations angulaires theta (radians)
     */
    double calculateFrictionLoss(double distanceX, double totalAngleChange) const
    {
        double exponent = frictionCoeff * (totalAngleChange + wobbleCoeff * distanceX);
        return initialTension * (1.0 - std::exp(-exponent));
    }
};

} // namespace TSA::Model
