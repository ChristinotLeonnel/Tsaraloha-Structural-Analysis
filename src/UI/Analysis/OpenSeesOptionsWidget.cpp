#include "OpenSeesOptionsWidget.h"

#include "../../Analysis/Engines/OpenSees/OpenSeesEngine.h"
#include "../../Analysis/OpenSeesManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
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
    lay->addWidget(groupSystem);

    // Exécutable OpenSees : réglage de l'application, hors réglages du projet (BUG-102 : seule la clé QSettings
    // OpenSees/ExecutablePath permettait de l'imposer, à la main)
    auto* groupExe = new QGroupBox(tr("Exécutable OpenSees"), this);
    auto* exeLayout = new QVBoxLayout(groupExe);
    m_executable = new QLabel(groupExe);
    m_executable->setWordWrap(true);
    m_executable->setTextInteractionFlags(Qt::TextSelectableByMouse);
    exeLayout->addWidget(m_executable);
    auto* buttons = new QHBoxLayout;
    auto* choose = new QPushButton(tr("Choisir…"), groupExe);
    auto* reset = new QPushButton(tr("Par défaut"), groupExe);
    reset->setToolTip(tr("Emplacement par défaut : dossier de l'application, puis PATH"));
    buttons->addWidget(choose);
    buttons->addWidget(reset);
    buttons->addStretch(1);
    exeLayout->addLayout(buttons);
    lay->addWidget(groupExe);
    connect(choose, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Exécutable OpenSees"), QString(),
#ifdef Q_OS_WIN
                                                          tr("Programmes (*.exe)")
#else
                                                          QString()
#endif
        );
        if (path.isEmpty()) return;
        // Vérifié avant d'être retenu : commande de contrôle exécutée (interpréteur Tcl et commandes OpenSees)
        const auto check = OpenSeesManager::instance().checkExecutable(path);
        if (!check.ok())
        {
            QMessageBox::warning(this, tr("Exécutable OpenSees"), tr("Ce programme n'est pas un OpenSees opérationnel :\n%1").arg(check.message));
            return;
        }
        OpenSeesManager::instance().setCustomExecutablePath(path);
        refreshExecutable();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        OpenSeesManager::instance().resetToDefaultPath();
        refreshExecutable();
    });
    refreshExecutable();

    loadSettings(OpenSeesEngine::settingsFromParameters(AnalysisParameters{}));
}

void OpenSeesOptionsWidget::refreshExecutable()
{
    if (!m_executable) return;
    const QString path = OpenSeesManager::instance().executablePath();
    m_executable->setText(path.isEmpty() ? tr("Aucun exécutable trouvé (dossier de l'application, PATH).") : QDir::toNativeSeparators(path));
}

void OpenSeesOptionsWidget::loadSettings(const QJsonObject& settings)
{
    AnalysisContext ctx;
    ctx.engineSettings[OpenSeesEngine::kId] = settings;
    const AnalysisParameters p = OpenSeesEngine::parametersFromContext(ctx);   // une seule conversion JSON

    select(m_system, static_cast<int>(p.systemSolver));
    select(m_constraints, static_cast<int>(p.constraintHandler));
    select(m_extraction, static_cast<int>(p.extractionLevel));
    m_maxStiffnessDofs->setValue(p.maxGlobalStiffnessDofs);
    m_kiloNewtons->setChecked(p.useKiloNewtons);
}

QJsonObject OpenSeesOptionsWidget::saveSettings() const
{
    AnalysisParameters p;
    p.systemSolver = static_cast<SystemSolver>(comboValue(m_system));
    p.constraintHandler = static_cast<ConstraintHandler>(comboValue(m_constraints));
    p.extractionLevel = static_cast<ExtractionLevel>(comboValue(m_extraction));
    p.maxGlobalStiffnessDofs = m_maxStiffnessDofs->value();
    p.useKiloNewtons = m_kiloNewtons->isChecked();
    return OpenSeesEngine::settingsFromParameters(p);
}

} // namespace TSA::UI
