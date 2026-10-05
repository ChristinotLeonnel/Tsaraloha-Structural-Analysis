#pragma once

#include "AnalysisEngineOptions.h"

class QCheckBox;
class QSpinBox;

namespace TSA::UI
{

/// Options du solveur 2D (méthode des déplacements) : hypothèse d'inextensibilité des barres
/// (méthode des rotations classique) et finesse des courbes d'efforts / déformée.
class Custom2DOptionsWidget final : public AnalysisEngineOptionsWidget
{
public:
    explicit Custom2DOptionsWidget(QWidget* parent = nullptr);
    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;

private:
    QCheckBox* m_inextensible = nullptr;
    QSpinBox* m_curvePoints = nullptr;
};

} // namespace TSA::UI
