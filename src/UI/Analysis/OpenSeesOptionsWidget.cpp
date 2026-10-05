#include "OpenSeesOptionsWidget.h"

#include "../../Analysis/Engines/OpenSees/OpenSeesEngine.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Analysis;

namespace
{
void select(QComboBox* c, int value)
{
    const int i = c->findData(value);
    if (i >= 0) c->setCurrentIndex(i);
}
int comboValue(const QComboBox* c) { return c->currentData().toInt(); }
} // namespace

OpenSeesOptionsWidget::OpenSeesOptionsWidget(QWidget* parent)
    : AnalysisEngineOptionsWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    auto* groupForm = new QGroupBox(tr("Formulation"), this);
    auto* form = new QFormLayout(groupForm);
    m_trussFormulation = new QComboBox(groupForm);
    m_trussFormulation->addItem(tr("Truss (linéaire standard)"), static_cast<int>(TrussFormulation::Truss));
    m_trussFormulation->addItem(tr("CorotTruss (grands déplacements)"), static_cast<int>(TrussFormulation::CorotTruss));
    m_trussFormulation->addItem(tr("TrussSection (section discrète)"), static_cast<int>(TrussFormulation::TrussSection));
    m_trussFormulation->addItem(tr("CorotTrussSection"), static_cast<int>(TrussFormulation::CorotTrussSection));
    form->addRow(tr("Formulation treillis :"), m_trussFormulation);
    m_geomTransf = new QComboBox(groupForm);
    m_geomTransf->addItem(tr("Linéaire (petits déplacements)"), static_cast<int>(GeomTransfType::Linear));
    m_geomTransf->addItem(tr("P-Delta (2nd ordre)"), static_cast<int>(GeomTransfType::PDelta));
    m_geomTransf->addItem(tr("Corotational (grands déplacements 3D)"), static_cast<int>(GeomTransfType::Corotational));
    form->addRow(tr("Transformation 3D :"), m_geomTransf);
    lay->addWidget(groupForm);

    m_groupNonlinear = new QGroupBox(tr("Résolution non linéaire"), this);
    auto* formNL = new QFormLayout(m_groupNonlinear);
    m_algorithm = new QComboBox(m_groupNonlinear);
    m_algorithm->addItem(tr("Newton-Raphson"), static_cast<int>(NonlinearAlgorithm::Newton));
    m_algorithm->addItem(tr("Newton avec Line Search"), static_cast<int>(NonlinearAlgorithm::NewtonLineSearch));
    m_algorithm->addItem(tr("Modified Newton"), static_cast<int>(NonlinearAlgorithm::ModifiedNewton));
    m_algorithm->addItem(tr("Krylov-Newton"), static_cast<int>(NonlinearAlgorithm::KrylovNewton));
    m_algorithm->addItem(tr("BFGS"), static_cast<int>(NonlinearAlgorithm::BFGS));
    m_algorithm->addItem(tr("Broyden"), static_cast<int>(NonlinearAlgorithm::Broyden));
    m_algorithm->addItem(tr("Secant Newton"), static_cast<int>(NonlinearAlgorithm::SecantNewton));
    formNL->addRow(tr("Algorithme :"), m_algorithm);
    m_integrator = new QComboBox(m_groupNonlinear);
    m_integrator->addItem(tr("Load Control"), static_cast<int>(IntegratorType::LoadControl));
    m_integrator->addItem(tr("Displacement Control"), static_cast<int>(IntegratorType::DisplacementControl));
    m_integrator->addItem(tr("Arc-Length"), static_cast<int>(IntegratorType::ArcLength));
    m_integrator->addItem(tr("Min Unbalanced Displacement Norm"), static_cast<int>(IntegratorType::MinUnbalDispNorm));
    formNL->addRow(tr("Intégrateur :"), m_integrator);
    m_numSteps = new QSpinBox(m_groupNonlinear);
    m_numSteps->setRange(1, 1000);
    formNL->addRow(tr("Nombre de pas :"), m_numSteps);
    m_stepSize = new QDoubleSpinBox(m_groupNonlinear);
    m_stepSize->setRange(1e-5, 10.0);
    m_stepSize->setDecimals(4);
    formNL->addRow(tr("Taille de pas (Δλ ou arc) :"), m_stepSize);
    m_tolerance = new QDoubleSpinBox(m_groupNonlinear);
    m_tolerance->setRange(1e-12, 1e-1);
    m_tolerance->setDecimals(8);
    formNL->addRow(tr("Tolérance de convergence :"), m_tolerance);
    m_maxIterations = new QSpinBox(m_groupNonlinear);
    m_maxIterations->setRange(5, 500);
    formNL->addRow(tr("Itérations maximales :"), m_maxIterations);
    lay->addWidget(m_groupNonlinear);

    m_groupDispControl = new QGroupBox(tr("Contrôle du déplacement"), this);
    auto* formDC = new QFormLayout(m_groupDispControl);
    m_controlNode = new QSpinBox(m_groupDispControl);
    m_controlNode->setRange(1, 999999);
    formDC->addRow(tr("Nœud contrôlé :"), m_controlNode);
    m_controlDof = new QComboBox(m_groupDispControl);
    m_controlDof->addItem(tr("UX"), 1);
    m_controlDof->addItem(tr("UY"), 2);
    m_controlDof->addItem(tr("UZ"), 3);
    formDC->addRow(tr("DDL contrôlé :"), m_controlDof);
    m_dispIncrement = new QDoubleSpinBox(m_groupDispControl);
    m_dispIncrement->setRange(-10.0, 10.0);
    m_dispIncrement->setDecimals(5);
    formDC->addRow(tr("Incrément (m) :"), m_dispIncrement);
    lay->addWidget(m_groupDispControl);

    auto* groupSystem = new QGroupBox(tr("Système et contraintes"), this);
    auto* formSys = new QFormLayout(groupSystem);
    m_system = new QComboBox(groupSystem);
    m_system->addItem(tr("BandGeneral"), static_cast<int>(SystemSolver::BandGeneral));
    m_system->addItem(tr("ProfileSPD"), static_cast<int>(SystemSolver::ProfileSPD));
    m_system->addItem(tr("SuperLU (creux)"), static_cast<int>(SystemSolver::SuperLU));
    m_system->addItem(tr("UmfPack (creux)"), static_cast<int>(SystemSolver::UmfPack));
    m_system->addItem(tr("BandSPD"), static_cast<int>(SystemSolver::BandSPD));
    m_system->addItem(tr("SparseGEN (creux)"), static_cast<int>(SystemSolver::SparseGEN));
    formSys->addRow(tr("Solveur :"), m_system);
    m_constraints = new QComboBox(groupSystem);
    m_constraints->addItem(tr("Transformation (recommandée)"), static_cast<int>(ConstraintHandler::Transformation));
    m_constraints->addItem(tr("Plain"), static_cast<int>(ConstraintHandler::Plain));
    m_constraints->addItem(tr("Penalty"), static_cast<int>(ConstraintHandler::Penalty));
    m_constraints->addItem(tr("Lagrange"), static_cast<int>(ConstraintHandler::Lagrange));
    formSys->addRow(tr("Constraint handler :"), m_constraints);
    m_extraction = new QComboBox(groupSystem);
    m_extraction->addItem(tr("LIGHT — déplacements, réactions, efforts"), static_cast<int>(ExtractionLevel::Light));
    m_extraction->addItem(tr("ADVANCED — + matrices de rigidité, mapping DDL"), static_cast<int>(ExtractionLevel::Advanced));
    m_extraction->setToolTip(tr("ADVANCED lance un second passage OpenSees (sans charge) pour extraire K_global, "
                                "les rigidités élémentaires et la numérotation des DDL."));
    formSys->addRow(tr("Extraction :"), m_extraction);
    m_maxStiffnessDofs = new QSpinBox(groupSystem);
    m_maxStiffnessDofs->setRange(0, 20000);
    m_maxStiffnessDofs->setSuffix(tr(" DDL"));
    formSys->addRow(tr("Plafond K_global :"), m_maxStiffnessDofs);
    m_kiloNewtons = new QCheckBox(tr("Unités kN, m, kPa"), groupSystem);
    formSys->addRow(QString(), m_kiloNewtons);
    m_saveAllSteps = new QCheckBox(tr("Enregistrer tous les incréments"), groupSystem);
    formSys->addRow(QString(), m_saveAllSteps);
    lay->addWidget(groupSystem);

    connect(m_integrator, &QComboBox::currentIndexChanged, this, [this]() { updateVisibility(); });
    loadSettings(OpenSeesEngine::settingsFromParameters(AnalysisParameters{}));
}

