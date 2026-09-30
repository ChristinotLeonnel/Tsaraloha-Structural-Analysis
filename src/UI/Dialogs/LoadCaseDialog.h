#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/**
 * @brief Dialogue centralisé de gestion des cas de charge (Load Cases),
 * combinaisons d'actions (Combinations), validation structurelle et export OpenSees.
 */
class LoadCaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadCaseDialog(TSA::Model::Model* model, QWidget* parent = nullptr);
    ~LoadCaseDialog() override = default;

private slots:
    void onAddLoadCase();
    void onRemoveLoadCase();
    void onResetEurocodes();

    void onAddCombination();
    void onRemoveCombination();

    void onValidateModel();
    void onGenerateOpenSeesScript();
    void onExportTclFile();

private:
    void setupUI();
    void refreshCasesTable();
    void refreshCombinationsTable();

    TSA::Model::Model* m_model = nullptr;

    QTabWidget* m_tabWidget = nullptr;

    // Onglet 1 : Cas de charge
    QTableWidget* m_tableCases = nullptr;
    QPushButton* m_btnAddCase = nullptr;
    QPushButton* m_btnRemoveCase = nullptr;
    QPushButton* m_btnResetEurocodes = nullptr;

    // Onglet 2 : Combinaisons
    QTableWidget* m_tableCombinations = nullptr;
    QPushButton* m_btnAddCombo = nullptr;
    QPushButton* m_btnRemoveCombo = nullptr;

    // Onglet 3 : OpenSees & Validation
    QLabel* m_lblValidationStatus = nullptr;
    QTextEdit* m_txtValidationReport = nullptr;
    QPushButton* m_btnValidate = nullptr;
    QPushButton* m_btnGenerateScript = nullptr;
    QPushButton* m_btnExportTcl = nullptr;
    QTextEdit* m_txtScriptPreview = nullptr;
};

} // namespace TSA::UI
