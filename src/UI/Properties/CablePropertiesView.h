#pragma once

#include "IElementPropertyView.h"
#include "../../Model/Cable/CableTypes.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace TSA::UI
{

/**
 * @brief Vue de propriétés spécialisée pour les Câbles structuraux et haubans (Règle 14).
 * Spécifiquement dédiée aux paramètres mécaniques et géométriques des câbles :
 * pré-tension T0, décompression (tension-only), flèche/sag, torons et ancrages normés.
 */
class CablePropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit CablePropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~CablePropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_cableId; }
    void refreshView() override;
    void refreshLibraries() override;
    void applyChanges() override;

private slots:
    void onWidgetChanged();
    void onCableTypeChanged(int index);
    void pickColor();

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;
    int m_cableId = -1;
    bool m_isLoading = false;

    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboType = nullptr;
    QComboBox* m_comboGeomMode = nullptr;
    QDoubleSpinBox* m_spinDiameter = nullptr;
    QDoubleSpinBox* m_spinArea = nullptr;
    QDoubleSpinBox* m_spinModulus = nullptr;
    QDoubleSpinBox* m_spinPrestress = nullptr;
    QDoubleSpinBox* m_spinSag = nullptr;
    QComboBox* m_comboStartAnchor = nullptr;
    QComboBox* m_comboEndAnchor = nullptr;
    QCheckBox* m_chkTensionOnly = nullptr;
    QLabel* m_lblStandardInfo = nullptr;
    QPushButton* m_btnColor = nullptr;
    QString m_colorHex = "#F59E0B";
};

} // namespace TSA::UI
