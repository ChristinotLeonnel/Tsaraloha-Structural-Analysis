#include "CableStandards.h"
#include <cmath>
#include <algorithm>

namespace TSA::Model
{

CableStandardsRegistry& CableStandardsRegistry::instance()
{
    static CableStandardsRegistry registry;
    return registry;
}

CableStandardsRegistry::CableStandardsRegistry()
{
    initializeOfficialStandards();
}

void CableStandardsRegistry::initializeOfficialStandards()
{
    m_products.clear();

    // =========================================================================
    // 1. EN 10138-3 (2011) : Aciers de précontrainte - Torons 7 fils (S7)
    // =========================================================================
    // Toron Y1860S7 - 12.5 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1860S7";
        p.designation = "EN10138-Y1860S7-12.5";
        p.nominalDiameter = 0.0125;
        p.nominalCrossSection = 93.0e-6;
        p.linearMass = 0.726;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 173.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils haute résistance très basse relaxation (TBR)";
        m_products.push_back(p);
    }

    // Toron Y1860S7 - 12.7 mm (0.5 pouce standard)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1860S7";
        p.designation = "EN10138-Y1860S7-12.7";
        p.nominalDiameter = 0.0127;
        p.nominalCrossSection = 98.7e-6;
        p.linearMass = 0.775;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 183.7e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils standard 0.5 pouce pour post-tension et précontrainte";
        m_products.push_back(p);
    }

    // Toron Y1860S7 - 12.9 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1860S7";
        p.designation = "EN10138-Y1860S7-12.9";
        p.nominalDiameter = 0.0129;
        p.nominalCrossSection = 100.0e-6;
        p.linearMass = 0.781;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 186.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils section 100 mm²";
        m_products.push_back(p);
    }

    // Toron Y1860S7 - 15.2 mm (0.6 pouce standard)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1860S7";
        p.designation = "EN10138-Y1860S7-15.2";
        p.nominalDiameter = 0.0152;
        p.nominalCrossSection = 139.0e-6;
        p.linearMass = 1.086;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 259.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils 0.6 pouce très répandu en ponts et dalles précontraintes";
        m_products.push_back(p);
    }

    // Toron Y1860S7 - 15.7 mm (0.62 pouce super-strand)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1860S7";
        p.designation = "EN10138-Y1860S7-15.7";
        p.nominalDiameter = 0.0157;
        p.nominalCrossSection = 150.0e-6;
        p.linearMass = 1.172;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 279.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils T15S haute performance pour haubans et gros tirants";
        m_products.push_back(p);
    }

    // Toron Y1770S7 - 15.2 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_3;
        p.standardName = "EN 10138-3";
        p.standardVersion = "2011";
        p.productType = CableType::Strand;
        p.grade = "Y1770S7";
        p.designation = "EN10138-Y1770S7-15.2";
        p.nominalDiameter = 0.0152;
        p.nominalCrossSection = 139.0e-6;
        p.linearMass = 1.086;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1770.0e6;
        p.proofStrength01 = 1560.0e6;
        p.minimumBreakingForce = 246.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Toron 7 fils classe 1770 MPa";
        m_products.push_back(p);
    }

    // =========================================================================
    // 2. EN 10138-4 : Barres lisses et filetées de précontrainte (ex: Macalloy / SAS)
    // =========================================================================
    // Barre Y1030 - 26.5 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_4;
        p.standardName = "EN 10138-4";
        p.standardVersion = "2011";
        p.productType = CableType::PrestressingBar;
        p.grade = "Y1030";
        p.designation = "EN10138-Bar-Y1030-26.5";
        p.nominalDiameter = 0.0265;
        p.nominalCrossSection = 551.5e-6;
        p.linearMass = 4.33;
        p.elasticModulus = 205.0e9;
        p.characteristicStrength = 1030.0e6;
        p.proofStrength01 = 835.0e6;
        p.minimumBreakingForce = 568.0e3;
        p.relaxation1000h = 3.5;
        p.notes = "Barre rigide de précontrainte filetée à froid";
        m_products.push_back(p);
    }

    // Barre Y1030 - 32 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_4;
        p.standardName = "EN 10138-4";
        p.standardVersion = "2011";
        p.productType = CableType::PrestressingBar;
        p.grade = "Y1030";
        p.designation = "EN10138-Bar-Y1030-32";
        p.nominalDiameter = 0.032;
        p.nominalCrossSection = 804.2e-6;
        p.linearMass = 6.31;
        p.elasticModulus = 205.0e9;
        p.characteristicStrength = 1030.0e6;
        p.proofStrength01 = 835.0e6;
        p.minimumBreakingForce = 828.0e3;
        p.relaxation1000h = 3.5;
        p.notes = "Barre de précontrainte SAS / Macalloy diam. 32 mm";
        m_products.push_back(p);
    }

    // Barre Y1030 - 36 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_4;
        p.standardName = "EN 10138-4";
        p.standardVersion = "2011";
        p.productType = CableType::PrestressingBar;
        p.grade = "Y1030";
        p.designation = "EN10138-Bar-Y1030-36";
        p.nominalDiameter = 0.036;
        p.nominalCrossSection = 1017.9e-6;
        p.linearMass = 7.99;
        p.elasticModulus = 205.0e9;
        p.characteristicStrength = 1030.0e6;
        p.proofStrength01 = 835.0e6;
        p.minimumBreakingForce = 1048.0e3;
        p.relaxation1000h = 3.5;
        p.notes = "Barre de précontrainte SAS / Macalloy diam. 36 mm";
        m_products.push_back(p);
    }

    // Barre Y1030 - 40 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_10138_4;
        p.standardName = "EN 10138-4";
        p.standardVersion = "2011";
        p.productType = CableType::PrestressingBar;
        p.grade = "Y1030";
        p.designation = "EN10138-Bar-Y1030-40";
        p.nominalDiameter = 0.040;
        p.nominalCrossSection = 1256.6e-6;
        p.linearMass = 9.87;
        p.elasticModulus = 205.0e9;
        p.characteristicStrength = 1030.0e6;
        p.proofStrength01 = 835.0e6;
        p.minimumBreakingForce = 1294.0e3;
        p.relaxation1000h = 3.5;
        p.notes = "Barre lourde de précontrainte et tirant d'ancrage";
        m_products.push_back(p);
    }

    // =========================================================================
    // 3. EN 1993-1-11 : Éléments tendus en acier (Haubans, Câbles clos FLC, Suspentes)
    // =========================================================================
    // Câble clos (Full Locked Coil - FLC) 50 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11";
        p.standardVersion = "2006";
        p.productType = CableType::StayCable;
        p.grade = "FLC-1570";
        p.designation = "EN1993-FLC-50";
        p.nominalDiameter = 0.050;
        p.nominalCrossSection = 1680.0e-6;
        p.linearMass = 13.9;
        p.elasticModulus = 160.0e9; // Module sécant stabilisé EN 1993-1-11
        p.characteristicStrength = 1570.0e6;
        p.proofStrength01 = 1300.0e6;
        p.minimumBreakingForce = 2375.0e3;
        p.relaxation1000h = 2.0;
        p.notes = "Câble clos monotoron à fils profilés Z anti-corrosion (Ponts et toitures)";
        m_products.push_back(p);
    }

    // Câble clos (Full Locked Coil - FLC) 80 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11";
        p.standardVersion = "2006";
        p.productType = CableType::StayCable;
        p.grade = "FLC-1570";
        p.designation = "EN1993-FLC-80";
        p.nominalDiameter = 0.080;
        p.nominalCrossSection = 4310.0e-6;
        p.linearMass = 35.7;
        p.elasticModulus = 160.0e9;
        p.characteristicStrength = 1570.0e6;
        p.proofStrength01 = 1300.0e6;
        p.minimumBreakingForce = 6090.0e3;
        p.relaxation1000h = 2.0;
        p.notes = "Câble clos de pont suspendu ou haubané de moyenne portée";
        m_products.push_back(p);
    }

    // Câble clos (Full Locked Coil - FLC) 120 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11";
        p.standardVersion = "2006";
        p.productType = CableType::SuspensionCable;
        p.grade = "FLC-1570";
        p.designation = "EN1993-FLC-120";
        p.nominalDiameter = 0.120;
        p.nominalCrossSection = 9700.0e-6;
        p.linearMass = 80.4;
        p.elasticModulus = 160.0e9;
        p.characteristicStrength = 1570.0e6;
        p.proofStrength01 = 1300.0e6;
        p.minimumBreakingForce = 13700.0e3;
        p.relaxation1000h = 2.0;
        p.notes = "Câble porteur principal pour pont suspendu";
        m_products.push_back(p);
    }

    // Suspente verticale (Hanger) 30 mm
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11";
        p.standardVersion = "2006";
        p.productType = CableType::Hanger;
        p.grade = "SpiralStrand-1770";
        p.designation = "EN1993-Hanger-30";
        p.nominalDiameter = 0.030;
        p.nominalCrossSection = 565.0e-6;
        p.linearMass = 4.65;
        p.elasticModulus = 150.0e9;
        p.characteristicStrength = 1770.0e6;
        p.proofStrength01 = 1450.0e6;
        p.minimumBreakingForce = 850.0e3;
        p.relaxation1000h = 2.5;
        p.notes = "Suspente spiralée galvanisée pour suspension de tablier";
        m_products.push_back(p);
    }

    // Hauban multi-torons PSS (Parallel Strand System) 19 x T15S (15.7 mm)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11 / fib 89";
        p.standardVersion = "2019";
        p.productType = CableType::StayCable;
        p.grade = "PSS-19xT15S";
        p.designation = "Stay-PSS-19-15.7";
        p.nominalDiameter = 0.090; // Diamètre enveloppe gaine PEHD
        p.nominalCrossSection = 19 * 150.0e-6; // 2850 mm²
        p.linearMass = 19 * 1.172 + 2.5; // Acier + gaine + cire
        p.elasticModulus = 195.0e9; // Torons parallèles non vrillés
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 19 * 279.0e3; // 5301 kN
        p.relaxation1000h = 2.0;
        p.manufacturer = "Freyssinet / VSL / DYWIDAG";
        p.notes = "Faisceau de 19 torons protégés individuellement (gaine PEHD extérieure)";
        m_products.push_back(p);
    }

    // Hauban multi-torons PSS 37 x T15S (15.7 mm)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11 / fib 89";
        p.standardVersion = "2019";
        p.productType = CableType::StayCable;
        p.grade = "PSS-37xT15S";
        p.designation = "Stay-PSS-37-15.7";
        p.nominalDiameter = 0.125;
        p.nominalCrossSection = 37 * 150.0e-6; // 5550 mm²
        p.linearMass = 37 * 1.172 + 4.2;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 37 * 279.0e3; // 10323 kN
        p.relaxation1000h = 2.0;
        p.manufacturer = "Freyssinet / VSL / DYWIDAG";
        p.notes = "Faisceau de 37 torons pour ponts à haubans de grande portée";
        m_products.push_back(p);
    }

    // Hauban multi-torons PSS 61 x T15S (15.7 mm)
    {
        StandardCableProduct p;
        p.standard = StandardCode::EN_1993_1_11;
        p.standardName = "EN 1993-1-11 / fib 89";
        p.standardVersion = "2019";
        p.productType = CableType::StayCable;
        p.grade = "PSS-61xT15S";
        p.designation = "Stay-PSS-61-15.7";
        p.nominalDiameter = 0.160;
        p.nominalCrossSection = 61 * 150.0e-6; // 9150 mm²
        p.linearMass = 61 * 1.172 + 6.8;
        p.elasticModulus = 195.0e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1640.0e6;
        p.minimumBreakingForce = 61 * 279.0e3; // 17019 kN
        p.relaxation1000h = 2.0;
        p.manufacturer = "Freyssinet / VSL / DYWIDAG";
        p.notes = "Faisceau de 61 torons pour grands ponts à haubans";
        m_products.push_back(p);
    }

    // =========================================================================
    // 4. ASTM A416 / A416M (USA / International)
    // =========================================================================
    // ASTM A416 Grade 270 - 0.5 in (12.7 mm)
    {
        StandardCableProduct p;
        p.standard = StandardCode::ASTM_A416;
        p.standardName = "ASTM A416";
        p.standardVersion = "2018";
        p.productType = CableType::Strand;
        p.grade = "Grade 270";
        p.designation = "ASTM-A416-Gr270-0.5in";
        p.nominalDiameter = 0.0127;
        p.nominalCrossSection = 98.71e-6;
        p.linearMass = 0.775;
        p.elasticModulus = 196.5e9; // 28500 ksi
        p.characteristicStrength = 1860.0e6; // 270 ksi
        p.proofStrength01 = 1670.0e6;
        p.minimumBreakingForce = 183.7e3; // 41.3 kips
        p.relaxation1000h = 2.5;
        p.notes = "Standard American seven-wire low-relaxation prestressing strand";
        m_products.push_back(p);
    }

    // ASTM A416 Grade 270 - 0.6 in (15.24 mm)
    {
        StandardCableProduct p;
        p.standard = StandardCode::ASTM_A416;
        p.standardName = "ASTM A416";
        p.standardVersion = "2018";
        p.productType = CableType::Strand;
        p.grade = "Grade 270";
        p.designation = "ASTM-A416-Gr270-0.6in";
        p.nominalDiameter = 0.01524;
        p.nominalCrossSection = 140.0e-6;
        p.linearMass = 1.102;
        p.elasticModulus = 196.5e9;
        p.characteristicStrength = 1860.0e6;
        p.proofStrength01 = 1670.0e6;
        p.minimumBreakingForce = 260.7e3; // 58.6 kips
        p.relaxation1000h = 2.5;
        p.notes = "Standard American seven-wire 0.6 in strand";
        m_products.push_back(p);
    }
}

