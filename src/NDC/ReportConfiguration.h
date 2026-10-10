#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <vector>

namespace TSA::NDC
{

/**
 * @brief Format du papier pour l'export PDF.
 */
enum class PageFormat
{
    A4,
    A3
};

/**
 * @brief Orientation de page pour l'export PDF.
 */
enum class PageOrientation
{
    Portrait,
    Landscape
};

/**
 * @brief Configuration complète et personnalisable de la Note de Calcul (NDC).
 * Gère les métadonnées de projet, la charte graphique, les options d'affichage 3D
 * et la sélection granulaire des sections/chapitres.
 * Sérialisable en format structuré `.tsareport` (JSON).
 */
struct ReportConfiguration
{
    // =========================================================================
    // 1. INFORMATIONS DU PROJET & DU BUREAU D'ÉTUDES
    // =========================================================================
    QString projectTitle = "Projet de Structure TSA";
    QString projectDescription = "Note de calcul justificative de dimensionnement et d'analyse structurale";
    QString projectNumber = "PRJ-2026-001";
    QString documentNumber = "NDC-STR-01";
    QString revision = "Rev 0";
    QString documentStatus = "Bon Pour Exécution (BPE)"; // Ex: "Projet d'Exécution", "Avant-Projet"
    
    QString engineerName = "Ingénieur Structure";
    QString organization = "Bureau d'Études Techniques";
    QString clientName = "Maître d'Ouvrage";
    QString organizationAddress = "";
    QString contactEmail = "";
    QString contactPhone = "";
    QString emissionDate = ""; // Si vide, date du jour courante

    // =========================================================================
    // 2. IDENTITÉ VISUELLE & CHARTE GRAPHIQUE
    // =========================================================================
    bool showTsaLogo = true;
    QString customLogoPath = "";       ///< Logo personnalisé du client / bureau
    QString secondaryLogoPath = "";    ///< Logo secondaire (partenaire, label)
    QString primaryColor = "#1a56db";  ///< Couleur d'accentuation (bleu technique TSA)
    QString fontFamily = "Segoe UI, Helvetica, Arial, sans-serif";
    int baseFontSizePt = 11;
    
    PageFormat pageFormat = PageFormat::A4;
    PageOrientation pageOrientation = PageOrientation::Portrait;
    double marginMmLeft = 15.0;
    double marginMmRight = 15.0;
    double marginMmTop = 15.0;
    double marginMmBottom = 15.0;

    bool enableHeader = true;
    QString customHeaderText = "TSA — Note de Calcul Structurale";
    bool enableFooter = true;
    bool enablePagination = true;      ///< Format "Page X sur Y"
    /// Template de mise en page (schéma « tsa-ndc/1 », docs/TEMPLATES.md) ; introuvable ou invalide : générateur historique.
    QString templateId = "tsa.ndc.standard";

    // =========================================================================
    // 3. SELECTION GRANULAIRE DES SECTIONS ET CONTENUS
    // =========================================================================
    bool includeCoverPage = true;
    bool includeToc = true;                        ///< Table des matières
    bool includeLof = true;                        ///< Liste des figures
    bool includeLot = true;                        ///< Liste des tableaux
    bool includeIntroduction = true;              ///< Objet de l'étude & hypothèses
    bool includeStandards = true;                 ///< Normes détectées
    bool includeModelGeometry = true;             ///< Géométrie globale, dimensions, niveaux
    bool includeMaterials = true;                 ///< Tableau des matériaux
    bool includeSections = true;                  ///< Tableau des profilés & sections
    bool includeBoundaryConditions = true;        ///< Appuis et liaisons
    bool includeLoadsAndCombinations = true;      ///< Charges et combinaisons ELU/ELS
    bool includeCalculationMethod = true;         ///< Méthode de calcul EF
    bool includeModelVerification = true;         ///< Contrôle du modèle (connectivité, équilibre)
    bool include3DModelSnapshots = true;          ///< Vues 3D du modèle global
    bool includeBendingMoment = true;             ///< Moments Mz, My
    bool includeShearForce = true;                ///< Efforts tranchants Vz, Vy
    bool includeAxialForce = true;                ///< Effort normal N
    bool includeTorsion = true;                   ///< Torsion Mx
    bool includeDisplacements = true;             ///< Déplacements nodaux Ux, Uy, Uz
    bool includeDeflections = true;               ///< Flèches de travée et limites ELS (L/250)
    bool includeReactions = true;                 ///< Réactions d'appuis
    bool includeMostStressedSummary = true;       ///< Synthèse de l'élément le plus sollicité
    bool includeExtremaSpatialTable = true;       ///< Localisation exacte (x, X, Y, Z)
    bool includeEnvelopes = true;                 ///< Enveloppes min / max / absmax
    bool includeDetailedElementTables = true;     ///< Tableaux barres par barres
    bool includeEurocodeDesignChecks = true;      ///< Vérifications normatives EC2 / EC3
    bool includeWarningsAndLimitations = true;    ///< Avertissements et limitations
    bool includeConclusion = true;                ///< Conclusion factuelle
    bool includeBibliography = true;              ///< Webographie et bibliographie vérifiée

    // =========================================================================
    // 4. CONFIGURATION DES CAPTURES 3D ET RÉSULTATS
    // =========================================================================
    int snapshotWidth = 1920;
    int snapshotHeight = 1080;
    double deformationScaleFactor = 50.0;         ///< Facteur graphique déformée (valeur réelle conservée)
    bool show3DNodes = true;
    bool show3DLoads = true;
    bool show3DAxes = true;

    // =========================================================================
    // SÉRIALISATION & PERSISTANCE (.tsareport)
    // =========================================================================
    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    bool saveToFile(const QString& filePath, QString* error = nullptr) const;
    bool loadFromFile(const QString& filePath, QString* error = nullptr);
};

} // namespace TSA::NDC
