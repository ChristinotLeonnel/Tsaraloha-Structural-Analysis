#pragma once

#include <string>
#include <vector>
#include <QString>

namespace TSA::Model
{
class Model;
class Node;
class Beam;
class Column;
class TrussMember;
struct Section;
struct Material;
}

namespace TSA::Standards
{

enum class ValidationSeverity
{
    Info,
    Warning,
    Error
};

struct ValidationIssue
{
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string category;       ///< ex: "Nœuds", "Sections", "Matériaux", "Stabilité", "Charges"
    std::string message;        ///< Description détaillée de l'anomalie
    int entityId = 0;           ///< Identifiant de l'entité concernée (ou 0 si global)
    std::string normativeRef;   ///< Référence normative ou règle violée
};

/**
 * @brief Rapport complet de validation géométrique, physique et normative du modèle.
 * [NORM: ISO/IEC 25010 §4.2.5, EN 1990]
 */
class ModelValidationReport
{
public:
    ModelValidationReport() = default;

    void addInfo(const std::string& category, const std::string& msg, int entityId = 0, const std::string& normRef = "")
    {
        m_issues.push_back({ ValidationSeverity::Info, category, msg, entityId, normRef });
    }

    void addWarning(const std::string& category, const std::string& msg, int entityId = 0, const std::string& normRef = "")
    {
        m_issues.push_back({ ValidationSeverity::Warning, category, msg, entityId, normRef });
    }

    void addError(const std::string& category, const std::string& msg, int entityId = 0, const std::string& normRef = "")
    {
        m_issues.push_back({ ValidationSeverity::Error, category, msg, entityId, normRef });
        m_hasErrors = true;
    }

    bool isValid() const noexcept { return !m_hasErrors; }
    bool hasErrors() const noexcept { return m_hasErrors; }
    bool hasWarnings() const noexcept;

    size_t errorCount() const;
    size_t warningCount() const;

    const std::vector<ValidationIssue>& issues() const noexcept { return m_issues; }

    std::vector<std::string> formattedErrors() const;
    std::vector<std::string> formattedWarnings() const;

    QString summary() const;

private:
    std::vector<ValidationIssue> m_issues;
    bool m_hasErrors = false;
};

/**
 * @brief Validateur exhaustif de cohérence physique, géométrique et normative pour TSA.
 */
class ModelValidator
{
public:
    ModelValidator() = default;

    /// Valide l'ensemble du modèle structural
    static ModelValidationReport validate(const TSA::Model::Model& model);

    /// Valide individuellement une section transversale
    static bool validateSection(const TSA::Model::Section& section, std::string* errorMsg = nullptr);

    /// Valide individuellement un matériau d'ingénierie
    static bool validateMaterial(const TSA::Model::Material& material, std::string* errorMsg = nullptr);

    /// Valide individuellement un nœud géométrique 3D
    static bool validateNode(const TSA::Model::Node& node, std::string* errorMsg = nullptr);
};

} // namespace TSA::Standards