std::vector<StandardCableProduct> CableStandardsRegistry::filterByStandard(StandardCode stdCode) const
{
    std::vector<StandardCableProduct> res;
    for (const auto& prod : m_products)
    {
        if (prod.standard == stdCode)
        {
            res.push_back(prod);
        }
    }
    return res;
}

std::vector<StandardCableProduct> CableStandardsRegistry::filterByType(CableType type) const
{
    std::vector<StandardCableProduct> res;
    for (const auto& prod : m_products)
    {
        if (prod.productType == type)
        {
            res.push_back(prod);
        }
    }
    return res;
}

std::optional<StandardCableProduct> CableStandardsRegistry::findByDesignation(const std::string& designation) const
{
    for (const auto& prod : m_products)
    {
        if (prod.designation == designation)
        {
            return prod;
        }
    }
    return std::nullopt;
}

std::optional<StandardCableProduct> CableStandardsRegistry::findProduct(const std::string& standardName, const std::string& query) const
{
    for (const auto& prod : m_products)
    {
        bool stdMatch = (standardName.empty() || 
                         prod.standardName.find(standardName) != std::string::npos || 
                         standardName.find(prod.standardName) != std::string::npos);
        if (!stdMatch) continue;

        if (prod.designation == query ||
            prod.designation.find(query) != std::string::npos ||
            prod.grade == query ||
            prod.grade.find(query) != std::string::npos)
        {
            return prod;
        }
    }
    return std::nullopt;
}

std::optional<StandardCableProduct> CableStandardsRegistry::findClosest(CableType type, double diameterMeters) const
{
    double bestDiff = 1e9;
    std::optional<StandardCableProduct> bestMatch;

    for (const auto& prod : m_products)
    {
        if (prod.productType == type || type == CableType::Generic)
        {
            double diff = std::abs(prod.nominalDiameter - diameterMeters);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestMatch = prod;
            }
        }
    }
    return bestMatch;
}

} // namespace TSA::Model
