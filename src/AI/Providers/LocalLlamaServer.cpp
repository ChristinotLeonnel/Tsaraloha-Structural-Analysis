#include "LocalLlamaServer.h"
#include "ProcessLifetime.h"

#include <QDateTime>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTimer>

namespace TSA::AI
{

LocalLlamaServer::LocalLlamaServer(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_healthTimer(new QTimer(this))
{
    m_healthTimer->setInterval(500);
    connect(m_healthTimer, &QTimer::timeout, this, &LocalLlamaServer::pollHealth);
}

LocalLlamaServer::~LocalLlamaServer()
{
    stop();
}

QStringList LocalLlamaServer::buildArguments(const LlamaServerConfig& c, int port)
{
    QStringList a{
        "-m", c.modelPath,
        "--host", "127.0.0.1",       // jamais exposé sur le réseau
        "--port", QString::number(port),
        "--alias", c.alias,
        "-c", QString::number(c.contextSize),
        "--jinja",                    // modèles de conversation + appels d'outils
    };
    if (c.deviceId.isEmpty() || c.gpuLayers == 0)
    {
        a << "--device" << "none" << "-ngl" << "0";
    }
    else
    {
        a << "--device" << c.deviceId;
        if (c.gpuLayers > 0)
            a << "-ngl" << QString::number(c.gpuLayers);
        else
            a << "--fit" << "on"; // llama.cpp répartit lui-même les couches selon la VRAM libre
    }
    if (c.threads > 0) a << "-t" << QString::number(c.threads);
    return a;
}

int LocalLlamaServer::findFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) return 0;
    const int port = probe.serverPort();
    probe.close();
    return port;
}

QUrl LocalLlamaServer::baseUrl() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/v1").arg(m_port));
}

void LocalLlamaServer::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void LocalLlamaServer::start(const LlamaServerConfig& config)
{
    stop();
    m_config = config;
    m_lastError.clear();
    m_log.clear();

    if (!QFileInfo(config.executable).isExecutable())
    {
        m_lastError = QStringLiteral("Moteur d'inférence local introuvable (llama-server).");
        setState(State::Failed);
        return;
    }
    if (!QFileInfo::exists(config.modelPath))
    {
        m_lastError = QStringLiteral("Fichier modèle introuvable : %1").arg(config.modelPath);
        setState(State::Failed);
        return;
    }
    m_port = findFreePort();
    if (m_port == 0)
    {
        m_lastError = QStringLiteral("Aucun port local disponible.");
        setState(State::Failed);
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(QFileInfo(config.executable).absolutePath());
    connect(m_process, &QProcess::readyRead, this, [this] {
        if (!m_process) return;
        const QStringList lines = QString::fromLocal8Bit(m_process->readAll()).split('\n', Qt::SkipEmptyParts);
        for (const QString& raw : lines)
        {
            const QString line = raw.trimmed();
            m_log << line;
            emit logLine(line);
        }
        while (m_log.size() > 300) m_log.removeFirst();
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        m_healthTimer->stop();
        if (m_state == State::Stopped) return;
        m_lastError = QStringLiteral("Le moteur IA s'est arrêté (code %1). %2").arg(code).arg(m_log.isEmpty() ? QString() : m_log.last());
        setState(State::Failed);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_healthTimer->stop();
        m_lastError = QStringLiteral("Impossible de démarrer llama-server.");
        setState(State::Failed);
    });

    setState(State::Starting);
    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
    connect(m_process, &QProcess::started, this, [this] {
        if (m_process) bindToParentLifetime(m_process->processId()); // pas d'orphelin si TSA plante
    });
    m_process->start(config.executable, buildArguments(config, m_port));
    m_healthTimer->start();
}

void LocalLlamaServer::pollHealth()
{
    if (m_state != State::Starting) { m_healthTimer->stop(); return; }
    if (QDateTime::currentMSecsSinceEpoch() - m_startedAtMs > m_config.startupTimeoutMs)
    {
        m_lastError = QStringLiteral("Le chargement du modèle dépasse %1 s.").arg(m_config.startupTimeoutMs / 1000);
        stop();
        setState(State::Failed);
        return;
    }
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/health").arg(m_port)));
    req.setTransferTimeout(2000);
    QNetworkReply* reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http == 200 && m_state == State::Starting)
        {
            m_healthTimer->stop();
            setState(State::Ready);
        }
    });
}

void LocalLlamaServer::stop()
{
    m_healthTimer->stop();
    const bool wasRunning = m_process && m_process->state() != QProcess::NotRunning;
    State previous = m_state;
    m_state = State::Stopped; // avant kill : la fin du processus n'est pas une panne
    if (m_process)
    {
        m_process->disconnect(this);
        if (wasRunning)
        {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    if (previous != State::Stopped) emit stateChanged(State::Stopped);
}

} // namespace TSA::AI
