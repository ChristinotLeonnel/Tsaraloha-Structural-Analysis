#pragma once

#include "IElementPropertyView.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;

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
    void pickColor();

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_nodeId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QDoubleSpinBox* m_spinX = nullptr;
    QDoubleSpinBox* m_spinY = nullptr;
    QDoubleSpinBox* m_spinZ = nullptr;
    QComboBox* m_comboSupport = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#2563EB";
};

} // namespace TSA::UI
