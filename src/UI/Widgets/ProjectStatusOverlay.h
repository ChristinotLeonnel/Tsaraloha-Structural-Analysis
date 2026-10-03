#pragma once

#include <QWidget>
#include <QImage>
#include <memory>

namespace TSA::Model {
class Model;
}

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
 * @brief Overlay HUD professionnel "ÉTAT DU PROJET" et Logo TSA
 * s'affichant en incrustation sur le viewport 3D conformément aux standards CAO.
 */
class ProjectStatusOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectStatusOverlay(OccView* occView, QWidget* parent = nullptr);
    ~ProjectStatusOverlay() override = default;

    void setModel(const TSA::Model::Model* model);
    void setGridManager(const TSA::Grid::GridManager* gridManager);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
    void setProjectInfo(const QString& projectName, const QString& filePath);

    void refreshStatus();
    void setDarkMode(bool dark);

    bool isCollapsed() const { return m_isCollapsed; }
    void setCollapsed(bool collapsed);

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
    const TSA::Model::Model* m_model = nullptr;
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
