#pragma once

#include <QDialog>
#include "../../Viewer/OccView.h"

class QTabWidget;
class QRadioButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;

namespace TSA::UI
{

class GridSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridSettingsDialog(OccView* occView, QWidget* parent = nullptr);
    ~GridSettingsDialog() override = default;

public slots:
    void applySettings();

private:
    void setupUi();

private:
    OccView* m_occView = nullptr;

    QTabWidget* m_tabWidget = nullptr;

    // Type de grille actif
    QRadioButton* m_radioCartesian = nullptr;
    QRadioButton* m_radioCylindrical = nullptr;
    QRadioButton* m_radioNone = nullptr;

    // Paramètres Cartésiens
    QDoubleSpinBox* m_cartXStep = nullptr;
    QDoubleSpinBox* m_cartYStep = nullptr;
    QDoubleSpinBox* m_cartXSize = nullptr;
    QDoubleSpinBox* m_cartYSize = nullptr;
    QDoubleSpinBox* m_cartZOffset = nullptr;
    QComboBox*      m_cartModeCombo = nullptr;

    // Paramètres Cylindriques
    QDoubleSpinBox* m_cylRadiusStep = nullptr;
    QSpinBox*       m_cylDivisions = nullptr;
    QDoubleSpinBox* m_cylMaxRadius = nullptr;
    QDoubleSpinBox* m_cylZOffset = nullptr;
    QComboBox*      m_cylModeCombo = nullptr;

    // Accrochage
    QCheckBox* m_snapCheck = nullptr;
};

} // namespace TSA::UI
