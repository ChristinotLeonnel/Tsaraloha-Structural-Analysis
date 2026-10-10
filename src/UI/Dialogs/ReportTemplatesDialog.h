#pragma once

// Documents par templates (docs/TEMPLATES.md) : choix du template et du rapport, options (titre, logo,
// couleur, en-tête, pied de page…), aperçu, diagnostics (données manquantes), export HTML / PDF, et
// gestion du dépôt (origine, modifié, import, export, personnalisation, édition contrôlée, suppression).
// Le dialogue ne connaît ni le modèle ni la mise en page : l'hôte fournit les données par schéma.

#include <QDialog>
#include <QJsonObject>

#include <functional>
#include <map>

class QTreeWidget;
class QTextBrowser;
class QPlainTextEdit;
class QFormLayout;
class QPushButton;
class QLabel;
class QComboBox;
class QWidget;

namespace TSA::UI
{

class ReportTemplatesDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param dataProvider données pour un schéma (« tsa-report-data/1 », « tsa-ndc/1 ») ; objet vide si indisponible
    ReportTemplatesDialog(std::function<QJsonObject(const QString& schema)> dataProvider, QWidget* parent = nullptr);

private:
    void populate(const QString& selectKey = {}, const QString& selectReport = {});
    void onSelectionChanged();
    void rebuildOptions();
    QJsonObject optionValues() const;
    void preview();
    QString currentKey() const;
    QString currentReport() const;
    QString renderCurrent(bool showErrors);
    void exportDocument(bool pdf);
    void importTemplate();
    void exportTemplate();
    void customize();
    void editFile();
    void removeTemplate();

    std::function<QJsonObject(const QString&)> m_dataProvider;
    std::map<QString, QJsonObject> m_dataCache;
    QTreeWidget* m_tree = nullptr;
    QWidget* m_optionsBox = nullptr;
    QFormLayout* m_optionsForm = nullptr;
    std::map<QString, QWidget*> m_optionWidgets;
    std::map<QString, QString> m_imageValues;
    QTextBrowser* m_preview = nullptr;
    QPlainTextEdit* m_diagnostics = nullptr;
    QLabel* m_info = nullptr;
    QComboBox* m_pageSize = nullptr;
    QPushButton* m_customize = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_remove = nullptr;
    QString m_lastHtml;
};

} // namespace TSA::UI
