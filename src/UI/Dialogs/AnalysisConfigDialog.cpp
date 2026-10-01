#include "AnalysisConfigDialog.h"
#include "../../Model/Model.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace TSA::UI
{

AnalysisConfigDialog::AnalysisConfigDialog(const TSA::Model::Model* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Configuration du Calcul Structural OpenSees"));
    setMinimumWidth(520);
    resize(540, 680);

    setupUi();
    populateCombos();
    updateFormVisibility();
}

void AnalysisConfigDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 1. Groupe Type d'Analyse
    auto* groupType = new QGroupBox(tr("1. Type d'Analyse & Formulation"), this);
    auto* formType = new QFormLayout(groupType);
    formType->setSpacing(8);

    m_comboType = new QComboBox(groupType);
    m_comboType->addItem(tr("Statique Linéaire"), static_cast<int>(TSA::Analysis::AnalysisType::LinearStatic));
    m_comboType->addItem(tr("Statique Non-Linéaire"), static_cast<int>(TSA::Analysis::AnalysisType::NonLinearStatic));
    m_comboType->addItem(tr("Analyse Modale (Vibrations propres)"), static_cast<int>(TSA::Analysis::AnalysisType::Modal));
    m_comboType->addItem(tr("Dynamique Temporelle"), static_cast<int>(TSA::Analysis::AnalysisType::DynamicTimeHistory));
    m_comboType->addItem(tr("Pushover"), static_cast<int>(TSA::Analysis::AnalysisType::Pushover));
    formType->addRow(tr("Type d'Analyse :"), m_comboType);

    m_comboTrussFormulation = new QComboBox(groupType);
    m_comboTrussFormulation->addItem(tr("Truss (Linéaire standard)"), static_cast<int>(TSA::Analysis::TrussFormulation::Truss));
    m_comboTrussFormulation->addItem(tr("CorotTruss (Corotationnel / Grands Déplacements)"), static_cast<int>(TSA::Analysis::TrussFormulation::CorotTruss));
    m_comboTrussFormulation->addItem(tr("TrussSection (Section discrète)"), static_cast<int>(TSA::Analysis::TrussFormulation::TrussSection));
    m_comboTrussFormulation->addItem(tr("CorotTrussSection (Corotationnel avec section)"), static_cast<int>(TSA::Analysis::TrussFormulation::CorotTrussSection));
    formType->addRow(tr("Formulation Treillis :"), m_comboTrussFormulation);

    m_comboGeomTransf = new QComboBox(groupType);
    m_comboGeomTransf->addItem(tr("Linéaire (Petits déplacements)"), static_cast<int>(TSA::Analysis::GeomTransfType::Linear));
    m_comboGeomTransf->addItem(tr("P-Delta (2nd ordre)"), static_cast<int>(TSA::Analysis::GeomTransfType::PDelta));
    m_comboGeomTransf->addItem(tr("Corotational (Grands déplacements 3D)"), static_cast<int>(TSA::Analysis::GeomTransfType::Corotational));
    formType->addRow(tr("Transformation 3D :"), m_comboGeomTransf);

    mainLayout->addWidget(groupType);

    // 2. Groupe Résolution Non-Linéaire & Itérative
    m_groupNonlinear = new QGroupBox(tr("2. Méthode de Résolution & Intégrateur"), this);
    auto* formNL = new QFormLayout(m_groupNonlinear);
    formNL->setSpacing(8);

    m_comboAlgorithm = new QComboBox(m_groupNonlinear);
    m_comboAlgorithm->addItem(tr("Newton-Raphson standard"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::Newton));
    m_comboAlgorithm->addItem(tr("Newton avec Line Search"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::NewtonLineSearch));
    m_comboAlgorithm->addItem(tr("Modified Newton"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::ModifiedNewton));
    m_comboAlgorithm->addItem(tr("Krylov-Newton"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::KrylovNewton));
    m_comboAlgorithm->addItem(tr("BFGS (Quasi-Newton)"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::BFGS));
    m_comboAlgorithm->addItem(tr("Broyden"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::Broyden));
    m_comboAlgorithm->addItem(tr("Secant Newton"), static_cast<int>(TSA::Analysis::NonlinearAlgorithm::SecantNewton));
    formNL->addRow(tr("Algorithme :"), m_comboAlgorithm);

    m_comboIntegrator = new QComboBox(m_groupNonlinear);
    m_comboIntegrator->addItem(tr("Load Control (Incrément de charge)"), static_cast<int>(TSA::Analysis::IntegratorType::LoadControl));
    m_comboIntegrator->addItem(tr("Displacement Control (Contrôle du déplacement)"), static_cast<int>(TSA::Analysis::IntegratorType::DisplacementControl));
    m_comboIntegrator->addItem(tr("Arc-Length (Longueur d'arc)"), static_cast<int>(TSA::Analysis::IntegratorType::ArcLength));
    m_comboIntegrator->addItem(tr("Min Unbalanced Displacement Norm"), static_cast<int>(TSA::Analysis::IntegratorType::MinUnbalDispNorm));
    formNL->addRow(tr("Intégrateur :"), m_comboIntegrator);

    m_spinNumSteps = new QSpinBox(m_groupNonlinear);
    m_spinNumSteps->setRange(1, 1000);
    m_spinNumSteps->setValue(20);
    formNL->addRow(tr("Nombre de pas d'analyse :"), m_spinNumSteps);

    m_spinStepSize = new QDoubleSpinBox(m_groupNonlinear);
    m_spinStepSize->setRange(1e-5, 10.0);
    m_spinStepSize->setDecimals(4);
    m_spinStepSize->setValue(0.05);
    formNL->addRow(tr("Taille de pas (Δλ ou arc) :"), m_spinStepSize);

    m_spinTolerance = new QDoubleSpinBox(m_groupNonlinear);
    m_spinTolerance->setRange(1e-12, 1e-1);
    m_spinTolerance->setDecimals(8);
    m_spinTolerance->setValue(1e-6);
    formNL->addRow(tr("Tolérance de convergence :"), m_spinTolerance);

    m_spinMaxIterations = new QSpinBox(m_groupNonlinear);
    m_spinMaxIterations->setRange(5, 500);
    m_spinMaxIterations->setValue(50);
    formNL->addRow(tr("Itérations maximales :"), m_spinMaxIterations);

    mainLayout->addWidget(m_groupNonlinear);

    // 3. Groupe Contrôle de Déplacement (Conditionnel)
    m_groupDispControl = new QGroupBox(tr("3. Paramètres de Contrôle du Déplacement"), this);
    auto* formDC = new QFormLayout(m_groupDispControl);
    formDC->setSpacing(8);

    m_spinControlNode = new QSpinBox(m_groupDispControl);
    m_spinControlNode->setRange(1, 999999);
    m_spinControlNode->setValue(1);
    formDC->addRow(tr("Nœud cible contrôlé :"), m_spinControlNode);

    m_comboControlDof = new QComboBox(m_groupDispControl);
    m_comboControlDof->addItem(tr("UX (Translation X)"), 1);
    m_comboControlDof->addItem(tr("UY (Translation Y)"), 2);
    m_comboControlDof->addItem(tr("UZ (Translation Z)"), 3);
    m_comboControlDof->setCurrentIndex(2); // UZ par défaut
    formDC->addRow(tr("DDL contrôlé :"), m_comboControlDof);

    m_spinDispIncr = new QDoubleSpinBox(m_groupDispControl);
    m_spinDispIncr->setRange(-10.0, 10.0);
    m_spinDispIncr->setDecimals(5);
    m_spinDispIncr->setValue(-0.001); // -1 mm
    formDC->addRow(tr("Incrément de déplacement (m) :"), m_spinDispIncr);

    mainLayout->addWidget(m_groupDispControl);

    // 4. Groupe Modal (Conditionnel)
    m_groupModal = new QGroupBox(tr("Paramètres Modaux"), this);
    auto* formModal = new QFormLayout(m_groupModal);
    m_spinEigenmodes = new QSpinBox(m_groupModal);
    m_spinEigenmodes->setRange(1, 30);
    m_spinEigenmodes->setValue(6);
    formModal->addRow(tr("Nombre de modes propres :"), m_spinEigenmodes);
    mainLayout->addWidget(m_groupModal);

    // 5. Groupe Solveur & Conditions
    auto* groupSystem = new QGroupBox(tr("Solveur Système & Contraintes"), this);
    auto* formSystem = new QFormLayout(groupSystem);
    formSystem->setSpacing(8);

    m_comboSystem = new QComboBox(groupSystem);
    m_comboSystem->addItem(tr("BandGeneral (Standard LAPACK)"), static_cast<int>(TSA::Analysis::SystemSolver::BandGeneral));
    m_comboSystem->addItem(tr("ProfileSPD (Symétrique défini positif)"), static_cast<int>(TSA::Analysis::SystemSolver::ProfileSPD));
    m_comboSystem->addItem(tr("SuperLU (Creux direct performant)"), static_cast<int>(TSA::Analysis::SystemSolver::SuperLU));
    m_comboSystem->addItem(tr("UmfPack (Creux non symétrique)"), static_cast<int>(TSA::Analysis::SystemSolver::UmfPack));
    m_comboSystem->addItem(tr("BandSPD (Bande symétrique défini positif)"), static_cast<int>(TSA::Analysis::SystemSolver::BandSPD));
    m_comboSystem->addItem(tr("SparseGEN (Creux général)"), static_cast<int>(TSA::Analysis::SystemSolver::SparseGEN));
    formSystem->addRow(tr("Solveur linéaire système :"), m_comboSystem);

    m_comboConstraints = new QComboBox(groupSystem);
    m_comboConstraints->addItem(tr("Transformation (Recommandée)"), static_cast<int>(TSA::Analysis::ConstraintHandler::Transformation));
    m_comboConstraints->addItem(tr("Plain (Sans transformation)"), static_cast<int>(TSA::Analysis::ConstraintHandler::Plain));
    m_comboConstraints->addItem(tr("Penalty (Méthode des pénalités)"), static_cast<int>(TSA::Analysis::ConstraintHandler::Penalty));
    m_comboConstraints->addItem(tr("Lagrange (Multiplicateurs de Lagrange)"), static_cast<int>(TSA::Analysis::ConstraintHandler::Lagrange));
    formSystem->addRow(tr("Gestion des contraintes :"), m_comboConstraints);

    mainLayout->addWidget(groupSystem);

    // 6. Options globales
    auto* optLayout = new QHBoxLayout();
    m_checkSelfWeight = new QCheckBox(tr("Inclure le poids propre"), this);
    m_checkSelfWeight->setChecked(true);
    optLayout->addWidget(m_checkSelfWeight);

    m_checkKiloNewtons = new QCheckBox(tr("Unités kN, m, kPa"), this);
    m_checkKiloNewtons->setChecked(true);
    optLayout->addWidget(m_checkKiloNewtons);

    m_checkSaveAllSteps = new QCheckBox(tr("Enregistrer tous les incréments"), this);
    m_checkSaveAllSteps->setChecked(true);
    optLayout->addWidget(m_checkSaveAllSteps);

    mainLayout->addLayout(optLayout);

    // 7. Boutons d'action
    auto* btnLayout = new QHBoxLayout();
    auto* btnCancel = new QPushButton(tr("Annuler"), this);
    auto* btnSolve = new QPushButton(tr("Lancer le calcul OpenSees"), this);
    btnSolve->setDefault(true);
    btnSolve->setStyleSheet("QPushButton { font-weight: bold; background-color: #2563EB; color: white; padding: 8px 16px; border-radius: 4px; }"
                            "QPushButton:hover { background-color: #1D4ED8; }");

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnSolve, &QPushButton::clicked, this, &AnalysisConfigDialog::onValidateAndSolve);

    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnSolve);
    mainLayout->addLayout(btnLayout);

    // Connexions interactives de filtrage
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisConfigDialog::onAnalysisTypeChanged);
    connect(m_comboIntegrator, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisConfigDialog::onIntegratorChanged);
}

