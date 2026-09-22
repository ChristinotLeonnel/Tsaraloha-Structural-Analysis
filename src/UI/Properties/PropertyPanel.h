#pragma once

#include <QWidget>
#include "../../Model/Model.h"

class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QFormLayout;
class QGroupBox;

namespace TSA::UI
{

class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(TSA::Model::Model* model, QWidget* parent = nullptr);
    ~PropertyPanel() override = default;

public slots:
    void showNodeProperties(int nodeId);
    void showBeamProperties(int beamId);
    void showColumnProperties(int columnId);
    void showSlabProperties(int slabId);
    void clearProperties();

private slots:
    void onNodeCoordinatesChanged();
    void onBeamDimensionsChanged();
    void onColumnDimensionsChanged();
    void onSlabPropertiesChanged();

private:
    void setupUi();

private:
    TSA::Model::Model* m_model = nullptr;

    int m_currentNodeId = -1;
    int m_currentBeamId = -1;
    int m_currentColumnId = -1;
    int m_currentSlabId = -1;
    bool m_isUpdating = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;

    // Panneau Nœud
    QGroupBox* m_nodeGroup = nullptr;
    QLabel* m_nodeIdLabel = nullptr;
    QDoubleSpinBox* m_nodeXSpin = nullptr;
    QDoubleSpinBox* m_nodeYSpin = nullptr;
    QDoubleSpinBox* m_nodeZSpin = nullptr;

    // Panneau Poutre
    QGroupBox* m_beamGroup = nullptr;
    QLabel* m_beamIdLabel = nullptr;
    QLabel* m_beamStartNodeLabel = nullptr;
    QLabel* m_beamEndNodeLabel = nullptr;
    QLabel* m_beamLengthLabel = nullptr;
    QDoubleSpinBox* m_beamWidthSpin = nullptr;
    QDoubleSpinBox* m_beamHeightSpin = nullptr;

    // Panneau Poteau
    QGroupBox* m_columnGroup = nullptr;
    QLabel* m_columnIdLabel = nullptr;
    QLabel* m_columnStartNodeLabel = nullptr;
    QLabel* m_columnEndNodeLabel = nullptr;
    QLabel* m_columnHeightLabel = nullptr;
    QDoubleSpinBox* m_columnWidthSpin = nullptr;
    QDoubleSpinBox* m_columnDepthSpin = nullptr;

    // Panneau Dalle
    QGroupBox* m_slabGroup = nullptr;
    QLabel* m_slabIdLabel = nullptr;
    QLabel* m_slabNodesLabel = nullptr;
    QLabel* m_slabAreaLabel = nullptr;
    QDoubleSpinBox* m_slabThicknessSpin = nullptr;
};

} // namespace TSA::UI
