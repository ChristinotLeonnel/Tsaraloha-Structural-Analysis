#include "PythonAIBridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace TSA::AI
{

PythonAIBridge::PythonAIBridge(QObject* parent) : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &PythonAIBridge::onReadyRead);
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString err = QString::fromUtf8(m_process.readAllStandardError()).trimmed().right(600);
        emit stopped(status == QProcess::CrashExit ? QStringLiteral("service Python arrêté brutalement. %1").arg(err)
                                                   : QStringLiteral("service Python terminé (code %1). %2").arg(code).arg(err));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) emit stopped(QStringLiteral("Python n'a pas pu être lancé : %1").arg(m_process.errorString()));
    });
}

PythonAIBridge::~PythonAIBridge()
{
    stop();
}

QString PythonAIBridge::packageRoot()
{
    const QString env = qEnvironmentVariable("TSA_AI_PYTHON_PATH");
    if (!env.isEmpty() && QFileInfo::exists(env + "/tsa_ai/__init__.py")) return QDir(env).absolutePath();
    QDir dir(QCoreApplication::applicationDirPath());
    for (int up = 0; up < 4; ++up)
    {
        if (QFileInfo::exists(dir.filePath("python/tsa_ai/__init__.py"))) return dir.absoluteFilePath("python");
        if (!dir.cdUp()) break;
    }
    return {};
}

QString PythonAIBridge::findPython()
{
    const QString env = qEnvironmentVariable("TSA_AI_PYTHON");
    if (!env.isEmpty() && QFileInfo(env).isExecutable()) return env;
#ifdef Q_OS_WIN
    // « py » d'abord : le lanceur n'existe qu'avec un vrai Python, alors que « python » peut être l'alias de
    // WindowsApps qui ne fait qu'inviter à installer Python depuis le Microsoft Store (code 9009).
    for (const char* name : { "py", "python", "python3" })
#else
    for (const char* name : { "python", "python3", "py" })
#endif
        if (const QString path = QStandardPaths::findExecutable(QString::fromLatin1(name)); !path.isEmpty()) return path;
    return {};
}

bool PythonAIBridge::start(const QString& pipeName, const QString& apiKey, QString* error)
{
    if (isRunning()) return true;
    const QString root = packageRoot(), python = findPython();
    if (root.isEmpty() || python.isEmpty())
    {
        if (error)
            *error = root.isEmpty() ? QStringLiteral("paquet python/tsa_ai introuvable (TSA_AI_PYTHON_PATH)")
                                    : QStringLiteral("Python introuvable (installer Python 3.9 ou plus, ou TSA_AI_PYTHON)");
        return false;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONPATH", root);
    env.insert("PYTHONIOENCODING", "utf-8");
    if (!apiKey.isEmpty()) env.insert("TSA_AI_API_KEY", apiKey); // environnement du processus seulement
    m_process.setProcessEnvironment(env);
    m_process.setWorkingDirectory(root);
    m_buffer.clear();
    m_process.start(python, { "-m", "tsa_ai", "--pipe", pipeName, "serve" });
    if (!m_process.waitForStarted(10000))
    {
        if (error) *error = QStringLiteral("démarrage de Python impossible : %1").arg(m_process.errorString());
        return false;
    }
    return true;
}

void PythonAIBridge::stop()
{
    if (!isRunning()) return;
    send(QJsonObject { { "type", "quit" } });
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(3000)) m_process.kill();
}

void PythonAIBridge::configure(const QString& preset, const QString& model, const QString& baseUrl)
{
    send(QJsonObject { { "type", "configure" }, { "preset", preset }, { "model", model }, { "base_url", baseUrl } });
}

void PythonAIBridge::chat(const QString& text) { send(QJsonObject { { "type", "chat" }, { "text", text } }); }
void PythonAIBridge::cancel() { send(QJsonObject { { "type", "cancel" } }); }
void PythonAIBridge::requestHardware(const QString& mode) { send(QJsonObject { { "type", "hardware" }, { "mode", mode } }); }

void PythonAIBridge::send(const QJsonObject& message)
{
    if (!isRunning()) return;
    m_process.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void PythonAIBridge::onReadyRead()
{
    m_buffer += m_process.readAllStandardOutput();
    for (qsizetype nl = m_buffer.indexOf('\n'); nl >= 0; nl = m_buffer.indexOf('\n'))
    {
        const QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject())
            emit event(doc.object());
        else
            emit event(QJsonObject { { "type", "error" }, { "kind", "malformed" }, { "message", QString::fromUtf8(line.left(200)) } });
    }
}

} // namespace TSA::AI
