#pragma once

// Pont vers la couche IA Python (python/tsa_ai, docs/AI_OPERATOR.md) : processus séparé « python -m tsa_ai serve »,
// une requête JSON par ligne sur son entrée, un événement JSON par ligne sur sa sortie. Asynchrone (QProcess) : le
// thread de l'interface n'attend jamais le réseau ni l'inférence. Python agit sur le projet uniquement par le canal
// d'automatisation et l'opérateur du document (mêmes commandes, même politique, même vérification que le panneau).
// La clé API n'est transmise que dans l'environnement du processus, jamais dans un message ni un journal.

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace TSA::AI
{

class PythonAIBridge : public QObject
{
    Q_OBJECT

public:
    explicit PythonAIBridge(QObject* parent = nullptr);
    ~PythonAIBridge() override;

    /// Dossier du paquet tsa_ai : variable TSA_AI_PYTHON_PATH, puis <app>/python, puis les dossiers parents (développement).
    static QString packageRoot();
    /// Interpréteur : variable TSA_AI_PYTHON, puis python / python3 / py du PATH.
    static QString findPython();

    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }
    /// Démarre le service sur le canal d'automatisation donné ; *error : motif si impossible.
    bool start(const QString& pipeName, const QString& apiKey, QString* error = nullptr);
    void stop();

    void configure(const QString& preset, const QString& model, const QString& baseUrl);
    void chat(const QString& text);
    void cancel();
    void requestHardware(const QString& mode = QStringLiteral("auto"));

signals:
    /// Événement du service (ready, configured, action, round, final, error, hardware) — QJsonObject tel que reçu.
    void event(const QJsonObject& event);
    /// Le processus s'est arrêté ou n'a pas pu démarrer.
    void stopped(const QString& reason);

private:
    void send(const QJsonObject& message);
    void onReadyRead();

    QProcess m_process;
    QByteArray m_buffer;
};

} // namespace TSA::AI
