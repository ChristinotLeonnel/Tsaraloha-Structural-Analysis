#pragma once

// « Configuration IA » : analyse du système et configuration recommandée (installation en un clic),
// gestion des modèles, mode LOCAL / CLOUD / AUTO et confidentialité, diagnostic, journal.

#include "../../AI/Core/AIOrchestrator.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace TSA::UI
{

class AIRuntimeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AIRuntimeDialog(TSA::AI::AIOrchestrator* orchestrator, QWidget* parent = nullptr);

    enum Page { SetupPage = 0, ModelsPage, PrivacyPage, DiagnosticsPage, LogPage };
    void showPage(Page page);

private:
    QWidget* buildSetupPage();
    QWidget* buildModelsPage();
    QWidget* buildPrivacyPage();
    QWidget* buildDiagnosticsPage();
    QWidget* buildLogPage();

    void refreshSetup();
    void refreshModels();
    void refreshLog();
    void installRecommended();
    void savePrivacy();
    QString selectedModelId() const;

private:
    TSA::AI::AIOrchestrator* m_ai = nullptr;
    QTabWidget* m_tabs = nullptr;

    // Configuration
    QLabel* m_systemLabel = nullptr;
    QLabel* m_recommendLabel = nullptr;
    QPushButton* m_btnInstall = nullptr;
    QPushButton* m_btnCalibrate = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_progressLabel = nullptr;
    bool m_installAfterDownload = false;

    // Modèles
    QTableWidget* m_models = nullptr;
    QPushButton* m_btnDownload = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnDefault = nullptr;
    QPushButton* m_btnCancelDownload = nullptr;

    // Mode & confidentialité
    QButtonGroup* m_modeGroup = nullptr;
    QCheckBox* m_chkAlwaysAsk = nullptr;
    QCheckBox* m_chkNeverFiles = nullptr;
    QCheckBox* m_chkAutoAllow = nullptr;
    QCheckBox* m_chkAutoStart = nullptr;
    QComboBox* m_cloudPreset = nullptr;
    QLineEdit* m_cloudUrl = nullptr;
    QLineEdit* m_cloudModel = nullptr;
    QLineEdit* m_cloudKey = nullptr;
    QComboBox* m_device = nullptr;

    // Diagnostic / journal
    QPlainTextEdit* m_diag = nullptr;
    QPushButton* m_btnDiag = nullptr;
    QTableWidget* m_log = nullptr;
};

} // namespace TSA::UI
