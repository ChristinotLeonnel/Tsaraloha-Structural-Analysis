#pragma once

// Types du domaine liés aux bibliothèques de définitions (TSA Core) : version sémantique, référence versionnée vers une
// définition de bibliothèque et caractéristiques mécaniques figées. Le modèle les stocke et le fichier .tsa les
// enregistre (traçabilité des propriétés issues d'une bibliothèque) ; le système d'extensions les réutilise sous leurs
// anciens noms (ExtensionSystem/ExtensionTypes.h, alias).

#include <optional>
#include <string>

namespace TSA::Model
{

/**
 * @brief Gestionnaire de version sémantique (SemVer 2.0).
 */
struct SemanticVersion
{
    int major = 1;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    SemanticVersion() = default;
    SemanticVersion(int maj, int min, int pat, const std::string& pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre) {}

    static std::optional<SemanticVersion> fromString(const std::string& str);
    std::string toString() const;

    bool operator==(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const { return *this < other || *this == other; }
    bool operator>(const SemanticVersion& other) const { return !(*this <= other); }
    bool operator>=(const SemanticVersion& other) const { return !(*this < other); }
};


/**
 * @brief Référence versionnée vers une définition de bibliothèque.
 * Stockée dans les éléments du modèle structural et les fichiers de projet (.tsa).
 */
struct DefinitionReference
{
    std::string libraryId;          // ex: "org.tsaraloha.tsalib"
    SemanticVersion libraryVersion; // ex: "1.0.0"
    std::string definitionId;       // ex: "concrete.c25_30" ou "steel.ipe200"
    SemanticVersion definitionVersion; // ex: "1.0"

    bool isValid() const { return !libraryId.empty() && !definitionId.empty(); }
    std::string toQualifiedKey() const { return libraryId + ":" + definitionId; }
};


/**
 * @brief Snapshot immuable des caractéristiques mécaniques au moment de l'affectation à un élément.
 * Garantit la reproductibilité absolue des calculs d'un ancien projet même si la bibliothèque externe évolue.
 */
struct MechanicalSnapshot
{
    double youngModulus = 0.0;     // Pa
    double poissonRatio = 0.0;     // sans unité
    double density = 0.0;          // kg/m³
    double characteristicStrength = 0.0; // Pa (fck, fpk, fk)
    double yieldStrength = 0.0;    // Pa (fy)
    double thermalCoeff = 0.0;     // 1/K

    bool operator==(const MechanicalSnapshot& other) const;
    bool operator!=(const MechanicalSnapshot& other) const { return !(*this == other); }
};

} // namespace TSA::Model