void OpenSeesOptionsWidget::loadSettings(const QJsonObject& settings)
{
    AnalysisContext ctx;
    ctx.engineSettings[OpenSeesEngine::kId] = settings;
    const AnalysisParameters p = OpenSeesEngine::parametersFromContext(ctx);   // une seule conversion JSON

    select(m_trussFormulation, static_cast<int>(p.trussFormulation));
    select(m_geomTransf, static_cast<int>(p.geomTransf));
    select(m_algorithm, static_cast<int>(p.algorithmType));
    select(m_integrator, static_cast<int>(p.integratorType));
    m_numSteps->setValue(p.numSteps);
    m_stepSize->setValue(p.stepSize);
    m_tolerance->setValue(p.tolerance);
    m_maxIterations->setValue(p.maxIterations);
    m_controlNode->setValue(p.controlNodeId);
    select(m_controlDof, p.controlDof);
    m_dispIncrement->setValue(p.dispIncrement);
    select(m_system, static_cast<int>(p.systemSolver));
    select(m_constraints, static_cast<int>(p.constraintHandler));
    select(m_extraction, static_cast<int>(p.extractionLevel));
    m_maxStiffnessDofs->setValue(p.maxGlobalStiffnessDofs);
    m_kiloNewtons->setChecked(p.useKiloNewtons);
    m_saveAllSteps->setChecked(p.saveAllSteps);
    updateVisibility();
}

