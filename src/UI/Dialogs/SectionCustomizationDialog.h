#pragma once

#include <QDialog>
#include "../../Model/Section.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace TSA::UI
{

class SectionPreviewWidget;

class SectionCustomizationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SectionCustomizationDialog(const TSA::Model::Section& initialSection = TSA::Model::Section::rectangular(0.40, 0.40), QWidget* parent = nullptr);
    ~SectionCustomizationDialog() override = default;

    TSA::Model::Section customizedSection() const;

private slots:
    void onShapeChanged(int index);
    void onDimensionsChanged();
    void onPresetSelected(int index);

private:
    void setupUi();
    void updateVisibility();
    void updateCalculatedProperties();
    void populatePresetsForShape(TSA::Model::SectionShape shape);

private:
    TSA::Model::Section m_section;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboShape = nullptr;
    QComboBox* m_comboPreset = nullptr;
    QLabel* m_lblPreset = nullptr;

    // Champs de dimensions
    QLabel* m_lblWidthB = nullptr;
    QDoubleSpinBox* m_spinWidthB = nullptr;

    QLabel* m_lblHeightH = nullptr;
    QDoubleSpinBox* m_spinHeightH = nullptr;

    QLabel* m_lblDiameterD = nullptr;
    QDoubleSpinBox* m_spinDiameterD = nullptr;

    QLabel* m_lblTw = nullptr;
    QDoubleSpinBox* m_spinTw = nullptr;

    QLabel* m_lblTf = nullptr;
    QDoubleSpinBox* m_spinTf = nullptr;

    // Aperçu interactif 2D & Diagramme
    SectionPreviewWidget* m_previewWidget = nullptr;

    // Caractéristiques calculées
    QLabel* m_lblArea = nullptr;
    QLabel* m_lblIy = nullptr;
    QLabel* m_lblIz = nullptr;
    QLabel* m_lblIt = nullptr;
    QLabel* m_lblW = nullptr;

    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnCancel = nullptr;

    bool m_isUpdating = false;
};

} // namespace TSA::UI
