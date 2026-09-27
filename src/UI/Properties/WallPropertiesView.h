#pragma once

#include "IElementPropertyView.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

class WallPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit WallPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~WallPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_wallId; }
    void refreshView() override;
    void refreshLibraries() override;
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void pickColor();

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_wallId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QDoubleSpinBox* m_spinThickness = nullptr;
    QDoubleSpinBox* m_spinHeight = nullptr;
    QComboBox* m_comboMaterial = nullptr;
    QLabel* m_lblNodes = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#9333EA";
};

} // namespace TSA::UI
