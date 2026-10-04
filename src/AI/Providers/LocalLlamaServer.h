#pragma once

// Processus llama-server piloté par TSA (même principe qu'OpenSees : exécutable externe via QProcess).
// L'inférence tourne hors du processus TSA : un plantage ou une saturation mémoire du moteur IA
// ne peut ni bloquer l'interface, ni le viewport, ni le solveur.

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;

namespace TSA::AI
{

struct LlamaServerConfig
{
    QString executable;
    QString modelPath;
    QString alias = QStringLiteral("tsa-local");
    int contextSize = 8192;
    QString deviceId;        // « Vulkan1 », « CUDA0 »… ; vide = CPU uniquement
    int gpuLayers = -1;      // -1 = répartition automatique (--fit) ; 0 = CPU ; n = imposé
    int threads = 0;         // 0 = défaut llama.cpp
    int startupTimeoutMs = 240000;
};

class LocalLlamaServer : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Starting, Ready, Failed };
    Q_ENUM(State)

    explicit LocalLlamaServer(QObject* parent = nullptr);
    ~LocalLlamaServer() override;

    /// Arguments de la ligne de commande (exposé pour les tests).
    static QStringList buildArguments(const LlamaServerConfig& config, int port);

    void start(const LlamaServerConfig& config);
    void stop();

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    QUrl baseUrl() const;               // http://127.0.0.1:<port>/v1
    const LlamaServerConfig& config() const { return m_config; }
    QString lastError() const { return m_lastError; }
    QStringList recentLog() const { return m_log; }

signals:
    void stateChanged(TSA::AI::LocalLlamaServer::State state);
    void logLine(const QString& line);

private:
    void setState(State s);
    void pollHealth();
    static int findFreePort();

private:
    LlamaServerConfig m_config;
    QPointer<QProcess> m_process;
    QNetworkAccessManager* m_network = nullptr;
    QTimer* m_healthTimer = nullptr;
    qint64 m_startedAtMs = 0;
    int m_port = 0;
    State m_state = State::Stopped;
    QString m_lastError;
    QStringList m_log;
};

} // namespace TSA::AI
