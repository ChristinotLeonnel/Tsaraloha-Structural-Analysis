#pragma once

#include "IElementPropertyView.h"
#include "../../Model/SupportDefinition.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QGroupBox;
class QWidget;

namespace TSA::UI
{

class NodePropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit NodePropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~NodePropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_nodeId; }
    void refreshView() override;
    void refreshLibraries() override {}
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void onPresetChanged(int index);
    void onDofChanged();
    void onOrientationChanged(int index);
    void pickColor();

private:
    void setupUi();
    void updateDofUiState();
    void syncUiFromSupport(const TSA::Model::SupportDefinition& supp);
    TSA::Model::SupportDefinition buildSupportFromUi() const;

    TSA::Model::Model* m_model = nullptr;
    int m_nodeId = -1;
    bool m_isLoading = false;

    // Géométrie du Nœud
    QLineEdit* m_editName = nullptr;
    QDoubleSpinBox* m_spinX = nullptr;
    QDoubleSpinBox* m_spinY = nullptr;
    QDoubleSpinBox* m_spinZ = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#2563EB";

    // Liaisons et Appuis
    QComboBox* m_comboPreset = nullptr;

    // 6 DDLs : Translation UX, UY, UZ
    QComboBox* m_comboTx = nullptr;
    QComboBox* m_comboTy = nullptr;
    QComboBox* m_comboTz = nullptr;
    QDoubleSpinBox* m_spinKx = nullptr;
    QDoubleSpinBox* m_spinKy = nullptr;
    QDoubleSpinBox* m_spinKz = nullptr;

    // 6 DDLs : Rotation RX, RY, RZ
    QComboBox* m_comboRx = nullptr;
    QComboBox* m_comboRy = nullptr;
    QComboBox* m_comboRz = nullptr;
    QDoubleSpinBox* m_spinKrx = nullptr;
    QDoubleSpinBox* m_spinKry = nullptr;
    QDoubleSpinBox* m_spinKrz = nullptr;

    // Orientation
    QComboBox* m_comboOrientation = nullptr;
    QWidget* m_customDirWidget = nullptr;
    QDoubleSpinBox* m_spinDirX = nullptr;
    QDoubleSpinBox* m_spinDirY = nullptr;
    QDoubleSpinBox* m_spinDirZ = nullptr;
};

} // namespace TSA::UI
