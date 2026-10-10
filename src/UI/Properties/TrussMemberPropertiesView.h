#pragma once

#include "../../Model/Material.h"

#include "IElementPropertyView.h"
#include "MemberEndNodesWidget.h"
#include "../../Model/TrussMember.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

class TrussMemberPropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit TrussMemberPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~TrussMemberPropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_memberId; }
    void refreshView() override;
    void refreshLibraries() override;
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void pickColor();

private:
    MemberEndNodesWidget* m_endNodes = nullptr;   // nœuds d'extrémité (appuis), lus dans le modèle
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_memberId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboRole = nullptr;
    QDoubleSpinBox* m_spinDiameter = nullptr;
    QComboBox* m_comboMaterial = nullptr;
    QLabel* m_lblNodes = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex; ///< couleur propre à l'élément ; vide : apparence du matériau
    TSA::Model::Material m_colorMaterial; ///< matériau affiché quand aucune couleur propre n'est choisie
};

} // namespace TSA::UI
