#pragma once

#include "../../Model/Material.h"

#include "IElementPropertyView.h"
#include "../../Model/Foundation.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

class FoundationPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit FoundationPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~FoundationPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_foundationId; }
    void refreshView() override;
    void refreshLibraries() override {}
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void pickColor();

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_foundationId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboType = nullptr;
    QDoubleSpinBox* m_spinWidthA = nullptr;
    QDoubleSpinBox* m_spinLengthB = nullptr;
    QDoubleSpinBox* m_spinHeightH = nullptr;
    QLabel* m_lblNode = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex; ///< couleur propre à l'élément ; vide : apparence du matériau
    TSA::Model::Material m_colorMaterial; ///< matériau affiché quand aucune couleur propre n'est choisie
};

} // namespace TSA::UI
