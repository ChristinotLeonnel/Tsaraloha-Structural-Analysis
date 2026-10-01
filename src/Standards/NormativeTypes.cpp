#include "NormativeTypes.h"

namespace TSA::Standards
{

QString standardFrameworkToString(StandardFramework stdCode)
{
    switch (stdCode)
    {
    case StandardFramework::ISO_IEC_25010:      return QStringLiteral("ISO/IEC 25010:2023");
    case StandardFramework::ISO_IEC_IEEE_12207: return QStringLiteral("ISO/IEC/IEEE 12207:2017");
    case StandardFramework::ISO_IEC_IEEE_29119: return QStringLiteral("ISO/IEC/IEEE 29119");
    case StandardFramework::ISO_IEC_IEEE_29148: return QStringLiteral("ISO/IEC/IEEE 29148:2018");
    case StandardFramework::ISO_9001:           return QStringLiteral("ISO 9001:2015");
    case StandardFramework::ISO_IEC_27001:      return QStringLiteral("ISO/IEC 27001");
    case StandardFramework::IEEE_Std_1063:      return QStringLiteral("IEEE Std 1063-2001 (R2007)");
    case StandardFramework::EN_1990:            return QStringLiteral("EN 1990 (Eurocode 0)");
    case StandardFramework::EN_1991:            return QStringLiteral("EN 1991 (Eurocode 1)");
    case StandardFramework::EN_1992:            return QStringLiteral("EN 1992 (Eurocode 2)");
    case StandardFramework::EN_1993:            return QStringLiteral("EN 1993 (Eurocode 3)");
    case StandardFramework::EN_1994:            return QStringLiteral("EN 1994 (Eurocode 4)");
    case StandardFramework::EN_1995:            return QStringLiteral("EN 1995 (Eurocode 5)");
    case StandardFramework::EN_1996:            return QStringLiteral("EN 1996 (Eurocode 6)");
    case StandardFramework::EN_1997:            return QStringLiteral("EN 1997 (Eurocode 7)");
    case StandardFramework::EN_1998:            return QStringLiteral("EN 1998 (Eurocode 8)");
    case StandardFramework::EN_1999:            return QStringLiteral("EN 1999 (Eurocode 9)");
    case StandardFramework::EN_1993_1_11:       return QStringLiteral("EN 1993-1-11");
    case StandardFramework::EN_10138:           return QStringLiteral("EN 10138");
    case StandardFramework::ASTM_A416:          return QStringLiteral("ASTM A416");
    case StandardFramework::fib_Bulletin_89:    return QStringLiteral("fib Bulletin 89");
    default:                                    return QStringLiteral("Autre");
    }
}

QString requirementStatusToString(RequirementStatus status)
{
    switch (status)
    {
    case RequirementStatus::Implemented:   return QStringLiteral("IMPLEMENTED");
    case RequirementStatus::Partial:       return QStringLiteral("PARTIAL");
    case RequirementStatus::Missing:       return QStringLiteral("MISSING");
    case RequirementStatus::NotApplicable: return QStringLiteral("NOT_APPLICABLE");
    case RequirementStatus::ToVerify:      return QStringLiteral("TO_VERIFY");
    default:                               return QStringLiteral("UNKNOWN");
    }
}

QString requirementDomainToString(RequirementDomain domain)
{
    switch (domain)
    {
    case RequirementDomain::Architecture:         return QStringLiteral("Architecture");
    case RequirementDomain::DataModel:            return QStringLiteral("Modèle de Données");
    case RequirementDomain::AnalysisFEM:          return QStringLiteral("Calcul Éléments Finis");
    case RequirementDomain::StructuralDesign:     return QStringLiteral("Dimensionnement Eurocodes");
    case RequirementDomain::ResultsVisualization: return QStringLiteral("Visualisation Résultats");
    case RequirementDomain::TestingQA:            return QStringLiteral("Stratégie de Test");
    case RequirementDomain::DocumentationUI:      return QStringLiteral("Documentation & UI");
    case RequirementDomain::SecurityIntegrity:    return QStringLiteral("Intégrité & Validation");
    default:                                      return QStringLiteral("Général");
    }
}

} // namespace TSA::Standards
