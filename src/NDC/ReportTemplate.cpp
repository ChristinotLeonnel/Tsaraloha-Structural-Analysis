#include "ReportTemplate.h"

namespace TSA::NDC
{

ReportConfiguration ReportTemplate::createTemplate(ReportTemplateType type)
{
    ReportConfiguration config;

    switch (type)
    {
    case ReportTemplateType::BureauEtudes:
        // Note détaillée (18 chapitres, NDCProjectNote) mise en page pour l'impression
        config.documentStatus = "Note de calcul — à viser par un ingénieur qualifié";
        config.noteLayout = NoteLayout::Detailed;
        config.templateId = "tsa.ndc.print";
        config.includeCoverPage = true;
        config.includeToc = true;
        config.includeLof = true;
        config.includeLot = true;
        config.includeIntroduction = true;
        config.includeStandards = true;
        config.includeModelGeometry = true;
        config.includeMaterials = true;
        config.includeSections = true;
        config.includeBoundaryConditions = true;
        config.includeLoadsAndCombinations = true;
        config.includeCalculationMethod = true;
        config.includeModelVerification = true;
        config.include3DModelSnapshots = true;
        config.includeBendingMoment = true;
        config.includeShearForce = true;
        config.includeAxialForce = true;
        config.includeTorsion = true;
        config.includeDisplacements = true;
        config.includeDeflections = true;
        config.includeReactions = true;
        config.includeMostStressedSummary = true;
        config.includeExtremaSpatialTable = true;
        config.includeEnvelopes = true;
        config.includeDetailedElementTables = true;
        config.includeEurocodeDesignChecks = true;
        config.includeWarningsAndLimitations = true;
        config.includeConclusion = true;
        config.includeBibliography = true;
        break;

    case ReportTemplateType::Eurocode:
        // Centré sur les vérifications de capacité et conformité réglementaire
        config.documentStatus = "Dossier Réglementaire Eurocode";
        config.customHeaderText = "Vérification Réglementaire Eurocodes (EN 1990 — EN 1998)";
        config.includeCoverPage = true;
        config.includeToc = true;
        config.includeLof = false;
        config.includeLot = true;
        config.includeIntroduction = true;
        config.includeStandards = true;
        config.includeModelGeometry = true;
        config.includeMaterials = true;
        config.includeSections = true;
        config.includeBoundaryConditions = true;
        config.includeLoadsAndCombinations = true;
        config.includeCalculationMethod = true;
        config.includeModelVerification = true;
        config.include3DModelSnapshots = true;
        config.includeBendingMoment = true;
        config.includeShearForce = true;
        config.includeAxialForce = true;
        config.includeTorsion = false;
        config.includeDisplacements = false;
        config.includeDeflections = true;
        config.includeReactions = true;
        config.includeMostStressedSummary = true;
        config.includeExtremaSpatialTable = true;
        config.includeEnvelopes = true;
        config.includeDetailedElementTables = false;
        config.includeEurocodeDesignChecks = true;
        config.includeWarningsAndLimitations = true;
        config.includeConclusion = true;
        config.includeBibliography = true;
        break;

    case ReportTemplateType::Universitaire:
        // Orienté recherche, formulation matricielle et calcul scientifique
        config.documentStatus = "Rapport Scientifique & Modélisation EF";
        config.customHeaderText = "Analyse Structurale Avancée par Éléments Finis";
        config.includeCoverPage = true;
        config.includeToc = true;
        config.includeLof = true;
        config.includeLot = true;
        config.includeIntroduction = true;
        config.includeStandards = true;
        config.includeModelGeometry = true;
        config.includeMaterials = true;
        config.includeSections = true;
        config.includeBoundaryConditions = true;
        config.includeLoadsAndCombinations = true;
        config.includeCalculationMethod = true;
        config.includeModelVerification = true;
        config.include3DModelSnapshots = true;
        config.includeBendingMoment = true;
        config.includeShearForce = true;
        config.includeAxialForce = true;
        config.includeTorsion = true;
        config.includeDisplacements = true;
        config.includeDeflections = true;
        config.includeReactions = true;
        config.includeMostStressedSummary = true;
        config.includeExtremaSpatialTable = true;
        config.includeEnvelopes = true;
        config.includeDetailedElementTables = true;
        config.includeEurocodeDesignChecks = false;
        config.includeWarningsAndLimitations = true;
        config.includeConclusion = true;
        config.includeBibliography = true;
        break;

    case ReportTemplateType::Minimal:
        // Note simplifiée (NDCProjectNote) : synthèse de quelques pages
        config.documentStatus = "Note de calcul simplifiée";
        config.noteLayout = NoteLayout::Simplified;
        config.templateId = "tsa.ndc.print";
        config.includeCoverPage = true;
        config.includeToc = false;
        config.includeLof = false;
        config.includeLot = false;
        config.includeIntroduction = true;
        config.includeStandards = false;
        config.includeModelGeometry = true;
        config.includeMaterials = false;
        config.includeSections = false;
        config.includeBoundaryConditions = false;
        config.includeLoadsAndCombinations = true;
        config.includeCalculationMethod = false;
        config.includeModelVerification = true;
        config.include3DModelSnapshots = true;
        config.includeBendingMoment = true;
        config.includeShearForce = false;
        config.includeAxialForce = false;
        config.includeTorsion = false;
        config.includeDisplacements = false;
        config.includeDeflections = true;
        config.includeReactions = true;
        config.includeMostStressedSummary = true;
        config.includeExtremaSpatialTable = false;
        config.includeEnvelopes = false;
        config.includeDetailedElementTables = false;
        config.includeEurocodeDesignChecks = false;
        config.includeWarningsAndLimitations = false;
        config.includeConclusion = true;
        config.includeBibliography = false;
        break;

    case ReportTemplateType::Custom:
        // Configuration utilisateur par défaut
        break;
    }

    return config;
}

QString ReportTemplate::templateName(ReportTemplateType type)
{
    switch (type)
    {
    case ReportTemplateType::BureauEtudes:   return "Bureau d'Études (Standard Exécution)";
    case ReportTemplateType::Eurocode:       return "Eurocode (Conformité Réglementaire)";
    case ReportTemplateType::Universitaire:  return "Universitaire & Scientifique (Détail EF)";
    case ReportTemplateType::Minimal:        return "Minimal (Synthèse Exécutive)";
    case ReportTemplateType::Custom:         return "Personnalisé (.tsareport)";
    }
    return "Standard";
}

QString ReportTemplate::templateDescription(ReportTemplateType type)
{
    switch (type)
    {
    case ReportTemplateType::BureauEtudes:
        return "Note détaillée en 18 chapitres : données, méthode et formules, résultats, câbles, vérifications calculées et liste des vérifications non calculées.";
    case ReportTemplateType::Eurocode:
        return "Focus sur la conformité aux normes EN 1990/1991/1992/1993/1998, critères d'état limite ultime (ELU) et de service (ELS).";
    case ReportTemplateType::Universitaire:
        return "Idéal pour mémoires et publications académiques : insistance sur la discrétisation EF, matrices, équilibre statique et modes vibratoires.";
    case ReportTemplateType::Minimal:
        return "Note simplifiée de quelques pages : hypothèses, charges, contrôle du modèle, résultats principaux, vérifications et limites.";
    case ReportTemplateType::Custom:
        return "Configuration utilisateur sur-mesure chargée depuis un profil sauvegardé.";
    }
    return "";
}

std::vector<ReportTemplateType> ReportTemplate::availableTemplates()
{
    return {
        ReportTemplateType::BureauEtudes,
        ReportTemplateType::Eurocode,
        ReportTemplateType::Universitaire,
        ReportTemplateType::Minimal,
        ReportTemplateType::Custom
    };
}

} // namespace TSA::NDC
