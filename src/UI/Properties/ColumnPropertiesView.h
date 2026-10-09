#pragma once

#include "IElementPropertyView.h"
#include "MemberEndNodesWidget.h"
#include "../../Model/Section.h"
#include "../../Model/Column.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

class SectionPreviewWidget;

/**
 * @brief Vue de propriétés spécialisée pour les Poteaux structuraux (Règle 14).
 * Spécifiquement dédiée aux poteaux : profilé vertical, rotation gamma, niveaux d'implantation.
 */
class ColumnPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit ColumnPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~ColumnPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_columnId; }
    void refreshView() override;
    void refreshLibraries() override;
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void onSectionTypeChanged(int index);
    void pickColor();

private:
    MemberEndNodesWidget* m_endNodes = nullptr;   // nœuds d'extrémité (appuis), lus dans le modèle
    void setupUi();
    void updateSectionVisibility(int secType);
    TSA::Model::Section getSectionFromUi() const;

    TSA::Model::Model* m_model = nullptr;
    int m_columnId = -1;
    bool m_isLoading = false;

    // En-tête Récapitulatif CAO
    QLabel* m_lblCadType = nullptr;
    QLabel* m_lblCadId = nullptr;
    QLabel* m_lblCadNodes = nullptr;
    QLabel* m_lblCadSection = nullptr;
    QLabel* m_lblCadMaterial = nullptr;
    QLabel* m_lblCadLength = nullptr;
    QLabel* m_lblCadLevel = nullptr;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboSectionType = nullptr;
    QComboBox* m_comboMaterial = nullptr;

    QDoubleSpinBox* m_spinWidth = nullptr;
    QDoubleSpinBox* m_spinHeight = nullptr;
    QDoubleSpinBox* m_spinRadius = nullptr;
    QDoubleSpinBox* m_spinRotation = nullptr;

    SectionPreviewWidget* m_previewWidget = nullptr;
    QLabel* m_lblNodes = nullptr;

    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#D97706";
};

} // namespace TSA::UI
