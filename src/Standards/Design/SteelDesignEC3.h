#pragma once

// ============================================================
// STRUCTURAL STANDARD & DESIGN
// Standard   : EN 1993-1-1:2005 (Eurocode 3 - Calcul des structures en acier)
// Clause     : §6.2 (Résistance des sections) & §6.3 (Stabilité des barres au flambement)
// Requirement: REQ-CALC-EC3-002
// ============================================================

#include <QString>
#include <cmath>
#include "../../Model/Section.h"
#include "../../Model/Material.h"
#include "../NationalAnnexConfig.h"

namespace TSA::Standards::Design
{

/**
 * @brief Courbes de flambement européennes selon EN 1993-1-1 Tableau 6.1.
 */
enum class EC3BucklingCurve
{
    a0,     ///< Courbe a0 : facteur d'imperfection alpha = 0.13
    a,      ///< Courbe a  : facteur d'imperfection alpha = 0.21
    b,      ///< Courbe b  : facteur d'imperfection alpha = 0.34
    c,      ///< Courbe c  : facteur d'imperfection alpha = 0.49
    d       ///< Courbe d  : facteur d'imperfection alpha = 0.76
};

/**
 * @brief Données d'entrée pour la vérification au flambement sous compression axiale (EN 1993-1-1 §6.3.1).
 */
struct EC3BucklingInput
{
    double Lcr = 3.0;                   ///< Longueur de flambement en mètres Lcr = beta * L
    double A = 0.005;                   ///< Aire de la section droite en m²
    double I = 1.0e-5;                  ///< Moment d'inertie de flexion autour de l'axe considéré en m⁴
    double E = 210.0e9;                 ///< Module d'élasticité de l'acier en Pa (ex: 210 GPa)
    double fy = 355.0e6;                ///< Limite d'élasticité de l'acier en Pa (ex: 355 MPa pour S355)
    double Ned = 0.0;                   ///< Effort normal de compression sollicitant de calcul ELU en N
    EC3BucklingCurve curve = EC3BucklingCurve::b; ///< Courbe de flambement applicable
    double gammaM1 = 1.00;              ///< Facteur partiel de sécurité pour la stabilité (EN 1993-1-1 §6.1)
};

/**
 * @brief Résultat de la vérification au flambement selon l'EN 1993-1-1 §6.3.1.
 */
struct EC3BucklingResult
{
    bool valid = false;                 ///< Calcul valide et cohérent
    double radiusOfGyration = 0.0;      ///< Rayon de giration i = sqrt(I / A) (m)
    double slenderness = 0.0;           ///< Élancement géométrique lambda = Lcr / i
    double eulerSlenderness = 0.0;      ///< Élancement de référence lambda_1 = pi * sqrt(E / fy)
    double reducedSlenderness = 0.0;    ///< Élancement réduit lambda_bar = lambda / lambda_1
    double imperfectionFactor = 0.0;   ///< Facteur d'imperfection alpha selon la courbe
    double phi = 0.0;                   ///< Paramètre auxiliaire Phi
    double chi = 1.0;                   ///< Coefficient de réduction au flambement chi <= 1.0
    double Nb_Rd = 0.0;                 ///< Effort normal résistant de calcul au flambement en N
    double utilizationRatio = 0.0;      ///< Taux de travail eta = Ned / Nb_Rd
    bool pass = false;                  ///< Vérification satisfaite si utilizationRatio <= 1.0
    QString summary;                    ///< Synthèse textuelle
};

/**
 * @brief Module de calcul et de vérification réglementaire selon l'Eurocode 3 (EN 1993-1-1).
 */
class SteelDesignEC3
{
public:
    /**
     * @brief Retourne le facteur d'imperfection alpha correspondant à la courbe de flambement.
     */
    static double imperfectionFactor(EC3BucklingCurve curve);

    /**
     * @brief Calcule la résistance plastique d'une section en traction axiale Npl,Rd = A * fy / gammaM0.
     */
    static double tensionResistance(double A_m2, double fy_Pa, double gammaM0 = 1.00);

    /**
     * @brief Calcule la résistance plastique d'une section en compression uniforme Nc,Rd = A * fy / gammaM0.
     */
    static double compressionResistance(double A_m2, double fy_Pa, double gammaM0 = 1.00);

    /**
     * @brief Calcule la résistance élastique d'une section en flexion simple Mc,Rd = Wel * fy / gammaM0.
     */
    static double bendingResistance(double Wel_m3, double fy_Pa, double gammaM0 = 1.00);

    /**
     * @brief Calcule la résistance plastique au cisaillement Vpl,Rd = Av * (fy / sqrt(3)) / gammaM0.
     */
    static double shearResistance(double Av_m2, double fy_Pa, double gammaM0 = 1.00);

    /**
     * @brief Effectue la vérification complète au flambement selon l'EN 1993-1-1 §6.3.1.
     */
    static EC3BucklingResult calculateBuckling(const EC3BucklingInput& input);

    /**
     * @brief Détermine la courbe de flambement par défaut selon le profil et l'axe considéré.
     */
    static EC3BucklingCurve defaultBucklingCurve(TSA::Model::SectionShape shape, bool strongAxis = true);

    /**
     * @brief Effectue la vérification au flambement directement depuis une Section et un Material du modèle.
     */
    static EC3BucklingResult calculateFromBar(
        const TSA::Model::Section& section,
        const TSA::Model::Material& steelMat,
        double memberLength_m,
        double beta,
        double Ned_N,
        bool strongAxis = true
    );
};

} // namespace TSA::Standards::Design
