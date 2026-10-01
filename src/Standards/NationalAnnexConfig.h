#pragma once

#include <string>
#include <map>
#include <QString>

namespace TSA::Standards
{

/**
 * @brief Pays ou Annexe Nationale Eurocode applicable.
 * [NORM: EN 1990:2002 Annexe A1, EN 1992-1-1, EN 1993-1-1]
 */
enum class NationalAnnexCode
{
    CEN_Default,    ///< Paramètres recommandés par défaut par le CEN (Eurocode de base)
    France_NF,      ///< Annexe Nationale Française (NF EN 1990/NA)
    Germany_DIN,    ///< Annexe Nationale Allemande (DIN EN 1990/NA)
    UK_BS           ///< Annexe Nationale Britannique (BS EN 1990/NA)
};

/**
 * @brief Catégorie d'usage du bâtiment selon l'EN 1991-1-1 Tableau 6.1.
 */
enum class BuildingCategory
{
    CatA_Domestic,      ///< Catégorie A : Habitation, résidentiel
    CatB_Office,        ///< Catégorie B : Bureaux
    CatC_Congregation,  ///< Catégorie C : Lieux de réunion (écoles, restaurants, salles)
    CatD_Shopping,      ///< Catégorie D : Commerces
    CatE_Storage,       ///< Catégorie E : Stockage et activités industrielles
    CatF_TrafficLight,  ///< Catégorie F : Véhicules légers (<= 30 kN)
    CatG_TrafficMedium, ///< Catégorie G : Véhicules moyens (30 kN à 160 kN)
    CatH_Roof           ///< Catégorie H : Toitures inaccessibles sauf entretien
};

/**
 * @brief Facteurs de simultanéité psi pour une action variable selon EN 1990 Tableau A1.1.
 */
struct PsiFactors
{
    double psi0 = 0.7;  ///< Facteur de combinaison pour valeur de combinaison
    double psi1 = 0.5;  ///< Facteur de combinaison pour valeur fréquente
    double psi2 = 0.3;  ///< Facteur de combinaison pour valeur quasi-permanente
};

/**
 * @brief Facteurs partiels de sécurité gamma selon EN 1990 §6.4.3 et Annexes Nationales.
 */
struct PartialSafetyFactors
{
    double gammaG_sup = 1.35;   ///< Facteur défavorable pour actions permanentes (ELU Fondamental)
    double gammaG_inf = 1.00;   ///< Facteur favorable pour actions permanentes
    double gammaQ = 1.50;       ///< Facteur pour action variable dominante

    // Facteurs matériaux Eurocode 2 (Béton)
    double gammaC = 1.50;       ///< Facteur partiel pour le béton (situation durable et transitoire)
    double gammaS = 1.15;       ///< Facteur partiel pour l'acier passif de béton armé

    // Facteurs matériaux Eurocode 3 (Acier de charpente)
    double gammaM0 = 1.00;      ///< Résistance des sections transversales (toutes classes)
    double gammaM1 = 1.00;      ///< Résistance des barres aux instabilités (flambement, déversement)
    double gammaM2 = 1.25;      ///< Résistance des sections nettes au droit des trous de fixation
};

/**
 * @brief Configuration centralisée de l'Annexe Nationale pour les calculs réglementaires.
 */
class NationalAnnexConfig
{
public:
    static NationalAnnexConfig& instance();

    NationalAnnexCode currentAnnex() const { return m_currentAnnex; }
    void setAnnex(NationalAnnexCode annex);

    QString annexName() const;
    QString annexDescription() const;

    /// Facteurs partiels de sécurité pour l'Annexe Nationale active
    PartialSafetyFactors safetyFactors() const;

    /// Facteurs psi pour une catégorie d'usage de bâtiment donnée
    PsiFactors psiForCategory(BuildingCategory cat) const;

    /// Facteurs psi pour les charges climatiques de neige
    PsiFactors psiForSnow(double altitudeMeters = 0.0) const;

    /// Facteurs psi pour les charges de vent
    PsiFactors psiForWind() const;

private:
    NationalAnnexConfig();

    NationalAnnexCode m_currentAnnex = NationalAnnexCode::France_NF;
};

QString nationalAnnexToString(NationalAnnexCode code);
QString buildingCategoryToString(BuildingCategory cat);

} // namespace TSA::Standards
