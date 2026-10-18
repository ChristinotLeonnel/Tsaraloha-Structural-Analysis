#pragma once

#include <QDialog>
#include <QString>
#include <QColor>
#include "../../Model/CreationPresets.h"

class QTabWidget;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QStackedWidget;

namespace TSA::UI
{

enum class PresetTarget
{
    Wall,
    Slab,
    Beam,
    Column,
    Foundation,
    Truss
};

class StructurePresetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StructurePresetDialog(
        TSA::Model::StructurePresets& presets,
        PresetTarget initialTarget = PresetTarget::Wall,
        QWidget* parent = nullptr);

    TSA::Model::StructurePresets presets() const;

private:
    void setupUi();
    QWidget* createWallTab();
    QWidget* createSlabTab();
    QWidget* createBeamTab();
    QWidget* createColumnTab();
    QWidget* createFoundationTab();
    QWidget* createTrussTab();
    void loadFromPresets();
    void saveToPresets();

    void updateColorButton(QPushButton* btn, const QString& hexColor, const QString& defaultHex);

private:
    TSA::Model::StructurePresets m_presets;
    PresetTarget m_initialTarget;

    QTabWidget* m_tabWidget = nullptr;
    QCheckBox*  m_chkShowEveryTime = nullptr;

    // Voile (Wall)
    QDoubleSpinBox* m_wallThickness = nullptr;
    QDoubleSpinBox* m_wallHeight = nullptr;
    QDoubleSpinBox* m_wallOffset = nullptr;
    QComboBox*      m_wallMaterial = nullptr;
    QPushButton*    m_wallColorBtn = nullptr;
    QString         m_wallColor;

    // Dalle (Slab)
    QDoubleSpinBox* m_slabThickness = nullptr;
    QComboBox*      m_slabMaterial = nullptr;
    QPushButton*    m_slabColorBtn = nullptr;
    QString         m_slabColor;

    // Poutre (Beam)
    QComboBox*      m_beamShapeType = nullptr;
    QStackedWidget* m_beamShapeStack = nullptr;
    QDoubleSpinBox* m_beamRectWidth = nullptr;
    QDoubleSpinBox* m_beamRectHeight = nullptr;
    QDoubleSpinBox* m_beamCircDiam = nullptr;
    QComboBox*      m_beamIpeNumber = nullptr;
    QComboBox*      m_beamHeaNumber = nullptr;
    QComboBox*      m_beamHebNumber = nullptr;
    QDoubleSpinBox* m_beamBetaAngle = nullptr;
    QComboBox*      m_beamMaterial = nullptr;
    QPushButton*    m_beamColorBtn = nullptr;
    QString         m_beamColor;

    // Poteau (Column)
    QComboBox*      m_colShapeType = nullptr;
    QStackedWidget* m_colShapeStack = nullptr;
    QDoubleSpinBox* m_colRectWidth = nullptr;
    QDoubleSpinBox* m_colRectHeight = nullptr;
    QDoubleSpinBox* m_colCircDiam = nullptr;
    QComboBox*      m_colIpeNumber = nullptr;
    QComboBox*      m_colHeaNumber = nullptr;
    QDoubleSpinBox* m_colBetaAngle = nullptr;
    QComboBox*      m_colMaterial = nullptr;
    QPushButton*    m_colColorBtn = nullptr;
    QString         m_colColor;

    // Fondation (semelle isolée posée par le mode Dessin Fondation)
    QDoubleSpinBox* m_footWidthA = nullptr;
    QDoubleSpinBox* m_footLengthB = nullptr;
    QDoubleSpinBox* m_footHeightH = nullptr;
    QComboBox*      m_footMaterial = nullptr;

    // Treillis (barre circulaire pleine ou tube, diamètre extérieur)
    QDoubleSpinBox* m_trussDiameter = nullptr;
    QComboBox*      m_trussMaterial = nullptr;
};

} // namespace TSA::UI