QJsonObject OpenSeesOptionsWidget::saveSettings() const
{
    AnalysisParameters p;
    p.trussFormulation = static_cast<TrussFormulation>(comboValue(m_trussFormulation));
    p.geomTransf = static_cast<GeomTransfType>(comboValue(m_geomTransf));
    p.algorithmType = p.algorithm = static_cast<NonlinearAlgorithm>(comboValue(m_algorithm));
    p.integratorType = p.integrator = static_cast<IntegratorType>(comboValue(m_integrator));
    p.numSteps = m_numSteps->value();
    p.stepSize = m_stepSize->value();
    p.tolerance = m_tolerance->value();
    p.maxIterations = m_maxIterations->value();
    p.controlNodeId = m_controlNode->value();
    p.controlDof = comboValue(m_controlDof);
    p.dispIncrement = m_dispIncrement->value();
    p.systemSolver = static_cast<SystemSolver>(comboValue(m_system));
    p.constraintHandler = static_cast<ConstraintHandler>(comboValue(m_constraints));
    p.extractionLevel = static_cast<ExtractionLevel>(comboValue(m_extraction));
    p.maxGlobalStiffnessDofs = m_maxStiffnessDofs->value();
    p.useKiloNewtons = m_kiloNewtons->isChecked();
    p.saveAllSteps = m_saveAllSteps->isChecked();
    return OpenSeesEngine::settingsFromParameters(p);
}

void OpenSeesOptionsWidget::setAnalysisContext(const AnalysisContext& context)
{
    m_type = context.type;
    updateVisibility();
}

void OpenSeesOptionsWidget::updateVisibility()
{
    const bool nonlinear = m_type == AnalysisType::NonLinearStatic;
    m_groupNonlinear->setVisible(nonlinear);
    m_groupDispControl->setVisible(nonlinear && comboValue(m_integrator) == static_cast<int>(IntegratorType::DisplacementControl));
}

} // namespace TSA::UI
