#pragma once

#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QStackedLayout>
#include <memory>
#include <array>

#include "PortTypes.h"
#include "PortWidget.h"

class OccView;

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
class NDCViewerWidget;
}

namespace TSA::UI
{

class Diagram2DWidget;
class ViewportContainer;

/**
 * @brief Gestionnaire de l'espace de travail Multi-Port (Multi-Viewport) de TSA.
 * Remplace l'affichage unique par une disposition flexible configurable (Vue unique, Double horizontale,
 * Double verticale, Grille 2x2, Onglets) reliant le modèle 3D, les résultats 3D, les diagrammes 2D et la Note de Calcul.
 */
class PortAreaWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PortAreaWidget(OccView* primaryOccView, ViewportContainer* primaryContainer, QWidget* parent = nullptr);
    ~PortAreaWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    PortLayout layoutMode() const { return m_layoutMode; }
    void setLayoutMode(PortLayout mode);

    bool syncCameras() const { return m_syncCameras; }
    void setSyncCameras(bool sync);

    PortWidget* port(int index) const;
    PortWidget* activePort() const;
    int activePortIndex() const { return m_activePortIndex; }
    void setActivePort(int index);

    void toggleMaximizePort(int index);

    OccView* primaryOccView() const { return m_primaryOccView; }
    Diagram2DWidget* diagramWidget() const { return m_diagramWidget; }
    TSA::NDC::NDCViewerWidget* ndcWidget() const { return m_ndcWidget; }

signals:
    void layoutModeChanged(PortLayout mode);
    void activePortChanged(int index, PortType type);
    void cameraSyncChanged(bool enabled);

private slots:
    void onPortTypeChanged(int portId, PortType newType);
    void onPortMaximizeRequested(int portId);
    void onPortActivated(int portId);
    void onPrimaryCameraChanged();

private:
    void setupUi();
    void applyLayout();
    void attachContentToPort(PortWidget* port, PortType type);

private:
    OccView* m_primaryOccView = nullptr;
    ViewportContainer* m_primaryContainer = nullptr;
    Diagram2DWidget* m_diagramWidget = nullptr;
    TSA::NDC::NDCViewerWidget* m_ndcWidget = nullptr;

    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    PortLayout m_layoutMode = PortLayout::Single;
    int m_activePortIndex = 0;
    int m_maximizedPortIndex = -1;
    bool m_syncCameras = false;

    std::array<PortWidget*, 4> m_ports{};

    // Conteneurs de disposition
    QVBoxLayout* m_rootLayout = nullptr;
    QWidget* m_containerWidget = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_subSplitterLeft = nullptr;
    QSplitter* m_subSplitterRight = nullptr;
    QTabWidget* m_tabWidget = nullptr;
};

} // namespace TSA::UI
