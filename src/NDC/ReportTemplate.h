#pragma once

#include "ReportConfiguration.h"
#include <QString>
#include <vector>

namespace TSA::NDC
{

/**
 * @brief Famille de modèles prédéfinis de Note de Calcul (Templates).
 */
enum class ReportTemplateType
{
    BureauEtudes,   ///< Modèle complet d'exécution / bureau d'études technique
    Eurocode,       ///< Modèle centré sur les ratios de conformité normative EN 1990/1992/1993
    Universitaire,  ///< Modèle scientifique détaillé (formulation EF, discrétisation, modes)
    Minimal,        ///< Synthèse exécutive compacte (extrema, réactions, conclusion)
    Custom          ///< Modèle personnalisé sauvegardé par l'utilisateur
};

/**
 * @brief Gestionnaire de modèle de rapport (Template).
 */
class ReportTemplate
{
public:
    /**
     * @brief Retourne la configuration par défaut pour le type de template sélectionné.
     */
    static ReportConfiguration createTemplate(ReportTemplateType type);

    /**
     * @brief Nom lisible du template.
     */
    static QString templateName(ReportTemplateType type);

    /**
     * @brief Description détaillée de l'usage du template.
     */
    static QString templateDescription(ReportTemplateType type);

    /**
     * @brief Liste de tous les types de templates disponibles.
     */
    static std::vector<ReportTemplateType> availableTemplates();
};

} // namespace TSA::NDC
