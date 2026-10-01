#pragma once

#include <string>
#include <vector>
#include <optional>
#include <QString>

namespace TSA::Standards
{

/**
 * @brief Fiche descriptive d'une bibliothèque tierce externe utilisée dans TSA.
 * [NORM: ISO/IEC 25010 §4.2.7, ISO/IEC/IEEE 12207 §6.4.3]
 */
struct ExternalLibraryInfo
{
    std::string name;                   ///< Nom officiel (ex: "Qt", "OpenCASCADE", "OpenSees")
    std::string version;                ///< Version réelle compilée ou liée (ex: "6.2+", "8.0.1", "3.8.0")
    std::string license;                ///< Type de licence (ex: "LGPLv3", "LGPLv2.1 with exception", "UC Berkeley")
    std::string purpose;                ///< Rôle et mission dans TSA
    std::string interfaceUsed;          ///< Couche d'adaptation interne (ex: "src/UI/", "src/Viewer/OccView", "src/Analysis/OpenSeesAdapter")
    std::string responsibility;         ///< Périmètre exact de responsabilité
    std::string constraints;            ///< Contraintes d'architecture et interdictions
    std::string compatibilityNotes;     ///< Compatibilité compilateur (MSVC / MinGW / Clang)
    std::string verificationTest;       ///< Test validant la bonne intégration
};

/**
 * @brief Registre centralisé des bibliothèques externes utilisées par TSA.
 */
class ExternalLibraryCatalog
{
public:
    static ExternalLibraryCatalog& instance();

    const std::vector<ExternalLibraryInfo>& allLibraries() const { return m_libraries; }
    std::optional<ExternalLibraryInfo> findByName(const std::string& name) const;

    QString generateDocumentationMarkdown() const;

private:
    ExternalLibraryCatalog();
    void initializeCatalog();

    std::vector<ExternalLibraryInfo> m_libraries;
};

} // namespace TSA::Standards
