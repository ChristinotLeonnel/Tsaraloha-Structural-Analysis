#pragma once

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include "../../Analysis/OpenSeesAnalysisBuilder.h"

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/**
 * @brief Dialogue moderne de configuration avancée du calcul structural OpenSees.
 * Permet de sélectionner et filtrer dynamiquement le type d'analyse, l'algorithme
 * non-linéaire, l'intégrateur, la formulation des treillis, le solveur de système et
 * les gestionnaires de contraintes.
 */
class AnalysisConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnalysisConfigDialog(const TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~AnalysisConfigDialog() override = default;

    TSA::Analysis::AnalysisParameters parameters() const;
    void setParameters(const TSA::Analysis::AnalysisParameters& params);

private slots:
    void onAnalysisTypeChanged(int index);
    void onIntegratorChanged(int index);
    void onValidateAndSolve();

private:
    void setupUi();
    void updateFormVisibility();
    void populateCombos();

private:
    const TSA::Model::Model* m_model = nullptr;
    TSA::Analysis::AnalysisParameters m_params;

    // Type d'analyse
    QComboBox* m_comboType = nullptr;

    // Formulations éléments
    QComboBox* m_comboTrussFormulation = nullptr;
    QComboBox* m_comboGeomTransf = nullptr;

    // Algorithme & Intégrateur
    QComboBox* m_comboAlgorithm = nullptr;
    QComboBox* m_comboIntegrator = nullptr;

    // Contrôle de déplacement
    QSpinBox* m_spinControlNode = nullptr;
    QComboBox* m_comboControlDof = nullptr;
    QDoubleSpinBox* m_spinDispIncr = nullptr;

    // Paramètres numériques
    QDoubleSpinBox* m_spinTolerance = nullptr;
    QSpinBox* m_spinMaxIterations = nullptr;
    QSpinBox* m_spinNumSteps = nullptr;
    QDoubleSpinBox* m_spinStepSize = nullptr;

    // Solveur & Contraintes
    QComboBox* m_comboSystem = nullptr;
    QComboBox* m_comboConstraints = nullptr;

    // Modal
    QSpinBox* m_spinEigenmodes = nullptr;

    // Options physiques
    QCheckBox* m_checkSelfWeight = nullptr;
    QCheckBox* m_checkKiloNewtons = nullptr;
    QCheckBox* m_checkSaveAllSteps = nullptr;

    // Extraction des résultats (Light / Advanced)
    QComboBox* m_comboExtraction = nullptr;
    QSpinBox* m_spinMaxStiffnessDofs = nullptr;

    // Widgets conteneurs dynamiques
    QGroupBox* m_groupNonlinear = nullptr;
    QGroupBox* m_groupDispControl = nullptr;
    QGroupBox* m_groupModal = nullptr;
};

} // namespace TSA::UI
