#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>

namespace TSA::UI
{

class SectionCutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SectionCutDialog(QWidget* parent = nullptr);
    ~SectionCutDialog() override = default;

    bool isCutEnabled() const;
    int selectedAxis() const; // 0=XY (Z), 1=XZ (Y), 2=YZ (X)
    double cutPosition() const;
    bool isFlipped() const;

    void setCutEnabled(bool enabled);
    void setSelectedAxis(int axis);
    void setCutPosition(double pos);
    void setCutLimits(double minVal, double maxVal);
    void setFlipped(bool flip);

signals:
    void clippingChanged(bool enabled, int axisIndex, double position, bool flip);

public slots:
    void onActiveLevelCutRequested(double zLevel);

private slots:
    void onEnabledToggled(bool checked);
    void onAxisChanged(int index);
    void onSliderValueChanged(int value);
    void onSpinBoxValueChanged(double value);
    void onFlipToggled(bool checked);

private:
    void setupUi();
    void emitChange();

private:
    QCheckBox*      m_enableCheck = nullptr;
    QComboBox*      m_axisCombo = nullptr;
    QSlider*        m_posSlider = nullptr;
    QDoubleSpinBox* m_posSpin = nullptr;
    QCheckBox*      m_flipCheck = nullptr;
    QPushButton*    m_btnCutAtLevel = nullptr;

    double m_minPos = -20.0;
    double m_maxPos = 50.0;
    bool   m_updating = false;
};

} // namespace TSA::UI
