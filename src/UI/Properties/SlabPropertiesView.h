#pragma once

#include "IElementPropertyView.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

class SlabPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit SlabPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~SlabPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_slabId; }
    void refreshView() override;
    void refreshLibraries() override;
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void pickColor();

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_slabId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QDoubleSpinBox* m_spinThickness = nullptr;
    QComboBox* m_comboMaterial = nullptr;
    QComboBox* m_comboType = nullptr;
    QLabel* m_lblNodes = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#10B981";
};

} // namespace TSA::UI
