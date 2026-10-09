#pragma once

#include "IElementPropertyView.h"
#include "MemberEndNodesWidget.h"
#include "../../Model/Section.h"
#include "../../Model/Beam.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QGroupBox;

namespace TSA::UI
{

class SectionPreviewWidget;

/**
 * @brief Vue de propriétés spécialisée pour les Poutres et barres linéaires (Règle 14).
 * Spécifiquement dédiée aux paramètres mécaniques et géométriques des poutres :
 * Sections normalisées Eurocodes / personnalisées, matériau, relâchements d'extrémités,
 * orientation d'angle gamma, excentrements et prévisualisation 2D B-Rep.
 */
class BeamPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit BeamPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~BeamPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_beamId; }
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
    void updateCalculatedProperties(const TSA::Model::Section& sec);
    TSA::Model::Section getSectionFromUi() const;

    TSA::Model::Model* m_model = nullptr;
    int m_beamId = -1;
    bool m_isLoading = false;

    // En-tête Récapitulatif CAO (Règle Métier & Standards)
    QLabel* m_lblCadType = nullptr;
    QLabel* m_lblCadId = nullptr;
    QLabel* m_lblCadNodes = nullptr;
    QLabel* m_lblCadSection = nullptr;
    QLabel* m_lblCadMaterial = nullptr;
    QLabel* m_lblCadLength = nullptr;
    QLabel* m_lblCadLevel = nullptr;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboRole = nullptr;
    QComboBox* m_comboSectionType = nullptr;
    QComboBox* m_comboMaterial = nullptr;

    // Dimensions de section
    QDoubleSpinBox* m_spinWidth = nullptr;
    QDoubleSpinBox* m_spinHeight = nullptr;
    QDoubleSpinBox* m_spinRadius = nullptr;
    QDoubleSpinBox* m_spinFlangeWidth = nullptr;
    QDoubleSpinBox* m_spinFlangeThick = nullptr;
    QDoubleSpinBox* m_spinWebThick = nullptr;
    QDoubleSpinBox* m_spinRotation = nullptr;

    // Excentrements
    QComboBox* m_comboEccentricity = nullptr;
    QDoubleSpinBox* m_spinEy = nullptr;
    QDoubleSpinBox* m_spinEz = nullptr;

    // Relâchements (Rotules)
    QCheckBox* m_chkStartUx = nullptr;
    QCheckBox* m_chkStartUy = nullptr;
    QCheckBox* m_chkStartUz = nullptr;
    QCheckBox* m_chkStartRx = nullptr;
    QCheckBox* m_chkStartRy = nullptr;
    QCheckBox* m_chkStartRz = nullptr;

    QCheckBox* m_chkEndUx = nullptr;
    QCheckBox* m_chkEndUy = nullptr;
    QCheckBox* m_chkEndUz = nullptr;
    QCheckBox* m_chkEndRx = nullptr;
    QCheckBox* m_chkEndRy = nullptr;
    QCheckBox* m_chkEndRz = nullptr;

    // Prévisualisation & Propriétés calculées
    SectionPreviewWidget* m_previewWidget = nullptr;
    QLabel* m_lblArea = nullptr;
    QLabel* m_lblIy = nullptr;
    QLabel* m_lblIz = nullptr;

    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#3B82F6";
};

} // namespace TSA::UI
