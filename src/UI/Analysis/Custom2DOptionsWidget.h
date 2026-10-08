#pragma once

#include "AnalysisEngineOptions.h"

class QCheckBox;
class QSpinBox;

namespace TSA::UI
{

/// Options du solveur 2D (méthode des déplacements) : hypothèse d'inextensibilité des barres
/// (méthode des rotations classique), finesse des courbes d'efforts / déformée et export du système
/// résolu K·U = F (dock « Données du calcul »).
class Custom2DOptionsWidget final : public AnalysisEngineOptionsWidget
{
public:
    explicit Custom2DOptionsWidget(QWidget* parent = nullptr);
    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;

private:
    QCheckBox* m_inextensible = nullptr;
    QSpinBox* m_curvePoints = nullptr;
    QCheckBox* m_exportSystem = nullptr;
};

} // namespace TSA::UI
