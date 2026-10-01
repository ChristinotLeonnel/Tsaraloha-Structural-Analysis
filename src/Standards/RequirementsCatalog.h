#pragma once

#include "NormativeTypes.h"
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <QString>

namespace TSA::Standards
{

/**
 * @brief Registre centralisé et singleton de traçabilité des exigences normatives de TSA.
 * [NORM: ISO/IEC/IEEE 12207 §6.4.3, ISO/IEC 25010, ISO/IEC/IEEE 29148, ISO 9001 §7.5]
 *
 * Permet de relier formellement chaque norme internationale/européenne à ses clauses,
 * son implémentation logicielle dans TSA et son cas de test unitaire associé.
 */
class RequirementsCatalog
{
public:
    static RequirementsCatalog& instance();

    /// Enregistre ou met à jour une exigence normative dans le catalogue
    void registerRequirement(const NormativeRequirement& req);

    /// Retourne l'ensemble exhaustif des exigences normatives
    const std::vector<NormativeRequirement>& allRequirements() const { return m_requirements; }

    /// Recherche une exigence par son identifiant unique canonique (ex: "REQ-SW-ARCH-001")
    std::optional<NormativeRequirement> findById(const std::string& id) const;

    /// Filtre les exigences par cadre normatif
    std::vector<NormativeRequirement> filterByStandard(StandardFramework stdCode) const;

    /// Filtre les exigences par domaine fonctionnel
    std::vector<NormativeRequirement> filterByDomain(RequirementDomain domain) const;

    /// Filtre les exigences par statut de mise en œuvre
    std::vector<NormativeRequirement> filterByStatus(RequirementStatus status) const;

    /// Nombre total d'exigences recensées
    size_t totalCount() const { return m_requirements.size(); }

    /// Nombre d'exigences pour un statut donné
    size_t countByStatus(RequirementStatus status) const;

    /// Génère la matrice de traçabilité complète au format Markdown
    QString generateMatrixMarkdown() const;

    /// Génère le rapport de conformité et de traçabilité au format HTML
    QString generateTraceabilityReportHtml() const;

private:
    RequirementsCatalog();
    void initializeDefaultCatalog();

    std::vector<NormativeRequirement> m_requirements;
    std::map<std::string, size_t> m_idIndex;
};

} // namespace TSA::Standards
