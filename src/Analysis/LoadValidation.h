#pragma once

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{

enum class ValidationSeverity
{
    Info,
    Warning,
    Error
};

struct ValidationMessage
{
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string category;
    std::string message;
};

class ValidationReport
{
public:
    ValidationReport() = default;

    void addInfo(const std::string& category, const std::string& msg)
    {
        m_messages.push_back({ ValidationSeverity::Info, category, msg });
    }

    void addWarning(const std::string& category, const std::string& msg)
    {
        m_messages.push_back({ ValidationSeverity::Warning, category, msg });
    }

    void addError(const std::string& category, const std::string& msg)
    {
        m_messages.push_back({ ValidationSeverity::Error, category, msg });
        m_hasError = true;
    }

    bool isValid() const noexcept { return !m_hasError; }
    bool hasErrors() const noexcept { return m_hasError; }
    bool hasWarnings() const noexcept;

    const std::vector<ValidationMessage>& messages() const noexcept { return m_messages; }

    std::vector<std::string> errors() const;
    std::vector<std::string> warnings() const;

    std::string summary() const;

private:
    std::vector<ValidationMessage> m_messages;
    bool m_hasError = false;
};

/**
 * @brief Moteur de validation préalable avant exécution ou export OpenSees.
 */
class LoadValidation
{
public:
    static ValidationReport validateModel(const TSA::Model::Model& model);
    static ValidationReport validate(const TSA::Model::Model& model) { return validateModel(model); }
};

} // namespace TSA::Analysis