void AnalysisConfigDialog::populateCombos()
{
    if (m_model && !m_model->nodes().empty())
    {
        // Choisir un premier nœud existant par défaut pour le contrôle de déplacement
        int firstNodeId = m_model->nodes().begin()->first;
        m_spinControlNode->setValue(firstNodeId);
    }
}

void AnalysisConfigDialog::updateFormVisibility()
{
    auto type = static_cast<TSA::Analysis::AnalysisType>(m_comboType->currentData().toInt());

    bool isNonLinear = (type == TSA::Analysis::AnalysisType::NonLinearStatic ||
                        type == TSA::Analysis::AnalysisType::Pushover);
    bool isModal = (type == TSA::Analysis::AnalysisType::Modal);

    m_groupNonlinear->setVisible(isNonLinear);
    m_groupModal->setVisible(isModal);

    if (isNonLinear)
    {
        auto intType = static_cast<TSA::Analysis::IntegratorType>(m_comboIntegrator->currentData().toInt());
        m_groupDispControl->setVisible(intType == TSA::Analysis::IntegratorType::DisplacementControl);
    }
    else
    {
        m_groupDispControl->setVisible(false);
    }

    // Filtrage des transformations géométriques : Corotational disponible en non-linéaire
    if (isNonLinear)
    {
        if (m_comboGeomTransf->currentIndex() == 0)
        {
            m_comboGeomTransf->setCurrentIndex(2); // Basculer sur Corotational par défaut
        }
    }
    else
    {
        m_comboGeomTransf->setCurrentIndex(0); // Linéaire par défaut
    }
}

