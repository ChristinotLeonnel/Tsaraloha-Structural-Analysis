#pragma once

#include "AnalysisEngineOptions.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLabel;

namespace TSA::UI
{

/// Options propres à OpenSees (système, contraintes, unités, extraction ADVANCED). Type d'analyse,
/// poids propre et nombre de modes sont des réglages communs de la fenêtre Analysis.
class OpenSeesOptionsWidget final : public AnalysisEngineOptionsWidget
{
    Q_OBJECT

public:
    explicit OpenSeesOptionsWidget(QWidget* parent = nullptr);

    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;

private:
    QComboBox* m_system = nullptr;
    QComboBox* m_constraints = nullptr;
    QComboBox* m_extraction = nullptr;
    QSpinBox* m_maxStiffnessDofs = nullptr;
    QCheckBox* m_kiloNewtons = nullptr;
    QLabel* m_executable = nullptr;   ///< exécutable OpenSees retenu (réglage de l'application, BUG-102)
    void refreshExecutable();
};

} // namespace TSA::UI
