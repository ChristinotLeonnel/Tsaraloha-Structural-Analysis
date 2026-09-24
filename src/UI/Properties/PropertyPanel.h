#pragma once

#include <QWidget>
#include <QScrollArea>
#include "../../Model/Model.h"

class QLabel;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QRadioButton;
class QGroupBox;
class QPushButton;

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
    void showWallProperties(int wallId);
    void showFoundationProperties(int foundationId);
    void showTrussMemberProperties(int memberId);
    void clearProperties();

signals:
    void elementModified();

private slots:
    void onApplyNode();
    void onApplyBeam();
    void onApplyColumn();
    void onApplySlab();
    void onApplyWall();
    void onApplyFoundation();
    void onApplyTruss();

private:
    void setupUi();
    void hideAllGroups();
    void setupMaterialCombo(QComboBox* combo);
    void setupSectionTypeCombo(QComboBox* combo);
    void setupColorButton(QPushButton* btn, const QString& hexColor);
    void pickColor(QString& targetColor, QPushButton* targetBtn, const QString& title);
    void updateBeamSectionVisibility(int secData);
    void updateColumnSectionVisibility(int secData);

private:
    TSA::Model::Model* m_model = nullptr;

    enum class CurrentType { None, Level, Node, Beam, Column, Slab, Wall, Foundation, Truss };
    CurrentType m_currentType = CurrentType::None;

    QString m_currentLevelId;
    int m_currentNodeId = -1;
    int m_currentBeamId = -1;
    int m_currentColumnId = -1;
    int m_currentSlabId = -1;
    int m_currentWallId = -1;
    int m_currentFoundationId = -1;
    int m_currentTrussId = -1;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;

    // Panneau Nœud
    QGroupBox* m_nodeGroup = nullptr;
    QLineEdit* m_nodeNameEdit = nullptr;
    QLabel* m_nodeIdLabel = nullptr;
    QLabel* m_nodeLevelLabel = nullptr;
    QDoubleSpinBox* m_nodeXSpin = nullptr;
    QDoubleSpinBox* m_nodeYSpin = nullptr;
    QDoubleSpinBox* m_nodeZSpin = nullptr;
    QComboBox* m_nodeSupportCombo = nullptr;
    QPushButton* m_nodeColorBtn = nullptr;
    QString m_nodeColor;

    // Panneau Poutre
    QGroupBox* m_beamGroup = nullptr;
    QLineEdit* m_beamNameEdit = nullptr;
    QLabel* m_beamIdLabel = nullptr;
    QLabel* m_beamStartNodeLabel = nullptr;
    QLabel* m_beamEndNodeLabel = nullptr;
    QLabel* m_beamLengthLabel = nullptr;
    QComboBox* m_beamSectionTypeCombo = nullptr;
    QLabel* m_beamWidthLabel = nullptr;
    QDoubleSpinBox* m_beamWidthSpin = nullptr;
    QLabel* m_beamHeightLabel = nullptr;
    QDoubleSpinBox* m_beamHeightSpin = nullptr;
    QComboBox* m_beamMaterialCombo = nullptr;
    QDoubleSpinBox* m_beamRotationSpin = nullptr;
    QPushButton* m_beamColorBtn = nullptr;
    QString m_beamColor;

    // Panneau Poteau
    QGroupBox* m_columnGroup = nullptr;
    QLineEdit* m_columnNameEdit = nullptr;
    QLabel* m_columnIdLabel = nullptr;
    QLabel* m_columnStartNodeLabel = nullptr;
    QLabel* m_columnEndNodeLabel = nullptr;
    QLabel* m_columnHeightLabel = nullptr;
    QComboBox* m_columnSectionTypeCombo = nullptr;
    QLabel* m_columnWidthLabel = nullptr;
    QDoubleSpinBox* m_columnWidthSpin = nullptr;
    QLabel* m_columnDepthLabel = nullptr;
    QDoubleSpinBox* m_columnDepthSpin = nullptr;
    QComboBox* m_columnMaterialCombo = nullptr;
    QDoubleSpinBox* m_columnRotationSpin = nullptr;
    QPushButton* m_columnColorBtn = nullptr;
    QString m_columnColor;

    // Panneau Dalle
    QGroupBox* m_slabGroup = nullptr;
    QLineEdit* m_slabNameEdit = nullptr;
    QLabel* m_slabIdLabel = nullptr;
    QLabel* m_slabNodesLabel = nullptr;
    QLabel* m_slabAreaLabel = nullptr;
    QDoubleSpinBox* m_slabThicknessSpin = nullptr;
    QComboBox* m_slabMaterialCombo = nullptr;
    QRadioButton* m_slabRadioOneWay = nullptr;
    QRadioButton* m_slabRadioTwoWay = nullptr;
    QRadioButton* m_slabRadioFlat = nullptr;
    QPushButton* m_slabColorBtn = nullptr;
    QString m_slabColor;

    // Panneau Voile (Wall)
    QGroupBox* m_wallGroup = nullptr;
    QLineEdit* m_wallNameEdit = nullptr;
    QLabel* m_wallIdLabel = nullptr;
    QLabel* m_wallStartNodeLabel = nullptr;
    QLabel* m_wallEndNodeLabel = nullptr;
    QLabel* m_wallLengthLabel = nullptr;
    QDoubleSpinBox* m_wallHeightSpin = nullptr;
    QDoubleSpinBox* m_wallThicknessSpin = nullptr;
    QComboBox* m_wallMaterialCombo = nullptr;
    QDoubleSpinBox* m_wallOffsetSpin = nullptr;
    QPushButton* m_wallColorBtn = nullptr;
    QString m_wallColor;

    // Panneau Fondation
    QGroupBox* m_foundationGroup = nullptr;
    QLineEdit* m_foundationNameEdit = nullptr;
    QLabel* m_foundationIdLabel = nullptr;
    QLabel* m_foundationNodeLabel = nullptr;
    QComboBox* m_foundationTypeCombo = nullptr;
    QDoubleSpinBox* m_foundationWidthASpin = nullptr;
    QDoubleSpinBox* m_foundationLengthBSpin = nullptr;
    QDoubleSpinBox* m_foundationHeightHSpin = nullptr;
    QComboBox* m_foundationMaterialCombo = nullptr;
    QDoubleSpinBox* m_foundationSoilCapacitySpin = nullptr;
    QPushButton* m_foundationColorBtn = nullptr;
    QString m_foundationColor;

    // Panneau Treillis / Contreventement
    QGroupBox* m_trussGroup = nullptr;
    QLineEdit* m_trussNameEdit = nullptr;
    QLabel* m_trussIdLabel = nullptr;
    QLabel* m_trussStartNodeLabel = nullptr;
    QLabel* m_trussEndNodeLabel = nullptr;
    QLabel* m_trussLengthLabel = nullptr;
    QComboBox* m_trussRoleCombo = nullptr;
    QDoubleSpinBox* m_trussDimensionSpin = nullptr;
    QComboBox* m_trussMaterialCombo = nullptr;
    QPushButton* m_trussColorBtn = nullptr;
    QString m_trussColor;
};

} // namespace TSA::UI
