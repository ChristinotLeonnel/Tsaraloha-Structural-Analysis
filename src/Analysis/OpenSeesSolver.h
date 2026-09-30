#pragma once

#include "CalculationSnapshot.h"
#include "OpenSeesAnalysisBuilder.h"
#include "ResultsModel.h"
#include "OpenSeesManager.h"

#include <QObject>
#include <QString>
#include <QProcess>
#include <memory>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{

/**
 * @brief Exécuteur asynchrone et non-bloquant du solveur structural OpenSees.
 * Gère le cycle de vie du processus, le journal de convergence en temps réel,
 * l'extraction des résultats et l'invalidation automatique.
 */
class OpenSeesSolver : public QObject
{
    Q_OBJECT

public:
    explicit OpenSeesSolver(QObject* parent = nullptr);
    ~OpenSeesSolver() override;

    bool isRunning() const { return m_isRunning; }
    const ResultsModel& results() const { return m_results; }
    ResultsModel& results() { return m_results; }

    /**
     * @brief Lance l'analyse de manière synchrone (bloquante pour la fonction appelante, utile pour les tests).
     */
    bool solveSynchronous(const TSA::Model::Model& model,
                          const AnalysisParameters& params = {},
                          QString* errorMessage = nullptr);

    /**
     * @brief Lance l'analyse de manière asynchrone dans un thread de travail (l'UI Qt reste 100% fluide).
     */
    void solveAsync(const TSA::Model::Model& model,
                    const AnalysisParameters& params = {});

    /**
     * @brief Interrompt le calcul en cours si actif.
     */
    void stop();

signals:
    void analysisStarted();
    void progressChanged(int percent, const QString& statusMessage);
    void logReceived(const QString& line);
    void analysisFinished(bool success, const QString& message);

private:
    bool executeWorkflow(const CalculationSnapshot& snapshot,
                         const AnalysisParameters& params,
                         QString* errorMessage);

    bool m_isRunning = false;
    bool m_stopRequested = false;
    QProcess* m_process = nullptr;
    ResultsModel m_results;
};

} // namespace TSA::Analysis
