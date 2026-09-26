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
class QCheckBox;

namespace TSA::UI
{

class SectionPreviewWidget;

class PropertyPanel : public QWidget, public TSA::Model::IModelObserver
{
    Q_OBJECT

public:
    explicit PropertyPanel(TSA::Model::Model* model, QWidget* parent = nullptr);
    ~PropertyPanel() override;

    void setModel(TSA::Model::Model* model);

public slots:
    void showLevelProperties(const QString& levelId);
    void showNodeProperties(int nodeId);
    void showBeamProperties(int beamId);
    void showColumnProperties(int columnId);
    void showSlabProperties(int slabId);
    void showWallProperties(int wallId);
    void showFoundationProperties(int foundationId);
    void showTrussMemberProperties(int memberId);
    void showCableProperties(int cableId);
    void clearProperties();
    void refreshLibraryLists();

signals:
    void elementModified();

protected:
    // IModelObserver overrides pour la synchronisation bidirectionnelle 100% temps réel
    void onNodeModified(const TSA::Model::Node& node) override;
    void onNodeRemoved(int nodeId) override;

    void onBeamModified(const TSA::Model::Beam& beam) override;
    void onBeamRemoved(int beamId) override;

    void onColumnModified(const TSA::Model::Column& column) override;
    void onColumnRemoved(int columnId) override;

    void onSlabModified(const TSA::Model::Slab& slab) override;
    void onSlabRemoved(int slabId) override;

    void onWallModified(const TSA::Model::Wall& wall) override;
    void onWallRemoved(int wallId) override;

    void onFoundationModified(const TSA::Model::Foundation& foundation) override;
    void onFoundationRemoved(int foundationId) override;

    void onTrussMemberModified(const TSA::Model::TrussMember& member) override;
    void onTrussMemberRemoved(int memberId) override;

    void onCableModified(const TSA::Model::Cable& cable) override;
    void onCableRemoved(int cableId) override;

    void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override;
    void onModelCleared() override;

private slots:
    void onApplyNode();
    void onApplyBeam();
    void onApplyColumn();
    void onApplySlab();
    void onApplyWall();
    void onApplyFoundation();
    void onApplyTruss();
    void onApplyCable();
    void onWidgetChanged();

private:
    void setupUi();
    void hideAllGroups();
    void setupMaterialCombo(QComboBox* combo);
    void setupSectionTypeCombo(QComboBox* combo);
    void setupColorButton(QPushButton* btn, const QString& hexColor);
    void pickColor(QString& targetColor, QPushButton* targetBtn, const QString& title);
    void updateBeamSectionVisibility(int secData);
    void updateColumnSectionVisibility(int secData);
    void updateBeamCalculatedProperties(const TSA::Model::Section& sec);
    void updateColumnCalculatedProperties(const TSA::Model::Section& sec);
    TSA::Model::Section getBeamSectionFromUi() const;
    TSA::Model::Section getColumnSectionFromUi() const;

private:
    TSA::Model::Model* m_model = nullptr;

    enum class CurrentType { None, Level, Node, Beam, Column, Slab, Wall, Foundation, Truss, Cable };
    CurrentType m_currentType = CurrentType::None;

    QString m_currentLevelId;
    int m_currentNodeId = -1;
    int m_currentBeamId = -1;
    int m_currentColumnId = -1;
    int m_currentSlabId = -1;
    int m_currentWallId = -1;
    int m_currentFoundationId = -1;
    int m_currentTrussId = -1;
    int m_currentCableId = -1;

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

    // Panneau Poutre / Barre
    QGroupBox* m_beamGroup = nullptr;
    QLineEdit* m_beamNameEdit = nullptr;
    QLabel* m_beamIdLabel = nullptr;
    QLabel* m_beamStartNodeLabel = nullptr;
    QLabel* m_beamEndNodeLabel = nullptr;
    QLabel* m_beamLengthLabel = nullptr;
    QComboBox* m_beamRoleCombo = nullptr;

    // Éditeur Graphique de Section (Poutre)
    QComboBox* m_beamSectionTypeCombo = nullptr;
    SectionPreviewWidget* m_beamSectionPreview = nullptr;
    QLabel* m_beamWidthLabel = nullptr;
    QDoubleSpinBox* m_beamWidthSpin = nullptr;
    QLabel* m_beamHeightLabel = nullptr;
    QDoubleSpinBox* m_beamHeightSpin = nullptr;
    QLabel* m_beamTwLabel = nullptr;
    QDoubleSpinBox* m_beamTwSpin = nullptr;
    QLabel* m_beamTfLabel = nullptr;
    QDoubleSpinBox* m_beamTfSpin = nullptr;

