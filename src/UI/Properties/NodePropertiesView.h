#pragma once

#include "IElementPropertyView.h"
#include "../../Model/SupportDefinition.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QGroupBox;
class QWidget;

namespace TSA::Analysis { class ResultsModel; }

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
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
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

    // Résultats OpenSees
    std::shared_ptr<TSA::Analysis::ResultsModel> m_resultsModel;
    QGroupBox* m_groupResults = nullptr;
    class QLabel* m_labelNodeStatus = nullptr;
    class QLabel* m_labelDispX = nullptr;
    class QLabel* m_labelDispY = nullptr;
    class QLabel* m_labelDispZ = nullptr;
    class QLabel* m_labelDispRes = nullptr;
    class QLabel* m_labelRotX = nullptr;
    class QLabel* m_labelRotY = nullptr;
    class QLabel* m_labelRotZ = nullptr;
    class QLabel* m_labelReactFx = nullptr;
    class QLabel* m_labelReactFy = nullptr;
    class QLabel* m_labelReactFz = nullptr;
    class QLabel* m_labelReactMx = nullptr;
    class QLabel* m_labelReactMy = nullptr;
    class QLabel* m_labelReactMz = nullptr;
};

} // namespace TSA::UI
