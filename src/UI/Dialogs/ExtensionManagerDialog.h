#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <vector>
#include <string>

#include "../../ExtensionSystem/ExtensionTypes.h"
#include "../../ExtensionSystem/DefinitionModels.h"

namespace TSA::UI
{

/**
 * @brief Dialogue moderne de gestion des bibliothèques et extensions TSALib.
 * Permet l'exploration par catégories, la recherche en temps réel, la consultation
 * des fiches techniques normatives, le rechargement à chaud (Hot-Reload) en 1 clic,
 * la validation d'intégrité et la télémétrie du cache.
 */
class ExtensionManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExtensionManagerDialog(QWidget* parent = nullptr);
    ~ExtensionManagerDialog() override = default;

    /**
     * @brief Sélectionne une catégorie spécifique dans l'arborescence.
     * @param categoryName Nom de la catégorie (ex: "Materials", "Sections", "Cables", "Textures", "Extensions")
     */
    void selectCategory(const QString& categoryName);

    /**
     * @brief Retourne le nombre d'éléments actuellement affichés dans la table.
     */
    int displayedItemCount() const;

    /**
     * @brief Filtre programmatiquement par texte de recherche.
     */
    void setSearchQuery(const QString& query);

signals:
    void extensionsReloaded();

public slots:
    void onNewExtension();
    void onReloadAll(bool showMessage = true);
    void onValidateAll(bool showMessage = true);
    void onImportExtension();
    void onExportExtension();
    void onOpenExtensionsFolder();

private slots:
    void onCategoryTreeSelectionChanged();
    void onItemTableSelectionChanged();
    void onSearchTextChanged(const QString& text);

private:
    void setupUi();
    void populateCategoryTree();
    void refreshCurrentCategory();
    void updateDetailView(int row);
    void updateTelemetryBar();

    void populateExtensionsList(const QString& filter);
    void populateMaterialsList(const QString& subCategory, const QString& filter);
    void populateSectionsList(const QString& subCategory, const QString& filter);
    void populateCablesList(const QString& subCategory, const QString& filter);
    void populateTexturesList(const QString& filter);
    void populateStandardsList(const QString& filter);

    QString formatMaterialHtml(const TSA::ExtensionSystem::MaterialDefinition& mat) const;
    QString formatSectionHtml(const TSA::ExtensionSystem::SectionDefinition& sec) const;
    QString formatCableHtml(const TSA::ExtensionSystem::CableCatalogDefinition& cab) const;
    QString formatExtensionHtml(const TSA::ExtensionSystem::ExtensionManifest& man) const;

private:
    // Widgets d'en-tête
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnNewExtension = nullptr;
    QPushButton* m_btnReloadAll = nullptr;
    QPushButton* m_btnValidateAll = nullptr;
    QPushButton* m_btnImport = nullptr;
    QPushButton* m_btnExport = nullptr;
    QPushButton* m_btnOpenFolder = nullptr;

    // Volet central Master-Detail
    QSplitter* m_mainSplitter = nullptr;
    QTreeWidget* m_categoryTree = nullptr;
    QTableWidget* m_itemsTable = nullptr;
    QTextBrowser* m_detailBrowser = nullptr;

    // Pied de page & Télémétrie
    QLabel* m_statusLabel = nullptr;
    QLabel* m_telemetryLabel = nullptr;
    QPushButton* m_btnClose = nullptr;

    // Données actuellement affichées dans la table
    enum class CurrentViewType
    {
        Extensions,
        Materials,
        Sections,
        Cables,
        Textures,
        Standards
    };
    CurrentViewType m_currentView = CurrentViewType::Materials;
    QString m_currentSubCategory;

    std::vector<TSA::ExtensionSystem::MaterialDefinition> m_currentMaterials;
    std::vector<TSA::ExtensionSystem::SectionDefinition> m_currentSections;
    std::vector<TSA::ExtensionSystem::CableCatalogDefinition> m_currentCables;
    std::vector<TSA::ExtensionSystem::ExtensionManifest> m_currentExtensions;
};

} // namespace TSA::UI
