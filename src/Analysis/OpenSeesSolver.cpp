#include "OpenSeesSolver.h"
#include "OpenSeesResultsReader.h"
#include "../Model/Model.h"
#include "../Standards/ModelValidator.h"
#include "OpenSeesModelMap.h"
#include <QElapsedTimer>
#include "../Standards/NationalAnnexConfig.h"
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
    // Le QProcess appartient au thread de calcul : on ne le manipule pas d'ici, la boucle d'attente
    // du calcul (attente par tranches de 100 ms) voit la demande et arrête le processus.
    m_stopRequested = true;
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

bool OpenSeesSolver::solveSnapshot(const CalculationSnapshot& snapshot,
                                   const AnalysisParameters& params,
                                   QString* errorMessage)
{
    if (m_isRunning)
    {
        if (errorMessage) *errorMessage = tr("Un calcul OpenSees est déjà en cours.");
        return false;
    }
    m_isRunning = true;
    m_stopRequested = false;
    m_results.clear();
    emit analysisStarted();

    const bool ok = executeWorkflow(snapshot, params, errorMessage);
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

    // Correspondance TSA ↔ OpenSees unique, partagée par le script et la lecture des résultats.
    const OpenSeesModelMap opsMap = OpenSeesModelMap::build(snapshot, localParams);
    const auto mapErrors = opsMap.validate(snapshot);
    if (!mapErrors.empty())
    {
        QString details = tr("Correspondance TSA/OpenSees incohérente :");
        for (const auto& e : mapErrors) details += "\n  • " + QString::fromStdString(e);
        if (errorMessage) *errorMessage = details;
        emit logReceived("[ERREUR] " + details);
        return false;
    }

    emit progressChanged(25, tr("Génération du script d'analyse Tcl..."));
    std::string script = OpenSeesAnalysisBuilder::buildScript(snapshot, opsMap, localParams);

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
                                                   opsMap,
                                                   localParams,
                                                   m_results,
                                                   &readErr);

    if (!readOk)
    {
        if (errorMessage) *errorMessage = QString::fromStdString(readErr);
        return false;
    }

    // 5. Passage « matrices » (mode Advanced uniquement) : script séparé, l'analyse principale
    //    et son solveur ne sont pas modifiés. Un échec ici n'invalide pas les résultats courants.
    if (localParams.extractionLevel == ExtractionLevel::Advanced)
    {
        emit progressChanged(90, tr("Extraction des matrices de rigidité (passage séparé)..."));
        const bool withK = opsMap.estimatedFreeDofs() <= localParams.maxGlobalStiffnessDofs;
        const std::string mscript = OpenSeesAnalysisBuilder::buildMatrixScript(snapshot, opsMap, localParams, withK);
        QFile mfile(workDirPath + "/" + QString::fromStdString(localParams.matrixScriptFile));
        QString matrixErr;
        bool matrixOk = false;
        QElapsedTimer timer;
        timer.start();
        if (mfile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            mfile.write(mscript.c_str(), static_cast<qint64>(mscript.size()));
            mfile.close();
            QProcess mproc;
            mproc.setWorkingDirectory(workDirPath);
            mproc.setProgram(exePath);
            mproc.setArguments(QStringList() << QString::fromStdString(localParams.matrixScriptFile));
            mproc.start();
            if (mproc.waitForStarted(5000))
            {
                while (mproc.state() == QProcess::Running && !m_stopRequested)
                    mproc.waitForFinished(100);
                if (m_stopRequested) mproc.kill();
                // OpenSees 3.8.0 peut écrire les « puts » Tcl sur l'un ou l'autre flux.
                const QString out = QString::fromUtf8(mproc.readAllStandardOutput());
                const QString errOut = QString::fromUtf8(mproc.readAllStandardError());
                if (out.contains("TSA_OPS_MATRICES_DONE") || errOut.contains("TSA_OPS_MATRICES_DONE"))
                {
                    std::string rerr;
                    matrixOk = OpenSeesResultsReader::readMatrixResults(workDirPath.toStdString(), snapshot, opsMap,
                                                                        localParams, withK, m_results, &rerr);
                    matrixErr = QString::fromStdString(rerr);
                }
                else
                {
                    matrixErr = tr("le passage matrices ne s'est pas terminé : %1").arg(errOut.trimmed().right(800));
                }
            }
            else
            {
                matrixErr = mproc.errorString();
            }
        }
        m_results.advanced().matrixRunDurationMs = static_cast<double>(timer.elapsed());
        if (!matrixOk)
        {
            m_results.advanced().warnings.push_back("Extraction des matrices impossible : " + matrixErr.toStdString());
            emit logReceived(QString("[AVERTISSEMENT] Extraction des matrices impossible : %1").arg(matrixErr));
        }
        else
        {
            emit logReceived(QString("[TSA] Matrices extraites en %1 ms (%2 équations, K_global %3).")
                .arg(m_results.advanced().matrixRunDurationMs)
                .arg(m_results.advanced().dofMap.equationCount())
                .arg(m_results.advanced().hasGlobalStiffness ? tr("disponible") : tr("non extraite")));
        }
    }

    // Traçabilité des métadonnées normatives d'exécution
    auto meta = m_results.executionMetadata();
    meta.solverEngine = "OpenSees";
    meta.solverVersion = OpenSeesManager::instance().versionInfo().versionString.toStdString();
    if (meta.solverVersion.empty()) meta.solverVersion = "3.8.0";
    meta.nationalAnnex = TSA::Standards::NationalAnnexConfig::instance().annexName().toStdString();
    meta.normativeFramework = "EN 1990:2002+A1:2005 / ISO/IEC 25010";
    if (params.targetCombinationId > 0)
    {
        meta.loadCombinationType = "Combinaison #" + std::to_string(params.targetCombinationId);
    }
    else if (params.targetLoadCaseId > 0)
    {
        meta.loadCombinationType = "Cas de charge #" + std::to_string(params.targetLoadCaseId);
    }
    else
    {
        meta.loadCombinationType = "Statique Linéaire";
    }
    m_results.setExecutionMetadata(meta);
    m_results.setAvailability(m_results.availabilityFromData());

    emit progressChanged(100, tr("Calcul et post-traitement terminés avec succès."));
    return true;
}

} // namespace TSA::Analysis
