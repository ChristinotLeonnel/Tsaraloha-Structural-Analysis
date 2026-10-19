#pragma once

#include "AnalysisEngineOptions.h"

class QComboBox;

namespace TSA::UI
{

/// Options du moteur EF 3D Fem3D : mode de calcul (Fem3D seul, ou comparaison avec OpenSees) et secours explicite
/// pour un modèle hors de son périmètre (refus, ou calcul par OpenSees étiqueté comme tel). Clés JSON : « mode »
/// (fem3d | compare) et « fallback » (none | opensees), interprétées par Fem3DEngine seul.
class Fem3DOptionsWidget final : public AnalysisEngineOptionsWidget
{
    Q_OBJECT

public:
    explicit Fem3DOptionsWidget(QWidget* parent = nullptr);
    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;

private:
    QComboBox* m_mode = nullptr;
    QComboBox* m_fallback = nullptr;
};

} // namespace TSA::UI
