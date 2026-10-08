#include "AnalysisController.h"

#include "ResultsModel.h"
#include "ResultsValidityGuard.h"
#include "../Model/Model.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>

namespace TSA::Analysis
{

AnalysisController::AnalysisController(TSA::Model::Model* model, const TSA::Grid::GridManager* grids, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_grids(grids)
    , m_registry(std::make_unique<AnalysisEngineRegistry>())
    , m_guard(std::make_unique<ResultsValidityGuard>(model))
    , m_lastRun(std::make_shared<AnalysisRunResult>())
{
    registerBuiltInEngines(*m_registry);
    m_manager = std::make_unique<AnalysisManager>(*m_registry);
    if (!m_registry->ids().empty()) m_context.engineId = m_registry->ids().front();

    // Différé : le garde est appelé au milieu d'une notification du modèle ; les vues ne doivent pas
    // être rafraîchies pendant celle-ci.
    QPointer<AnalysisController> self(this);
    m_guard->setStaleCallback([self] {
        if (self) QTimer::singleShot(0, self, [self] { if (self) emit self->resultsBecameStale(); });
    });
}

AnalysisController::~AnalysisController()
{
    if (m_worker)
    {
        // Le thread lit le modèle préparé (copie) et le moteur : il doit finir avant leur destruction.
        m_manager->cancel(m_runningEngine);
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }
}

bool AnalysisController::restoreContextFromModel()
{
    AnalysisContext context;
    bool ok = true;
    if (m_model && !m_model->analysisSettingsJson().empty())
    {
        ok = false;
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(m_model->analysisSettingsJson()));
        if (doc.isObject()) context = AnalysisContext::fromJson(doc.object(), &ok);
        if (!ok) context = AnalysisContext();
    }
    // Moteur absent de cette installation : premier moteur disponible.
    if (!m_registry->engine(context.engineId) && !m_registry->ids().empty())
        context.engineId = m_registry->ids().front();
    m_context = context;
    return ok;
}

bool AnalysisController::storeContextInModel()
{
    if (!m_model) return false;
    const std::string json = QJsonDocument(m_context.toJson()).toJson(QJsonDocument::Compact).toStdString();
    if (json == m_model->analysisSettingsJson()) return false;
    m_model->setAnalysisSettingsJson(json);
    m_model->setModified(true);   // réglages enregistrés avec le projet
    return true;
}

PreparedAnalysis AnalysisController::prepare(const AnalysisContext& context) const
{
    if (!m_model) return {};
    return m_manager->prepare(*m_model, m_grids, context);
}

AnalysisRunCallbacks AnalysisController::makeCallbacks()
{
    // Appelés depuis le thread de travail : émission directe, mise en file par les connexions.
    AnalysisRunCallbacks callbacks;
    callbacks.log = [this](const std::string& line) { emit logMessage(QString::fromStdString(line)); };
    callbacks.progress = [this](int pct, const std::string& status) {
        emit progressChanged(pct, QString::fromStdString(status));
    };
    return callbacks;
}

bool AnalysisController::start(const AnalysisContext& context, const PreparedAnalysis& prepared)
{
    if (m_worker || !prepared.canRun()) return false;
    const AnalysisEngine* engine = m_registry->engine(context.engineId);
    if (!engine) return false;

    m_runningEngine = context.engineId;
    auto job = std::make_shared<const PreparedAnalysis>(prepared);
    auto result = std::make_shared<AnalysisRunResult>();
    m_lastRun = result;
    const AnalysisRunCallbacks callbacks = makeCallbacks();
    m_worker = QThread::create([this, context, job, result, callbacks] {
        *result = m_manager->run(context, *job, callbacks);
    });
    connect(m_worker, &QThread::finished, this, &AnalysisController::onWorkerFinished);
    emit started(QString::fromStdString(engine->info().name));
    m_worker->start();
    return true;
}

bool AnalysisController::start(QString* error)
{
    if (m_worker)
    {
        if (error) *error = tr("Un calcul est déjà en cours.");
        return false;
    }
    const PreparedAnalysis prepared = prepare();
    if (!prepared.canRun())
    {
        if (error)
        {
            QStringList errors;
            for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) errors << QString::fromStdString(e);
            *error = errors.isEmpty() ? tr("Le modèle d'analyse n'a pas pu être extrait.") : errors.join(QStringLiteral("\n"));
        }
        return false;
    }
    if (!start(m_context, prepared))
    {
        if (error) *error = tr("Aucun moteur d'analyse sélectionné.");
        return false;
    }
    return true;
}

void AnalysisController::cancel()
{
    if (m_worker) m_manager->cancel(m_runningEngine);
}

void AnalysisController::onWorkerFinished()
{
    if (!m_worker) return;
    m_worker->wait();
    m_worker->deleteLater();
    m_worker = nullptr;

    AnalysisRunResult& r = *m_lastRun;
    if (r.success) publishResults(std::make_shared<ResultsModel>(std::move(r.results)));
    emit finished(r.success, QString::fromStdString(r.message));
}

AnalysisRunResult AnalysisController::runBlocking(const AnalysisContext& context, const PreparedAnalysis& prepared)
{
    if (m_worker)
    {
        AnalysisRunResult busy;
        busy.message = "Un calcul est déjà en cours.";
        return busy;
    }
    m_runningEngine = context.engineId;
    *m_lastRun = m_manager->run(context, prepared, makeCallbacks());
    AnalysisRunResult summary;
    summary.success = m_lastRun->success;
    summary.message = m_lastRun->message;
    if (m_lastRun->success) publishResults(std::make_shared<ResultsModel>(std::move(m_lastRun->results)));
    else summary.results = m_lastRun->results;   // journal du moteur en cas d'échec
    return summary;
}

bool AnalysisController::resultsUpToDate() const
{
    return m_results && m_guard->resultsUpToDate();
}

void AnalysisController::publishResults(const std::shared_ptr<ResultsModel>& results)
{
    m_results = results;
    if (m_results) m_guard->trackResults(m_results);
    else m_guard->clearResults();
    emit resultsChanged();
}

void AnalysisController::clearResults()
{
    publishResults(nullptr);
}

} // namespace TSA::Analysis
