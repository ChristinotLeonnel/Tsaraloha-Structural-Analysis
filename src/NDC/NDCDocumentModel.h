#pragma once

#include <QString>
#include "App/ProductInfo.h"
#include <QStringList>
#include <vector>
#include <memory>
#include <utility>
#include "ReportConfiguration.h"

namespace TSA::NDC
{

/**
 * @brief Figure ou capture graphique intégrée dans la note de calcul.
 * Représente les diagrammes 3D, courbes d'efforts, déformées ou vues d'ensemble.
 */
struct NDCFigure
{
    int number = 1;                         ///< Numéro de figure (Figure 1, Figure 2...)
    QString caption;                        ///< Légende explicite
    QString imageBase64;                    ///< Données de l'image (data:image/png;base64,... ou SVG)
    QString localFilePath;                  ///< Chemin local vers l'image (si non embarquée)
    QString viewName;                       ///< Ex: "Vue Isométrique 3D", "Élévation XZ"
    QString elementRef;                     ///< Référence de l'élément représenté (ex: "Poutre B125")
    QString loadCaseOrCombo;                ///< Combinaison ou cas de charge représenté (ex: "ELU-01")
    int widthPercent = 90;                  ///< Largeur d'affichage en % (défaut: 90%)
};

/**
 * @brief Tableau de données ou de résultats avec mise en forme typographique.
 */
struct NDCTable
{
    int number = 1;                         ///< Numéro de tableau (Tableau 1, Tableau 2...)
    QString caption;                        ///< Titre / légende du tableau
    QStringList headers;                    ///< En-têtes de colonnes
    std::vector<QString> columnAlignments;  ///< Alignements : "left", "center", "right"
    std::vector<QStringList> rows;          ///< Lignes de données
};

/**
 * @brief Section technique composant un chapitre de la note.
 */
struct NDCSection
{
    QString title;                                      ///< Titre de la sous-section
    QStringList paragraphs;                             ///< Paragraphes de texte justificatif
    std::vector<std::pair<QString, QString>> keyValues; ///< Paires clé-valeur (données tabulées)
    std::vector<NDCTable> tables;                       ///< Tableaux inclus
    std::vector<NDCFigure> figures;                     ///< Figures / captures 3D incluses
};

/**
 * @brief Chapitre numéroté de la Note de Calcul.
 */
struct NDCChapter
{
    int number = 1;                         ///< Numéro du chapitre
    QString title;                          ///< Titre du chapitre
    std::vector<NDCSection> sections;       ///< Sous-sections
};

/**
 * @brief Modèle en mémoire complet de la Note de Calcul (NDC).
 * Génère le document sous forme de code HTML/CSS prêt pour l'affichage interactif
 * et pour l'impression / export PDF vectoriel haute définition (IEEE Std 1063 & Eurocodes).
 */
class NDCDocument
{
public:
    NDCDocument();

    ReportConfiguration config;
    QString softwareVersion = QStringLiteral("%1 v%2 (Moteur EF : OpenSees v3.8.0)").arg(TSA::Product::name(), TSA::Product::kReportVersion);
    QString standardReference = "Eurocodes (EN 1990, EN 1991, EN 1992, EN 1993, EN 1998)";

    std::vector<NDCChapter> chapters;

    void addChapter(const NDCChapter& chapter) { chapters.push_back(chapter); }
    void clear() { chapters.clear(); }

    [[nodiscard]] std::vector<NDCFigure> allFigures() const;
    [[nodiscard]] std::vector<NDCTable> allTables() const;

    [[nodiscard]] QString toHtml() const;
    [[nodiscard]] QString toPlainText() const;

private:
    static QString getTsaLogoSvg();
};

} // namespace TSA::NDC
