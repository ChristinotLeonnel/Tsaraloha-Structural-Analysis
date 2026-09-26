#pragma once

#include <QDialog>
#include <gp_Pnt.hxx>
#include "../../Grid/GridType.h"

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class OccView;

namespace TSA::UI
{

class GridAdvancedSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridAdvancedSettingsDialog(
        const gp_Pnt& origin,
        double rotationDeg,
        const TSA::Grid::GridDisplaySettings& displaySettings,
        QWidget* parent = nullptr,
        OccView* occView = nullptr);
    ~GridAdvancedSettingsDialog() override = default;

    gp_Pnt origin() const;
    double rotationDeg() const;
    TSA::Grid::GridDisplaySettings displaySettings() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void onPickColor();
    void onPickOriginClicked();

private:
    void setupUi();

private:
    OccView* m_occView = nullptr;
    QDoubleSpinBox* m_spnOriginX = nullptr;
    QDoubleSpinBox* m_spnOriginY = nullptr;
    QDoubleSpinBox* m_spnOriginZ = nullptr;
    QPushButton* m_btnPickOrigin = nullptr;
    QDoubleSpinBox* m_spnRotation = nullptr;

    QDoubleSpinBox* m_spnExtension = nullptr;
    QDoubleSpinBox* m_spnBubbleRadius = nullptr;
    QCheckBox* m_chkShowBubbles = nullptr;

    QComboBox* m_cmbLineStyle = nullptr;
    QDoubleSpinBox* m_spnLineWidth = nullptr;
    QPushButton* m_btnColor = nullptr;

    QString m_selectedColor;
};

} // namespace TSA::UI
