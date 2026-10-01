#include "NationalAnnexConfig.h"

namespace TSA::Standards
{

NationalAnnexConfig& NationalAnnexConfig::instance()
{
    static NationalAnnexConfig s_instance;
    return s_instance;
}

NationalAnnexConfig::NationalAnnexConfig()
    : m_currentAnnex(NationalAnnexCode::France_NF)
{
}

void NationalAnnexConfig::setAnnex(NationalAnnexCode annex)
{
    m_currentAnnex = annex;
}

QString NationalAnnexConfig::annexName() const
{
    return nationalAnnexToString(m_currentAnnex);
}

QString NationalAnnexConfig::annexDescription() const
{
    switch (m_currentAnnex)
    {
    case NationalAnnexCode::France_NF:
        return QStringLiteral("Annexe Nationale Française (NF EN 1990/NA:2011, NF EN 1992-1-1/NA, NF EN 1993-1-1/NA)");
    case NationalAnnexCode::Germany_DIN:
        return QStringLiteral("Annexe Nationale Allemande (DIN EN 1990/NA:2010, DIN EN 1992-1-1/NA, DIN EN 1993-1-1/NA)");
    case NationalAnnexCode::UK_BS:
        return QStringLiteral("Annexe Nationale Britannique (BS EN 1990/NA:2004, BS EN 1992-1-1/NA, BS EN 1993-1-1/NA)");
    case NationalAnnexCode::CEN_Default:
    default:
        return QStringLiteral("Recommandations Européennes Standard du CEN (EN 1990:2002+A1:2005 sans modification nationale)");
    }
}

PartialSafetyFactors NationalAnnexConfig::safetyFactors() const
{
    PartialSafetyFactors factors;

    switch (m_currentAnnex)
    {
    case NationalAnnexCode::France_NF:
        factors.gammaG_sup = 1.35;
        factors.gammaG_inf = 1.00;
        factors.gammaQ = 1.50;
        factors.gammaC = 1.50;
        factors.gammaS = 1.15;
        factors.gammaM0 = 1.00;
        factors.gammaM1 = 1.00;
        factors.gammaM2 = 1.25;
        break;

    case NationalAnnexCode::Germany_DIN:
        factors.gammaG_sup = 1.35;
        factors.gammaG_inf = 1.00;
        factors.gammaQ = 1.50;
        factors.gammaC = 1.50;
        factors.gammaS = 1.15;
        factors.gammaM0 = 1.00;
        factors.gammaM1 = 1.10; // Spécificité DIN pour la stabilité
        factors.gammaM2 = 1.25;
        break;

    case NationalAnnexCode::UK_BS:
        factors.gammaG_sup = 1.35; // ou 1.25 sous conditions spécifiques
        factors.gammaG_inf = 1.00;
        factors.gammaQ = 1.50;
        factors.gammaC = 1.50;
        factors.gammaS = 1.15;
        factors.gammaM0 = 1.00;
        factors.gammaM1 = 1.00;
        factors.gammaM2 = 1.10; // Spécificité BS pour assemblage
        break;

    case NationalAnnexCode::CEN_Default:
    default:
        factors.gammaG_sup = 1.35;
        factors.gammaG_inf = 1.00;
        factors.gammaQ = 1.50;
        factors.gammaC = 1.50;
        factors.gammaS = 1.15;
        factors.gammaM0 = 1.00;
        factors.gammaM1 = 1.00;
        factors.gammaM2 = 1.25;
        break;
    }

    return factors;
}

PsiFactors NationalAnnexConfig::psiForCategory(BuildingCategory cat) const
{
    PsiFactors f;
    switch (cat)
    {
    case BuildingCategory::CatA_Domestic:
        f.psi0 = 0.7; f.psi1 = 0.5; f.psi2 = 0.3;
        break;
    case BuildingCategory::CatB_Office:
        f.psi0 = 0.7; f.psi1 = 0.5; f.psi2 = 0.3;
        break;
    case BuildingCategory::CatC_Congregation:
        f.psi0 = 0.7; f.psi1 = 0.7; f.psi2 = 0.6;
        break;
    case BuildingCategory::CatD_Shopping:
        f.psi0 = 0.7; f.psi1 = 0.7; f.psi2 = 0.6;
        break;
    case BuildingCategory::CatE_Storage:
        f.psi0 = 1.0; f.psi1 = 0.9; f.psi2 = 0.8;
        break;
    case BuildingCategory::CatF_TrafficLight:
        f.psi0 = 0.7; f.psi1 = 0.7; f.psi2 = 0.6;
        break;
    case BuildingCategory::CatG_TrafficMedium:
        f.psi0 = 0.7; f.psi1 = 0.5; f.psi2 = 0.3;
        break;
    case BuildingCategory::CatH_Roof:
        f.psi0 = 0.0; f.psi1 = 0.0; f.psi2 = 0.0;
        break;
    }
    return f;
}

PsiFactors NationalAnnexConfig::psiForSnow(double altitudeMeters) const
{
    PsiFactors f;
    if (altitudeMeters > 1000.0)
    {
        // EN 1990 Tableau A1.1 : Finlande, Islande, Norvège, Suède ou altitude > 1000 m
        f.psi0 = 0.70;
        f.psi1 = 0.50;
        f.psi2 = 0.20;
    }
    else
    {
        // Reste des pays CEN pour altitude <= 1000 m
        f.psi0 = 0.50;
        f.psi1 = 0.20;
        f.psi2 = 0.00;
    }
    return f;
}

PsiFactors NationalAnnexConfig::psiForWind() const
{
    // EN 1990 Tableau A1.1 : Charges de vent sur les bâtiments
    PsiFactors f;
    f.psi0 = 0.60;
    f.psi1 = 0.20;
    f.psi2 = 0.00;
    return f;
}

QString nationalAnnexToString(NationalAnnexCode code)
{
    switch (code)
    {
    case NationalAnnexCode::France_NF:   return QStringLiteral("France (NF EN 1990/NA)");
    case NationalAnnexCode::Germany_DIN: return QStringLiteral("Allemagne (DIN EN 1990/NA)");
    case NationalAnnexCode::UK_BS:       return QStringLiteral("Royaume-Uni (BS EN 1990/NA)");
    case NationalAnnexCode::CEN_Default:
    default:                             return QStringLiteral("CEN Recommandé (Standard)");
    }
}

QString buildingCategoryToString(BuildingCategory cat)
{
    switch (cat)
    {
    case BuildingCategory::CatA_Domestic:      return QStringLiteral("Catégorie A : Résidentiel / Habitation");
    case BuildingCategory::CatB_Office:        return QStringLiteral("Catégorie B : Bureaux");
    case BuildingCategory::CatC_Congregation:  return QStringLiteral("Catégorie C : Réunion / Écoles / Salles");
    case BuildingCategory::CatD_Shopping:      return QStringLiteral("Catégorie D : Commerces");
    case BuildingCategory::CatE_Storage:       return QStringLiteral("Catégorie E : Stockage / Industriel");
    case BuildingCategory::CatF_TrafficLight:  return QStringLiteral("Catégorie F : Véhicules légers (<= 30 kN)");
    case BuildingCategory::CatG_TrafficMedium: return QStringLiteral("Catégorie G : Véhicules moyens (30-160 kN)");
    case BuildingCategory::CatH_Roof:          return QStringLiteral("Catégorie H : Toitures inaccessibles");
    default:                                   return QStringLiteral("Catégorie Inconnue");
    }
}

} // namespace TSA::Standards
