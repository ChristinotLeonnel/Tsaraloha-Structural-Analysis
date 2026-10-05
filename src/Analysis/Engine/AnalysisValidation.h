#pragma once

// Résultat de validation commun à tous les moteurs d'analyse (contrôles génériques du modèle
// d'analyse + contrôles propres au moteur). Indépendant de Qt Widgets et du solveur.

#include <string>
#include <vector>

namespace TSA::Analysis
{

enum class ValidationSeverity
{
    Info,      ///< contrôle réussi / information (« ✓ Nœuds valides »)
    Warning,   ///< calcul possible, mais l'utilisateur doit en être averti
    Error      ///< calcul impossible
};

struct ValidationMessage
{
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string category;   ///< « Portée », « Éléments », « Charges », « Moteur »…
    std::string text;       ///< message explicite destiné à l'utilisateur
};

class ValidationResult
{
public:
    void addInfo(const std::string& category, const std::string& text) { add(ValidationSeverity::Info, category, text); }
    void addWarning(const std::string& category, const std::string& text) { add(ValidationSeverity::Warning, category, text); }
    void addError(const std::string& category, const std::string& text) { add(ValidationSeverity::Error, category, text); }
    void add(ValidationSeverity s, const std::string& category, const std::string& text)
    {
        m_messages.push_back({ s, category, text });
    }
    void merge(const ValidationResult& other)
    {
        m_messages.insert(m_messages.end(), other.m_messages.begin(), other.m_messages.end());
    }

    const std::vector<ValidationMessage>& messages() const { return m_messages; }
    bool isValid() const { return count(ValidationSeverity::Error) == 0; }
    bool hasWarnings() const { return count(ValidationSeverity::Warning) > 0; }
    std::size_t count(ValidationSeverity s) const
    {
        std::size_t n = 0;
        for (const auto& m : m_messages) n += (m.severity == s) ? 1 : 0;
        return n;
    }
    /// Messages d'une gravité donnée, préfixés de leur catégorie.
    std::vector<std::string> texts(ValidationSeverity s) const
    {
        std::vector<std::string> out;
        for (const auto& m : m_messages)
            if (m.severity == s) out.push_back(m.category.empty() ? m.text : m.category + " : " + m.text);
        return out;
    }

private:
    std::vector<ValidationMessage> m_messages;
};

} // namespace TSA::Analysis
