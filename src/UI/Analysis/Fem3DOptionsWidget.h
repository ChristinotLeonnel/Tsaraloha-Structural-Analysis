#pragma once

#include "AnalysisEngineOptions.h"

class QComboBox;

namespace TSA::UI
{

/// Options du moteur EF 3D Fem3D : mode de calcul (Fem3D seul, ou comparaison avec OpenSees). Clé JSON : « mode »
/// (fem3d | compare), interprétée par Fem3DEngine seul. Un modèle hors périmètre est refusé (secours retiré, BUG-144).
class Fem3DOptionsWidget final : public AnalysisEngineOptionsWidget
{
    Q_OBJECT

public:
    explicit Fem3DOptionsWidget(QWidget* parent = nullptr);
    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;

private:
    QComboBox* m_mode = nullptr;
};

} // namespace TSA::UI
