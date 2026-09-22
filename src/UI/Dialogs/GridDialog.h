#pragma once

#include <QDialog>
#include "../../Grid/GridDefinition.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTabWidget;
class QRadioButton;
class QCheckBox;

namespace TSA::UI
{

class GridDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridDialog(QWidget* parent = nullptr);
    explicit GridDialog(const TSA::Grid::GridDefinition& existingDef, QWidget* parent = nullptr);
    ~GridDialog() override = default;

    TSA::Grid::GridDefinition getDefinition() const;

private slots:
    void onTypeChanged(int index);
    void onCartesianModeToggled();
    void onCylindricalModeToggled();

private:
    void setupUi();
    void loadFromDefinition(const TSA::Grid::GridDefinition& def);

private:
    bool m_isEditMode = false;
    std::string m_gridId;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;

    // Origine
    QDoubleSpinBox* m_originXSpin = nullptr;
    QDoubleSpinBox* m_originYSpin = nullptr;
    QDoubleSpinBox* m_originZSpin = nullptr;

    QTabWidget* m_paramTabs = nullptr;

    // --- Onglet Cartésien ---
    QWidget* m_cartesianTab = nullptr;
    QRadioButton* m_radioCartRegular = nullptr;
    QRadioButton* m_radioCartCustom = nullptr;

    // Mode régulier
    QSpinBox* m_xCountSpin = nullptr;
    QDoubleSpinBox* m_xSpacingSpin = nullptr;
    QSpinBox* m_yCountSpin = nullptr;
    QDoubleSpinBox* m_ySpacingSpin = nullptr;
    QSpinBox* m_zCountSpin = nullptr;
    QDoubleSpinBox* m_zSpacingSpin = nullptr;

    // Mode personnalisé (coordonnées explicites séparées par point-virgule)
    QLineEdit* m_xCustomEdit = nullptr;
    QLineEdit* m_yCustomEdit = nullptr;
    QLineEdit* m_zCustomEdit = nullptr;

    // --- Onglet Cylindrique ---
    QWidget* m_cylindricalTab = nullptr;
    QRadioButton* m_radioCylRegular = nullptr;
    QRadioButton* m_radioCylCustom = nullptr;

    // Mode régulier
    QSpinBox* m_radiusCountSpin = nullptr;
    QDoubleSpinBox* m_radiusSpacingSpin = nullptr;
    QSpinBox* m_angleCountSpin = nullptr;
    QDoubleSpinBox* m_angleSpacingSpin = nullptr;
    QSpinBox* m_cylZCountSpin = nullptr;
    QDoubleSpinBox* m_cylZSpacingSpin = nullptr;

    // Mode personnalisé
    QLineEdit* m_radiiCustomEdit = nullptr;
    QLineEdit* m_anglesCustomEdit = nullptr;
    QLineEdit* m_cylZCustomEdit = nullptr;
};

} // namespace TSA::UI
