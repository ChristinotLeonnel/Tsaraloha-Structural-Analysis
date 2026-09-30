#include "LoadCaseDialog.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Analysis/LoadValidation.h"
#include "../../Analysis/OpenSeesAdapter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFontDatabase>

namespace TSA::UI
{

LoadCaseDialog::LoadCaseDialog(TSA::Model::Model* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Cas de Charges, Combinaisons & Solveur OpenSees"));
    resize(780, 560);
    setupUI();
    refreshCasesTable();
    refreshCombinationsTable();
    onValidateModel();
}

void LoadCaseDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget(this);

    // =========================================================================
    // ONGLET 1 : Cas de charges
    // =========================================================================
    auto* tabCases = new QWidget(this);
    auto* casesLayout = new QVBoxLayout(tabCases);

    m_tableCases = new QTableWidget(this);
    m_tableCases->setColumnCount(5);
    m_tableCases->setHorizontalHeaderLabels({ tr("ID"), tr("Nom"), tr("Catégorie"), tr("Poids propre"), tr("Description") });
    m_tableCases->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableCases->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableCases->setEditTriggers(QAbstractItemView::NoEditTriggers);
    casesLayout->addWidget(m_tableCases);

    auto* casesBtnLayout = new QHBoxLayout();
    m_btnAddCase = new QPushButton(tr("➕ Ajouter un Cas..."), this);
    m_btnRemoveCase = new QPushButton(tr("🗑️ Supprimer"), this);
    m_btnResetEurocodes = new QPushButton(tr("↺ Réinitialiser Eurocodes (G, Q, W, S, E)"), this);
    casesBtnLayout->addWidget(m_btnAddCase);
    casesBtnLayout->addWidget(m_btnRemoveCase);
    casesBtnLayout->addStretch();
    casesBtnLayout->addWidget(m_btnResetEurocodes);
    casesLayout->addLayout(casesBtnLayout);

    m_tabWidget->addTab(tabCases, tr("1. Cas de Charges (Patterns)"));

    // =========================================================================
    // ONGLET 2 : Combinaisons d'actions
    // =========================================================================
    auto* tabCombos = new QWidget(this);
    auto* combosLayout = new QVBoxLayout(tabCombos);

    m_tableCombinations = new QTableWidget(this);
    m_tableCombinations->setColumnCount(4);
    m_tableCombinations->setHorizontalHeaderLabels({ tr("ID"), tr("Nom"), tr("Type"), tr("Formule de Combinaison") });
    m_tableCombinations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableCombinations->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableCombinations->setEditTriggers(QAbstractItemView::NoEditTriggers);
    combosLayout->addWidget(m_tableCombinations);

    auto* combosBtnLayout = new QHBoxLayout();
    m_btnAddCombo = new QPushButton(tr("➕ Nouvelle Combinaison..."), this);
    m_btnRemoveCombo = new QPushButton(tr("🗑️ Supprimer"), this);
    combosBtnLayout->addWidget(m_btnAddCombo);
    combosBtnLayout->addWidget(m_btnRemoveCombo);
    combosBtnLayout->addStretch();
    combosLayout->addLayout(combosBtnLayout);

    m_tabWidget->addTab(tabCombos, tr("2. Combinaisons d'Actions"));

    // =========================================================================
    // ONGLET 3 : OpenSees & Validation
    // =========================================================================
    auto* tabOpenSees = new QWidget(this);
    auto* osLayout = new QVBoxLayout(tabOpenSees);

    m_lblValidationStatus = new QLabel(this);
    m_lblValidationStatus->setStyleSheet("font-weight: bold; padding: 4px; border-radius: 4px;");
    osLayout->addWidget(m_lblValidationStatus);

    m_txtValidationReport = new QTextEdit(this);
    m_txtValidationReport->setMaximumHeight(100);
    m_txtValidationReport->setReadOnly(true);
    osLayout->addWidget(m_txtValidationReport);

    auto* scriptBtnLayout = new QHBoxLayout();
    m_btnValidate = new QPushButton(tr("🔍 Valider le Modèle"), this);
    m_btnGenerateScript = new QPushButton(tr("⚡ Générer Script OpenSees (Tcl)"), this);
    m_btnGenerateScript->setStyleSheet("background-color: #00adb5; color: white; font-weight: bold;");
    m_btnExportTcl = new QPushButton(tr("💾 Exporter fichier .tcl..."), this);
    scriptBtnLayout->addWidget(m_btnValidate);
    scriptBtnLayout->addWidget(m_btnGenerateScript);
    scriptBtnLayout->addWidget(m_btnExportTcl);
    scriptBtnLayout->addStretch();
    osLayout->addLayout(scriptBtnLayout);

    m_txtScriptPreview = new QTextEdit(this);
    m_txtScriptPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_txtScriptPreview->setPlaceholderText(tr("Cliquez sur 'Générer Script OpenSees' pour visualiser le code Tcl de calcul."));
    osLayout->addWidget(m_txtScriptPreview);

    m_tabWidget->addTab(tabOpenSees, tr("3. Validation & OpenSees"));

