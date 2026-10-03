#pragma once

#include <QWidget>
#include <QImage>
#include <memory>

#include "../../Model/Model.h"

namespace TSA::Grid {
class GridManager;
}

namespace TSA::Analysis {
class ResultsModel;
}

class OccView;
class QLabel;
class QPushButton;
class QGridLayout;

namespace TSA::UI {

/**
 * @brief Widget miniature vectoriel rendant le modèle réel en temps réel
 * et suivant l'orientation de la caméra du viewport sans latence.
 */
class ModelMinimapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModelMinimapWidget(OccView* occView, QWidget* parent = nullptr);
    ~ModelMinimapWidget() override = default;

    void setModel(const TSA::Model::Model* model);
    void updateView();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    OccView* m_occView = nullptr;
    const TSA::Model::Model* m_model = nullptr;
};

/**
 * @brief Panneau CAD "ÉTAT DU PROJET" professionnel
 * observant directement le modèle structural unique source de vérité.
 */
class ProjectStatusOverlay : public QWidget, public TSA::Model::IModelObserver
{
    Q_OBJECT

public:
    explicit ProjectStatusOverlay(OccView* occView, QWidget* parent = nullptr);
    ~ProjectStatusOverlay() override;

    void setModel(TSA::Model::Model* model);
    void setGridManager(const TSA::Grid::GridManager* gridManager);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
    void setProjectInfo(const QString& projectName, const QString& filePath);

    void refreshStatus();
    void setDarkMode(bool dark);

    bool isCollapsed() const { return m_isCollapsed; }
    void setCollapsed(bool collapsed);

    // TSA::Model::IModelObserver implementation
    void onNodeAdded(const TSA::Model::Node&) override { refreshStatus(); }
    void onNodeModified(const TSA::Model::Node&) override { refreshStatus(); }
    void onNodeRemoved(int) override { refreshStatus(); }
    void onBeamAdded(const TSA::Model::Beam&) override { refreshStatus(); }
    void onBeamModified(const TSA::Model::Beam&) override { refreshStatus(); }
    void onBeamRemoved(int) override { refreshStatus(); }
    void onColumnAdded(const TSA::Model::Column&) override { refreshStatus(); }
    void onColumnModified(const TSA::Model::Column&) override { refreshStatus(); }
    void onColumnRemoved(int) override { refreshStatus(); }
    void onSlabAdded(const TSA::Model::Slab&) override { refreshStatus(); }
    void onSlabModified(const TSA::Model::Slab&) override { refreshStatus(); }
    void onSlabRemoved(int) override { refreshStatus(); }
    void onWallAdded(const TSA::Model::Wall&) override { refreshStatus(); }
    void onWallModified(const TSA::Model::Wall&) override { refreshStatus(); }
    void onWallRemoved(int) override { refreshStatus(); }
    void onFoundationAdded(const TSA::Model::Foundation&) override { refreshStatus(); }
    void onFoundationModified(const TSA::Model::Foundation&) override { refreshStatus(); }
    void onFoundationRemoved(int) override { refreshStatus(); }
    void onCableAdded(const TSA::Model::Cable&) override { refreshStatus(); }
    void onCableModified(const TSA::Model::Cable&) override { refreshStatus(); }
    void onCableRemoved(int) override { refreshStatus(); }
    void onTrussMemberAdded(const TSA::Model::TrussMember&) override { refreshStatus(); }
    void onTrussMemberModified(const TSA::Model::TrussMember&) override { refreshStatus(); }
    void onTrussMemberRemoved(int) override { refreshStatus(); }
    void onModelCleared() override { refreshStatus(); }

signals:
    void overlayToggled(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onToggleCollapse();

private:
    void setupUi();
    void updateTheme();

private:
    OccView* m_occView = nullptr;
    TSA::Model::Model* m_model = nullptr;
    const TSA::Grid::GridManager* m_gridManager = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_resultsModel;

    QString m_projectName;
    QString m_filePath;
    bool m_isDarkMode = true;
    bool m_isCollapsed = false;

    // UI Widgets
    QWidget* m_cardWidget = nullptr;
    QLabel* m_lblTitle = nullptr;
    QLabel* m_lblProjectName = nullptr;
    QPushButton* m_btnCollapse = nullptr;

    QLabel* m_lblNodes = nullptr;
    QLabel* m_lblBeams = nullptr;
    QLabel* m_lblColumns = nullptr;
    QLabel* m_lblSlabs = nullptr;
    QLabel* m_lblWalls = nullptr;
    QLabel* m_lblFoundations = nullptr;
    QLabel* m_lblLevels = nullptr;
    QLabel* m_lblGrids = nullptr;
    QLabel* m_lblLoads = nullptr;
    QLabel* m_lblCombos = nullptr;
    QLabel* m_lblCalculation = nullptr;

    ModelMinimapWidget* m_minimap = nullptr;
    QWidget* m_statsContainer = nullptr;
};

/**
 * @brief Logo officiel TSA transparent aux clics de souris,
 * ancré au coin inférieur droit du viewport.
 */
class TSALogoOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit TSALogoOverlay(QWidget* parent = nullptr);
    ~TSALogoOverlay() override = default;

    void setDarkMode(bool dark);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_isDarkMode = true;
};

} // namespace TSA::UI
