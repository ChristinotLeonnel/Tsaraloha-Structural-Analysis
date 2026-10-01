#pragma once

// ============================================================
// STRUCTURAL STANDARD & DESIGN
// Standard   : EN 1992-1-1:2004 (Eurocode 2 - Calcul des structures en béton)
// Clause     : §6.1 (Flexion simple aux ELU) & §9.2.1.1 (Ferraillage minimal)
// Requirement: REQ-CALC-EC2-002
// ============================================================

#include <QString>
#include <cmath>
#include "../../Model/Section.h"
#include "../../Model/Material.h"
#include "../NationalAnnexConfig.h"

namespace TSA::Standards::Design
{

/**
 * @brief Données d'entrée pour le dimensionnement en flexion simple selon l'EN 1992-1-1 §6.1.
 */
struct EC2BeamBendingInput
{
    double b = 0.30;            ///< Largeur de la poutre en mètres
    double h = 0.50;            ///< Hauteur totale de la poutre en mètres
    double d = 0.0;             ///< Hauteur utile en mètres (si 0.0, calculée par d = h - cover)
    double cover = 0.05;        ///< Enrobage nominal + étrier + rayon armature (défaut: 50 mm = 0.05 m)
    double fck = 25.0e6;        ///< Résistance caractéristique du béton à 28 jours en Pa (ex: 25 MPa)
    double fyk = 500.0e6;       ///< Limite d'élasticité de l'acier passif en Pa (ex: 500 MPa, B500B)
    double Med = 0.0;           ///< Moment sollicitant de calcul ELU en N.m (valeur absolue)
    double alphaCC = 1.0;       ///< Coefficient tenant compte des effets à long terme (1.0 selon annexe FR)
    double gammaC = 1.50;       ///< Coefficient partiel du béton aux ELU (EN 1992-1-1 Tableau 2.1N)
    double gammaS = 1.15;       ///< Coefficient partiel de l'acier aux ELU (EN 1992-1-1 Tableau 2.1N)
};

/**
 * @brief Résultat du dimensionnement des armatures longitudinales selon l'EN 1992-1-1 §6.1.
 */
struct EC2BeamBendingResult
{
    bool valid = false;                 ///< Calcul valide et cohérent
    bool requiresCompressionSteel = false; ///< True si le moment dépasse la capacité sans armatures comprimées
    double fcd = 0.0;                   ///< Résistance de calcul du béton fcd = alphaCC * fck / gammaC (Pa)
    double fyd = 0.0;                   ///< Résistance de calcul de l'acier fyd = fyk / gammaS (Pa)
    double effectiveDepth = 0.0;        ///< Hauteur utile effective d (m)
    double mu_cu = 0.0;                 ///< Moment réduit agissant mu_cu = Med / (b * d^2 * fcd)
    double mu_lu = 0.372;               ///< Moment réduit limite pour acier B500B (pivot B)
    double z = 0.0;                     ///< Bras de levier des forces internes (m)
    double As_required = 0.0;           ///< Section d'acier tendu requise As = Med / (z * fyd) (m²)
    double As_min = 0.0;                ///< Section minimale d'acier selon EN 1992-1-1 §9.2.1.1 (m²)
    double As_max = 0.0;                ///< Section maximale d'acier (0.04 * Ac) (m²)
    double As_provided = 0.0;           ///< Section d'acier finale préconisée max(As_required, As_min) (m²)
    double utilizationRatio = 0.0;      ///< Ratio d'utilisation de la section mu_cu / mu_lu
    QString summary;                    ///< Synthèse lisible
};

/**
 * @brief Vérificateur et dimensionneur d'éléments en béton armé selon l'Eurocode 2.
 */
class ConcreteDesignEC2
{
public:
    /**
     * @brief Calcule le ferraillage longitudinal en flexion simple d'une section rectangulaire.
     * Implémente le diagramme parabole-rectangle simplifié (bloc rectangulaire EN 1992-1-1 §3.1.7).
     */
    static EC2BeamBendingResult calculateRectangularBeamFlexure(const EC2BeamBendingInput& input);

    /**
     * @brief Dimensionne les armatures à partir d'une Section et d'un Material du modèle TSA.
     */
    static EC2BeamBendingResult calculateFromModel(
        const TSA::Model::Section& section,
        const TSA::Model::Material& concreteMat,
        double Med_Nm,
        double fyk_Pa = 500.0e6,
        double cover_m = 0.05
    );
};

} // namespace TSA::Standards::Design
