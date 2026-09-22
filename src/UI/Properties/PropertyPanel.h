#pragma once

#include <QWidget>
#include "../../Model/Model.h"

class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;
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
    void showLevelProperties(const QString& levelId);
    void showNodeProperties(int nodeId);
    void showBeamProperties(int beamId);
    void showColumnProperties(int columnId);
    void showSlabProperties(int slabId);
    void clearProperties();

private slots:
    void onLevelNameChanged();
    void onLevelElevationChanged();
    void onLevelVisibleChanged(bool checked);
    void onNodeCoordinatesChanged();
    void onBeamDimensionsChanged();
    void onColumnDimensionsChanged();
    void onSlabPropertiesChanged();

private:
    void setupUi();

private:
    TSA::Model::Model* m_model = nullptr;

    QString m_currentLevelId;
    int m_currentNodeId = -1;
    int m_currentBeamId = -1;
    int m_currentColumnId = -1;
    int m_currentSlabId = -1;
    bool m_isUpdating = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;

    // Panneau Niveau / Étage
    QGroupBox* m_levelGroup = nullptr;
    QLabel* m_levelIdLabel = nullptr;
    QLineEdit* m_levelNameEdit = nullptr;
    QDoubleSpinBox* m_levelElevationSpin = nullptr;
    QCheckBox* m_levelVisibleCheck = nullptr;

    // Panneau Nœud
    QGroupBox* m_nodeGroup = nullptr;
    QLabel* m_nodeIdLabel = nullptr;
    QLabel* m_nodeLevelLabel = nullptr;
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
    QLabel* m_columnVerticalLabel = nullptr;
    QLabel* m_columnElevationRangeLabel = nullptr;
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