void AnalysisConfigDialog::onAnalysisTypeChanged(int /*index*/)
{
    updateFormVisibility();
}

void AnalysisConfigDialog::onIntegratorChanged(int /*index*/)
{
    updateFormVisibility();
}

void AnalysisConfigDialog::onValidateAndSolve()
{
    if (m_model && m_model->nodes().empty())
    {
        QMessageBox::warning(this, tr("Calcul"), tr("Le modèle ne contient aucun nœud."));
        return;
    }

    // Récupérer les paramètres
    m_params.type = static_cast<TSA::Analysis::AnalysisType>(m_comboType->currentData().toInt());
    m_params.trussFormulation = static_cast<TSA::Analysis::TrussFormulation>(m_comboTrussFormulation->currentData().toInt());
    m_params.geomTransf = static_cast<TSA::Analysis::GeomTransfType>(m_comboGeomTransf->currentData().toInt());

    m_params.algorithmType = static_cast<TSA::Analysis::NonlinearAlgorithm>(m_comboAlgorithm->currentData().toInt());
    m_params.integratorType = static_cast<TSA::Analysis::IntegratorType>(m_comboIntegrator->currentData().toInt());

    m_params.numSteps = m_spinNumSteps->value();
    m_params.stepSize = m_spinStepSize->value();
    m_params.tolerance = m_spinTolerance->value();
    m_params.maxIterations = m_spinMaxIterations->value();

    m_params.controlNodeId = m_spinControlNode->value();
    m_params.controlDof = m_comboControlDof->currentData().toInt();
    m_params.dispIncrement = m_spinDispIncr->value();

    m_params.numEigenmodes = m_spinEigenmodes->value();

    m_params.systemSolver = static_cast<TSA::Analysis::SystemSolver>(m_comboSystem->currentData().toInt());
    m_params.constraintHandler = static_cast<TSA::Analysis::ConstraintHandler>(m_comboConstraints->currentData().toInt());

    m_params.includeSelfWeight = m_checkSelfWeight->isChecked();
    m_params.useKiloNewtons = m_checkKiloNewtons->isChecked();
    m_params.saveAllSteps = m_checkSaveAllSteps->isChecked();

    accept();
}

