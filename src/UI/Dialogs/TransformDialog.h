#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;
class QRadioButton;

namespace TSA::UI
{

enum class TransformMode
{
    Move,
    Copy
};

class TransformDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransformDialog(TransformMode mode = TransformMode::Move, QWidget* parent = nullptr);
    ~TransformDialog() override = default;

    TransformMode mode() const;
    double deltaX() const;
    double deltaY() const;
    double deltaZ() const;
    int repetitions() const;

private:
    void setupUi();

private:
    TransformMode m_initialMode;

    QRadioButton* m_radioMove = nullptr;
    QRadioButton* m_radioCopy = nullptr;

    QDoubleSpinBox* m_dxSpin = nullptr;
    QDoubleSpinBox* m_dySpin = nullptr;
    QDoubleSpinBox* m_dzSpin = nullptr;
    QSpinBox* m_repetitionsSpin = nullptr;
};

} // namespace TSA::UI
