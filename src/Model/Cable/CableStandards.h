#pragma once

#include "CableTypes.h"
#include <string>
#include <vector>
#include <optional>

namespace TSA::Model
{

/**
 * @brief Entrée normative officielle décrivant un composant de câble selon sa norme et son édition.
 * Toutes les valeurs numériques proviennent directement des normes publiées (EN 10138, EN 1993-1-11, ASTM).
 */
struct StandardCableProduct
{
    StandardCode standard = StandardCode::EN_10138_3;
    std::string standardName = "EN 10138-3";
    std::string standardVersion = "2011";
    CableType productType = CableType::Strand;
    std::string grade = "Y1860S7";
    std::string designation = "Y1860S7-15.7";

    // Propriétés géométriques (unités SI TSA : mètres et m²)
    double nominalDiameter = 0.0157;       // d en m (ex: 15.7 mm = 0.0157 m)
    double nominalCrossSection = 150.0e-6;  // Ap en m² (ex: 150 mm² = 150e-6 m²)
    double linearMass = 1.172;              // Masse linéique en kg/m

    // Propriétés mécaniques (unités SI TSA : Pascals et Newtons)
    double elasticModulus = 195.0e9;        // E en Pa (ex: 195 GPa pour torons)
    double characteristicStrength = 1860.0e6;// fpk en Pa (ex: 1860 MPa)
    double proofStrength01 = 1640.0e6;      // fp0.1k en Pa (limite conventionnelle d'élasticité à 0.1%)
    double minimumBreakingForce = 279.0e3;  // Fpk en N (ex: 279 kN)
    double relaxation1000h = 2.5;           // Perte de relaxation isotherme à 1000 h (%) à 0.70 fpk (Classe 2)

    // Coefficients normatifs
    double partialSafetyFactorGammaM = 1.15;// gamma_M / gamma_s selon Eurocode
    double thermalCoeff = 1.2e-5;           // Dilatation thermique 1/K

    std::string manufacturer;               // Fabricant / Système certifié (ex: Freyssinet, VSL, DYWIDAG, Macalloy)
    std::string notes;
};

/**
 * @brief Gestionnaire des catalogues et normes officielles pour câbles de génie civil.
 */
class CableStandardsRegistry
{
public:
    static CableStandardsRegistry& instance();

    /// Retourne la liste exhaustive de tous les produits normatifs enregistrés
    const std::vector<StandardCableProduct>& allProducts() const { return m_products; }

    /// Recherche par norme
    std::vector<StandardCableProduct> filterByStandard(StandardCode stdCode) const;

    /// Recherche par type de câble (Strand, StayCable, PrestressingBar, etc.)
    std::vector<StandardCableProduct> filterByType(CableType type) const;

    /// Recherche par désignation exacte
    std::optional<StandardCableProduct> findByDesignation(const std::string& designation) const;

    /// Recherche par norme et nom de produit ou grade
    std::optional<StandardCableProduct> findProduct(const std::string& standardName, const std::string& query) const;

    /// Recherche du produit le plus proche par diamètre nominal et type
    std::optional<StandardCableProduct> findClosest(CableType type, double diameterMeters) const;

private:
    CableStandardsRegistry();
    void initializeOfficialStandards();

    std::vector<StandardCableProduct> m_products;
};

} // namespace TSA::Model
