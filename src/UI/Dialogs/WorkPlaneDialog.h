#pragma once

#include <QDialog>
#include <memory>
#include <vector>
#include "../../Coordinate/WorkPlane.h"

class OccView;
class QRadioButton;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QGroupBox;

namespace TSA::Coordinate
{
    class LevelManager;
}

namespace TSA::UI
{

class WorkPlaneDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkPlaneDialog(OccView* occView,
                             TSA::Coordinate::LevelManager* levelManager = nullptr,
                             QWidget* parent = nullptr);
    ~WorkPlaneDialog() override = default;

public slots:
    void applySettings();
    void resetToDefault();

private slots:
    void onModeChanged();
    void onLevelSelected(int index);

private:
    void setupUi();
    void syncFromOccView();

private:
    OccView* m_occView = nullptr;
    TSA::Coordinate::LevelManager* m_levelManager = nullptr;

    // Boutons radio de mode
    QRadioButton* m_rbXY = nullptr;
    QRadioButton* m_rbLevel = nullptr;
    QRadioButton* m_rbXZ = nullptr;
    QRadioButton* m_rbYZ = nullptr;
    QRadioButton* m_rbThreePoints = nullptr;

    // Contrôles contextuels
    QComboBox* m_comboLevels = nullptr;
    QDoubleSpinBox* m_spinOffset = nullptr;

    // 3 points personnalisés
    QGroupBox* m_grpThreePoints = nullptr;
    QDoubleSpinBox* m_p1X = nullptr;
    QDoubleSpinBox* m_p1Y = nullptr;
    QDoubleSpinBox* m_p1Z = nullptr;
    QDoubleSpinBox* m_p2X = nullptr;
    QDoubleSpinBox* m_p2Y = nullptr;
    QDoubleSpinBox* m_p2Z = nullptr;
    QDoubleSpinBox* m_p3X = nullptr;
    QDoubleSpinBox* m_p3Y = nullptr;
    QDoubleSpinBox* m_p3Z = nullptr;

    // Options d'affichage et de navigation
    QCheckBox* m_chkVisible = nullptr;
    QCheckBox* m_chkAlignCamera = nullptr;
};

} // namespace TSA::UI