TSA::Analysis::AnalysisParameters AnalysisConfigDialog::parameters() const
{
    return m_params;
}

void AnalysisConfigDialog::setParameters(const TSA::Analysis::AnalysisParameters& params)
{
    m_params = params;

    int typeIdx = m_comboType->findData(static_cast<int>(params.type));
    if (typeIdx >= 0) m_comboType->setCurrentIndex(typeIdx);

    int trussIdx = m_comboTrussFormulation->findData(static_cast<int>(params.trussFormulation));
    if (trussIdx >= 0) m_comboTrussFormulation->setCurrentIndex(trussIdx);

    int gtIdx = m_comboGeomTransf->findData(static_cast<int>(params.geomTransf));
    if (gtIdx >= 0) m_comboGeomTransf->setCurrentIndex(gtIdx);

    int algoIdx = m_comboAlgorithm->findData(static_cast<int>(params.algorithmType));
    if (algoIdx >= 0) m_comboAlgorithm->setCurrentIndex(algoIdx);

    int intIdx = m_comboIntegrator->findData(static_cast<int>(params.integratorType));
    if (intIdx >= 0) m_comboIntegrator->setCurrentIndex(intIdx);

    m_spinNumSteps->setValue(params.numSteps);
    m_spinStepSize->setValue(params.stepSize);
    m_spinTolerance->setValue(params.tolerance);
    m_spinMaxIterations->setValue(params.maxIterations);

    m_spinControlNode->setValue(params.controlNodeId);
    int dofIdx = m_comboControlDof->findData(params.controlDof);
    if (dofIdx >= 0) m_comboControlDof->setCurrentIndex(dofIdx);
    m_spinDispIncr->setValue(params.dispIncrement);

    m_spinEigenmodes->setValue(params.numEigenmodes);

    int sysIdx = m_comboSystem->findData(static_cast<int>(params.systemSolver));
    if (sysIdx >= 0) m_comboSystem->setCurrentIndex(sysIdx);

    int chIdx = m_comboConstraints->findData(static_cast<int>(params.constraintHandler));
    if (chIdx >= 0) m_comboConstraints->setCurrentIndex(chIdx);

    m_checkSelfWeight->setChecked(params.includeSelfWeight);
    m_checkKiloNewtons->setChecked(params.useKiloNewtons);
    m_checkSaveAllSteps->setChecked(params.saveAllSteps);

    updateFormVisibility();
}

} // namespace TSA::UI
