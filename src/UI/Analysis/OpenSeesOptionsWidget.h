#pragma once

#include "AnalysisEngineOptions.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QSpinBox;

namespace TSA::UI
{

/// Options propres à OpenSees (formulations, algorithme, intégrateur, système, contraintes,
/// extraction ADVANCED). Reprend les réglages de l'ancien AnalysisConfigDialog ; type d'analyse,
/// poids propre et nombre de modes sont des réglages communs de la fenêtre Analysis.
class OpenSeesOptionsWidget final : public AnalysisEngineOptionsWidget
{
public:
    explicit OpenSeesOptionsWidget(QWidget* parent = nullptr);

    void loadSettings(const QJsonObject& settings) override;
    QJsonObject saveSettings() const override;
    void setAnalysisContext(const TSA::Analysis::AnalysisContext& context) override;

private:
    void updateVisibility();

    TSA::Analysis::AnalysisType m_type = TSA::Analysis::AnalysisType::LinearStatic;

    QComboBox* m_trussFormulation = nullptr;
    QComboBox* m_geomTransf = nullptr;
    QGroupBox* m_groupNonlinear = nullptr;
    QComboBox* m_algorithm = nullptr;
    QComboBox* m_integrator = nullptr;
    QSpinBox* m_numSteps = nullptr;
    QDoubleSpinBox* m_stepSize = nullptr;
    QDoubleSpinBox* m_tolerance = nullptr;
    QSpinBox* m_maxIterations = nullptr;
    QGroupBox* m_groupDispControl = nullptr;
    QSpinBox* m_controlNode = nullptr;
    QComboBox* m_controlDof = nullptr;
    QDoubleSpinBox* m_dispIncrement = nullptr;
    QComboBox* m_system = nullptr;
    QComboBox* m_constraints = nullptr;
    QComboBox* m_extraction = nullptr;
    QSpinBox* m_maxStiffnessDofs = nullptr;
    QCheckBox* m_kiloNewtons = nullptr;
    QCheckBox* m_saveAllSteps = nullptr;
};

} // namespace TSA::UI
