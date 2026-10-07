#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <memory>

#include <QUrl>
#include <QUrlQuery>
#include "NDCDocumentModel.h"
#include "ReportManager.h"
#include "../Analysis/AnalysisTypes.h"

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::NDC
{

/**
 * @brief Widget interactif de visualisation et d'exploration de la Note de Calcul (NDC).
 * Comprend un sommaire hiérarchique navigable, une recherche textuelle en temps réel,
 * la personnalisation avancée des templates/chapitres, le contrôle du zoom,
 * la navigation bidirectionnelle avec le modèle 3D, et les exports direct en PDF ou HTML.
 */
class NDCViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NDCViewerWidget(QWidget* parent = nullptr);
    ~NDCViewerWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    /// Régénère immédiatement la note (demande explicite : bouton, menu, configuration).
    void refreshDocument();
    const NDCDocument& document() const { return m_document; }

    ReportManager& reportManager() { return m_reportManager; }
    const ReportManager& reportManager() const { return m_reportManager; }
    const ReportConfiguration& configuration() const { return m_reportManager.configuration(); }
    void setConfiguration(const ReportConfiguration& config);

signals:
    /**
     * @brief Émis lorsque l'utilisateur clique sur un lien hypertexte renvoyant à un élément structural
     * (tsa://element?kind=beam|column|truss|cable&id=N : les identifiants sont uniques par famille).
     */
    void elementSelected(TSA::Analysis::StructuralElementKind kind, int elementId);

private slots:
    void onTocItemClicked(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);
    void onFindNext();
    void onFindPrevious();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    void onExportPdf();
    void onExportHtml();
    void onConfigureClicked();
    void onAnchorClicked(const QUrl& url);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void populateToc();
    /// Le modèle / les résultats / la configuration ont changé : régénération différée.
    /// La génération complète (HTML + QTextBrowser::setHtml) coûtait ~13 s par appel sur un
    /// modèle de 1 400 barres en Debug, et était déclenchée deux fois à chaque ouverture de
    /// projet, même dock NDC fermé. Elle n'a désormais lieu que si le widget est visible, une
    /// seule fois par rafale de changements, ou à son prochain affichage.
    void requestRefresh();

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;
    NDCDocument m_document;
    ReportManager m_reportManager;
    bool m_documentDirty = true;
    bool m_refreshPending = false;

    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_tocTree = nullptr;
    QTextBrowser* m_browser = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnFindPrev = nullptr;
    QPushButton* m_btnFindNext = nullptr;
    QPushButton* m_btnZoomIn = nullptr;
    QPushButton* m_btnZoomOut = nullptr;
    QPushButton* m_btnConfigure = nullptr;
    QPushButton* m_btnExportPdf = nullptr;
    QPushButton* m_btnExportHtml = nullptr;
    QPushButton* m_btnRefresh = nullptr;
};

} // namespace TSA::NDC