    // Propriétés calculées (Poutre)
    QLabel* m_beamPropArea = nullptr;
    QLabel* m_beamPropIy = nullptr;
    QLabel* m_beamPropIz = nullptr;
    QLabel* m_beamPropIt = nullptr;
    QLabel* m_beamPropW = nullptr;

    // Matériau & Orientation (Poutre)
    QComboBox* m_beamMaterialCombo = nullptr;
    QDoubleSpinBox* m_beamRotationSpin = nullptr;

    // Excentrement (Poutre)
    QComboBox* m_beamEccentricityCombo = nullptr;
    QDoubleSpinBox* m_beamEySpin = nullptr;
    QDoubleSpinBox* m_beamEzSpin = nullptr;

    // Relâchements aux extrémités (Poutre)
    QCheckBox* m_beamStartUx = nullptr;
    QCheckBox* m_beamStartUy = nullptr;
    QCheckBox* m_beamStartUz = nullptr;
    QCheckBox* m_beamStartRx = nullptr;
    QCheckBox* m_beamStartRy = nullptr;
    QCheckBox* m_beamStartRz = nullptr;
    QCheckBox* m_beamEndUx = nullptr;
    QCheckBox* m_beamEndUy = nullptr;
    QCheckBox* m_beamEndUz = nullptr;
    QCheckBox* m_beamEndRx = nullptr;
    QCheckBox* m_beamEndRy = nullptr;
    QCheckBox* m_beamEndRz = nullptr;

    QPushButton* m_beamColorBtn = nullptr;
    QString m_beamColor;

    // Panneau Poteau
    QGroupBox* m_columnGroup = nullptr;
    QLineEdit* m_columnNameEdit = nullptr;
    QLabel* m_columnIdLabel = nullptr;
    QLabel* m_columnStartNodeLabel = nullptr;
    QLabel* m_columnEndNodeLabel = nullptr;
    QLabel* m_columnHeightLabel = nullptr;

    // Éditeur Graphique de Section (Poteau)
    QComboBox* m_columnSectionTypeCombo = nullptr;
    SectionPreviewWidget* m_columnSectionPreview = nullptr;
    QLabel* m_columnWidthLabel = nullptr;
    QDoubleSpinBox* m_columnWidthSpin = nullptr;
    QLabel* m_columnDepthLabel = nullptr;
    QDoubleSpinBox* m_columnDepthSpin = nullptr;
    QLabel* m_columnTwLabel = nullptr;
    QDoubleSpinBox* m_columnTwSpin = nullptr;
    QLabel* m_columnTfLabel = nullptr;
    QDoubleSpinBox* m_columnTfSpin = nullptr;

    // Propriétés calculées (Poteau)
    QLabel* m_columnPropArea = nullptr;
    QLabel* m_columnPropIy = nullptr;
    QLabel* m_columnPropIz = nullptr;
    QLabel* m_columnPropIt = nullptr;
    QLabel* m_columnPropW = nullptr;

    // Matériau & Orientation (Poteau)
    QComboBox* m_columnMaterialCombo = nullptr;
    QDoubleSpinBox* m_columnRotationSpin = nullptr;

    // Excentrement (Poteau)
    QComboBox* m_columnEccentricityCombo = nullptr;
    QDoubleSpinBox* m_columnEySpin = nullptr;
    QDoubleSpinBox* m_columnEzSpin = nullptr;

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

    // Panneau Câble & Tension System
    QGroupBox* m_cableGroup = nullptr;
    QLineEdit* m_cableNameEdit = nullptr;
    QLabel* m_cableIdLabel = nullptr;
    QLabel* m_cableStartNodeLabel = nullptr;
    QLabel* m_cableEndNodeLabel = nullptr;
    QLabel* m_cableLengthLabel = nullptr;
    QComboBox* m_cableTypeCombo = nullptr;
    QComboBox* m_cableGeomModeCombo = nullptr;
    QComboBox* m_cableStandardCombo = nullptr;
    QDoubleSpinBox* m_cableDiaSpin = nullptr;
    QDoubleSpinBox* m_cableAreaSpin = nullptr;
    QDoubleSpinBox* m_cableModulusSpin = nullptr;
    QDoubleSpinBox* m_cableInitialTensionSpin = nullptr;
    QDoubleSpinBox* m_cableSagSpin = nullptr;
    QComboBox* m_cableStartAnchorCombo = nullptr;
    QComboBox* m_cableEndAnchorCombo = nullptr;
    QCheckBox* m_cableTensionOnlyCheck = nullptr;
    QLabel* m_cableErnstModulusLabel = nullptr;
    QPushButton* m_cableColorBtn = nullptr;
    QString m_cableColor;

    // Synchronisation en direct
    QCheckBox* m_chkLiveSync = nullptr;
    bool m_isLoading = false;
    bool m_isUpdatingFromSelf = false;
};

} // namespace TSA::UI