    mainLayout->addWidget(m_tabWidget);

    auto* bottomBtnLayout = new QHBoxLayout();
    auto* btnClose = new QPushButton(tr("Fermer"), this);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomBtnLayout);

    // Connexions
    connect(m_btnAddCase, &QPushButton::clicked, this, &LoadCaseDialog::onAddLoadCase);
    connect(m_btnRemoveCase, &QPushButton::clicked, this, &LoadCaseDialog::onRemoveLoadCase);
    connect(m_btnResetEurocodes, &QPushButton::clicked, this, &LoadCaseDialog::onResetEurocodes);

    connect(m_btnAddCombo, &QPushButton::clicked, this, &LoadCaseDialog::onAddCombination);
    connect(m_btnRemoveCombo, &QPushButton::clicked, this, &LoadCaseDialog::onRemoveCombination);

    connect(m_btnValidate, &QPushButton::clicked, this, &LoadCaseDialog::onValidateModel);
    connect(m_btnGenerateScript, &QPushButton::clicked, this, &LoadCaseDialog::onGenerateOpenSeesScript);
    connect(m_btnExportTcl, &QPushButton::clicked, this, &LoadCaseDialog::onExportTclFile);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void LoadCaseDialog::refreshCasesTable()
{
    m_tableCases->setRowCount(0);
    if (!m_model) return;

    int row = 0;
    for (const auto& [id, lc] : m_model->loadManager().loadCases())
    {
        m_tableCases->insertRow(row);
        m_tableCases->setItem(row, 0, new QTableWidgetItem(QString::number(id)));
        m_tableCases->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(lc.name())));
        m_tableCases->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(TSA::Model::loadCategoryToString(lc.category()))));
        m_tableCases->setItem(row, 3, new QTableWidgetItem(lc.isSelfWeightIncluded() ? tr("Oui (x%1)").arg(lc.selfWeightFactor()) : tr("Non")));
        m_tableCases->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(lc.description())));
        row++;
    }
}

void LoadCaseDialog::refreshCombinationsTable()
{
    m_tableCombinations->setRowCount(0);
    if (!m_model) return;

    int row = 0;
    for (const auto& [id, combo] : m_model->loadManager().combinations())
    {
        m_tableCombinations->insertRow(row);
        m_tableCombinations->setItem(row, 0, new QTableWidgetItem(QString::number(id)));
        m_tableCombinations->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(combo.name())));

        QString typeStr = tr("Personnalisée");
        switch (combo.type())
        {
        case TSA::Model::LoadCombinationType::ULS_Fundamental: typeStr = tr("ELU Fondamental"); break;
        case TSA::Model::LoadCombinationType::ULS_Accidental:  typeStr = tr("ELU Accidentel"); break;
        case TSA::Model::LoadCombinationType::ULS_Seismic:     typeStr = tr("ELU Sismique"); break;
        case TSA::Model::LoadCombinationType::SLS_Characteristic: typeStr = tr("ELS Caractéristique"); break;
        case TSA::Model::LoadCombinationType::SLS_Frequent:       typeStr = tr("ELS Fréquente"); break;
        case TSA::Model::LoadCombinationType::SLS_QuasiPermanent: typeStr = tr("ELS Quasi-permanente"); break;
        default: break;
        }
        m_tableCombinations->setItem(row, 2, new QTableWidgetItem(typeStr));
        m_tableCombinations->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(combo.formula(m_model->loadManager().loadCases()))));
        row++;
    }
}

void LoadCaseDialog::onAddLoadCase()
{
    if (!m_model) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Nouveau Cas de Charge"), tr("Nom du cas (ex: Q2, Vent_Y, Neige) :"), QLineEdit::Normal, "Q2", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QStringList cats = { tr("Dead (Permanent)"), tr("Live (Exploitation)"), tr("Wind (Vent)"), tr("Snow (Neige)"), tr("Seismic (Séisme)") };
    QString catChoice = QInputDialog::getItem(this, tr("Catégorie"), tr("Catégorie Eurocode :"), cats, 1, false, &ok);
    if (!ok) return;

    TSA::Model::LoadCaseCategory cat = TSA::Model::LoadCaseCategory::Live;
    if (catChoice.contains("Dead")) cat = TSA::Model::LoadCaseCategory::Dead;
    else if (catChoice.contains("Wind")) cat = TSA::Model::LoadCaseCategory::Wind;
    else if (catChoice.contains("Snow")) cat = TSA::Model::LoadCaseCategory::Snow;
    else if (catChoice.contains("Seismic")) cat = TSA::Model::LoadCaseCategory::Seismic;

    m_model->pushUndoState("Créer Cas de Charge");
    TSA::Model::LoadCase lc(0, name.trimmed().toStdString(), cat, false, 1.0, "");
    m_model->loadManager().addLoadCase(lc);
    refreshCasesTable();
}

