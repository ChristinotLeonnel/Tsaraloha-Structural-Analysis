#include "OpenSeesSolver.h"
#include "OpenSeesResultsReader.h"
#include "../Model/Model.h"
#include "../Standards/ModelValidator.h"
#include "../Diagnostics/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QDateTime>

namespace TSA::Analysis
{

OpenSeesSolver::OpenSeesSolver(QObject* parent)
    : QObject(parent)
{
}

OpenSeesSolver::~OpenSeesSolver()
{
    stop();
}

void OpenSeesSolver::stop()
{
    m_stopRequested = true;
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

bool OpenSeesSolver::solveSynchronous(const TSA::Model::Model& model,
                                     const AnalysisParameters& params,
                                     QString* errorMessage)
{
    m_isRunning = true;
    m_stopRequested = false;
    emit analysisStarted();

    // 1. Validation pré-calcul normative (ISO/IEC 25010 §4.2.5, EN 1990)
    emit progressChanged(5, tr("Validation normative et physique du modèle..."));
    auto report = TSA::Standards::ModelValidator::validateForAnalysis(model, params);
    if (!report.isValid())
    {
        m_isRunning = false;
        QString errDetails = tr("Échec de la validation normative avant calcul :\n") + report.summary();
        for (const auto& err : report.formattedErrors())
        {
            errDetails += "\n  • " + QString::fromStdString(err);
        }
        if (errorMessage) *errorMessage = errDetails;
        TSA_LOG_ERROR("OpenSeesSolver", "PreAnalysisValidationError", errDetails.toStdString());
        emit logReceived(QString("[ERREUR NORMATIVE] Échec de validation du modèle avant calcul :\n%1").arg(errDetails));
        emit analysisFinished(false, errDetails);
        return false;
    }

    if (report.hasWarnings())
    {
        for (const auto& warn : report.formattedWarnings())
        {
            TSA_LOG_WARN("OpenSeesSolver", "PreAnalysisWarning", warn);
            emit logReceived(QString("[AVERTISSEMENT] %1").arg(QString::fromStdString(warn)));
        }
    }

    // 2. Capture snapshot immuable (sécurité modèle TSA)
    emit progressChanged(15, tr("Génération du snapshot calculatoire..."));
    CalculationSnapshot snapshot = CalculationSnapshot::capture(model);

    bool ok = executeWorkflow(snapshot, params, errorMessage);
    m_isRunning = false;

    emit analysisFinished(ok, ok ? tr("Calcul OpenSees achevé avec succès.")
                                 : (errorMessage ? *errorMessage : tr("Échec du calcul.")));
    return ok;
}

void OpenSeesSolver::solveAsync(const TSA::Model::Model& model,
                               const AnalysisParameters& params)
{
    if (m_isRunning) return;
    m_isRunning = true;
    m_stopRequested = false;

    emit analysisStarted();
    emit progressChanged(5, tr("Validation normative et physique du modèle..."));

    // Validation pré-calcul sur le thread principal
    auto report = TSA::Standards::ModelValidator::validateForAnalysis(model, params);
    if (!report.isValid())
    {
        m_isRunning = false;
        QString errDetails = tr("Échec de la validation normative avant calcul :\n") + report.summary();
        for (const auto& err : report.formattedErrors())
        {
            errDetails += "\n  • " + QString::fromStdString(err);
        }
        TSA_LOG_ERROR("OpenSeesSolver", "PreAnalysisValidationError", errDetails.toStdString());
        emit logReceived(QString("[ERREUR NORMATIVE] Échec de validation du modèle avant calcul :\n%1").arg(errDetails));
        emit analysisFinished(false, errDetails);
        return;
    }

    if (report.hasWarnings())
    {
        for (const auto& warn : report.formattedWarnings())
        {
            TSA_LOG_WARN("OpenSeesSolver", "PreAnalysisWarning", warn);
            emit logReceived(QString("[AVERTISSEMENT] %1").arg(QString::fromStdString(warn)));
        }
    }

    emit progressChanged(10, tr("Initialisation de l'analyse asynchrone..."));

    // Capture immédiate du snapshot sur le thread principal pour éviter tout accès concurrent
    CalculationSnapshot snapshot = CalculationSnapshot::capture(model);

    QThread* worker = QThread::create([this, snapshot, params]() {
        QString err;
        bool ok = executeWorkflow(snapshot, params, &err);
        m_isRunning = false;
        emit analysisFinished(ok, ok ? tr("Calcul terminé avec succès.") : err);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

bool OpenSeesSolver::executeWorkflow(const CalculationSnapshot& snapshot,
                                     const AnalysisParameters& params,
                                     QString* errorMessage)
{
    // Contrôle des bornes du snapshot
    if (snapshot.nodeCount() == 0)
    {
        if (errorMessage) *errorMessage = tr("Le snapshot calculatoire ne contient aucun nœud.");
        emit logReceived("[ERREUR] Le snapshot calculatoire ne contient aucun nœud.");
        return false;
    }
    if (snapshot.elementCount() == 0)
    {
        if (errorMessage) *errorMessage = tr("Le snapshot calculatoire ne contient aucun élément structural.");
        emit logReceived("[ERREUR] Le snapshot calculatoire ne contient aucun élément structural.");
        return false;
    }

    TSA_LOG_INFO("OpenSeesSolver", "ExecutionWorkflowStarted",
                 "Nodes: " + std::to_string(snapshot.nodeCount()) +
                 ", Elements: " + std::to_string(snapshot.elementCount()));

    // 1. Vérification et disponibilité d'OpenSees
    emit progressChanged(20, tr("Vérification de l'environnement OpenSees..."));
    QString opsErr;
    if (!OpenSeesManager::instance().ensureAvailable(&opsErr))
    {
        if (errorMessage) *errorMessage = tr("Moteur OpenSees indisponible : %1").arg(opsErr);
        emit logReceived(QString("[ERREUR] %1").arg(opsErr));
        return false;
    }

    QString exePath = OpenSeesManager::instance().executablePath();

    // 2. Dossier de travail temporaire isolé
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        if (errorMessage) *errorMessage = tr("Impossible de créer le dossier de calcul temporaire.");
        return false;
    }

    QString workDirPath = tempDir.path();
    AnalysisParameters localParams = params;
    localParams.workingDir = workDirPath.toStdString();

    emit progressChanged(25, tr("Génération du script d'analyse Tcl..."));
    std::string script = OpenSeesAnalysisBuilder::buildScript(snapshot, localParams);

    QString scriptFilePath = workDirPath + "/model.tcl";
    QFile scriptFile(scriptFilePath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage) *errorMessage = tr("Impossible d'écrire le script Tcl.");
        return false;
    }
    scriptFile.write(script.c_str(), static_cast<qint64>(script.size()));
    scriptFile.close();

    // 3. Exécution du processus OpenSees
    emit progressChanged(40, tr("Lancement du solveur OpenSees..."));
    emit logReceived(QString("[TSA] Lancement de %1 sur %2").arg(exePath, scriptFilePath));

    QProcess process;
    m_process = &process;
    process.setWorkingDirectory(workDirPath);
    process.setProgram(exePath);
    process.setArguments(QStringList() << "model.tcl");

    QString stdOutLog;
    QString stdErrLog;

    QObject::connect(&process, &QProcess::readyReadStandardOutput, [&]() {
        QString out = QString::fromUtf8(process.readAllStandardOutput());
        stdOutLog += out;
        QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines)
        {
            emit logReceived(line.trimmed());
        }
    });

    QObject::connect(&process, &QProcess::readyReadStandardError, [&]() {
        QString err = QString::fromUtf8(process.readAllStandardError());
        stdErrLog += err;
        emit logReceived(QString("[STDERR] %1").arg(err.trimmed()));
    });

    process.start();
    if (!process.waitForStarted(5000))
    {
        m_process = nullptr;
        if (errorMessage) *errorMessage = tr("Échec du démarrage d'OpenSees : %1").arg(process.errorString());
        return false;
    }

    emit progressChanged(60, tr("Résolution en cours par OpenSees..."));

    while (process.state() == QProcess::Running)
    {
        if (m_stopRequested)
        {
            process.kill();
            m_process = nullptr;
            if (errorMessage) *errorMessage = tr("Calcul interrompu par l'utilisateur.");
            return false;
        }
        process.waitForFinished(100);
    }

    stdOutLog += QString::fromUtf8(process.readAllStandardOutput());
    stdErrLog += QString::fromUtf8(process.readAllStandardError());

    m_process = nullptr;
    int exitCode = process.exitCode();

    emit progressChanged(85, tr("Analyse du journal et lecture des résultats..."));
    m_results.clearLog();
    m_results.appendLog(stdOutLog.toStdString());
    if (!stdErrLog.isEmpty())
    {
        m_results.appendLog("\n[ERRORS / WARNINGS]\n" + stdErrLog.toStdString());
    }

    // Détection de non-convergence
    if (stdOutLog.contains("CONVERGENCE_FAIL") || stdOutLog.contains("Analysis Failed"))
    {
        if (errorMessage) *errorMessage = tr("Non-convergence détectée lors de la résolution OpenSees.");
        m_results.setValid(false);
        return false;
    }

    if (exitCode != 0)
    {
        if (errorMessage) *errorMessage = tr("OpenSees s'est terminé avec le code d'erreur %1").arg(exitCode);
        m_results.setValid(false);
        return false;
    }

    // 4. Extraction et désérialisation des résultats
    std::string readErr;
    bool readOk = OpenSeesResultsReader::readResults(workDirPath.toStdString(),
                                                   snapshot,
                                                   localParams,
                                                   (stdOutLog + "\n" + stdErrLog).toStdString(),
                                                   m_results,
                                                   &readErr);

    if (!readOk)
    {
        if (errorMessage) *errorMessage = QString::fromStdString(readErr);
        return false;
    }

    emit progressChanged(100, tr("Calcul et post-traitement terminés avec succès."));
    return true;
}

} // namespace TSA::Analysis