void LoadCaseDialog::onRemoveLoadCase()
{
    int row = m_tableCases->currentRow();
    if (row < 0 || !m_model) return;

    int caseId = m_tableCases->item(row, 0)->text().toInt();
    if (QMessageBox::question(this, tr("Confirmation"),
                              tr("Voulez-vous supprimer le cas de charge #%1 et toutes ses charges associées ?").arg(caseId))
        == QMessageBox::Yes)
    {
        m_model->pushUndoState("Supprimer Cas de Charge");
        m_model->loadManager().removeLoadCase(caseId);
        refreshCasesTable();
        refreshCombinationsTable();
    }
}

void LoadCaseDialog::onResetEurocodes()
{
    if (!m_model) return;
    if (QMessageBox::question(this, tr("Réinitialisation"),
                              tr("Voulez-vous réinitialiser les cas de charges et combinaisons selon les valeurs Eurocode par défaut ?"))
        == QMessageBox::Yes)
    {
        m_model->pushUndoState("Réinitialiser Cas Eurocodes");
        m_model->loadManager().resetToDefaults();
        refreshCasesTable();
        refreshCombinationsTable();
        onValidateModel();
    }
}

void LoadCaseDialog::onAddCombination()
{
    if (!m_model) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Nouvelle Combinaison"), tr("Nom de la combinaison :"), QLineEdit::Normal, "1.35G + 1.5Q", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_model->pushUndoState("Créer Combinaison");
    TSA::Model::LoadCombination combo(0, name.trimmed().toStdString(), TSA::Model::LoadCombinationType::ULS_Fundamental);
    // Par défaut associer le cas 1 (G) à 1.35 et le cas 2 (Q) à 1.50 s'ils existent
    if (m_model->loadManager().getLoadCase(1)) combo.setFactor(1, 1.35);
    if (m_model->loadManager().getLoadCase(2)) combo.setFactor(2, 1.50);

    m_model->loadManager().addCombination(combo);
    refreshCombinationsTable();
}

void LoadCaseDialog::onRemoveCombination()
{
    int row = m_tableCombinations->currentRow();
    if (row < 0 || !m_model) return;

    int comboId = m_tableCombinations->item(row, 0)->text().toInt();
    m_model->pushUndoState("Supprimer Combinaison");
    m_model->loadManager().removeCombination(comboId);
    refreshCombinationsTable();
}

void LoadCaseDialog::onValidateModel()
{
    if (!m_model) return;
    TSA::Analysis::ValidationReport rep = TSA::Analysis::LoadValidation::validateModel(*m_model);

    if (rep.isValid())
    {
        if (rep.hasWarnings())
        {
            m_lblValidationStatus->setText(tr("⚠️ Modèle Validé avec %1 Avertissement(s)").arg(rep.warnings().size()));
            m_lblValidationStatus->setStyleSheet("background-color: #ffeaa7; color: #d63031; font-weight: bold; padding: 6px;");
        }
        else
        {
            m_lblValidationStatus->setText(tr("✅ Modèle Valide pour OpenSees"));
            m_lblValidationStatus->setStyleSheet("background-color: #55efc4; color: #00b894; font-weight: bold; padding: 6px;");
        }
    }
    else
    {
        m_lblValidationStatus->setText(tr("❌ Échec de Validation : %1 Erreur(s)").arg(rep.errors().size()));
        m_lblValidationStatus->setStyleSheet("background-color: #ff7675; color: white; font-weight: bold; padding: 6px;");
    }

    QString reportText;
    for (const auto& err : rep.errors())
    {
        reportText += "❌ " + QString::fromStdString(err) + "\n";
    }
    for (const auto& w : rep.warnings())
    {
        reportText += "⚠️ " + QString::fromStdString(w) + "\n";
    }
    if (reportText.isEmpty())
    {
        reportText = tr("Tous les éléments, nœuds, appuis, cas et combinaisons sont conformes aux spécifications OpenSees.");
    }
    m_txtValidationReport->setText(reportText);
}

void LoadCaseDialog::onGenerateOpenSeesScript()
{
    if (!m_model) return;
    onValidateModel();

    TSA::Analysis::OpenSeesOptions opt;
    opt.includeSelfWeight = true;
    opt.includeAnalysisCommands = true;
    opt.useKiloNewtons = true;

    std::string script = TSA::Analysis::OpenSeesAdapter::generateTclScript(*m_model, opt);
    m_txtScriptPreview->setText(QString::fromStdString(script));
}

void LoadCaseDialog::onExportTclFile()
{
    if (!m_model) return;
    QString path = QFileDialog::getSaveFileName(this, tr("Exporter Modèle OpenSees"), "model.tcl", tr("Scripts OpenSees Tcl (*.tcl)"));
    if (path.isEmpty()) return;

    TSA::Analysis::OpenSeesOptions opt;
    opt.includeSelfWeight = true;
    opt.includeAnalysisCommands = true;
    opt.useKiloNewtons = true;

    if (TSA::Analysis::OpenSeesAdapter::exportToFile(path.toStdString(), *m_model, opt))
    {
        QMessageBox::information(this, tr("Export Réussi"), tr("Script OpenSees exporté avec succès dans :\n%1").arg(path));
    }
    else
    {
        QMessageBox::critical(this, tr("Erreur"), tr("Impossible d'écrire le fichier sur le disque."));
    }
}

} // namespace TSA::UI
